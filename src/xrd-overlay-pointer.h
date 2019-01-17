/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_OVERLAY_POINTER_H_
#define XRD_OVERLAY_POINTER_H_

#include <glib-object.h>
#include "xrd-overlay-model.h"

#include "openvr-action.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_POINTER xrd_overlay_pointer_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayPointer, xrd_overlay_pointer, XRD, OVERLAY_POINTER,
                      XrdOverlayModel)

struct _XrdOverlayPointer
{
  XrdOverlayModel parent;

  float default_length;
  float length;
};

XrdOverlayPointer *xrd_overlay_pointer_new (int controller_index);

void
xrd_overlay_pointer_move (XrdOverlayPointer *self,
                          graphene_matrix_t *transform);

void
xrd_overlay_pointer_set_length (XrdOverlayPointer *self,
                                float              length);

void
xrd_overlay_pointer_reset_length (XrdOverlayPointer *self);



G_END_DECLS

#endif /* XRD_OVERLAY_POINTER_H_ */
