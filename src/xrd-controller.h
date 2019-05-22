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

struct _XrdController
{
  GObject parent;

  guint64 controller_handle;
  XrdWindow *hover_window;
  XrdPointer *pointer_ray;
  XrdPointerTip *pointer_tip;
  HoverState hover_state;
  GrabState grab_state;
};

XrdController *xrd_controller_new (guint64 controller_handle);

G_END_DECLS

#endif /* XRD_GLIB_CONTROLLER_H_ */
