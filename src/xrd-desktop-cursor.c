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
