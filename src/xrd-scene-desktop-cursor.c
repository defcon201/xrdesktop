/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-desktop-cursor.h"

#include "xrd-desktop-cursor.h"

//G_DEFINE_TYPE (XrdSceneDesktopCursor, xrd_scene_desktop_cursor, G_TYPE_OBJECT)

static void
xrd_scene_desktop_cursor_interface_init (XrdDesktopCursorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdSceneDesktopCursor, xrd_scene_desktop_cursor,
                         XRD_TYPE_SCENE_OBJECT,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_DESKTOP_CURSOR,
                                                xrd_scene_desktop_cursor_interface_init))

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

static void
_submit_texture (XrdSceneDesktopCursor *self,
                 GulkanClient          *uploader,
                 GulkanTexture         *texture,
                 int                    hotspot_x,
                 int                    hotspot_y)
{
  (void) self;
  (void) uploader;
  (void) texture;
  (void) hotspot_x;
  (void) hotspot_y;

  g_warning ("stub: _submit_texture\n");
}

static void
_update (XrdSceneDesktopCursor *self,
         XrdWindow             *window,
         graphene_point3d_t    *intersection)
{
  (void) self;
  (void) window;
  (void) intersection;

  g_warning ("stub: _update\n");
}

static void
_show (XrdSceneDesktopCursor *self)
{
  (void) self;

  g_warning ("stub: _show\n");
}

static void
_hide (XrdSceneDesktopCursor *self)
{
  (void) self;
  g_warning ("stub: _hide\n");
}

static void
_set_constant_width (XrdSceneDesktopCursor *self,
                     graphene_point3d_t    *cursor_point)
{
  (void) self;
  (void) cursor_point;
  g_warning ("stub: _set_constant_width\n");
}

static void
xrd_scene_desktop_cursor_interface_init (XrdDesktopCursorInterface *iface)
{
  iface->submit_texture = (void*) _submit_texture;
  iface->update = (void*) _update;
  iface->show = (void*) _show;
  iface->hide = (void*) _hide;
  iface->set_constant_width = (void*) _set_constant_width;
}
