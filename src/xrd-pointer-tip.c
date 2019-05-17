/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-pointer-tip.h"

#include <gdk/gdk.h>

#include "xrd-math.h"
#include "xrd-settings.h"
#include "graphene-ext.h"
#include <openvr-system.h>

G_DEFINE_INTERFACE (XrdPointerTip, xrd_pointer_tip, G_TYPE_OBJECT)

static void
xrd_pointer_tip_default_init (XrdPointerTipInterface *iface)
{
  (void) iface;
}

void
xrd_pointer_tip_update (XrdPointerTip      *self,
                        graphene_matrix_t  *pose,
                        graphene_point3d_t *intersection_point)
{
  graphene_matrix_t transform;
  graphene_matrix_init_from_matrix (&transform, pose);
  xrd_math_matrix_set_translation_point (&transform, intersection_point);
  xrd_pointer_tip_set_transformation (self, &transform);

  xrd_pointer_tip_update_apparent_size (self);
}

void
xrd_pointer_tip_set_transformation (XrdPointerTip     *self,
                                    graphene_matrix_t *matrix)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->set_transformation (self, matrix);
}

void
xrd_pointer_tip_get_transformation (XrdPointerTip     *self,
                                    graphene_matrix_t *matrix)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->get_transformation (self, matrix);
}

void
xrd_pointer_tip_show (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->show (self);
}

void
xrd_pointer_tip_hide (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->hide (self);
}

void
xrd_pointer_tip_set_width_meters (XrdPointerTip *self,
                                  float          meters)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->set_width_meters (self, meters);
}

void
xrd_pointer_tip_submit_texture (XrdPointerTip *self,
                                GulkanClient  *client,
                                GulkanTexture *texture)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->submit_texture (self, client, texture);
}

XrdPointerTipData*
xrd_pointer_tip_get_data (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->get_data (self);
}

GulkanClient*
xrd_pointer_tip_get_gulkan_client (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->get_gulkan_client (self);
}

/* Settings related methods */
static void
_update_width_meters (GSettings *settings, gchar *key, gpointer _data)
{
  XrdPointerTipData *data = (XrdPointerTipData*)_data;
  XrdPointerTipSettings *s = &data->settings;

  s->width_meters =
    g_settings_get_double (settings, key) * XRD_TIP_VIEWPORT_SCALE;

  if (s->keep_apparent_size)
      xrd_pointer_tip_update_apparent_size (data->tip);
  else
    xrd_pointer_tip_set_width_meters (data->tip, s->width_meters);
}

static void
_update_keep_apparent_size (GSettings *settings, gchar *key, gpointer _data)
{
  XrdPointerTipData *data = (XrdPointerTipData*)_data;
  XrdPointerTipSettings *s = &data->settings;

  s->keep_apparent_size = g_settings_get_boolean (settings, key);
  if (s->keep_apparent_size)
    xrd_pointer_tip_update_apparent_size (data->tip);
  else
    xrd_pointer_tip_set_width_meters (data->tip, s->width_meters);
}

static void
_update_active_color (GSettings *settings, gchar *key, gpointer _data)
{
  XrdPointerTipData *data = (XrdPointerTipData*)_data;
  XrdPointerTipSettings *s = &data->settings;
  GVariant *var = g_settings_get_value (settings, key);

  double r, g, b;
  g_variant_get (var, "(ddd)", &r, &g, &b);
  graphene_point3d_init (&s->active_color, r, g, b);
}

static void
_update_passive_color (GSettings *settings, gchar *key, gpointer _data)
{
  XrdPointerTipData *data = (XrdPointerTipData*)_data;
  XrdPointerTipSettings *s = &data->settings;
  GVariant *var = g_settings_get_value (settings, key);

  double r, g, b;
  g_variant_get (var, "(ddd)", &r, &g, &b);
  graphene_point3d_init (&s->passive_color, r, g, b);
}

static void
_init_texture (XrdPointerTip *self)
{
  GulkanClient *client = xrd_pointer_tip_get_gulkan_client (self);
  XrdPointerTipData *data = xrd_pointer_tip_get_data (self);

  GdkPixbuf* pixbuf = xrd_pointer_tip_render (XRD_POINTER_TIP (self), 1.0f);

  if (data->texture)
    g_object_unref (data->texture);

  data->texture = gulkan_texture_new_from_pixbuf (client->device, pixbuf,
                                                  VK_FORMAT_R8G8B8A8_UNORM);
  gulkan_client_upload_pixbuf (client, data->texture, pixbuf);
  g_object_unref (pixbuf);

  xrd_pointer_tip_submit_texture (self, client, data->texture);
}

static void
_update_texture_res (GSettings *settings, gchar *key, gpointer _data)
{
  XrdPointerTipData *data = (XrdPointerTipData*)_data;
  XrdPointerTipSettings *s = &data->settings;

  GVariant *texture_res = g_settings_get_value (settings, key);
  g_variant_get (texture_res, "(ii)", &s->texture_width, &s->texture_height);

  _init_texture (data->tip);
}

static void
_update_pulse_alpha (GSettings *settings, gchar *key, gpointer _data)
{
  XrdPointerTipData *data = (XrdPointerTipData*)_data;
  XrdPointerTipSettings *s = &data->settings;
  s->pulse_alpha = g_settings_get_double (settings, key);
}

