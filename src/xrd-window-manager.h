/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_WINDOW_MANAGER_H_
#define XRD_WINDOW_MANAGER_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>

#include <openvr-glib.h>

#include "xrd-window.h"

G_BEGIN_DECLS

#define XRD_TYPE_WINDOW_MANAGER xrd_window_manager_get_type()
G_DECLARE_FINAL_TYPE (XrdWindowManager, xrd_window_manager, XRD,
                      WINDOW_MANAGER, GObject)

typedef struct XrdNoHoverEvent
{
  graphene_matrix_t pose;
  int controller_index;
} XrdNoHoverEvent;

typedef struct TransformTransition
{
  XrdWindow *window;
  graphene_matrix_t from;
  graphene_matrix_t to;
  float from_scaling;
  float to_scaling;
  float interpolate;
} TransformTransition;

typedef struct HoverState
{
  XrdWindow *window;
  graphene_matrix_t pose;
  float             distance;
  graphene_point_t  intersection_offset;
} HoverState;

typedef struct GrabState
{
  XrdWindow    *window;
  graphene_quaternion_t window_rotation;
  /* the rotation induced by the overlay being moved on the controller arc */
  graphene_quaternion_t window_transformed_rotation_neg;
  graphene_point3d_t offset_translation_point;
} GrabState;

typedef enum
{
  XRD_WINDOW_HOVERABLE           = 1 << 0,
  XRD_WINDOW_DRAGGABLE           = 1 << 1,
  XRD_WINDOW_MANAGED             = 1 << 2,
  XRD_WINDOW_FOLLOW_HEAD         = 1 << 3,
  XRD_WINDOW_DESTROY_WITH_PARENT = 1 << 4,
  XRD_WINDOW_MANAGER_BUTTON      = 1 << 5,
} XrdWindowFlags;

struct _XrdWindowManager;

XrdWindowManager *xrd_window_manager_new (void);

void
xrd_window_manager_arrange_reset (XrdWindowManager *self);

gboolean
xrd_window_manager_arrange_sphere (XrdWindowManager *self);

void
xrd_window_manager_add_window (XrdWindowManager *self,
                               XrdWindow        *window,
                               XrdWindowFlags    flags);

void
xrd_window_manager_remove_window (XrdWindowManager *self,
                                  XrdWindow        *window);

void
xrd_window_manager_drag_start (XrdWindowManager *self,
                               int controller_index);

void
xrd_window_manager_scale (XrdWindowManager *self,
                          GrabState *grab_state,
                          float factor,
                          float update_rate_ms);

void
xrd_window_manager_check_grab (XrdWindowManager *self,
                               int controller_index);

void
xrd_window_manager_check_release (XrdWindowManager *self,
                                 int controller_index);

void
xrd_window_manager_update_pose (XrdWindowManager *self,
                                graphene_matrix_t *pose,
                                int controller_index);

void
xrd_window_manager_save_reset_transform (XrdWindowManager *self,
                                         XrdWindow *window);

gboolean
xrd_window_manager_is_hovering (XrdWindowManager *self);

gboolean
xrd_window_manager_is_grabbing (XrdWindowManager *self);

gboolean
xrd_window_manager_is_grabbed (XrdWindowManager *self,
                               XrdWindow *window);

gboolean
xrd_window_manager_is_hovered (XrdWindowManager *self,
                               XrdWindow *window);

void
xrd_window_manager_poll_window_events (XrdWindowManager *self);

GrabState *
xrd_window_manager_get_grab_state (XrdWindowManager *self,
                                   int controller_index);

HoverState *
xrd_window_manager_get_hover_state (XrdWindowManager *self,
                                    int controller_index);

void
xrd_window_manager_set_pin (XrdWindowManager *self,
                            XrdWindow *win,
                            gboolean pin);

gboolean
xrd_window_manager_is_pinned (XrdWindowManager *self,
                              XrdWindow *win);

GSList *
xrd_window_manager_get_windows (XrdWindowManager *self);

void
xrd_window_manager_show_pinned_only (XrdWindowManager *self,
                                     gboolean pinned_only);

void
xrd_window_manager_show_controls (XrdWindowManager *self,
                                  gboolean          show_controls);

gboolean
xrd_window_manager_is_controls_shown (XrdWindowManager *self);

GSList *
xrd_window_manager_get_buttons (XrdWindowManager *self);

G_END_DECLS

#endif /* XRD_WINDOW_MANAGER_H_ */
