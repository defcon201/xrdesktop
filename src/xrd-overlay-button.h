/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_OVERLAY_BUTTON_H_
#define XRD_OVERLAY_BUTTON_H_

#include <glib-object.h>
#include "xrd-overlay-window.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_BUTTON xrd_overlay_button_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayButton, xrd_overlay_button, XRD, OVERLAY_BUTTON,
                      XrdOverlayWindow)

struct _XrdOverlayButton
{
  XrdOverlayWindow parent;
};

XrdOverlayButton *
xrd_overlay_button_new (gchar *text);

void
xrd_overlay_button_unmark (XrdOverlayButton *self);

void
xrd_overlay_button_mark_color (XrdOverlayButton *self,
                               float r, float g, float b);
G_END_DECLS

#endif /* XRD_OVERLAY_BUTTON_H_ */