void
xrd_pointer_tip_init_settings (XrdPointerTip     *self,
                               XrdPointerTipData *data)
{
  data->tip = self;

  /* tip resolution config has to happen after self->uploader gets set */
  xrd_settings_connect_and_apply (G_CALLBACK (_update_texture_res),
                                  "pointer-tip-resolution", data);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_passive_color),
                                  "pointer-tip-passive-color", data);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_active_color),
                                  "pointer-tip-active-color", data);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_pulse_alpha),
                                  "pointer-tip-pulse-alpha", data);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_keep_apparent_size),
                                  "pointer-tip-keep-apparent-size", data);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_width_meters),
                                  "pointer-tip-width-meters", data);
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
      float pulse_scale = XRD_TIP_VIEWPORT_SCALE * (1.0f - progress);
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
xrd_pointer_tip_render (XrdPointerTip *self,
                        float          progress)
{
  XrdPointerTipData *data = xrd_pointer_tip_get_data (self);

  int w = data->settings.texture_width * XRD_TIP_VIEWPORT_SCALE;
  int h = data->settings.texture_height * XRD_TIP_VIEWPORT_SCALE;

  graphene_point3d_t *color = data->active ? &data->settings.active_color :
                                             &data->settings.passive_color;

  double radius = data->settings.texture_width / 2.0;

  GdkPixbuf *pixbuf =
    _render_cairo (w, h, radius, color, data->settings.pulse_alpha, progress);

  return pixbuf;
}

static gboolean
_animate_cb (gpointer _animation)
{
  XrdPointerTipAnimation *animation = (XrdPointerTipAnimation *) _animation;
  XrdPointerTip *tip = animation->tip;

  XrdPointerTipData *data = xrd_pointer_tip_get_data (tip);

  GulkanClient *client = xrd_pointer_tip_get_gulkan_client (tip);

  GdkPixbuf* pixbuf = xrd_pointer_tip_render (tip, animation->progress);
  gulkan_client_upload_pixbuf (client, data->texture, pixbuf);
  g_object_unref (pixbuf);

  xrd_pointer_tip_submit_texture (tip, client, data->texture);

  animation->progress += 0.05f;

  if (animation->progress > 1)
    {
      data->animation = NULL;
      g_free (animation);
      return FALSE;
    }

  return TRUE;
}

void
xrd_pointer_tip_animate_pulse (XrdPointerTip *self)
{
  XrdPointerTipData *data = xrd_pointer_tip_get_data (self);
  if (data->animation != NULL)
    xrd_pointer_tip_set_active (data->tip, data->active);

  data->animation = g_malloc (sizeof (XrdPointerTipAnimation));
  data->animation->progress = 0;
  data->animation->tip = data->tip;
  data->animation->callback_id = g_timeout_add (20, _animate_cb,
                                                data->animation);
}

static void
_update_texture (XrdPointerTip *self)
{
  XrdPointerTipData *data = xrd_pointer_tip_get_data (self);
  GulkanClient *client = xrd_pointer_tip_get_gulkan_client (self);

  GdkPixbuf* pixbuf = xrd_pointer_tip_render (self, 1.0f);

  gulkan_client_upload_pixbuf (client, data->texture, pixbuf);
  g_object_unref (pixbuf);

  xrd_pointer_tip_submit_texture (self, client, data->texture);
}

/** xrd_pointer_tip_set_active:
 * Changes whether the active or inactive style is rendered.
 * Also cancels animations. */
void
xrd_pointer_tip_set_active (XrdPointerTip *self,
                            gboolean       active)
{
  XrdPointerTipData *data = xrd_pointer_tip_get_data (self);

  if (data->texture == NULL)
    return;

  if (data->animation != NULL)
    {
      g_source_remove (data->animation->callback_id);
      g_free (data->animation);
      data->animation = NULL;
    }
  else if (data->active == active)
    return;

  /* Do not skip renderint to the texture even when self->active == active.
   * An animation changes the texture, so when an animation is cancelled, we
   * want to re-render the current state. */
  data->active = active;

  _update_texture (self);
}

/* note: Move pointer tip to the desired location before calling. */
void
xrd_pointer_tip_update_apparent_size (XrdPointerTip *self)
{
  XrdPointerTipData *data = xrd_pointer_tip_get_data (self);

  if (!data->settings.keep_apparent_size)
    return;

  graphene_matrix_t tip_pose;
  xrd_pointer_tip_get_transformation (self, &tip_pose);
;
  graphene_point3d_t tip_point;
  graphene_matrix_get_translation_point3d (&tip_pose, &tip_point);

  graphene_matrix_t hmd_pose;
  gboolean has_pose = openvr_system_get_hmd_pose (&hmd_pose);
  if (!has_pose)
    {
      xrd_pointer_tip_set_width_meters (self, data->settings.width_meters);
      return;
    }

  graphene_point3d_t hmd_point;
  graphene_matrix_get_translation_point3d (&hmd_pose, &hmd_point);

  float distance = graphene_point3d_distance (&tip_point, &hmd_point, NULL);

  float w = data->settings.width_meters
            / XRD_TIP_APPARENT_SIZE_DISTANCE * distance;

  xrd_pointer_tip_set_width_meters (self, w);
}
