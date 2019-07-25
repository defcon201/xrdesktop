/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-controller.h"

struct _XrdController
{
  GObject parent;

  guint64 controller_handle;
  XrdPointer *pointer_ray;
  XrdPointerTip *pointer_tip;
  XrdHoverState hover_state;
  XrdGrabState grab_state;

  graphene_matrix_t pose_hand_grip;
};

G_DEFINE_TYPE (XrdController, xrd_controller, G_TYPE_OBJECT)

static void
xrd_controller_finalize (GObject *gobject);

static void
xrd_controller_class_init (XrdControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_controller_finalize;
}

static void
xrd_controller_init (XrdController *self)
{
  self->hover_state.distance = 1.0f;
  self->hover_state.window = NULL;
  self->grab_state.window = NULL;
  self->grab_state.transform_lock = XRD_TRANSFORM_LOCK_NONE;
}

XrdController *
xrd_controller_new (guint64 controller_handle)
{
  XrdController *controller =
    (XrdController*) g_object_new (XRD_TYPE_CONTROLLER, 0);
  controller->controller_handle = controller_handle;
  return controller;
}

static void
xrd_controller_finalize (GObject *gobject)
{
  XrdController *self = XRD_CONTROLLER (gobject);
  g_object_unref (self->pointer_ray);
  g_object_unref (self->pointer_tip);
  (void) self;
}

XrdPointer *
xrd_controller_get_pointer (XrdController *self)
{
  return self->pointer_ray;
}

XrdPointerTip *
xrd_controller_get_pointer_tip (XrdController *self)
{
  return self->pointer_tip;
}

void
xrd_controller_set_pointer (XrdController *self, XrdPointer *pointer)
{
  self->pointer_ray = pointer;
}

void
xrd_controller_set_pointer_tip (XrdController *self, XrdPointerTip *tip)
{
  self->pointer_tip = tip;
}

guint64
xrd_controller_get_handle (XrdController *self)
{
  return self->controller_handle;
}

XrdHoverState *
xrd_controller_get_hover_state (XrdController *self)
{
  return &self->hover_state;
}

XrdGrabState *
xrd_controller_get_grab_state (XrdController *self)
{
  return &self->grab_state;
}

void
xrd_controller_reset_grab_state (XrdController *self)
{
  self->grab_state.window = NULL;
  graphene_point3d_init (&self->grab_state.grab_offset, 0, 0, 0);
  graphene_quaternion_init_identity (
    &self->grab_state.inverse_controller_rotation);
  graphene_quaternion_init_identity (&self->grab_state.window_rotation);
  self->grab_state.transform_lock = XRD_TRANSFORM_LOCK_NONE;
}

void
xrd_controller_reset_hover_state (XrdController *self)
{
  self->hover_state.window = NULL;
  graphene_point_init (&self->hover_state.intersection_2d, 0, 0);
  self->hover_state.distance = 1.0;
  graphene_matrix_init_identity (&self->hover_state.pose);
}

void
xrd_controller_update_pose_hand_grip (XrdController *self,
                                      graphene_matrix_t *pose)
{
  graphene_matrix_init_from_matrix (&self->pose_hand_grip, pose);
}

void
xrd_controller_get_pose_hand_grip (XrdController *self,
                                   graphene_matrix_t *pose)
{
  graphene_matrix_init_from_matrix (pose, &self->pose_hand_grip);
}

void
xrd_controller_hide_pointer (XrdController *self)
{
  gboolean visible =
    xrd_pointer_is_visible (self->pointer_ray) ||
    xrd_pointer_tip_is_visible (self->pointer_tip);

  if (visible)
    {
      xrd_pointer_hide (self->pointer_ray);
      xrd_pointer_tip_hide (self->pointer_tip);
    }

}

void
xrd_controller_show_pointer (XrdController *self)
{
  gboolean visible =
    xrd_pointer_is_visible (self->pointer_ray) ||
    xrd_pointer_tip_is_visible (self->pointer_tip);

  if (!visible)
    {
      xrd_pointer_show (self->pointer_ray);
      xrd_pointer_tip_show (self->pointer_tip);
    }
}

gboolean
xrd_controller_is_pointer_visible (XrdController *self)
{
  return
    xrd_pointer_is_visible (self->pointer_ray) ||
    xrd_pointer_tip_is_visible (self->pointer_tip);
}
