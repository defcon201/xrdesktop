/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_OVERLAY_WINDOW_H_
#define XRD_GLIB_OVERLAY_WINDOW_H_

#include <glib-object.h>

#include <openvr-overlay.h>
#include <gulkan-texture.h>

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_WINDOW xrd_overlay_window_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayWindow, xrd_overlay_window,
                      XRD, OVERLAY_WINDOW, GObject)

struct _XrdOverlayWindow
{
  GObject parent;

  gpointer      *native;
  OpenVROverlay *overlay;
  GulkanTexture *texture;
  int            width;
  int            height;
  uint32_t       gl_texture;
  gboolean       recreate;
};

XrdOverlayWindow *xrd_overlay_window_new (void);

G_END_DECLS

#endif /* XRD_GLIB_OVERLAY_WINDOW_H_ */
