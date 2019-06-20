/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-pointer.h"
#include "xrd-window.h"

#include "graphene-ext.h"

G_DEFINE_INTERFACE (XrdPointer, xrd_pointer, G_TYPE_OBJECT)

static void
xrd_pointer_default_init (XrdPointerInterface *iface)
{
  (void) iface;
}

void
xrd_pointer_move (XrdPointer        *self,
                  graphene_matrix_t *transform)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  iface->move (self, transform);
}

void
xrd_pointer_set_length (XrdPointer *self,
                        float       length)
{
  XrdPointerData *data = xrd_pointer_get_data (self);
  if (length == data->length)
    return;

  data->length = length;

  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  iface->set_length (self, length);
}

float
xrd_pointer_get_default_length (XrdPointer *self)
{
  XrdPointerData *data = xrd_pointer_get_data (self);
  return data->default_length;
}

void
xrd_pointer_reset_length (XrdPointer *self)
{
  XrdPointerData *data = xrd_pointer_get_data (self);
  xrd_pointer_set_length (self, data->default_length);
}

XrdPointerData*
xrd_pointer_get_data (XrdPointer *self)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  return iface->get_data (self);
}

void
xrd_pointer_init (XrdPointer *self)
{
  XrdPointerData *data = xrd_pointer_get_data (self);
  data->start_offset = -0.02f;
  data->default_length = 5.0f;
  data->length = data->default_length;
  data->visible = TRUE;
}

void
xrd_pointer_set_transformation (XrdPointer        *self,
                                graphene_matrix_t *matrix)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  iface->set_transformation (self, matrix);
}

void
xrd_pointer_get_transformation (XrdPointer        *self,
                                graphene_matrix_t *matrix)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  iface->get_transformation (self, matrix);
}

void
xrd_pointer_get_ray (XrdPointer     *self,
                     graphene_ray_t *res)
{
  XrdPointerData *data = xrd_pointer_get_data (self);

  graphene_matrix_t mat;
  xrd_pointer_get_transformation (self, &mat);

  graphene_vec4_t start;
  graphene_vec4_init (&start, 0, 0, data->start_offset, 1);
  graphene_matrix_transform_vec4 (&mat, &start, &start);

  graphene_vec4_t end;
  graphene_vec4_init (&end, 0, 0, -data->length, 1);
  graphene_matrix_transform_vec4 (&mat, &end, &end);

  graphene_vec4_t direction_vec4;
  graphene_vec4_subtract (&end, &start, &direction_vec4);

  graphene_point3d_t origin;
  graphene_vec3_t direction;

  graphene_vec3_t vec3_start;
  graphene_vec4_get_xyz (&start, &vec3_start);
  graphene_point3d_init_from_vec3 (&origin, &vec3_start);

  graphene_vec4_get_xyz (&direction_vec4, &direction);

  graphene_ray_init (res, &origin, &direction);
}

gboolean
xrd_pointer_get_intersection (XrdPointer      *self,
                              XrdWindow       *window,
                              float           *distance,
                              graphene_vec3_t *res)
{
  graphene_ray_t ray;
  xrd_pointer_get_ray (self, &ray);

  graphene_plane_t plane;
  xrd_window_get_plane (window, &plane);

  *distance = graphene_ray_get_distance_to_plane (&ray, &plane);
  if (*distance == INFINITY)
    return FALSE;

  graphene_ray_get_direction (&ray, res);
  graphene_vec3_scale (res, *distance, res);

  graphene_vec3_t origin;
  graphene_ray_get_origin_vec3 (&ray, &origin);
  graphene_vec3_add (&origin, res, res);

  graphene_matrix_t inverse;

  graphene_matrix_t model_matrix;
  xrd_window_get_transformation (window, &model_matrix);

  graphene_matrix_inverse (&model_matrix, &inverse);

  graphene_vec4_t intersection_vec4;
  graphene_vec4_init_from_vec3 (&intersection_vec4, res, 1.0f);

  graphene_vec4_t intersection_origin;
  graphene_matrix_transform_vec4 (&inverse,
                                  &intersection_vec4,
                                  &intersection_origin);

  float f[4];
  graphene_vec4_to_float (&intersection_origin, f);

  /* Test if we are in [0-aspect_ratio, 0-1] plane coordinates */
  float aspect_ratio = xrd_window_get_aspect_ratio (window);

  if (f[0] >= -aspect_ratio / 2.0f && f[0] <= aspect_ratio / 2.0f
      && f[1] >= -0.5f && f[1] <= 0.5f)
    return TRUE;

  return FALSE;
}

void
xrd_pointer_show (XrdPointer *self)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  iface->show (self);

  XrdPointerData *data = xrd_pointer_get_data (self);
  data->visible = TRUE;
}

void
xrd_pointer_hide (XrdPointer *self)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  iface->hide (self);

  XrdPointerData *data = xrd_pointer_get_data (self);
  data->visible = FALSE;
}

gboolean
xrd_pointer_is_visible (XrdPointer *self)
{
  XrdPointerData *data = xrd_pointer_get_data (self);
  return data->visible;
}

void
xrd_pointer_set_selected_window (XrdPointer *self,
                                 XrdWindow  *window)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  iface->set_selected_window (self, window);
}
