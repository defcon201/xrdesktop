/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_DESKTOP_CURSOR_H_
#define XRD_DESKTOP_CURSOR_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define XRD_TYPE_DESKTOP_CURSOR xrd_desktop_cursor_get_type()
G_DECLARE_INTERFACE (XrdDesktopCursor, xrd_desktop_cursor, XRD, DESKTOP_CURSOR, GObject)

struct _XrdDesktopCursorInterface
{
  GTypeInterface parent;
};

G_END_DECLS

#endif /* XRD_DESKTOP_CURSOR_H_ */
