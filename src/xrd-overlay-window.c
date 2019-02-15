/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-window.h"
#include <gdk/gdk.h>
#include <glib/gprintf.h>

G_DEFINE_TYPE (XrdOverlayWindow, xrd_overlay_window, G_TYPE_OBJECT)

enum {
  MOTION_NOTIFY_EVENT,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  SHOW,
  DESTROY,
  SCROLL_EVENT,
  KEYBOARD_PRESS_EVENT,
  KEYBOARD_CLOSE_EVENT,
  GRAB_START_EVENT,
  GRAB_EVENT,
  RELEASE_EVENT,
  HOVER_START_EVENT,
  HOVER_EVENT,
  HOVER_END_EVENT,
  LAST_SIGNAL
};

static guint window_signals[LAST_SIGNAL] = { 0 };

static guint new_window_index = 0;

static void
xrd_overlay_window_finalize (GObject *gobject);

static void
xrd_overlay_window_class_init (XrdOverlayWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = xrd_overlay_window_finalize;


  window_signals[MOTION_NOTIFY_EVENT] =
    g_signal_new ("motion-notify-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[BUTTON_PRESS_EVENT] =
    g_signal_new ("button-press-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new ("button-release-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[SHOW] =
    g_signal_new ("show",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_FIRST,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[DESTROY] =
    g_signal_new ("destroy",
                   G_TYPE_FROM_CLASS (klass),
                     G_SIGNAL_RUN_CLEANUP |
                      G_SIGNAL_NO_RECURSE |
                      G_SIGNAL_NO_HOOKS,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[SCROLL_EVENT] =
    g_signal_new ("scroll-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[KEYBOARD_PRESS_EVENT] =
    g_signal_new ("keyboard-press-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[KEYBOARD_CLOSE_EVENT] =
    g_signal_new ("keyboard-close-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[GRAB_START_EVENT] =
    g_signal_new ("grab-start-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[GRAB_EVENT] =
    g_signal_new ("grab-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[RELEASE_EVENT] =
    g_signal_new ("release-event",
                  G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_END_EVENT] =
    g_signal_new ("hover-end-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_EVENT] =
    g_signal_new ("hover-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_START_EVENT] =
    g_signal_new ("hover-start-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}
void
_grab_start_cb (OpenVROverlay *overlay,
                gpointer       event,
                gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[GRAB_START_EVENT], 0, event);
}

void
_grab_cb (OpenVROverlay *overlay,
          gpointer       event,
          gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[GRAB_EVENT], 0, event);
}
void
_release_cb (OpenVROverlay *overlay,
             gpointer       event,
             gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[RELEASE_EVENT], 0, event);
}
void
_hover_end_cb (OpenVROverlay *overlay,
               gpointer       event,
               gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_END_EVENT], 0, event);
}
void
_hover_cb (OpenVROverlay *overlay,
           gpointer       event,
           gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_EVENT], 0, event);
}
void
_hover_start_cb (OpenVROverlay *overlay,
                 gpointer       event,
                 gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_START_EVENT], 0, event);
}


gboolean
xrd_overlay_window_set_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat)
{
  gboolean res = openvr_overlay_set_transform_absolute (self->overlay, mat);
  return res;
}

gboolean
xrd_overlay_window_get_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat)
{
  gboolean res = openvr_overlay_get_transform_absolute (self->overlay, mat);
  return res;
}

static float
_texture_size_to_xr_size (XrdOverlayWindow *self, int texture_size)
{
  return (float)texture_size / self->ppm * self->scaling_factor;
}

gboolean
xrd_overlay_window_get_xr_width (XrdOverlayWindow *self, float *meters)
{
  *meters = _texture_size_to_xr_size (self, self->texture_width);
  return TRUE;
}

gboolean
xrd_overlay_window_get_xr_height (XrdOverlayWindow *self, float *meters)
{
  *meters = _texture_size_to_xr_size (self, self->texture_height);
  return TRUE;
}

gboolean
xrd_overlay_window_get_scaling_factor (XrdOverlayWindow *self, float *factor)
{
  *factor = self->scaling_factor;
  return TRUE;
}

gboolean
xrd_overlay_window_set_scaling_factor (XrdOverlayWindow *self, float factor)
{
  self->scaling_factor = factor;
  float xr_width = _texture_size_to_xr_size (self, self->texture_width);
  openvr_overlay_set_width_meters (self->overlay, xr_width);
  return TRUE;
}

void
xrd_overlay_window_poll_event (XrdOverlayWindow *self)
{
  openvr_overlay_poll_event (self->overlay);
}

gboolean
xrd_overlay_window_intersects (XrdOverlayWindow   *self,
                               graphene_matrix_t  *pointer_transformation_matrix,
                               graphene_point3d_t *intersection_point)
{
  gboolean res = openvr_overlay_intersects (self->overlay,
                                            intersection_point,
                                            pointer_transformation_matrix);
  return res;
}

gboolean
xrd_overlay_window_intersection_to_window_coords (XrdOverlayWindow   *self,
                                                  graphene_point3d_t *intersection_point,
                                                  PixelSize          *size_pixels,
                                                  graphene_point_t   *window_coords)
{
  gboolean res =
      openvr_overlay_get_2d_intersection (self->overlay, intersection_point,
                                          size_pixels, window_coords);
  return res;
}

gboolean
xrd_overlay_window_intersection_to_offset_center (XrdOverlayWindow *self,
                                                  graphene_point3d_t *intersection_point,
                                                  graphene_point_t   *offset_center)
{
  gboolean res =
      openvr_overlay_get_2d_offset (self->overlay, intersection_point, offset_center);
  return res;
}


void
xrd_overlay_window_emit_grab_start (XrdOverlayWindow *self,
                                    OpenVRControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[GRAB_START_EVENT], 0, event);
}

void
xrd_overlay_window_emit_grab (XrdOverlayWindow *self,
                              OpenVRGrabEvent *event)
{
  g_signal_emit (self, window_signals[GRAB_EVENT], 0, event);
}

void
xrd_overlay_window_emit_release (XrdOverlayWindow *self,
                                 OpenVRControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[RELEASE_EVENT], 0, event);
}

