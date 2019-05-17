/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_DESKTOP_CURSOR_H_
#define XRD_DESKTOP_CURSOR_H_

#include <glib-object.h>
#include <graphene.h>

#include "xrd-window.h"

G_BEGIN_DECLS

#define XRD_TYPE_DESKTOP_CURSOR xrd_desktop_cursor_get_type()
G_DECLARE_INTERFACE (XrdDesktopCursor, xrd_desktop_cursor,
                     XRD, DESKTOP_CURSOR, GObject)

typedef struct _XrdDesktopCursorData
{
  XrdDesktopCursor *cursor;

  gboolean keep_apparent_size;
  /* setting, either absolute size or the apparent size in 3 meter distance */
  float width_meters;

  /* cached values set by apparent size and used in hotspot calculation */
  float cached_width_meters;

  int hotspot_x;
  int hotspot_y;

  int texture_width;
  int texture_height;
} XrdDesktopCursorData;

struct _XrdDesktopCursorInterface
{
  GTypeInterface parent;

  void
  (*submit_texture) (XrdDesktopCursor *self,
                     GulkanClient     *uploader,
                     GulkanTexture    *texture,
                     int               hotspot_x,
                     int               hotspot_y);

  void
  (*show) (XrdDesktopCursor *self);

  void
  (*hide) (XrdDesktopCursor *self);

  void
  (*set_width_meters) (XrdDesktopCursor *self, float meters);

  XrdDesktopCursorData*
  (*get_data) (XrdDesktopCursor *self);

  void
  (*get_transformation) (XrdDesktopCursor  *self,
                         graphene_matrix_t *matrix);

  void
  (*set_transformation) (XrdDesktopCursor  *self,
                         graphene_matrix_t *matrix);
};

void
xrd_desktop_cursor_submit_texture (XrdDesktopCursor *self,
                                   GulkanClient     *uploader,
                                   GulkanTexture    *texture,
                                   int               hotspot_x,
                                   int               hotspot_y);

void
xrd_desktop_cursor_update (XrdDesktopCursor   *self,
                           XrdWindow          *window,
                           graphene_point3d_t *intersection);

void
xrd_desktop_cursor_show (XrdDesktopCursor *self);

void
xrd_desktop_cursor_hide (XrdDesktopCursor *self);

void
xrd_desktop_cursor_update_apparent_size (XrdDesktopCursor   *self,
                                         graphene_point3d_t *cursor_point);

void
xrd_desktop_cursor_set_width_meters (XrdDesktopCursor *self, float meters);

XrdDesktopCursorData*
xrd_desktop_cursor_get_data (XrdDesktopCursor *self);

void
xrd_desktop_cursor_get_transformation (XrdDesktopCursor  *self,
                                       graphene_matrix_t *matrix);

void
xrd_desktop_cursor_set_transformation (XrdDesktopCursor  *self,
                                       graphene_matrix_t *matrix);

void
xrd_desktop_cursor_init_settings (XrdDesktopCursor *self);

G_END_DECLS

#endif /* XRD_DESKTOP_CURSOR_H_ */
