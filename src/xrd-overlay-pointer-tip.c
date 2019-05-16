/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gdk/gdk.h>
#include "xrd-overlay-pointer-tip.h"
#include "openvr-math.h"
#include "xrd-settings.h"
#include "xrd-math.h"
#include "graphene-ext.h"
#include "xrd-pointer-tip.h"

typedef struct XrdPointerTipAnimation
{
  XrdPointerTip *tip;
  float progress;
  guint callback_id;
} XrdPointerTipAnimation;


/*
 * Since the pulse animation surrounds the tip and would
 * exceed the canvas size, we need to scale it to fit the pulse.
 */
#define VIEWPORT_SCALE 3

/*
 * The distance in meters for which apparent size and regular size are equal.
 */
#define APPARENT_SIZE_DISTANCE 3.0f

struct _XrdOverlayPointerTip
{
  OpenVROverlay parent;
  GulkanTexture *texture;
  gboolean active;

  gboolean keep_apparent_size;
  float width_meters;

  /* Pointer to the data of the currently running animation.
   * Must be freed when an animation callback is cancelled. */
  XrdPointerTipAnimation *animation;

  OpenVROverlayUploader *uploader;

  graphene_point3d_t active_color;
  graphene_point3d_t passive_color;

  double pulse_alpha;

  int texture_width;
  int texture_height;
};

static void
xrd_overlay_pointer_tip_interface_init (XrdPointerTipInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdOverlayPointerTip, xrd_overlay_pointer_tip,
                         OPENVR_TYPE_OVERLAY,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_POINTER_TIP,
                                                xrd_overlay_pointer_tip_interface_init))

static void
xrd_overlay_pointer_tip_finalize (GObject *gobject);

static void
xrd_overlay_pointer_tip_class_init (XrdOverlayPointerTipClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_pointer_tip_finalize;
}

static void
_set_width_meters (XrdPointerTip *tip, float meters)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_set_width_meters (OPENVR_OVERLAY(self), meters);
}

static void
xrd_overlay_pointer_tip_init (XrdOverlayPointerTip *self)
{
  self->active = FALSE;
  self->texture = NULL;
  self->animation = NULL;
}

/** draws a circle in the center of a cairo surface of dimensions WIDTHxHEIGHT.
 * scale affects the radius of the circle and should be in [0,2].
 * a_in is the alpha value at the center, a_out at the outer border. */
static void
_draw_gradient_circle (cairo_t              *cr,
                       int                   w,
                       int                   h,
                       double                radius,
                       graphene_point3d_t   *color,
                       double                a_in,
                       double                a_out)
{
  double center_x = w / 2;
  double center_y = h / 2;

  cairo_pattern_t *pat = cairo_pattern_create_radial (center_x, center_y,
                                                      0.75 * radius,
                                                      center_x, center_y,
                                                      radius);
  cairo_pattern_add_color_stop_rgba (pat, 0,
                                     color->x, color->y, color->z, a_in);

  cairo_pattern_add_color_stop_rgba (pat, 1,
                                     color->x, color->y, color->z, a_out);
  cairo_set_source (cr, pat);
  cairo_arc (cr, center_x, center_y, radius, 0, 2 * M_PI);
  cairo_fill (cr);
  cairo_pattern_destroy (pat);
}

static GdkPixbuf*
_render_cairo (int                 w,
               int                 h,
               double              radius,
               graphene_point3d_t *color,
               double              pulse_alpha,
               float               progress)
{
  cairo_surface_t *surface =
      cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);

  cairo_t *cr = cairo_create (surface);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  /* Draw pulse */
  if (progress != 1.0)
    {
      float pulse_scale = VIEWPORT_SCALE * (1.0f - progress);
      graphene_point3d_t white = { 1.0f, 1.0f, 1.0f };
      _draw_gradient_circle (cr, w, h, radius * pulse_scale, &white,
                             pulse_alpha, 0.0);
    }

  cairo_set_operator (cr, CAIRO_OPERATOR_MULTIPLY);

  /* Draw tip */
  _draw_gradient_circle (cr, w, h, radius, color, 1.0, 0.0);

  cairo_destroy (cr);

  /* Since we cannot set a different format for raw upload,
   * we need to use GdkPixbuf to suit OpenVRs needs */
  GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, w, h);

  cairo_surface_destroy (surface);

  return pixbuf;
}

/** _render:
 * Renders the pointer tip with the desired colors.
 * If background scale is > 1, a transparent white background circle is rendered
 * behind the pointer tip. */
