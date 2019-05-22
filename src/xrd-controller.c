/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-controller.h"

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
  self->hover_window = NULL;
  self->hover_state.distance = 1.0f;
  self->hover_state.window = NULL;
  self->grab_state.window = NULL;
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
