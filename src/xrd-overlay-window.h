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
#include <openvr-overlay.h>
#include "xrd-window.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_WINDOW xrd_overlay_window_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayWindow, xrd_overlay_window,
                      XRD, OVERLAY_WINDOW, OpenVROverlay)

struct _XrdOverlayWindow;

XrdOverlayWindow *
xrd_overlay_window_new (const gchar *title);

XrdOverlayWindow *
xrd_overlay_window_new_from_meters (const gchar *title,
                                    float        width_meters,
                                    float        height_meters);

XrdOverlayWindow *
xrd_overlay_window_new_from_ppm (const gchar *title,
                                 uint32_t     width_pixels,
                                 uint32_t     height_pixels,
                                 float        ppm);

XrdOverlayWindow *
xrd_overlay_window_new_from_native (const gchar *title,
                                    gpointer     native,
                                    uint32_t     width_pixels,
                                    uint32_t     height_pixels,
                                    float        ppm);

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

void
xrd_overlay_window_poll_event (XrdOverlayWindow *self);

gboolean
xrd_overlay_window_intersects (XrdOverlayWindow   *self,
                               graphene_matrix_t  *pointer_transformation_matrix,
                               graphene_point3d_t *intersection_point);

gboolean
xrd_overlay_window_intersection_to_pixels (XrdOverlayWindow   *self,
                                           graphene_point3d_t *intersection_point,
                                           XrdPixelSize       *size_pixels,
                                           graphene_point_t   *window_coords);

gboolean
xrd_overlay_window_intersection_to_2d_offset_meter (XrdOverlayWindow *self,
                                                    graphene_point3d_t *intersection_point,
                                                    graphene_point_t   *offset_center);

void
xrd_overlay_window_add_child (XrdOverlayWindow *self,
                              XrdOverlayWindow *child,
                              graphene_point_t *offset_center);

void
xrd_overlay_window_set_color (XrdOverlayWindow *self,
                              graphene_vec3_t *color);

void
xrd_overlay_window_set_flip_y (XrdOverlayWindow *self,
                               gboolean flip_y);

void
xrd_overlay_window_set_hidden (XrdOverlayWindow *self,
                               gboolean hidden);

gboolean
xrd_overlay_window_get_hidden (XrdOverlayWindow *self);

G_END_DECLS

#endif /* XRD_GLIB_OVERLAY_WINDOW_H_ */
