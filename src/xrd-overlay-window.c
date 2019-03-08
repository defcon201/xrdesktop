/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-window.h"
#include <gdk/gdk.h>
#include <glib/gprintf.h>
#include <openvr-overlay.h>
#include <openvr-overlay-uploader.h>

G_DEFINE_TYPE (XrdOverlayWindow, xrd_overlay_window, XRD_TYPE_WINDOW)

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

static void
xrd_overlay_window_finalize (GObject *gobject);

static void
xrd_overlay_window_constructed (GObject *gobject);

static void
xrd_overlay_window_class_init (XrdOverlayWindowClass *klass)
{
  /* TODO: parent, not G_OBJECT */
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = xrd_overlay_window_finalize;
  object_class->constructed = xrd_overlay_window_constructed;


  XrdWindowClass *xrd_window_class = XRD_WINDOW_CLASS (klass);
  /* TODO: void* cast avoids warning about first argument type mismatch.
   * Local funcs have XrdOverlayWindow *self, parent have XrdWindow *self. */
  xrd_window_class->xrd_window_set_transformation_matrix =
      (void*)xrd_overlay_window_set_transformation_matrix;
  xrd_window_class->xrd_window_get_transformation_matrix =
      (void*)xrd_overlay_window_get_transformation_matrix;
  xrd_window_class->xrd_window_submit_texture =
      (void*)xrd_overlay_window_submit_texture;
  xrd_window_class->xrd_window_set_scaling_factor =
      (void*)xrd_overlay_window_set_scaling_factor;
  xrd_window_class->xrd_window_poll_event =
      (void*)xrd_overlay_window_poll_event;
  xrd_window_class->xrd_window_intersects =
      (void*)xrd_overlay_window_intersects;
  xrd_window_class->xrd_window_intersection_to_window_coords =
      (void*)xrd_overlay_window_intersection_to_window_coords;
  xrd_window_class->xrd_window_intersection_to_offset_center =
      (void*)xrd_overlay_window_intersection_to_offset_center;
  xrd_window_class->xrd_window_emit_grab_start =
      (void*)xrd_overlay_window_emit_grab_start;
  xrd_window_class->xrd_window_emit_grab =
      (void*)xrd_overlay_window_emit_grab;
  xrd_window_class->xrd_window_emit_release =
      (void*)xrd_overlay_window_emit_release;
  xrd_window_class->xrd_window_emit_hover_end =
      (void*)xrd_overlay_window_emit_hover_end;
  xrd_window_class->xrd_window_emit_hover =
      (void*)xrd_overlay_window_emit_hover;
  xrd_window_class->xrd_window_emit_hover_start =
      (void*)xrd_overlay_window_emit_hover_start;
  xrd_window_class->xrd_window_add_child =
      (void*)xrd_overlay_window_add_child;

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
_grab_start_cb (gpointer overlay,
                gpointer event,
                gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[GRAB_START_EVENT], 0, event);
}

void
_grab_cb (gpointer overlay,
          gpointer event,
          gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[GRAB_EVENT], 0, event);
}
void
_release_cb (gpointer overlay,
             gpointer event,
             gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[RELEASE_EVENT], 0, event);
}
void
_hover_end_cb (gpointer overlay,
               gpointer event,
               gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_END_EVENT], 0, event);
}
void
_hover_cb (gpointer overlay,
           gpointer event,
           gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_EVENT], 0, event);
}
void
_hover_start_cb (gpointer overlay,
                 gpointer event,
                 gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_START_EVENT], 0, event);
}

// TODO: missing in upstream
/**
 * graphene_point_scale:
 * @p: a #graphene_point_t
 * @factor: the scaling factor
 * @res: (out caller-allocates): return location for the scaled point
 *
 * Scales the coordinates of the given #graphene_point_t by
 * the given @factor.
 */
void
graphene_point_scale (const graphene_point_t *p,
                      float                   factor,
                      graphene_point_t       *res)
{
  graphene_point_init (res, p->x * factor, p->y * factor);
}

static void
_scale_move_child (XrdOverlayWindow *self)
{

  XrdWindow *xrd_window = XRD_WINDOW (self);
  XrdOverlayWindow *child = XRD_OVERLAY_WINDOW (xrd_window->child_window);

  xrd_overlay_window_set_scaling_factor (child, xrd_window->scaling_factor);

  graphene_point_t scaled_offset;
  graphene_point_scale (&xrd_window->child_offset_center,
                        xrd_window->scaling_factor / xrd_window->ppm,
                        &scaled_offset);

  graphene_point3d_t scaled_offset3d = {
    .x = scaled_offset.x,
    .y = scaled_offset.y,
    .z = 0.01
  };
  graphene_matrix_t child_transform;
  graphene_matrix_init_translate (&child_transform, &scaled_offset3d);

  graphene_matrix_t parent_transform;
  xrd_overlay_window_get_transformation_matrix (self, &parent_transform);

  graphene_matrix_multiply (&child_transform, &parent_transform,
                            &child_transform);

  xrd_overlay_window_set_transformation_matrix (child, &child_transform);

}

gboolean
xrd_overlay_window_set_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat)
{
  XrdWindow *xrd_window = XRD_WINDOW (self);

  gboolean res = openvr_overlay_set_transform_absolute (self->overlay, mat);
  if (xrd_window->child_window)
    _scale_move_child (self);
  return res;
}

gboolean
xrd_overlay_window_get_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat)
{
  gboolean res = openvr_overlay_get_transform_absolute (self->overlay, mat);
  return res;
}

