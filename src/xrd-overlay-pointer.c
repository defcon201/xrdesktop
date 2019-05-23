/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-pointer.h"
#include "xrd-pointer.h"

struct _XrdOverlayPointer
{
  XrdOverlayModel parent;

  float default_length;
  float length;
};

static void
xrd_overlay_pointer_pointer_interface_init (XrdPointerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdOverlayPointer, xrd_overlay_pointer, XRD_TYPE_OVERLAY_MODEL,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_POINTER,
                                                xrd_overlay_pointer_pointer_interface_init))

static void
xrd_overlay_pointer_finalize (GObject *gobject);

static void
xrd_overlay_pointer_class_init (XrdOverlayPointerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_pointer_finalize;
}

static void
xrd_overlay_pointer_init (XrdOverlayPointer *self)
{
  self->default_length = 5.0;
  self->length = 5.0;
}

XrdOverlayPointer *
xrd_overlay_pointer_new (int controller_index)
{
  XrdOverlayPointer *self = (XrdOverlayPointer*) g_object_new (XRD_TYPE_OVERLAY_POINTER, 0);

  char key[k_unVROverlayMaxKeyLength];
  snprintf (key, k_unVROverlayMaxKeyLength - 1, "pointer-%d",
            controller_index);

  if (!xrd_overlay_model_initialize (XRD_OVERLAY_MODEL (self), key, key))
    return NULL;

  /*
   * The pointer itself should always be visible on top of overlays,
   * so we use UINT32_MAX here.
   */
  openvr_overlay_set_sort_order (OPENVR_OVERLAY (self), UINT32_MAX);

  graphene_vec4_t color;
  graphene_vec4_init (&color, 1., 1., 1., 1.);

  if (!xrd_overlay_model_set_model (XRD_OVERLAY_MODEL (self),
                                    "{system}laser_pointer",
                                    &color))
    return NULL;

  if (!openvr_overlay_set_width_meters (OPENVR_OVERLAY (self), 0.01f))
    return NULL;

  if (!openvr_overlay_show (OPENVR_OVERLAY (self)))
    return NULL;

  return self;
}

static void
xrd_overlay_pointer_finalize (GObject *gobject)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (gobject);
  (void) self;
  G_OBJECT_CLASS (xrd_overlay_pointer_parent_class)->finalize (gobject);
}

static void
_move (XrdPointer        *pointer,
       graphene_matrix_t *transform)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (pointer);
  graphene_matrix_t scale_matrix;
  graphene_matrix_init_scale (&scale_matrix, 1.0f, 1.0f, self->length);
  graphene_matrix_t scaled;
  graphene_matrix_multiply (&scale_matrix, transform, &scaled);
  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), &scaled);
}

static void
_set_length (XrdPointer *pointer,
             float       length)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (pointer);
  self->length = length;
}

static void
_reset_length (XrdPointer *pointer)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (pointer);
  self->length = self->default_length;
}

static float
_get_default_length (XrdPointer *pointer)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (pointer);
  return self->default_length;
}

static void
xrd_overlay_pointer_pointer_interface_init (XrdPointerInterface *iface)
{
  iface->move = _move;
  iface->set_length = _set_length;
  iface->get_default_length = _get_default_length;
  iface->reset_length = _reset_length;
}
