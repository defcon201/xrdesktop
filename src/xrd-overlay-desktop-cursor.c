/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-desktop-cursor.h"

#include "openvr-math.h"

G_DEFINE_TYPE (XrdOverlayDesktopCursor, xrd_overlay_desktop_cursor, OPENVR_TYPE_OVERLAY)

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
  (void) self;
}

XrdOverlayDesktopCursor *
xrd_overlay_desktop_cursor_new (OpenVROverlayUploader *uploader)
{
  XrdOverlayDesktopCursor *self =
      (XrdOverlayDesktopCursor*) g_object_new (XRD_TYPE_OVERLAY_DESKTOP_CURSOR, 0);
  self->uploader = g_object_ref (uploader);
  self->pixbuf = NULL;
  self->texture = NULL;

  /* TODO: settings */
  self->cursor_width_meter = 0.125;

  openvr_overlay_create_width (OPENVR_OVERLAY (self),
                               "org.xrdesktop.cursor", "XR Desktop Cursor",
                               self->cursor_width_meter);
  if (!openvr_overlay_is_valid (OPENVR_OVERLAY (self)))
    {
      g_printerr ("Cursor overlay unavailable.\n");
      return NULL;
    }

  /* pointer ray is MAX, pointer tip is MAX - 1, so cursor is MAX - 2 */
  openvr_overlay_set_sort_order (OPENVR_OVERLAY (self), UINT32_MAX - 2);

  openvr_overlay_show (OPENVR_OVERLAY (self));
  
  return self;
}

void
xrd_overlay_desktop_cursor_upload_pixbuf (XrdOverlayDesktopCursor *self,
                                          GdkPixbuf *pixbuf,
                                          int hotspot_x,
                                          int hotspot_y)
{
  GulkanClient *client = GULKAN_CLIENT (self->uploader);

  if (!self->pixbuf ||
      gdk_pixbuf_get_width (pixbuf) != gdk_pixbuf_get_width (self->pixbuf) ||
      gdk_pixbuf_get_height (pixbuf) != gdk_pixbuf_get_height (self->pixbuf))
    {
      if (self->texture)
        g_object_unref (self->texture);
      self->texture = gulkan_texture_new_from_pixbuf (client->device, pixbuf,
                                                      VK_FORMAT_R8G8B8A8_UNORM);

      if (self->pixbuf)
        g_object_unref (self->pixbuf);
      self->pixbuf = pixbuf;
    }

  gulkan_client_upload_pixbuf (client, self->texture, pixbuf);
  
  openvr_overlay_uploader_submit_frame (self->uploader, OPENVR_OVERLAY (self),
                                        self->texture);

  self->hotspot_x = hotspot_x;
  self->hotspot_y = hotspot_y;
}

void
xrd_overlay_desktop_cursor_update (XrdOverlayDesktopCursor *self,
                                   XrdOverlayWindow        *window,
                                   graphene_point3d_t      *intersection)
{
  if (!self->pixbuf)
    return;

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
  openvr_overlay_get_2d_offset (window->overlay, intersection, &offset_2d);
  
  graphene_point3d_t offset_3d;
  graphene_point3d_init (&offset_3d, offset_2d.x, offset_2d.y, 0);

  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, &offset_3d);

  /* TODO: following assumes width==height. Are there non quadratic cursors? */

  graphene_point3d_t cursor_radius;
  graphene_point3d_init (&cursor_radius,
                         self->cursor_width_meter / 2.,
                         - self->cursor_width_meter / 2., 0);
  graphene_matrix_translate (&transform, &cursor_radius);

  float cursor_hotspot_meter_x = - self->hotspot_x /
      ((float)gdk_pixbuf_get_width (self->pixbuf)) * self->cursor_width_meter;
  float cursor_hotspot_meter_y = + self->hotspot_y /
      ((float)gdk_pixbuf_get_height (self->pixbuf)) * self->cursor_width_meter;

  graphene_point3d_t cursor_hotspot;
  graphene_point3d_init (&cursor_hotspot, cursor_hotspot_meter_x,
                         cursor_hotspot_meter_y, 0);
  graphene_matrix_translate (&transform, &cursor_hotspot);

  graphene_matrix_t overlay_transform;
  openvr_overlay_get_transform_absolute (window->overlay, &overlay_transform);
  graphene_matrix_multiply(&transform, &overlay_transform, &transform);

  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), &transform);
}

void
xrd_overlay_desktop_cursor_show (XrdOverlayDesktopCursor *self)
{
  openvr_overlay_show (OPENVR_OVERLAY (self));
}

void
xrd_overlay_desktop_cursor_hide (XrdOverlayDesktopCursor *self)
{
  openvr_overlay_hide (OPENVR_OVERLAY (self));
}

static void
xrd_overlay_desktop_cursor_finalize (GObject *gobject)
{
  XrdOverlayDesktopCursor *self = XRD_OVERLAY_DESKTOP_CURSOR (gobject);
  if (self->pixbuf)
    g_object_unref (self->pixbuf);
  if (self->texture)
    g_object_unref (self->texture);
  g_object_unref (self->uploader);
  (void) self;
}