void
xrd_overlay_window_submit_texture (XrdOverlayWindow *self,
                                   GulkanClient *client,
                                   GulkanTexture *texture)
{
  XrdWindow *xrd_window = XRD_WINDOW (self);

  OpenVROverlayUploader *uploader = OPENVR_OVERLAY_UPLOADER (client);

  if (xrd_window->texture_width != texture->width ||
      xrd_window->texture_height != texture->height)
    {
      float new_xr_width =
        xrd_window_pixel_to_xr_scale (xrd_window, texture->width);

      openvr_overlay_set_width_meters (self->overlay, new_xr_width);

      xrd_window->texture_width = texture->width;
      xrd_window->texture_height = texture->height;
      /* Mouse scale is required for the intersection test */
      openvr_overlay_set_mouse_scale (self->overlay, xrd_window->texture_width,
                                      xrd_window->texture_height);
    }

  openvr_overlay_uploader_submit_frame(uploader, self->overlay, texture);
}

gboolean
xrd_overlay_window_set_scaling_factor (XrdOverlayWindow *self, float factor)
{
  XrdWindow *xrd_window = XRD_WINDOW (self);
  xrd_window->scaling_factor = factor;

  float xr_width =
      xrd_window_pixel_to_xr_scale (xrd_window, xrd_window->texture_width);

  openvr_overlay_set_width_meters (self->overlay, xr_width);

  if (xrd_window->child_window)
    _scale_move_child (self);

  return TRUE;
}

void
xrd_overlay_window_add_child (XrdOverlayWindow *self,
                              XrdOverlayWindow *child,
                              graphene_point_t *offset_center)
{
  XrdWindow *xrd_window = XRD_WINDOW (self);
  xrd_window->child_window = XRD_WINDOW (child);
  graphene_point_init_from_point (&xrd_window->child_offset_center,
                                  offset_center);

  if (child)
    {
      _scale_move_child (self);
      XRD_WINDOW (child)->parent_window = XRD_WINDOW (self);
    }
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
                                                  XrdPixelSize       *size_pixels,
                                                  graphene_point_t   *window_coords)
{
  PixelSize pix_size = {
    .width = size_pixels->width,
    .height = size_pixels->height
  };
  gboolean res =
      openvr_overlay_get_2d_intersection (self->overlay, intersection_point,
                                          &pix_size, window_coords);
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
                                    XrdControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[GRAB_START_EVENT], 0, event);
}

void
xrd_overlay_window_emit_grab (XrdOverlayWindow *self,
                              XrdGrabEvent *event)
{
  g_signal_emit (self, window_signals[GRAB_EVENT], 0, event);
}

void
xrd_overlay_window_emit_release (XrdOverlayWindow *self,
                                 XrdControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[RELEASE_EVENT], 0, event);
}

void
xrd_overlay_window_emit_hover_end (XrdOverlayWindow *self,
                                   XrdControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[HOVER_END_EVENT], 0, event);
}

void
xrd_overlay_window_emit_hover (XrdOverlayWindow    *self,
                               XrdHoverEvent *event)
{
  g_signal_emit (self, window_signals[HOVER_EVENT], 0, event);
}

void
xrd_overlay_window_emit_hover_start (XrdOverlayWindow *self,
                                     XrdControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[HOVER_START_EVENT], 0, event);
}

static void
xrd_overlay_window_init (XrdOverlayWindow *self)
{
  (void) self;
}

/** xrd_overlay_window_new:
 * Create a new XrdWindow. Note that the window will only have dimensions after
 * a texture is uploaded. */
XrdOverlayWindow *
xrd_overlay_window_new (gchar *window_title, float ppm, gpointer native)
{
  XrdOverlayWindow *self =
      (XrdOverlayWindow*) g_object_new (XRD_TYPE_OVERLAY_WINDOW,
                                        "window-title", window_title,
                                         NULL);


  XrdWindow *xrd_window = XRD_WINDOW (self);
  xrd_window->ppm = ppm;
  xrd_window->native = native;

  return self;
}

static void
xrd_overlay_window_constructed (GObject *gobject)
{
  G_OBJECT_CLASS (xrd_overlay_window_parent_class)->constructed (gobject);

  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (gobject);
  XrdWindow *xrd_window = XRD_WINDOW (self);
  XrdWindowClass *parent_klass = XRD_WINDOW_GET_CLASS (xrd_window);

  gchar overlay_id_str [25];
  g_sprintf (overlay_id_str, "xrd-window-%d", parent_klass->windows_created);

  self->overlay = openvr_overlay_new ();
  openvr_overlay_create (self->overlay, overlay_id_str,
                         xrd_window->window_title->str);

  /* g_print ("Created overlay %s\n", overlay_id_str); */

  if (!openvr_overlay_is_valid (self->overlay))
  {
    g_printerr ("Overlay unavailable.\n");
    return;
  }

  openvr_overlay_show (self->overlay);

  parent_klass->windows_created++;
}

static void
xrd_overlay_window_finalize (GObject *gobject)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (gobject);
  XrdWindow *xrd_window = XRD_WINDOW (self);

  if (self->overlay)
    g_object_unref (self->overlay);

  /* TODO: find a better solution to clean up */
  if (xrd_window->parent_window)
    xrd_window->parent_window->child_window = NULL;

  /* TODO: a child window should not exist without a parent window anyway,
   * but it will be cleaned up already because the child window on the desktop
   * will most likely close already. */
  if (xrd_window->child_window)
    xrd_window->child_window->parent_window = NULL;

  G_OBJECT_CLASS (xrd_overlay_window_parent_class)->finalize (gobject);
}
