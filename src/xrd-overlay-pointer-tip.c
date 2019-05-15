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

struct _XrdOverlayPointerTip
{
  OpenVROverlay parent;
  GulkanTexture *texture;
  gboolean active;


  gboolean use_constant_apparent_width;
  float overlay_width;

  /* 0, or the id of the currently running animation. */
  guint animation_callback_id;

  /* Pointer to the data of the currently running animation.
   * Must be freed when an animation callback is cancelled. */
  struct Animation *animation_data;

  OpenVROverlayUploader  *uploader;

  double inactive_r;
  double inactive_g;
  double inactive_b;
  double active_r;
  double active_g;
  double active_b;
  double background_alpha;

  int tip_width;
  int tip_height;

  /* How much bigger the texture should be than the tip itsef.
   * Used for animations. */
  int texture_size_factor;
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
  self->animation_callback_id = 0;
  self->animation_data = NULL;
  /* TODO: setting? */
  self->texture_size_factor = 3;
}

typedef struct Animation
{
  XrdOverlayPointerTip *self;
  OpenVROverlayUploader *uploader;
  float progress;
} Animation;

/** draws a circle in the center of a cairo surface of dimensions WIDTHxHEIGHT.
 * scale affects the radius of the circle and should be in [0,2].
 * a_in is the alpha value at the center, a_out at the outer border. */
void
draw_gradient_circle (XrdOverlayPointerTip *self,
                      cairo_t *cr,
                      double r,
                      double g,
                      double b,
                      double a_in,
                      double a_out,
                      float  scale)
{
  int tex_width = self->tip_width * self->texture_size_factor;
  int tex_height = self->tip_height * self->texture_size_factor;

  double center_x = tex_width / 2;
  double center_y = tex_height / 2;

  double radius = (self->tip_width / 2.) * scale;

  cairo_pattern_t *pat = cairo_pattern_create_radial (center_x, center_y,
                                                      0.75 * radius,
                                                      center_x, center_y,
                                                      radius);
  cairo_pattern_add_color_stop_rgba (pat, 0, r, g, b, a_in);

  cairo_pattern_add_color_stop_rgba (pat, 1, r, g, b, a_out);
  cairo_set_source (cr, pat);
  cairo_arc (cr, center_x, center_y, radius, 0, 2 * M_PI);
  cairo_fill (cr);
  cairo_pattern_destroy (pat);
}

/** _render_tip_pixbuf:
 * Renders the pointer tip with the desired colors.
 * If background scale is > 1, a transparent white background circle is rendered
 * behind the pointer tip. */
GdkPixbuf*
_render_tip_pixbuf (XrdOverlayPointerTip *self,
                    double r, double g, double b, float background_scale)
{
  int tex_width = self->tip_width * self->texture_size_factor;
  int tex_height = self->tip_height * self->texture_size_factor;

  cairo_surface_t *surface =
      cairo_image_surface_create (CAIRO_FORMAT_ARGB32, tex_width, tex_height);

  cairo_t *cr = cairo_create (surface);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  draw_gradient_circle (self, cr, 1.0, 1.0, 1.0, self->background_alpha,
                        0.0, background_scale);

  cairo_set_operator (cr, CAIRO_OPERATOR_MULTIPLY);
  draw_gradient_circle (self, cr, r, g, b, 1.0, 0.0, 1.0);

  cairo_destroy (cr);

  /* Since we cannot set a different format for raw upload,
   * we need to use GdkPixbuf to suit OpenVRs needs */
  GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0,
                                                   tex_width, tex_height);

  cairo_surface_destroy (surface);

  return pixbuf;
}

/** _render_current_tip:
 * single place deciding how the rendering based on current state looks like */
