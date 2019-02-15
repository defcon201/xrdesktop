/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_OVERLAY_WINDOW_MANAGER_H_
#define XRD_OVERLAY_WINDOW_MANAGER_H_

#include <glib-object.h>

#include "xrd-overlay-window.h"
#include "openvr-action.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_WINDOW_MANAGER xrd_overlay_window_manager_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayWindowManager, xrd_overlay_window_manager, XRD,
                      OVERLAY_WINDOW_MANAGER, GObject)

typedef struct OpenVRNoHoverEvent
{
  graphene_matrix_t pose;
  int controller_index;
} OpenVRNoHoverEvent;

typedef struct TransformTransition
{
  XrdOverlayWindow *window;
  graphene_matrix_t from;
  graphene_matrix_t to;
  float from_width;
  float to_width;
  float interpolate;
} TransformTransition;

typedef struct HoverState
{
  OpenVROverlay    *overlay;
  graphene_matrix_t pose;
  float             distance;
  graphene_point_t  intersection_offset;
} HoverState;

typedef struct GrabState
{
  OpenVROverlay    *overlay;
  graphene_quaternion_t overlay_rotation;
  /* the rotation induced by the overlay being moved on the controller arc */
  graphene_quaternion_t overlay_transformed_rotation_neg;
  graphene_point3d_t offset_translation_point;
} GrabState;

typedef enum
{
  OPENVR_OVERLAY_HOVER               = 1 << 0,
  OPENVR_OVERLAY_GRAB                = 1 << 1,
  OPENVR_OVERLAY_DESTROY_WITH_PARENT = 1 << 2
} OpenVROverlayFlags;

struct _XrdOverlayWindowManager
{
  GObject parent;

  GSList *grab_windows;
  GSList *hover_windows;
  GSList *destroy_windows;

  HoverState hover_state[OPENVR_CONTROLLER_COUNT];
  GrabState grab_state[OPENVR_CONTROLLER_COUNT];

  GHashTable *reset_transforms;
  GHashTable *reset_widths;
};

XrdOverlayWindowManager *xrd_overlay_window_manager_new (void);

void
xrd_overlay_window_manager_arrange_reset (XrdOverlayWindowManager *self);

gboolean
xrd_overlay_window_manager_arrange_sphere (XrdOverlayWindowManager *self);

void
xrd_overlay_window_manager_add_window (XrdOverlayWindowManager *self,
                                       XrdOverlayWindow        *window,
                                       OpenVROverlayFlags       flags);

void
xrd_overlay_window_manager_remove_window (XrdOverlayWindowManager *self,
                                          XrdOverlayWindow        *window);

void
xrd_overlay_window_manager_drag_start (XrdOverlayWindowManager *self,
                                       int controller_index);

void
xrd_overlay_window_manager_scale (XrdOverlayWindowManager *self,
                                  GrabState *grab_state,
                                  float factor,
                                  float update_rate_ms);

void
xrd_overlay_window_manager_check_grab (XrdOverlayWindowManager *self,
                                       int controller_index);

void
xrd_overlay_window_manager_check_release (XrdOverlayWindowManager *self,
                                          int controller_index);

void
xrd_overlay_window_manager_update_pose (XrdOverlayWindowManager *self,
                                        graphene_matrix_t *pose,
                                        int controller_index);

void
xrd_overlay_window_manager_save_reset_transform (XrdOverlayWindowManager *self,
                                                 XrdOverlayWindow *window);

gboolean
xrd_overlay_window_manager_is_hovering (XrdOverlayWindowManager *self);

gboolean
xrd_overlay_window_manager_is_grabbing (XrdOverlayWindowManager *self);

gboolean
xrd_overlay_window_manager_is_grabbed (XrdOverlayWindowManager *self,
                                       XrdOverlayWindow *window);

gboolean
xrd_overlay_window_manager_is_hovered (XrdOverlayWindowManager *self,
                                       XrdOverlayWindow *window);

void
xrd_overlay_window_manager_poll_overlay_events (XrdOverlayWindowManager *self);

G_END_DECLS

#endif /* XRD_OVERLAY_WINDOW_MANAGER_H_ */
