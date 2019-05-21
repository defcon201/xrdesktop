/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-desktop-cursor.h"

#include <openvr-glib.h>

#include "xrd-settings.h"
#include "xrd-math.h"
#include "graphene-ext.h"
#include "xrd-desktop-cursor.h"

struct _XrdOverlayDesktopCursor
{
  OpenVROverlay parent;

  XrdDesktopCursorData data;
};

static void
xrd_overlay_desktop_cursor_interface_init (XrdDesktopCursorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdOverlayDesktopCursor, xrd_overlay_desktop_cursor, OPENVR_TYPE_OVERLAY,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_DESKTOP_CURSOR,
                                                xrd_overlay_desktop_cursor_interface_init))

static void
xrd_overlay_desktop_cursor_finalize (GObject *gobject);

static void
xrd_overlay_desktop_cursor_class_init (XrdOverlayDesktopCursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_desktop_cursor_finalize;
}

static void
xrd_overlay_desktop_cursor_init (XrdOverlayDesktopCursor *self)
{
  self->data.texture_width = 0;
  self->data.texture_height = 0;

  openvr_overlay_create (OPENVR_OVERLAY (self),
                         "org.xrdesktop.cursor", "XR Desktop Cursor");
  if (!openvr_overlay_is_valid (OPENVR_OVERLAY (self)))
    {
      g_printerr ("Cursor overlay unavailable.\n");
      return;
    }

  xrd_desktop_cursor_init_settings (XRD_DESKTOP_CURSOR (self));

  /* pointer ray is MAX, pointer tip is MAX - 1, so cursor is MAX - 2 */
  openvr_overlay_set_sort_order (OPENVR_OVERLAY (self), UINT32_MAX - 2);

  openvr_overlay_show (OPENVR_OVERLAY (self));
}

XrdOverlayDesktopCursor *
xrd_overlay_desktop_cursor_new ()
{
  return (XrdOverlayDesktopCursor*) g_object_new (XRD_TYPE_OVERLAY_DESKTOP_CURSOR, 0);
}

static void
_submit_texture (XrdDesktopCursor *cursor,
                 GulkanClient     *uploader,
                 GulkanTexture    *texture,
                 int               hotspot_x,
                 int               hotspot_y)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (cursor);

  openvr_overlay_uploader_submit_frame (OPENVR_OVERLAY_UPLOADER (uploader),
                                        OPENVR_OVERLAY (self), texture);

  self->data.hotspot_x = hotspot_x;
  self->data.hotspot_y = hotspot_y;

  self->data.texture_width = gulkan_texture_get_width (texture);
  self->data.texture_height = gulkan_texture_get_height (texture);
}

static void
_show (XrdDesktopCursor *cursor)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (cursor);
  openvr_overlay_show (OPENVR_OVERLAY (self));
}

static void
_hide (XrdDesktopCursor *cursor)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (cursor);
  openvr_overlay_hide (OPENVR_OVERLAY (self));
}

static void
_set_width_meters (XrdDesktopCursor *cursor, float width)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (cursor);
  openvr_overlay_set_width_meters (OPENVR_OVERLAY (self), width);
  self->data.cached_width_meters = width;
}

static void
xrd_overlay_desktop_cursor_finalize (GObject *gobject)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (gobject);
  openvr_overlay_destroy (OPENVR_OVERLAY (self));
}

static XrdDesktopCursorData*
_get_data (XrdDesktopCursor *cursor)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (cursor);
  return &self->data;
}

static void
_get_transformation (XrdDesktopCursor  *cursor,
                     graphene_matrix_t *matrix)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (cursor);
  openvr_overlay_get_transform_absolute (OPENVR_OVERLAY(self), matrix);
}

static void
_set_transformation (XrdDesktopCursor  *cursor,
                     graphene_matrix_t *matrix)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (cursor);
  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), matrix);
}

static void
xrd_overlay_desktop_cursor_interface_init (XrdDesktopCursorInterface *iface)
{
  iface->submit_texture = _submit_texture;
  iface->show = _show;
  iface->hide = _hide;
  iface->set_width_meters = _set_width_meters;
  iface->get_data = _get_data;
  iface->get_transformation = _get_transformation;
  iface->set_transformation = _set_transformation;
}