GdkPixbuf*
_render (XrdOverlayPointerTip *self,
         float                 progress)
{
  int w = self->texture_width * VIEWPORT_SCALE;
  int h = self->texture_height * VIEWPORT_SCALE;

  graphene_point3d_t *color =
    self->active ? &self->active_color : &self->passive_color;

  double radius = self->texture_width / 2.0;

  GdkPixbuf *pixbuf =
    _render_cairo (w, h, radius, color, self->pulse_alpha, progress);

  return pixbuf;
}

gboolean
_animate_cb (gpointer _animation)
{
  XrdPointerTipAnimation *animation = (XrdPointerTipAnimation *) _animation;
  XrdPointerTip *tip = animation->tip;
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);

  GulkanClient *client = GULKAN_CLIENT (self->uploader);

  GdkPixbuf* pixbuf =_render (self, animation->progress);
  gulkan_client_upload_pixbuf (client, self->texture, pixbuf);
  g_object_unref (pixbuf);

  openvr_overlay_uploader_submit_frame (self->uploader,
                                        OPENVR_OVERLAY (self),
                                        self->texture);

  animation->progress += 0.05f;

  if (animation->progress > 1)
    {
      self->animation = NULL;
      g_free (animation);
      return FALSE;
    }

  return TRUE;
}

static void
_animate_pulse (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);

  if (self->animation != NULL)
    {
      xrd_pointer_tip_set_active (XRD_POINTER_TIP (self), self->active);
    }

  self->animation = g_malloc (sizeof (XrdPointerTipAnimation));
  self->animation->progress = 0;
  self->animation->tip = XRD_POINTER_TIP (self);
  self->animation->callback_id = g_timeout_add (20, _animate_cb,
                                                self->animation);
}

static void
_update_width_meters (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;

  self->width_meters = g_settings_get_double (settings, key) * VIEWPORT_SCALE;

  if (self->keep_apparent_size)
      xrd_pointer_tip_set_constant_width (XRD_POINTER_TIP (self));
  else
    openvr_overlay_set_width_meters (OPENVR_OVERLAY (self), self->width_meters);
}

static void
_update_keep_apparent_size (GSettings *settings, gchar *key, gpointer _self)
{
  XrdOverlayPointerTip *self = _self;
  self->keep_apparent_size = g_settings_get_boolean (settings, key);
  if (self->keep_apparent_size)
    xrd_pointer_tip_set_constant_width (XRD_POINTER_TIP (self));
  else
    openvr_overlay_set_width_meters (OPENVR_OVERLAY (self), self->width_meters);
}

static void
_update_active_color (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;
  GVariant *var = g_settings_get_value (settings, key);

  double r, g, b;
  g_variant_get (var, "(ddd)", &r, &g, &b);
  graphene_point3d_init (&self->active_color, r, g, b);
}

static void
_update_passive_color (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;
  GVariant *var = g_settings_get_value (settings, key);

  double r, g, b;
  g_variant_get (var, "(ddd)", &r, &g, &b);
  graphene_point3d_init (&self->passive_color, r, g, b);
}

static void
_init_texture (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);

  GulkanClient *client = GULKAN_CLIENT (self->uploader);

  GdkPixbuf* pixbuf = _render (self, 1.0f);

  if (self->texture)
    g_object_unref (self->texture);

  self->texture =
    gulkan_texture_new_from_pixbuf (client->device, pixbuf,
                                    VK_FORMAT_R8G8B8A8_UNORM);
  gulkan_client_upload_pixbuf (client, self->texture, pixbuf);
  g_object_unref (pixbuf);
}

static void
_update_texture_res (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;
  GVariant *texture_res = g_settings_get_value (settings, key);
  g_variant_get (texture_res, "(ii)",
                 &self->texture_width, &self->texture_height);

  _init_texture (XRD_POINTER_TIP (self));
}

static void
_update_pulse_alpha (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;
  self->pulse_alpha = g_settings_get_double (settings, key);
}

static void
_init_settings (XrdOverlayPointerTip *self)
{
  /* tip resolution config has to happen after self->uploader gets set */
  xrd_settings_connect_and_apply (G_CALLBACK (_update_texture_res),
                                  "pointer-tip-resolution", self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_passive_color),
                                  "pointer-tip-passive-color", self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_active_color),
                                  "pointer-tip-active-color", self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_pulse_alpha),
                                  "pointer-tip-pulse-alpha", self);

  xrd_settings_connect_and_apply (G_CALLBACK
                                  (_update_keep_apparent_size),
                                  "pointer-tip-keep-apparent-size",
                                  self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_width_meters),
                                  "pointer-tip-width-meters", self);
}



