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
  XrdSceneWindow parent;

  XrdDesktopCursorData data;
};

static void
xrd_scene_desktop_cursor_interface_init (XrdDesktopCursorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdSceneDesktopCursor, xrd_scene_desktop_cursor,
                         XRD_TYPE_SCENE_WINDOW,
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
  XrdSceneDesktopCursor *self =
    (XrdSceneDesktopCursor*) g_object_new (XRD_TYPE_SCENE_DESKTOP_CURSOR, 0);

  g_object_set (self,
                "texture-width", 64,
                "texture-height", 64,
                NULL);

  xrd_scene_window_initialize (XRD_SCENE_WINDOW (self));

  xrd_desktop_cursor_init_settings (XRD_DESKTOP_CURSOR (self));

  return self;
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
                 GulkanClient     *client,
                 GulkanTexture    *texture,
                 int               hotspot_x,
                 int               hotspot_y)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  XrdSceneWindow *window = XRD_SCENE_WINDOW (self);
  xrd_window_submit_texture (XRD_WINDOW (window), client, texture);

  self->data.hotspot_x = hotspot_x;
  self->data.hotspot_y = hotspot_y;

  self->data.texture_width = texture->width;
  self->data.texture_height = texture->height;
}

static void
_show (XrdDesktopCursor *cursor)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  obj->visible = TRUE;
}

static void
_hide (XrdDesktopCursor *cursor)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  obj->visible = FALSE;
}

static void
_set_width_meters (XrdDesktopCursor *cursor, float meters)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  XrdSceneWindow *window = XRD_SCENE_WINDOW (self);
  xrd_scene_window_set_width_meters (window, meters);
}

static XrdDesktopCursorData*
_get_data (XrdDesktopCursor *cursor)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  return &self->data;
}

static void
_get_transformation (XrdDesktopCursor  *cursor,
                     graphene_matrix_t *matrix)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  graphene_matrix_t transformation =
    xrd_scene_object_get_transformation (XRD_SCENE_OBJECT (self));
  graphene_matrix_init_from_matrix (matrix, &transformation);
}

static void
_set_transformation (XrdDesktopCursor  *cursor,
                     graphene_matrix_t *matrix)
{
  XrdSceneDesktopCursor *self = XRD_SCENE_DESKTOP_CURSOR (cursor);
  xrd_scene_object_set_transformation (XRD_SCENE_OBJECT (self), matrix);
}

static void
xrd_scene_desktop_cursor_interface_init (XrdDesktopCursorInterface *iface)
{
  iface->submit_texture = _submit_texture;
  iface->show = _show;
  iface->hide = _hide;
  iface->set_width_meters = _set_width_meters;
  iface->get_data = _get_data;
  iface->get_transformation = _get_transformation;
  iface->set_transformation = _set_transformation;
}
