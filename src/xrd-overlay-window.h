/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_OVERLAY_WINDOW_H_
#define XRD_GLIB_OVERLAY_WINDOW_H_

#include <glib-object.h>

#include <gulkan-texture.h>
#include "xrd-window.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_WINDOW xrd_overlay_window_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayWindow, xrd_overlay_window,
                      XRD, OVERLAY_WINDOW, XrdWindow)

struct _XrdOverlayWindow
{
  XrdWindow parent;

  gpointer      overlay;
  gboolean       recreate;
};

XrdOverlayWindow *
xrd_overlay_window_new (gchar *window_title, float ppm, gpointer native);

XrdOverlayWindow *
xrd_overlay_window_new_from_overlay (gpointer *overlay,
                                     int width,
                                     int height);

void
xrd_overlay_window_init_overlay (XrdOverlayWindow *self,
                                 gpointer *overlay,
                                 int width,
                                 int height);

gboolean
xrd_overlay_window_set_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat);

gboolean
xrd_overlay_window_get_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat);

void
xrd_overlay_window_submit_texture (XrdOverlayWindow *self,
                                   GulkanClient *client,
                                   GulkanTexture *texture);

gboolean
xrd_overlay_window_set_scaling_factor (XrdOverlayWindow *self, float factor);

void
xrd_overlay_window_poll_event (XrdOverlayWindow *self);

gboolean
xrd_overlay_window_intersects (XrdOverlayWindow   *self,
                               graphene_matrix_t  *pointer_transformation_matrix,
                               graphene_point3d_t *intersection_point);

gboolean
xrd_overlay_window_intersection_to_window_coords (XrdOverlayWindow   *self,
                                                  graphene_point3d_t *intersection_point,
                                                  XrdPixelSize       *size_pixels,
                                                  graphene_point_t   *window_coords);

gboolean
xrd_overlay_window_intersection_to_offset_center (XrdOverlayWindow *self,
                                                  graphene_point3d_t *intersection_point,
                                                  graphene_point_t   *offset_center);


void
xrd_overlay_window_emit_grab_start (XrdOverlayWindow *self,
                                    XrdControllerIndexEvent *event);

void
xrd_overlay_window_emit_grab (XrdOverlayWindow *self,
                              XrdGrabEvent *event);

void
xrd_overlay_window_emit_release (XrdOverlayWindow *self,
                                 XrdControllerIndexEvent *event);

void
xrd_overlay_window_emit_hover_end (XrdOverlayWindow *self,
                                   XrdControllerIndexEvent *event);

void
xrd_overlay_window_emit_hover (XrdOverlayWindow    *self,
                               XrdHoverEvent *event);

void
xrd_overlay_window_emit_hover_start (XrdOverlayWindow *self,
                                     XrdControllerIndexEvent *event);

void
xrd_overlay_window_add_child (XrdOverlayWindow *self,
                              XrdOverlayWindow *child,
                              graphene_point_t *offset_center);

G_END_DECLS

#endif /* XRD_GLIB_OVERLAY_WINDOW_H_ */
