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
#include <openvr-overlay-uploader.h>
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
  uint32_t       texture_width;
  uint32_t       texture_height;
  GString        *window_title;

  double ppm;
  double scaling_factor;

  graphene_matrix_t vr_transform;
  gboolean       recreate;

  /* A window that is pinned on top of this window and follows this window's
   * position and scaling */
  XrdOverlayWindow *child_window;
  XrdOverlayWindow *parent_window;

  graphene_point_t child_offset_center;
};

XrdOverlayWindow *
xrd_overlay_window_new (gchar *window_title, int width, int height,
                        gpointer native);

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

/* TODO: More generic class than OpenVROverlayUploader */
void
xrd_overlay_window_submit_texture (XrdOverlayWindow *self,
                                   OpenVROverlayUploader *uploader,
                                   GulkanTexture *texture);

float
xrd_overlay_window_pixel_to_xr_scale (XrdOverlayWindow *self, int pixel);

gboolean
xrd_overlay_window_get_xr_width (XrdOverlayWindow *self, float *meters);

gboolean
xrd_overlay_window_get_xr_height (XrdOverlayWindow *self, float *meters);

gboolean
xrd_overlay_window_get_scaling_factor (XrdOverlayWindow *self, float *factor);

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

void
xrd_overlay_window_add_child (XrdOverlayWindow *self,
                              XrdOverlayWindow *child,
                              graphene_point_t *offset_center);


/* TODO: this is a stopgap solution for so children can init a window.
 * Pretty sure there's a more glib like solution. */
void
xrd_overlay_window_internal_init (XrdOverlayWindow *self);

G_END_DECLS

#endif /* XRD_GLIB_OVERLAY_WINDOW_H_ */
