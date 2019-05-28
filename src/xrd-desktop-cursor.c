/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-desktop-cursor.h"

#include <openvr-glib.h>

#include "xrd-settings.h"
#include "graphene-ext.h"
#include "xrd-pointer-tip.h"

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
  iface->submit_texture (self, uploader, texture, hotspot_x, hotspot_y);
}

void
xrd_desktop_cursor_show (XrdDesktopCursor *self)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  iface->show (self);
}

void
xrd_desktop_cursor_hide (XrdDesktopCursor *self)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  iface->hide (self);
}

void
xrd_desktop_cursor_set_width_meters (XrdDesktopCursor *self, float meters)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  iface->set_width_meters (self, meters);
}

XrdDesktopCursorData*
xrd_desktop_cursor_get_data (XrdDesktopCursor *self)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  return iface->get_data (self);
}

void
xrd_desktop_cursor_get_transformation (XrdDesktopCursor  *self,
                                       graphene_matrix_t *matrix)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  iface->get_transformation (self, matrix);
}

void
xrd_desktop_cursor_set_transformation (XrdDesktopCursor  *self,
                                       graphene_matrix_t *matrix)
{
  XrdDesktopCursorInterface* iface = XRD_DESKTOP_CURSOR_GET_IFACE (self);
  iface->set_transformation (self, matrix);
}

static void
_update_width_meters (GSettings *settings, gchar *key, gpointer _data)
{
  XrdDesktopCursorData *data = (XrdDesktopCursorData*) _data;
  XrdDesktopCursor *self = data->cursor;

  data->width_meters = (float) g_settings_get_double (settings, key);
  xrd_desktop_cursor_set_width_meters (self, data->width_meters);
}

static void
_update_keep_apparent_size (GSettings *settings, gchar *key, gpointer _data)
{
  XrdDesktopCursorData *data = (XrdDesktopCursorData*) _data;

  XrdDesktopCursor *self = data->cursor;
  data->keep_apparent_size = g_settings_get_boolean (settings, key);
  if (data->keep_apparent_size)
    {
      graphene_matrix_t cursor_pose;
      xrd_desktop_cursor_get_transformation (self, &cursor_pose);

      graphene_vec3_t cursor_point_vec;
      graphene_matrix_get_translation_vec3 (&cursor_pose, &cursor_point_vec);
      graphene_point3d_t cursor_point;
      graphene_point3d_init_from_vec3 (&cursor_point, &cursor_point_vec);

      xrd_desktop_cursor_update_apparent_size (XRD_DESKTOP_CURSOR (self),
                                               &cursor_point);
    }
  else
    xrd_desktop_cursor_set_width_meters (XRD_DESKTOP_CURSOR (self),
                                         data->width_meters);
}

void
xrd_desktop_cursor_init_settings (XrdDesktopCursor *self)
{
  XrdDesktopCursorData *data = xrd_desktop_cursor_get_data (self);
  data->cursor = self;

  xrd_settings_connect_and_apply (G_CALLBACK (_update_width_meters),
                                  "desktop-cursor-width-meters", data);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_keep_apparent_size),
                                  "pointer-tip-keep-apparent-size", data);
}

void
xrd_desktop_cursor_update (XrdDesktopCursor   *self,
                           XrdWindow          *window,
                           graphene_point3d_t *intersection)
{
  XrdDesktopCursorData *data = xrd_desktop_cursor_get_data (self);

  if (data->texture_width == 0 || data->texture_height == 0)
    return;

  /* TODO: first we have to know the size of the cursor at the target  position
   * so we can calculate the hotspot.
   * Setting the size first flickers sometimes a bit.
   * */
  xrd_desktop_cursor_update_apparent_size (XRD_DESKTOP_CURSOR (self),
                                           intersection);

  /* Calculate the position of the cursor in the space of the window it is "on",
   * because the cursor is rotated in 3d to lie on the overlay's plane:
   * Move a point (center of the cursor) from the origin
   * 1) To the offset it has on the overlay it is on.
   *    This places the cursor's center at the target point on the overlay.
   * 2) half the width of the cursor right, half the height down.
   *    This places the upper left corner of the cursor at the target point.
   * 3) the offset of the hotspot up/left.
   *    This places exactly the hotspot at the target point. */

  graphene_point_t intersection_2d;
  xrd_window_get_intersection_2d (window, intersection, &intersection_2d);

  graphene_point3d_t offset_3d;
  graphene_point3d_init (&offset_3d, intersection_2d.x, intersection_2d.y, 0);

  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, &offset_3d);

  /* TODO: following assumes width==height. Are there non quadratic cursors? */

  graphene_point3d_t cursor_radius;
  graphene_point3d_init (&cursor_radius,
                         data->cached_width_meters / 2.f,
                         - data->cached_width_meters / 2.f, 0);
  graphene_matrix_translate (&transform, &cursor_radius);

  float cursor_hotspot_meter_x = - data->hotspot_x /
      ((float)data->texture_width) * data->cached_width_meters;
  float cursor_hotspot_meter_y = + data->hotspot_y /
      ((float)data->texture_height) * data->cached_width_meters;

  graphene_point3d_t cursor_hotspot;
  graphene_point3d_init (&cursor_hotspot,
                         cursor_hotspot_meter_x,
                         cursor_hotspot_meter_y, 0);
  graphene_matrix_translate (&transform, &cursor_hotspot);

  graphene_matrix_t overlay_transform;
  xrd_window_get_transformation (window, &overlay_transform);
  graphene_matrix_multiply(&transform, &overlay_transform, &transform);

  xrd_desktop_cursor_set_transformation (self, &transform);
}

void
xrd_desktop_cursor_update_apparent_size (XrdDesktopCursor   *self,
                                         graphene_point3d_t *cursor_point)
{
  XrdDesktopCursorData *data = xrd_desktop_cursor_get_data (self);

  if (!data->keep_apparent_size)
    return;

  graphene_matrix_t hmd_pose;
  gboolean has_pose = openvr_system_get_hmd_pose (&hmd_pose);
  if (!has_pose)
    {
      xrd_desktop_cursor_set_width_meters (XRD_DESKTOP_CURSOR (self),
                                           data->width_meters);
      return;
    }

  graphene_point3d_t hmd_point;
  graphene_matrix_get_translation_point3d (&hmd_pose, &hmd_point);

  float distance = graphene_point3d_distance (cursor_point, &hmd_point, NULL);

  /* divide distance by 3 so the width and the apparent width are the same at
   * a distance of 3 meters. This makes e.g. self->width = 0.3 look decent in
   * both cases at typical usage distances. */
  float new_width = data->width_meters
                    / XRD_TIP_APPARENT_SIZE_DISTANCE * distance;

  data->cached_width_meters = new_width;

  xrd_desktop_cursor_set_width_meters (XRD_DESKTOP_CURSOR (self), new_width);
}

