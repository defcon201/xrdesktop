/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_OVERLAY_MANAGER_H_
#define XRD_OVERLAY_MANAGER_H_

#include <glib-object.h>

#include "openvr-overlay.h"
#include "openvr-action.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_MANAGER xrd_overlay_manager_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayManager, xrd_overlay_manager, XRD,
                      OVERLAY_MANAGER, GObject)

typedef struct OpenVRNoHoverEvent
{
  graphene_matrix_t pose;
  int controller_index;
} OpenVRNoHoverEvent;

typedef struct TransformTransition
{
  OpenVROverlay *overlay;
  graphene_matrix_t from;
  graphene_matrix_t to;
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

struct _XrdOverlayManager
{
  GObject parent;

  GSList *grab_overlays;
  GSList *hover_overlays;
  GSList *destroy_overlays;

  HoverState hover_state[OPENVR_CONTROLLER_COUNT];
  GrabState grab_state[OPENVR_CONTROLLER_COUNT];

  GHashTable *reset_transforms;
};

XrdOverlayManager *xrd_overlay_manager_new (void);

void
xrd_overlay_manager_arrange_reset (XrdOverlayManager *self);

gboolean
xrd_overlay_manager_arrange_sphere (XrdOverlayManager *self,
                                       uint32_t              grid_width,
                                       uint32_t              grid_height);

void
xrd_overlay_manager_add_overlay (XrdOverlayManager *self,
                                    OpenVROverlay        *overlay,
                                    OpenVROverlayFlags    flags);

void
xrd_overlay_manager_remove_overlay (XrdOverlayManager *self,
                                       OpenVROverlay        *overlay);

void
xrd_overlay_manager_drag_start (XrdOverlayManager *self,
                                   int                   controller_index);

void
xrd_overlay_manager_scale (XrdOverlayManager *self,
                              GrabState *grab_state,
                              float factor,
                              float update_rate_ms);

void
xrd_overlay_manager_check_grab (XrdOverlayManager *self,
                                   int                   controller_index);

void
xrd_overlay_manager_check_release (XrdOverlayManager *self,
                                      int                   controller_index);

void
xrd_overlay_manager_update_pose (XrdOverlayManager *self,
                                    graphene_matrix_t    *pose,
                                    int                   controller_index);

void
xrd_overlay_manager_save_reset_transform (XrdOverlayManager *self,
                                             OpenVROverlay        *overlay);

gboolean
xrd_overlay_manager_is_hovering (XrdOverlayManager *self);

gboolean
xrd_overlay_manager_is_grabbing (XrdOverlayManager *self);

gboolean
xrd_overlay_manager_is_grabbed (XrdOverlayManager *self,
                                   OpenVROverlay *overlay);

gboolean
xrd_overlay_manager_is_hovered (XrdOverlayManager *self,
                                   OpenVROverlay *overlay);

G_END_DECLS

#endif /* XRD_OVERLAY_MANAGER_H_ */
