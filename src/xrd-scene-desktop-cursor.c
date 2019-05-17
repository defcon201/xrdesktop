/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-desktop-cursor.h"

#include "xrd-desktop-cursor.h"

struct _XrdSceneDesktopCursor
{
  XrdSceneObject parent;

  XrdDesktopCursorData data;
};

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
  self->data.texture_width = 0;
  self->data.texture_height = 0;
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
_submit_texture (XrdDesktopCursor *cursor,
                 GulkanClient     *uploader,
                 GulkanTexture    *texture,
                 int               hotspot_x,
                 int               hotspot_y)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  (void) self;
  (void) uploader;
  (void) texture;
  (void) hotspot_x;
  (void) hotspot_y;

  // g_warning ("stub: _submit_texture\n");
}

static void
_update (XrdDesktopCursor *cursor,
         XrdWindow             *window,
         graphene_point3d_t    *intersection)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  (void) self;
  (void) window;
  (void) intersection;

  g_warning ("stub: _update\n");
}

static void
_show (XrdDesktopCursor *cursor)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  (void) self;

  g_warning ("stub: _show\n");
}

static void
_hide (XrdDesktopCursor *cursor)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  (void) self;
  g_warning ("stub: _hide\n");
}

static void
_update_apparent_size (XrdDesktopCursor *cursor,
                       graphene_point3d_t    *cursor_point)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  (void) self;
  (void) cursor_point;
  g_warning ("stub: _set_constant_width\n");
}

static void
_set_width_meters (XrdDesktopCursor *cursor, float width)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  (void) self;
  (void) width;
  g_warning ("stub: _set_width_meters\n");
}

static XrdDesktopCursorData*
_get_data (XrdDesktopCursor *cursor)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  return &self->data;
}

static void
xrd_scene_desktop_cursor_interface_init (XrdDesktopCursorInterface *iface)
{
  iface->submit_texture = _submit_texture;
  iface->update = _update;
  iface->show = _show;
  iface->hide = _hide;
  iface->update_apparent_size = _update_apparent_size;
  iface->set_width_meters = _set_width_meters;
  iface->get_data = _get_data;
}
