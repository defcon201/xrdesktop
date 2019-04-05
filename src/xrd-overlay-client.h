/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_OVERLAY_CLIENT_H_
#define XRD_GLIB_OVERLAY_CLIENT_H_

#include <glib-object.h>
#include <gmodule.h>

#include <openvr-action-set.h>

#include "xrd-overlay-pointer.h"
#include "xrd-overlay-pointer-tip.h"
#include "xrd-window-manager.h"
#include "xrd-input-synth.h"
#include "xrd-overlay-desktop-cursor.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_CLIENT xrd_overlay_client_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayClient, xrd_overlay_client,
                      XRD, OVERLAY_CLIENT, GObject)

typedef struct XrdClientController
{
  XrdOverlayClient *self;
  int               index;
} XrdClientController;

struct _XrdOverlayClient;

XrdOverlayClient *xrd_overlay_client_new (void);

XrdOverlayWindow *
xrd_overlay_client_add_window (XrdOverlayClient *self,
                               const char       *title,
                               gpointer          native,
                               float             ppm,
                               gboolean          is_child,
                               gboolean          follow_head);

void
xrd_overlay_client_remove_window (XrdOverlayClient *self,
                                  XrdOverlayWindow *window);

GulkanClient *
xrd_overlay_client_get_uploader (XrdOverlayClient *self);

XrdWindowManager *
xrd_overlay_client_get_manager (XrdOverlayClient *self);

XrdOverlayDesktopCursor *
xrd_overlay_client_get_cursor (XrdOverlayClient *self);

G_END_DECLS

#endif /* XRD_GLIB_OVERLAY_CLIENT_H_ */
