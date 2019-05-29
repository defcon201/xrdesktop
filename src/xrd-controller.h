/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_CONTROLLER_H_
#define XRD_GLIB_CONTROLLER_H_

#include <glib-object.h>

#include "xrd-window.h"
#include "xrd-pointer.h"
#include "xrd-pointer-tip.h"

G_BEGIN_DECLS

#define XRD_TYPE_CONTROLLER xrd_controller_get_type()
G_DECLARE_FINAL_TYPE (XrdController, xrd_controller, XRD, CONTROLLER, GObject)

typedef struct HoverState
{
  XrdWindow *window;
  graphene_matrix_t pose;
  float             distance;
  graphene_point_t  intersection_2d;
} HoverState;

typedef struct GrabState
{
  XrdWindow    *window;
  graphene_quaternion_t window_rotation;
  /* the rotation induced by the overlay being moved on the controller arc */
  graphene_quaternion_t window_transformed_rotation_neg;
  graphene_point3d_t offset_translation_point;
} GrabState;

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

HoverState *
xrd_controller_get_hover_state (XrdController *self);

GrabState *
xrd_controller_get_grab_state (XrdController *self);

void
xrd_controller_reset_grab_state (XrdController *self);

void
xrd_controller_reset_hover_state (XrdController *self);

G_END_DECLS

#endif /* XRD_GLIB_CONTROLLER_H_ */
