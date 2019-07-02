/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_WINDOW_H_
#define XRD_WINDOW_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>

#include <graphene.h>
#include <gulkan.h>

#include "xrd-pointer.h"

typedef struct XrdPixelSize
{
  uint32_t width;
  uint32_t height;
} XrdPixelSize;

typedef struct XrdHoverEvent
{
  graphene_point3d_t point;
  graphene_matrix_t  pose;
  float              distance;
  guint64            controller_handle;
} XrdHoverEvent;

typedef struct XrdGrabEvent
{
  graphene_matrix_t  pose;
  guint64            controller_handle;
} XrdGrabEvent;

typedef struct XrdControllerIndexEvent
{
  guint64 controller_handle;
} XrdControllerIndexEvent;

/** _XrdWindowState:
 * copy of the state of a window carried over an overlay<->scene switch */
typedef struct
{
  gpointer native;
  gchar *title;
  float scale;
  float initial_width;
  float initial_height;
  uint32_t texture_width;
  uint32_t texture_height;
  graphene_matrix_t reset_transform;
  float reset_scale;
  gboolean pinned;

  float current_width;
  float current_height;
  graphene_matrix_t transform;

  gboolean is_draggable;
  int child_index;
  graphene_point_t child_offset_center;
} XrdWindowState;

G_BEGIN_DECLS

#define XRD_TYPE_WINDOW xrd_window_get_type()
G_DECLARE_INTERFACE (XrdWindow, xrd_window, XRD, WINDOW, GObject)

typedef struct XrdWindowData
{
  GObject parent;
  gpointer native;

  uint32_t texture_width;
  uint32_t texture_height;
  GString *title;

  gboolean selected;

  graphene_point_t initial_size_meters;

  float scale;
  graphene_matrix_t vr_transform;

  /* A window that is pinned on top of this window and follows this window's
   * position and scaling */
  XrdWindow *child_window;
  XrdWindow *parent_window;

  graphene_point_t child_offset_center;

  graphene_matrix_t reset_transform;
  float reset_scale;

  gboolean pinned;

  /* cache of the currently rendered texture */
  GulkanTexture *texture;
} XrdWindowData;

struct _XrdWindowInterface
{
  GTypeInterface parent;

  gboolean
  (*set_transformation) (XrdWindow         *self,
                         graphene_matrix_t *mat);

  gboolean
  (*get_transformation) (XrdWindow         *self,
                         graphene_matrix_t *mat);

  gboolean
  (*get_transformation_no_scale) (XrdWindow         *self,
                                  graphene_matrix_t *mat);

  void
  (*submit_texture) (XrdWindow *self,
                     GulkanClient *client,
                     GulkanTexture *texture);

  void
  (*poll_event) (XrdWindow *self);

  void
  (*emit_grab_start) (XrdWindow *self, XrdControllerIndexEvent *event);

  void
  (*emit_grab) (XrdWindow *self, XrdGrabEvent *event);

  void
  (*emit_release) (XrdWindow *self, XrdControllerIndexEvent *event);

  void
  (*emit_hover_end) (XrdWindow *self, XrdControllerIndexEvent *event);

  void
  (*emit_hover) (XrdWindow *self, XrdHoverEvent *event);

  void
  (*emit_hover_start) (XrdWindow *self, XrdControllerIndexEvent *event);

  void
  (*add_child) (XrdWindow *self, XrdWindow *child,
                graphene_point_t *offset_center);

  void
  (*set_color) (XrdWindow *self, const graphene_vec3_t *color);

  void
  (*set_flip_y) (XrdWindow *self,
                 gboolean flip_y);

  void
  (*show) (XrdWindow *self);

  void
  (*hide) (XrdWindow *self);

  gboolean
  (*is_visible) (XrdWindow *self);

  void
  (*constructed) (GObject *object);

  XrdWindowData*
  (*get_data) (XrdWindow *self);

  guint windows_created;
};

gboolean
xrd_window_set_transformation (XrdWindow *self, graphene_matrix_t *mat);

gboolean
xrd_window_get_transformation (XrdWindow *self, graphene_matrix_t *mat);

gboolean
xrd_window_get_transformation_no_scale (XrdWindow         *self,
                                        graphene_matrix_t *mat);

void
xrd_window_submit_texture (XrdWindow    *self,
                           GulkanClient *client,
                           GulkanTexture *texture);

void
xrd_window_poll_event (XrdWindow *self);

gboolean
xrd_window_intersects (XrdWindow          *self,
                       XrdPointer         *pointer,
                       graphene_matrix_t  *pointer_transformation,
                       graphene_point3d_t *intersection);

void
xrd_window_get_intersection_2d_pixels (XrdWindow          *self,
                                       graphene_point3d_t *intersection_3d,
                                       graphene_point_t   *intersection_pixels);

void
xrd_window_get_intersection_2d (XrdWindow          *self,
                                graphene_point3d_t *intersection_3d,
                                graphene_point_t   *intersection_2d);


void
xrd_window_emit_grab_start (XrdWindow *self,
                            XrdControllerIndexEvent *event);

void
xrd_window_emit_grab (XrdWindow *self,
                      XrdGrabEvent *event);

void
xrd_window_emit_release (XrdWindow *self,
                         XrdControllerIndexEvent *event);

void
xrd_window_emit_hover_end (XrdWindow *self,
                           XrdControllerIndexEvent *event);

void
xrd_window_emit_hover (XrdWindow    *self,
                       XrdHoverEvent *event);

void
xrd_window_emit_hover_start (XrdWindow *self,
                             XrdControllerIndexEvent *event);

void
xrd_window_add_child (XrdWindow *self,
                      XrdWindow *child,
                      graphene_point_t *offset_center);

void
xrd_window_select (XrdWindow *self);

void
xrd_window_deselect (XrdWindow *self);

gboolean
xrd_window_is_selected (XrdWindow *self);

void
xrd_window_end_selection (XrdWindow *self);

void
xrd_window_set_flip_y (XrdWindow *self,
                       gboolean flip_y);

float
xrd_window_get_current_ppm (XrdWindow *self);

float
xrd_window_get_initial_ppm (XrdWindow *self);

void
xrd_window_show (XrdWindow *self);

void
xrd_window_hide (XrdWindow *self);

gboolean
xrd_window_is_visible (XrdWindow *self);

float
xrd_window_get_current_width_meters (XrdWindow *self);

float
xrd_window_get_current_height_meters (XrdWindow *self);

XrdWindowData*
xrd_window_get_data (XrdWindow *self);

void
xrd_window_update_child (XrdWindow *self);

void
xrd_window_get_normal (XrdWindow       *self,
                       graphene_vec3_t *normal);

void
xrd_window_get_plane (XrdWindow        *self,
                      graphene_plane_t *res);

float
xrd_window_get_aspect_ratio (XrdWindow *self);

void
xrd_window_set_color (XrdWindow *self, const graphene_vec3_t *color);

void
xrd_window_set_reset_transformation (XrdWindow *self,
                                     graphene_matrix_t *transform,
                                     float scale);

void
xrd_window_get_reset_transformation (XrdWindow *self,
                                     graphene_matrix_t *transform,
                                     float *scale);

void
xrd_window_set_pin (XrdWindow *self,
                    gboolean pinned,
                    gboolean hide_unpinned);

gboolean
xrd_window_is_pinned (XrdWindow *self);

G_END_DECLS

#endif /* XRD_WINDOW_H_ */