GdkPixbuf*
_render_current_tip (XrdOverlayPointerTip *self, float background_scale)
{
  GdkPixbuf* pixbuf = self->active ?
      _render_tip_pixbuf (self, self->active_r, self->active_g,
                          self->active_b, background_scale) :
      _render_tip_pixbuf (self, self->inactive_r, self->inactive_g,
                          self->inactive_b, background_scale);
  return pixbuf;
}

gboolean
_animate_cb (gpointer _animation)
{
  Animation *animation = (Animation *) _animation;
  XrdOverlayPointerTip *self = animation->self;

  GulkanClient *client = GULKAN_CLIENT (animation->uploader);

  float background_scale = self->texture_size_factor *
                             (1 - animation->progress);

  GdkPixbuf* active_pixbuf =_render_current_tip (self, background_scale);

  gulkan_client_upload_pixbuf (client, animation->self->texture, active_pixbuf);

  g_object_unref (active_pixbuf);

  openvr_overlay_uploader_submit_frame (animation->uploader,
                                       OPENVR_OVERLAY (animation->self),
                                       animation->self->texture);

  animation->progress += 0.05f;

  if (animation->progress > 1)
    {
      animation->self->animation_callback_id = 0;
      animation->self->animation_data = NULL;
      g_free (animation);
      return FALSE;
    }

  return TRUE;
}

static void
_animate_pulse (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);

  if (self->animation_callback_id != 0)
    {
      xrd_pointer_tip_set_active (XRD_POINTER_TIP (self), self->active);
    }
  Animation *animation =  g_malloc (sizeof *animation);
  animation->progress = 0;
  animation->uploader = self->uploader;
  animation->self = self;
  self->animation_callback_id = g_timeout_add (20, _animate_cb, animation);
  self->animation_data = animation;
}

static void
_update_width (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;

  self->overlay_width = g_settings_get_double (settings, key);
  self->overlay_width *= self->texture_size_factor;

  if (self->use_constant_apparent_width)
      xrd_pointer_tip_set_constant_width (XRD_POINTER_TIP (self));
  else
    openvr_overlay_set_width_meters
        (OPENVR_OVERLAY (self), self->overlay_width);
}

static void
_update_use_constant_apparent_width (GSettings *settings, gchar *key,
                                     gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;
  self->use_constant_apparent_width = g_settings_get_boolean (settings, key);
  if (self->use_constant_apparent_width)
    xrd_pointer_tip_set_constant_width (XRD_POINTER_TIP (self));
  else
    openvr_overlay_set_width_meters
        (OPENVR_OVERLAY (self), self->overlay_width);
}

static void
_update_active_rgb (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;
  GVariant *active_rgb = g_settings_get_value (settings, key);
  g_variant_get (active_rgb, "(ddd)",
                 &self->active_r, &self->active_g, &self->active_b);
}

static void
_update_inactive_rgb (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;
  GVariant *inactive_rgb = g_settings_get_value (settings, key);
  g_variant_get (inactive_rgb, "(ddd)",
                 &self->inactive_r, &self->inactive_g, &self->inactive_b);
}

static void
_upload_current (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);

  GulkanClient *client = GULKAN_CLIENT (self->uploader);

  GdkPixbuf* pixbuf = _render_current_tip (self, 0.0);
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
                 &self->tip_width, &self->tip_height);

  if (self->texture)
    g_object_unref (self->texture);

  _upload_current (XRD_POINTER_TIP (self));
}

