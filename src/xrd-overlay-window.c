/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-window.h"

#include <glib/gprintf.h>
#include <openvr-overlay.h>
#include <openvr-overlay-uploader.h>
#include "xrd-math.h"

G_DEFINE_TYPE (XrdOverlayWindow, xrd_overlay_window, XRD_TYPE_WINDOW)

static void
_scale_move_child (XrdOverlayWindow *self);

static void
notify_property_scale_changed (GObject *object,
                               GParamSpec *pspec,
                               gpointer user_data)
{
  (void) pspec;
  (void) user_data;

  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (object);
  XrdWindow *xrd_window = XRD_WINDOW (object);

  float xr_width =
      xrd_window_pixel_to_xr_scale (xrd_window, xrd_window->texture_width);

  openvr_overlay_set_width_meters (self->overlay, xr_width);

  if (xrd_window->child_window)
    _scale_move_child (self);
}

static void
xrd_overlay_window_finalize (GObject *gobject);

static void
xrd_overlay_window_constructed (GObject *gobject);

static void
xrd_overlay_window_class_init (XrdOverlayWindowClass *klass)
{
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
  xrd_window_class->xrd_window_poll_event =
      (void*)xrd_overlay_window_poll_event;
  xrd_window_class->xrd_window_intersects =
      (void*)xrd_overlay_window_intersects;
  xrd_window_class->xrd_window_intersection_to_window_coords =
      (void*)xrd_overlay_window_intersection_to_window_coords;
  xrd_window_class->xrd_window_intersection_to_offset_center =
      (void*)xrd_overlay_window_intersection_to_offset_center;
  xrd_window_class->xrd_window_add_child =
      (void*)xrd_overlay_window_add_child;
}

static void
_scale_move_child (XrdOverlayWindow *self)
{

  XrdWindow *xrd_window = XRD_WINDOW (self);
  XrdOverlayWindow *child = XRD_OVERLAY_WINDOW (xrd_window->child_window);

  g_object_set (G_OBJECT(child), "scaling-factor", xrd_window->scaling_factor, NULL);

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
                                        "ppm", ppm,
                                        "native", native,
                                         NULL);
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

  g_signal_connect(xrd_window, "notify::scaling-factor",
                   (GCallback)notify_property_scale_changed, NULL);

  g_signal_connect(xrd_window, "notify::ppm",
                   (GCallback)notify_property_scale_changed, NULL);
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
