/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_OVERLAY_BUTTON_H_
#define XRD_OVERLAY_BUTTON_H_

#include <glib-object.h>
#include "openvr-overlay.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_BUTTON xrd_overlay_button_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayButton, xrd_overlay_button, XRD, OVERLAY_BUTTON,
                      OpenVROverlay)

struct _XrdOverlayButton
{
  OpenVROverlay parent;
};

XrdOverlayButton *
xrd_overlay_button_new (gchar *id, gchar *text);

G_END_DECLS

#endif /* XRD_OVERLAY_BUTTON_H_ */
