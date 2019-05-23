/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-pointer.h"

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
  return iface->move (self, transform);
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
  return iface->set_length (self, length);
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
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  return iface->get_intersection (self, window, distance, res);
}