XrdOverlayPointerTip *
xrd_overlay_pointer_tip_new (int controller_index,
                             OpenVROverlayUploader  *uploader)
{
  XrdOverlayPointerTip *self =
    (XrdOverlayPointerTip*) g_object_new (XRD_TYPE_OVERLAY_POINTER_TIP, 0);

  /* our uploader ref needs to stay valid as long as pointer tip exists */
  g_object_ref (uploader);
  self->uploader = uploader;

  char key[k_unVROverlayMaxKeyLength];
  snprintf (key, k_unVROverlayMaxKeyLength - 1, "intersection-%d",
            controller_index);

  /* the texture has 2x the size of the pointer, so the overlay should be 2x
   * the desired size of the default pointer too. */
  openvr_overlay_create (OPENVR_OVERLAY (self), key, key);

  if (!openvr_overlay_is_valid (OPENVR_OVERLAY (self)))
    {
      g_printerr ("Intersection overlay unavailable.\n");
      return NULL;
    }

  /*
   * The crosshair should always be visible, except the pointer can
   * occlude it. The pointer has max sort order, so the crosshair gets max -1
   */
  openvr_overlay_set_sort_order (OPENVR_OVERLAY (self), UINT32_MAX - 1);

  _init_settings (self);

  return self;
}

/** xrd_overlay_pointer_tip_set_active:
 * Changes whether the active or inactive style is rendered.
 * Also cancels animations. */
static void
_set_active (XrdPointerTip *tip,
             gboolean       active)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);

  if (self->texture == NULL)
    return;

  if (self->animation != NULL)
    {
      g_source_remove (self->animation->callback_id);
      g_free (self->animation);
      self->animation = NULL;
    }

  /* Do not skip renderint to the texture even when self->active == active.
   * An animation changes the texture, so when an animation is cancelled, we
   * want to re-render the current state. */

  GulkanClient *client = GULKAN_CLIENT (self->uploader);

  self->active = active;
  GdkPixbuf* pixbuf = _render (self, 1.0f);

  gulkan_client_upload_pixbuf (client, self->texture, pixbuf);
  g_object_unref (pixbuf);

  openvr_overlay_uploader_submit_frame (self->uploader, OPENVR_OVERLAY (self),
                                        self->texture);
}

static void
xrd_overlay_pointer_tip_finalize (GObject *gobject)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (gobject);
  (void) self;

  /* release the ref we set in pointer tip init */
  g_object_unref (self->uploader);
  if (self->texture)
    g_object_unref (self->texture);

  G_OBJECT_CLASS (xrd_overlay_pointer_tip_parent_class)->finalize (gobject);
}

/* note: Move pointer tip to the desired location before calling. */
static void
_update_apparent_size (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);

  if (!self->keep_apparent_size)
    return;

  graphene_matrix_t tip_pose;
  openvr_overlay_get_transform_absolute (OPENVR_OVERLAY(self), &tip_pose);
;
  graphene_point3d_t tip_point;
  graphene_matrix_get_translation_point3d (&tip_pose, &tip_point);

  graphene_matrix_t hmd_pose;
  gboolean has_pose = openvr_system_get_hmd_pose (&hmd_pose);
  if (!has_pose)
    {
      openvr_overlay_set_width_meters (OPENVR_OVERLAY(self),
                                       self->width_meters);
      return;
    }

  graphene_point3d_t hmd_point;
  graphene_matrix_get_translation_point3d (&hmd_pose, &hmd_point);

  float distance = graphene_point3d_distance (&tip_point, &hmd_point, NULL);

  float new_width = self->width_meters / APPARENT_SIZE_DISTANCE * distance;

  openvr_overlay_set_width_meters (OPENVR_OVERLAY(self), new_width);
}

static void
_set_transformation (XrdPointerTip     *tip,
                     graphene_matrix_t *matrix)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), matrix);
}

static void
_get_transformation (XrdPointerTip     *tip,
                     graphene_matrix_t *matrix)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_get_transform_absolute (OPENVR_OVERLAY(self), matrix);
}


static void
_show (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_show (OPENVR_OVERLAY (self));
}

static void
_hide (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_hide (OPENVR_OVERLAY (self));
}

static void
_submit_texture (XrdPointerTip *tip,
                 GulkanClient  *client,
                 GulkanTexture *texture)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_uploader_submit_frame (OPENVR_OVERLAY_UPLOADER (client),
                                        OPENVR_OVERLAY (self),
                                        texture);
}

static void
xrd_overlay_pointer_tip_interface_init (XrdPointerTipInterface *iface)
{
  iface->set_constant_width = _update_apparent_size;
  iface->set_active = _set_active;
  iface->animate_pulse = _animate_pulse;
  iface->set_transformation = _set_transformation;
  iface->get_transformation = _get_transformation;
  iface->show = _show;
  iface->hide = _hide;
  iface->set_width_meters = _set_width_meters;
  iface->submit_texture = _submit_texture;
}

