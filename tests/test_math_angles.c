/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>

#include "xrd-math.h"

#define FLOAT_DELTA 0.0001f

static gboolean
feq (float a, float b)
{
  float diff = fabsf (a - b);
  return diff < FLOAT_DELTA;
}

static void
_test_xrd_math_get_rotation_angles ()
{
  graphene_vec3_t minus_z;
  graphene_vec3_init (&minus_z, 0, 0, -1);

  graphene_vec3_t z;
  graphene_vec3_init (&z, 0, 0, 1);

  graphene_vec3_t up;
  graphene_vec3_init (&up, 0, 1, 0);

  graphene_vec3_t left;
  graphene_vec3_init (&left, -1, 0, 0);

  graphene_vec3_t front_right;
  graphene_vec3_init (&front_right, 1, 0, -1);

  float azimuth;
  float inclination;

  xrd_math_get_rotation_angles (&minus_z, &azimuth, &inclination);
  g_debug ("%f %f\n", (double)azimuth, (double)inclination);
  g_assert (feq (azimuth, 0.) && feq (inclination, 0.));

  xrd_math_get_rotation_angles (&left, &azimuth, &inclination);
  g_debug ("%f %f\n", (double)azimuth, (double)inclination);
  g_assert (feq (azimuth, -90.) && feq (inclination, 0.));

  /* all azimuth angles are "valid" */
  xrd_math_get_rotation_angles (&up, &azimuth, &inclination);
  g_debug ("%f %f\n", (double)azimuth, (double)inclination);
  g_assert  (feq (inclination, 90.));

  xrd_math_get_rotation_angles (&front_right, &azimuth, &inclination);
  g_debug ("%f %f\n", (double)azimuth, (double)inclination);
  g_assert (feq (azimuth, 45.));

  graphene_vec3_t front_right_up;
  graphene_vec3_init (&front_right_up, 0, 0, -1);
  graphene_matrix_t rot_up_45;
  graphene_matrix_init_rotate (&rot_up_45, 45, graphene_vec3_x_axis ());
  graphene_matrix_t rot_right_45;
  graphene_matrix_init_rotate (&rot_right_45, - 45, graphene_vec3_y_axis ());

  graphene_matrix_transform_vec3 (&rot_up_45, &front_right_up, &front_right_up);
  graphene_matrix_transform_vec3 (&rot_right_45,
                                  &front_right_up, &front_right_up);

  xrd_math_get_rotation_angles (&front_right_up, &azimuth, &inclination);
  g_debug ("%f %f\n", (double)azimuth, (double)inclination);
  g_assert (feq (azimuth, 45.) && feq (inclination, 45.));
}

static void
_test_xrd_math_sphere_to_3d_coords ()
{
  graphene_point3d_t point;
  xrd_math_sphere_to_3d_coords (90., 0., 1.0, &point);
  g_debug ("%f %f %f\n", (double)point.x, (double)point.y, (double)point.z);
  g_assert (feq (point.x, 1.) && feq (point.y, 0.) && feq (point.z, 0));

  xrd_math_sphere_to_3d_coords (0., 0., 1.0, &point);
  g_debug ("%f %f %f\n", (double)point.x, (double)point.y, (double)point.z);
  g_assert (feq (point.x, 0.) && feq (point.y, 0.) && feq (point.z, -1));
}

int
main ()
{
  _test_xrd_math_get_rotation_angles ();
  _test_xrd_math_sphere_to_3d_coords ();

  return 0;
}
