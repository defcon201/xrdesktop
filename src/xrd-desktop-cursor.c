/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-desktop-cursor.h"

G_DEFINE_INTERFACE (XrdDesktopCursor, xrd_desktop_cursor, G_TYPE_OBJECT)


static void
xrd_desktop_cursor_default_init (XrdDesktopCursorInterface *iface)
{
  (void) iface;
}

void
xrd_desktop_cursor_submit_texture (XrdDesktopCursor *self,
                                   GulkanClient     *uploader,
                                   GulkanTexture    *texture,
                                   int               hotspot_x,
                                   int               hotspot_y)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  return iface->submit_texture (self, uploader, texture, hotspot_x, hotspot_y);
}

void
xrd_desktop_cursor_update (XrdDesktopCursor   *self,
                           XrdWindow          *window,
                           graphene_point3d_t *intersection)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  return iface->update (self, window, intersection);
}

void
xrd_desktop_cursor_show (XrdDesktopCursor *self)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  return iface->show (self);
}

void
xrd_desktop_cursor_hide (XrdDesktopCursor *self)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  return iface->hide (self);
}

void
xrd_desktop_cursor_update_apparent_size (XrdDesktopCursor   *self,
                                         graphene_point3d_t *cursor_point)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  return iface->update_apparent_size (self, cursor_point);
}
