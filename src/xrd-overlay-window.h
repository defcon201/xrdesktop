/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_OVERLAY_WINDOW_H_
#define XRD_GLIB_OVERLAY_WINDOW_H_

#include <glib-object.h>

#include <openvr-overlay.h>
#include <gulkan-texture.h>

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_WINDOW xrd_overlay_window_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayWindow, xrd_overlay_window,
                      XRD, OVERLAY_WINDOW, GObject)

struct _XrdOverlayWindow
{
  GObject parent;

  gpointer      *native;
  OpenVROverlay *overlay;
  GulkanTexture *texture;
  uint32_t       gl_texture;
  int            texture_width;
  int            texture_height;

  float xr_width;
  float xr_height;
  graphene_matrix_t vr_transform;
  gboolean       recreate;
};

XrdOverlayWindow *
xrd_overlay_window_new ();

XrdOverlayWindow *
xrd_overlay_window_new_from_overlay (OpenVROverlay *overlay,
                                     int width,
                                     int height);

void
xrd_overlay_window_init_overlay (XrdOverlayWindow *self,
                                 OpenVROverlay *overlay,
                                 int width,
                                 int height);

gboolean
xrd_overlay_window_set_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat);

gboolean
xrd_overlay_window_get_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat);

gboolean
xrd_overlay_window_set_xr_width (XrdOverlayWindow *self, float meters);

gboolean
xrd_overlay_window_get_xr_width (XrdOverlayWindow *self, float *meters);

void
xrd_overlay_window_poll_event (XrdOverlayWindow *self);

gboolean
xrd_overlay_window_intersects (XrdOverlayWindow   *self,
                               graphene_matrix_t  *pointer_transformation_matrix,
                               graphene_point3d_t *intersection_point);

gboolean
xrd_overlay_window_intersection_to_window_coords (XrdOverlayWindow   *self,
                                                  graphene_point3d_t *intersection_point,
                                                  PixelSize          *size_pixels,
                                                  graphene_point_t   *window_coords);

gboolean
xrd_overlay_window_intersection_to_offset_center (XrdOverlayWindow *self,
                                                  graphene_point3d_t *intersection_point,
                                                  graphene_point_t   *offset_center);


void
xrd_overlay_window_emit_grab_start (XrdOverlayWindow *self,
                                    OpenVRControllerIndexEvent *event);

void
xrd_overlay_window_emit_grab (XrdOverlayWindow *self,
                              OpenVRGrabEvent *event);

void
xrd_overlay_window_emit_release (XrdOverlayWindow *self,
                                 OpenVRControllerIndexEvent *event);

void
xrd_overlay_window_emit_hover_end (XrdOverlayWindow *self,
                                   OpenVRControllerIndexEvent *event);

void
xrd_overlay_window_emit_hover (XrdOverlayWindow    *self,
                               OpenVRHoverEvent *event);

void
xrd_overlay_window_emit_hover_start (XrdOverlayWindow *self,
                                     OpenVRControllerIndexEvent *event);

G_END_DECLS

#endif /* XRD_GLIB_OVERLAY_WINDOW_H_ */
