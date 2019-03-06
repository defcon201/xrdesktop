/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-window.h"

G_DEFINE_TYPE (XrdWindow, xrd_window, G_TYPE_OBJECT)

static void
xrd_window_finalize (GObject *gobject);

static void
xrd_window_class_init (XrdWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = xrd_window_finalize;
}

gboolean
xrd_window_set_transformation_matrix (XrdWindow *self, graphene_matrix_t *mat)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_set_transformation_matrix == NULL)
      return FALSE;
  return klass->xrd_window_set_transformation_matrix (self, mat);
}

gboolean
xrd_window_get_transformation_matrix (XrdWindow *self, graphene_matrix_t *mat)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_get_transformation_matrix == NULL)
      return FALSE;
  return klass->xrd_window_get_transformation_matrix (self, mat);
}

void
xrd_window_submit_texture (XrdWindow *self,
                           GulkanClient *client,
                           GulkanTexture *texture)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_submit_texture == NULL)
      return;
  return klass->xrd_window_submit_texture (self, client, texture);
}

float
xrd_window_pixel_to_xr_scale (XrdWindow *self, int pixel)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_pixel_to_xr_scale == NULL)
      return NAN;
  return klass->xrd_window_pixel_to_xr_scale (self, pixel);
}

gboolean
xrd_window_get_xr_width (XrdWindow *self, float *meters)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_get_xr_width == NULL)
      return FALSE;
  return klass->xrd_window_get_xr_width (self, meters);
}


gboolean
xrd_window_get_xr_height (XrdWindow *self, float *meters)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_get_xr_height == NULL)
      return FALSE;
  return klass->xrd_window_get_xr_height (self, meters);
}

gboolean
xrd_window_get_scaling_factor (XrdWindow *self, float *factor)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_get_scaling_factor == NULL)
      return FALSE;
  return klass->xrd_window_get_scaling_factor (self, factor);
}

gboolean
xrd_window_set_scaling_factor (XrdWindow *self, float factor)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_set_scaling_factor == NULL)
      return FALSE;
  return klass->xrd_window_set_scaling_factor (self, factor);
}

void
xrd_window_poll_event (XrdWindow *self)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_poll_event == NULL)
      return;
  return klass->xrd_window_poll_event (self);
}

gboolean
xrd_window_intersects (XrdWindow   *self,
                       graphene_matrix_t  *pointer_transformation_matrix,
                       graphene_point3d_t *intersection_point)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_intersects == NULL)
      return FALSE;
  return klass->xrd_window_intersects (self,
                                       pointer_transformation_matrix,
                                       intersection_point);
}

gboolean
xrd_window_intersection_to_window_coords (XrdWindow   *self,
                                          graphene_point3d_t *intersection_point,
                                          XrdPixelSize       *size_pixels,
                                          graphene_point_t   *window_coords)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_intersection_to_window_coords == NULL)
      return FALSE;
  return klass->xrd_window_intersection_to_window_coords (self,
                                                          intersection_point,
                                                          size_pixels,
                                                          window_coords);
}

gboolean
xrd_window_intersection_to_offset_center (XrdWindow *self,
                                          graphene_point3d_t *intersection_point,
                                          graphene_point_t   *offset_center)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_intersection_to_offset_center == NULL)
      return FALSE;
  return klass->xrd_window_intersection_to_offset_center (self,
                                                          intersection_point,
                                                          offset_center);
}


void
xrd_window_emit_grab_start (XrdWindow *self,
                            XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_grab_start == NULL)
      return;
  return klass->xrd_window_emit_grab_start (self, event);
}


void
xrd_window_emit_grab (XrdWindow *self,
                      XrdGrabEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_grab == NULL)
      return;
  return klass->xrd_window_emit_grab (self, event);
}

void
xrd_window_emit_release (XrdWindow *self,
                         XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_release == NULL)
      return;
  return klass->xrd_window_emit_release (self, event);
}

void
xrd_window_emit_hover_end (XrdWindow *self,
                           XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_hover_end == NULL)
      return;
  return klass->xrd_window_emit_hover_end (self, event);
}


void
xrd_window_emit_hover (XrdWindow    *self,
                       XrdHoverEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_hover == NULL)
      return;
  return klass->xrd_window_emit_hover (self, event);
}

void
xrd_window_emit_hover_start (XrdWindow *self,
                             XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_hover_start == NULL)
      return;
  return klass->xrd_window_emit_hover_start (self, event);
}

void
xrd_window_add_child (XrdWindow *self,
                      XrdWindow *child,
                      graphene_point_t *offset_center)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_add_child == NULL)
      return;
  return klass->xrd_window_add_child (self, child, offset_center);
}

/* TODO: this is a stopgap solution for so children can init a window.
 * Pretty sure there's a more glib like solution. */
void
xrd_window_internal_init (XrdWindow *self)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_internal_init == NULL)
      return;
  return klass->xrd_window_internal_init (self);
}

void
xrd_window_init (XrdWindow *self)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  (void) klass;
}

static void
xrd_window_finalize (GObject *gobject)
{
  XrdWindow *self = XRD_WINDOW (gobject);
  (void) self;
  G_OBJECT_CLASS (xrd_window_parent_class)->finalize (gobject);
}