void
xrd_overlay_window_emit_hover_end (XrdOverlayWindow *self,
                                   OpenVRControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[HOVER_END_EVENT], 0, event);
}

void
xrd_overlay_window_emit_hover (XrdOverlayWindow    *self,
                               OpenVRHoverEvent *event)
{
  g_signal_emit (self, window_signals[HOVER_EVENT], 0, event);
}

void
xrd_overlay_window_emit_hover_start (XrdOverlayWindow *self,
                                     OpenVRControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[HOVER_START_EVENT], 0, event);
}

static void
_disconnect_func (gpointer instance, gpointer func)
{
  g_signal_handlers_disconnect_matched (instance,
                                        G_SIGNAL_MATCH_FUNC,
                                        0,
                                        0,
                                        NULL,
                                        func,
                                        NULL);
}

static void
_disconnect_signals (XrdOverlayWindow *self)
{
  if (!self->overlay)
    return;
  _disconnect_func (self->overlay, _grab_start_cb);
  _disconnect_func (self->overlay, _grab_cb);
  _disconnect_func (self->overlay, _release_cb);
  _disconnect_func (self->overlay, _hover_start_cb);
  _disconnect_func (self->overlay, _hover_cb);
  _disconnect_func (self->overlay, _hover_end_cb);
}

static void
_connect_signals (XrdOverlayWindow *self)
{
  if (!self->overlay)
    return;
  g_signal_connect (self->overlay, "grab-start-event",
                    (GCallback) _grab_start_cb, self);
  g_signal_connect (self->overlay, "grab-event",
                    (GCallback) _grab_cb, self);
  g_signal_connect (self->overlay, "release-event",
                    (GCallback) _release_cb, self);
  g_signal_connect (self->overlay, "hover-start-event",
                    (GCallback) _hover_start_cb, self);
  g_signal_connect (self->overlay, "hover-event",
                    (GCallback) _hover_cb, self);
  g_signal_connect (self->overlay, "hover-end-event",
                    (GCallback) _hover_end_cb, self);
}

void
xrd_overlay_window_internal_init (XrdOverlayWindow *self)
{
  /* TODO: ppm setting */
  self->ppm = 300.0;
  self->scaling_factor = 1.0;

  gchar overlay_id_str [25];
  g_sprintf (overlay_id_str, "xrd-window-%d", new_window_index);

  float xr_width = _texture_size_to_xr_size (self, self->texture_width);

  self->overlay = openvr_overlay_new ();
  openvr_overlay_create_width (self->overlay, overlay_id_str,
                               self->window_title->str, xr_width);

  if (!openvr_overlay_is_valid (self->overlay))
  {
    g_printerr ("Overlay unavailable.\n");
    return;
  }

   /* Mouse scale is required for the intersection test */
  openvr_overlay_set_mouse_scale (self->overlay, self->texture_width,
                                  self->texture_height);

  openvr_overlay_show (self->overlay);

  _connect_signals (self);

  new_window_index++;
}

static void
xrd_overlay_window_init (XrdOverlayWindow *self)
{
  (void) self;
}

XrdOverlayWindow *
xrd_overlay_window_new (gchar *window_title, int width, int height,
                        gpointer native, GulkanTexture *gulkan_texture,
                        guint gl_texture)
{
  XrdOverlayWindow *self = (XrdOverlayWindow*) g_object_new (XRD_TYPE_OVERLAY_WINDOW, 0);

  self->overlay = NULL;
  self->native = native,
  self->texture = gulkan_texture;
  self->gl_texture = gl_texture;
  self->texture_width = width;
  self->texture_height = height;
  self->window_title = g_string_new (window_title);

  xrd_overlay_window_internal_init (self);
  return self;
}

static void
xrd_overlay_window_finalize (GObject *gobject)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (gobject);

  _disconnect_signals (self);

  if (self->overlay)
    g_object_unref (self->overlay);
  if (self->texture)
    g_object_unref (self->texture);

  G_OBJECT_CLASS (xrd_overlay_window_parent_class)->finalize (gobject);
}
