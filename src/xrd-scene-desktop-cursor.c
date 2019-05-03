/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-desktop-cursor.h"

G_DEFINE_TYPE (XrdSceneDesktopCursor, xrd_scene_desktop_cursor, G_TYPE_OBJECT)

static void
xrd_scene_desktop_cursor_finalize (GObject *gobject);

static void
xrd_scene_desktop_cursor_class_init (XrdSceneDesktopCursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_desktop_cursor_finalize;
}

static void
xrd_scene_desktop_cursor_init (XrdSceneDesktopCursor *self)
{
  (void) self;
}

XrdSceneDesktopCursor *
xrd_scene_desktop_cursor_new (void)
{
  return (XrdSceneDesktopCursor*) g_object_new (XRD_TYPE_SCENE_DESKTOP_CURSOR, 0);
}

static void
xrd_scene_desktop_cursor_finalize (GObject *gobject)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (gobject);
  (void) self;

  G_OBJECT_CLASS (xrd_scene_desktop_cursor_parent_class)->finalize (gobject);
}
