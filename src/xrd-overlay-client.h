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

#include "xrd-client.h"

#include "xrd-overlay-pointer.h"
#include "xrd-overlay-pointer-tip.h"
#include "xrd-window-manager.h"
#include "xrd-input-synth.h"
#include "xrd-overlay-desktop-cursor.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_CLIENT xrd_overlay_client_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayClient, xrd_overlay_client,
                      XRD, OVERLAY_CLIENT, XrdClient)

struct _XrdOverlayClient;

XrdOverlayClient *xrd_overlay_client_new (void);

XrdOverlayWindow *
xrd_overlay_client_add_window (XrdOverlayClient *self,
                               const char       *title,
                               gpointer          native,
                               float             ppm,
                               gboolean          is_child,
                               gboolean          follow_head);

gboolean
xrd_overlay_client_add_button (XrdOverlayClient   *self,
                               XrdWindow         **button,
                               gchar              *label,
                               graphene_point3d_t *position,
                               GCallback           press_callback,
                               gpointer            press_callback_data);
GulkanClient *
xrd_overlay_client_get_uploader (XrdOverlayClient *self);

XrdOverlayDesktopCursor *
xrd_overlay_client_get_cursor (XrdOverlayClient *self);

XrdWindow *
xrd_overlay_client_get_keyboard_window (XrdOverlayClient *self);

void
xrd_overlay_client_save_reset_transform (XrdOverlayClient *self,
                                         XrdWindow *window);

void
xrd_overlay_client_submit_cursor_texture (XrdOverlayClient *self,
                                          GulkanClient *client,
                                          GulkanTexture *texture,
                                          int hotspot_x,
                                          int hotspot_y);

G_END_DECLS

#endif /* XRD_GLIB_OVERLAY_CLIENT_H_ */