static void
_update_background_alpha (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayPointerTip *self = user_data;
  self->background_alpha = g_settings_get_double (settings, key);
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

  /* tip resolution config has to happen after self->uploader gets set */
  xrd_settings_connect_and_apply (G_CALLBACK (_update_texture_res),
                                  "pointer-tip-resolution", self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_inactive_rgb),
                                  "inactive-pointer-tip-color", self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_active_rgb),
                                  "active-pointer-tip-color", self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_background_alpha),
                                  "pointer-tip-animation-alpha", self);

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

  xrd_settings_connect_and_apply (G_CALLBACK
                                  (_update_use_constant_apparent_width),
                                  "pointer-tip-apparent-width-is-constant",
                                  self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_width),
                                  "pointer-tip-width", self);

  /*
   * The crosshair should always be visible, except the pointer can
   * occlude it. The pointer has max sort order, so the crosshair gets max -1
   */
  openvr_overlay_set_sort_order (OPENVR_OVERLAY (self), UINT32_MAX - 1);

  _upload_current (XRD_POINTER_TIP (self));

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

  if (self->animation_callback_id != 0)
    {
      g_source_remove (self->animation_callback_id);
      self->animation_callback_id = 0;
      g_free (self->animation_data);
      self->animation_data = NULL;
    }

  /* Do not skip renderint to the texture even when self->active == active.
   * An animation changes the texture, so when an animation is cancelled, we
   * want to re-render the current state. */

  GulkanClient *client = GULKAN_CLIENT (self->uploader);

  self->active = active;
  GdkPixbuf* pixbuf = _render_current_tip (self, 0.0);

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

/* TODO: scene app needs device poses too. Put in openvr_system? */
static gboolean
_get_hmd_pose (graphene_matrix_t *pose)
{
  OpenVRContext *context = openvr_context_get_instance ();
  VRControllerState_t state;
  if (context->system->IsTrackedDeviceConnected(k_unTrackedDeviceIndex_Hmd) &&
      context->system->GetTrackedDeviceClass (k_unTrackedDeviceIndex_Hmd) ==
          ETrackedDeviceClass_TrackedDeviceClass_HMD &&
      context->system->GetControllerState (k_unTrackedDeviceIndex_Hmd,
                                           &state, sizeof(state)))
    {
      /* k_unTrackedDeviceIndex_Hmd should be 0 => posearray[0] */
      TrackedDevicePose_t openvr_pose;
      context->system->GetDeviceToAbsoluteTrackingPose (context->origin, 0,
                                                        &openvr_pose, 1);
      openvr_math_matrix34_to_graphene (&openvr_pose.mDeviceToAbsoluteTracking,
                                        pose);

      return openvr_pose.bDeviceIsConnected &&
             openvr_pose.bPoseIsValid &&
             openvr_pose.eTrackingResult ==
                 ETrackingResult_TrackingResult_Running_OK;
    }
  return FALSE;
}

/* note: Move pointer tip to the desired location before calling. */
static void
_set_constant_width (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);

  if (!self->use_constant_apparent_width)
    return;

  graphene_matrix_t tip_pose;
  openvr_overlay_get_transform_absolute (OPENVR_OVERLAY(self), &tip_pose);
;
  graphene_point3d_t tip_point;
  graphene_matrix_get_translation_point3d (&tip_pose, &tip_point);

  graphene_matrix_t hmd_pose;
  gboolean has_pose = _get_hmd_pose (&hmd_pose);
  if (!has_pose)
    {
      openvr_overlay_set_width_meters (OPENVR_OVERLAY(self),
                                       self->overlay_width);
      return;
    }

  graphene_point3d_t hmd_point;
  graphene_matrix_get_translation_point3d (&hmd_pose, &hmd_point);

  float distance = graphene_point3d_distance (&tip_point, &hmd_point, NULL);

  /* divide distance by 3 so the width and the apparent width are the same at
   * a distance of 3 meters. This makes e.g. self->width = 0.3 look decent in
   * both cases at typical usage distances. */
  float new_width = self->overlay_width / 3.0 * distance;

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
xrd_overlay_pointer_tip_interface_init (XrdPointerTipInterface *iface)
{
  iface->set_constant_width = _set_constant_width;
  iface->set_active = _set_active;
  iface->animate_pulse = _animate_pulse;
  iface->set_transformation = _set_transformation;
  iface->get_transformation = _get_transformation;
  iface->show = _show;
  iface->hide = _hide;
  iface->set_width_meters = _set_width_meters;
}

