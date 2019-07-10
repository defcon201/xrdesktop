/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_CONTROLLER_H_
#define XRD_CONTROLLER_H_

#include <glib-object.h>

#include "xrd-window.h"
#include "xrd-pointer.h"
#include "xrd-pointer-tip.h"

G_BEGIN_DECLS

#define XRD_TYPE_CONTROLLER xrd_controller_get_type()
G_DECLARE_FINAL_TYPE (XrdController, xrd_controller, XRD, CONTROLLER, GObject)

/**
 * XrdTransformLock:
 * @XRD_TRANSFORM_LOCK_NONE: The grab action does not currently have a transformation it is locked to.
 * @XRD_TRANSFORM_LOCK_PUSH_PULL: Only push pull transformation can be performed.
 * @XRD_TRANSFORM_LOCK_SCALE: Only a scale transformation can be performed.
 *
 * The type of transformation the grab action is currently locked to.
 * This will be detected at the begginging of a grab transformation
 * and reset after the transformation is done.
 *
 **/
typedef enum {
  XRD_TRANSFORM_LOCK_NONE,
  XRD_TRANSFORM_LOCK_PUSH_PULL,
  XRD_TRANSFORM_LOCK_SCALE
} XrdTransformLock;

typedef struct {
  XrdWindow        *window;
  graphene_matrix_t pose;
  float             distance;
  graphene_point_t  intersection_2d;
} XrdHoverState;

typedef struct {
  XrdWindow    *window;

  /* window rotation, controller rotation, offset at the moment the window
   * was grabbed, enables keeping transform when grabbing a window.  */
  graphene_quaternion_t window_rotation;
  graphene_quaternion_t inverse_controller_rotation;
  graphene_point3d_t grab_offset;

  XrdTransformLock transform_lock;
} XrdGrabState;

XrdController *xrd_controller_new (guint64 controller_handle);

XrdPointer *
xrd_controller_get_pointer (XrdController *self);

XrdPointerTip *
xrd_controller_get_pointer_tip (XrdController *self);

void
xrd_controller_set_pointer (XrdController *self, XrdPointer *pointer);

void
xrd_controller_set_pointer_tip (XrdController *self, XrdPointerTip *tip);

guint64
xrd_controller_get_handle (XrdController *self);

XrdHoverState *
xrd_controller_get_hover_state (XrdController *self);

XrdGrabState *
xrd_controller_get_grab_state (XrdController *self);

void
xrd_controller_reset_grab_state (XrdController *self);

void
xrd_controller_reset_hover_state (XrdController *self);

void
xrd_controller_update_pose_hand_grip (XrdController *self,
                                      graphene_matrix_t *pose);

void
xrd_controller_get_pose_hand_grip (XrdController *self,
                                   graphene_matrix_t *pose);

G_END_DECLS

#endif /* XRD_CONTROLLER_H_ */
