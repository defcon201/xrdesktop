/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-desktop-cursor.h"

#include "openvr-math.h"
#include "xrd-settings.h"
#include "xrd-math.h"
#include "graphene-ext.h"
#include "xrd-desktop-cursor.h"
#include "xrd-pointer-tip.h"

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
_update_width_meters (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayDesktopCursor *self = user_data;
  self->data.width_meters = g_settings_get_double (settings, key);
  xrd_desktop_cursor_set_width_meters (XRD_DESKTOP_CURSOR (self),
                                       self->data.width_meters);
}

static void
_update_keep_apparent_size (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayDesktopCursor *self = user_data;
  self->data.keep_apparent_size = g_settings_get_boolean (settings, key);
  if (self->data.keep_apparent_size)
    {
      graphene_matrix_t cursor_pose;
      openvr_overlay_get_transform_absolute (OPENVR_OVERLAY(self), &cursor_pose);

      graphene_vec3_t cursor_point_vec;
      graphene_matrix_get_translation_vec3 (&cursor_pose, &cursor_point_vec);
      graphene_point3d_t cursor_point;
      xrd_desktop_cursor_update_apparent_size (XRD_DESKTOP_CURSOR (self),
                                               &cursor_point);
    }
  else
    xrd_desktop_cursor_set_width_meters (XRD_DESKTOP_CURSOR (self),
                                         self->data.width_meters);
}

static void
_init_settings (XrdOverlayDesktopCursor *self)
{
  xrd_settings_connect_and_apply (G_CALLBACK (_update_width_meters),
                                  "desktop-cursor-width-meters", self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_keep_apparent_size),
                                  "pointer-tip-keep-apparent-size", self);
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

  _init_settings (self);

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

  self->data.texture_width = texture->width;
  self->data.texture_height = texture->height;
}

static void
_update (XrdDesktopCursor   *cursor,
         XrdWindow          *window,
         graphene_point3d_t *intersection)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (cursor);

  if (self->data.texture_width == 0 || self->data.texture_height == 0)
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

  graphene_point_t offset_2d;
  xrd_window_intersection_to_2d_offset_meter (window, intersection, &offset_2d);

  graphene_point3d_t offset_3d;
  graphene_point3d_init (&offset_3d, offset_2d.x, offset_2d.y, 0);

  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, &offset_3d);

  /* TODO: following assumes width==height. Are there non quadratic cursors? */

  graphene_point3d_t cursor_radius;
  graphene_point3d_init (&cursor_radius,
                         self->data.cached_width_meters / 2.,
                         - self->data.cached_width_meters / 2., 0);
  graphene_matrix_translate (&transform, &cursor_radius);

  float cursor_hotspot_meter_x = - self->data.hotspot_x /
      ((float)self->data.texture_width) * self->data.cached_width_meters;
  float cursor_hotspot_meter_y = + self->data.hotspot_y /
      ((float)self->data.texture_height) * self->data.cached_width_meters;

  graphene_point3d_t cursor_hotspot;
  graphene_point3d_init (&cursor_hotspot, cursor_hotspot_meter_x,
                         cursor_hotspot_meter_y, 0);
  graphene_matrix_translate (&transform, &cursor_hotspot);

  graphene_matrix_t overlay_transform;
  xrd_window_get_transformation (window, &overlay_transform);
  graphene_matrix_multiply(&transform, &overlay_transform, &transform);

  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), &transform);
}

static void
_update_apparent_size (XrdDesktopCursor   *cursor,
                       graphene_point3d_t *cursor_point)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (cursor);

  if (!self->data.keep_apparent_size)
    return;

  graphene_matrix_t hmd_pose;
  gboolean has_pose = openvr_system_get_hmd_pose (&hmd_pose);
  if (!has_pose)
    {
      xrd_desktop_cursor_set_width_meters (XRD_DESKTOP_CURSOR (self),
                                           self->data.width_meters);
      return;
    }

  graphene_point3d_t hmd_point;
  graphene_matrix_get_translation_point3d (&hmd_pose, &hmd_point);

  float distance = graphene_point3d_distance (cursor_point, &hmd_point, NULL);

  /* divide distance by 3 so the width and the apparent width are the same at
   * a distance of 3 meters. This makes e.g. self->width = 0.3 look decent in
   * both cases at typical usage distances. */
  float new_width = self->data.width_meters
                    / XRD_TIP_APPARENT_SIZE_DISTANCE * distance;

  xrd_desktop_cursor_set_width_meters (XRD_DESKTOP_CURSOR (self), new_width);
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
xrd_overlay_desktop_cursor_interface_init (XrdDesktopCursorInterface *iface)
{
  iface->submit_texture = _submit_texture;
  iface->update = _update;
  iface->show = _show;
  iface->hide = _hide;
  iface->update_apparent_size = _update_apparent_size;
  iface->set_width_meters = _set_width_meters;
  iface->get_data = _get_data;
}
