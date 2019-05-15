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


struct _XrdOverlayPointerTip;

XrdOverlayPointerTip *
xrd_overlay_pointer_tip_new (int controller_index,
                             OpenVROverlayUploader *uploader);

G_END_DECLS

#endif /* XRD_OVERLAY_POINTER_TIP_H_ */
