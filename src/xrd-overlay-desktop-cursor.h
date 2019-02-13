/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_OVERLAY_DESKTOP_CURSOR_H_
#define XRD_GLIB_OVERLAY_DESKTOP_CURSOR_H_

#include <glib-object.h>

#include "openvr-overlay-uploader.h"
#include "xrd-overlay-window.h"
#include "openvr-overlay.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_DESKTOP_CURSOR xrd_overlay_desktop_cursor_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayDesktopCursor, xrd_overlay_desktop_cursor, XRD,
                      OVERLAY_DESKTOP_CURSOR, OpenVROverlay)

struct _XrdOverlayDesktopCursor
{
  OpenVROverlay parent;

  OpenVROverlayUploader *uploader;

  float cursor_width_meter;

  int hotspot_x;
  int hotspot_y;

  GdkPixbuf *pixbuf;
  /* texture is cached to minimize texture allocations */
  GulkanTexture *texture;
};

XrdOverlayDesktopCursor *
xrd_overlay_desktop_cursor_new (OpenVROverlayUploader *uploader);

void
xrd_overlay_desktop_cursor_upload_pixbuf (XrdOverlayDesktopCursor *self,
                                          GdkPixbuf *pixbuf,
                                          int hotspot_x,
                                          int hotspot_y);

void
xrd_overlay_desktop_cursor_update (XrdOverlayDesktopCursor *self,
                                   XrdOverlayWindow        *window,
                                   graphene_point3d_t      *intersection);

void
xrd_overlay_desktop_cursor_show (XrdOverlayDesktopCursor *self);

void
xrd_overlay_desktop_cursor_hide (XrdOverlayDesktopCursor *self);

G_END_DECLS

#endif /* XRD_GLIB_OVERLAY_DESKTOP_CURSOR_H_ */
