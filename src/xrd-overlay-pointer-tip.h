/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_OVERLAY_POINTER_TIP_H_
#define XRD_OVERLAY_POINTER_TIP_H_

#include <glib-object.h>
#include <gulkan-texture.h>

#include "openvr-overlay-uploader.h"
#include "openvr-overlay.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_POINTER_TIP xrd_overlay_pointer_tip_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayPointerTip, xrd_overlay_pointer_tip, XRD,
                      OVERLAY_POINTER_TIP, OpenVROverlay)

struct Animation;

struct _XrdOverlayPointerTip;

XrdOverlayPointerTip *
xrd_overlay_pointer_tip_new (int controller_index,
                             OpenVROverlayUploader *uploader);

void
xrd_overlay_pointer_tip_set_constant_width (XrdOverlayPointerTip *self);

void
xrd_overlay_pointer_tip_update (XrdOverlayPointerTip *self,
                                graphene_matrix_t    *pose,
                                graphene_point3d_t   *intersection_point);

void
xrd_overlay_pointer_tip_set_active (XrdOverlayPointerTip  *self,
                                    gboolean               active);

void
xrd_overlay_pointer_tip_init_vulkan (XrdOverlayPointerTip  *self);

void
xrd_overlay_pointer_tip_animate_pulse (XrdOverlayPointerTip  *self);

void
xrd_overlay_pointer_tip_set_transformation_matrix (XrdOverlayPointerTip *self,
                                                   graphene_matrix_t *matrix);

void
xrd_overlay_pointer_tip_show (XrdOverlayPointerTip *self);

void
xrd_overlay_pointer_tip_hide (XrdOverlayPointerTip *self);


G_END_DECLS

#endif /* XRD_OVERLAY_POINTER_TIP_H_ */
