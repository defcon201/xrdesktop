/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-pointer.h"
#include "xrd-pointer.h"

struct _XrdOverlayPointer
{
  XrdOverlayModel parent;

  XrdPointerData data;
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
  xrd_pointer_init (XRD_POINTER (self));
}

XrdOverlayPointer *
xrd_overlay_pointer_new (guint64 controller_index)
{
  XrdOverlayPointer *self = (XrdOverlayPointer*) g_object_new (XRD_TYPE_OVERLAY_POINTER, 0);

  char key[k_unVROverlayMaxKeyLength];
  snprintf (key, k_unVROverlayMaxKeyLength - 1, "pointer-%ld",
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
  graphene_matrix_init_scale (&scale_matrix, 1.0f, 1.0f, self->data.length);
  graphene_matrix_t scaled;
  graphene_matrix_multiply (&scale_matrix, transform, &scaled);
  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), &scaled);
}

static void
_set_length (XrdPointer *pointer,
             float       length)
{
  (void) pointer;
  (void) length;
}

static XrdPointerData*
_get_data (XrdPointer *pointer)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (pointer);
  return &self->data;
}

static void
_set_transformation (XrdPointer        *pointer,
                     graphene_matrix_t *matrix)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (pointer);
  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), matrix);
}

static void
_get_transformation (XrdPointer        *pointer,
                     graphene_matrix_t *matrix)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (pointer);
  openvr_overlay_get_transform_absolute (OPENVR_OVERLAY(self), matrix);
}

static void
_set_selected_window (XrdPointer *pointer,
                      XrdWindow  *window)
{
  (void) pointer;
  (void) window;
}

static void
_show (XrdPointer *pointer)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (pointer);
  openvr_overlay_show (OPENVR_OVERLAY (self));
}

static void
_hide (XrdPointer *pointer)
{
  XrdOverlayPointer *self = XRD_OVERLAY_POINTER (pointer);
  openvr_overlay_hide (OPENVR_OVERLAY (self));
}

static void
xrd_overlay_pointer_pointer_interface_init (XrdPointerInterface *iface)
{
  iface->move = _move;
  iface->set_length = _set_length;
  iface->get_data = _get_data;
  iface->set_transformation = _set_transformation;
  iface->get_transformation = _get_transformation;
  iface->set_selected_window = _set_selected_window;
  iface->show = _show;
  iface->hide = _hide;
}
