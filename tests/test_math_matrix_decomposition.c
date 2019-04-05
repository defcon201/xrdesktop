/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include "xrd-math.h"

void
graphene_quaternion_to_float (const graphene_quaternion_t *q,
                              float                       *dest)
{
  graphene_vec4_t v;
  graphene_quaternion_to_vec4 (q, &v);
  graphene_vec4_to_float (&v, dest);
}

void
graphene_quaternion_print (const graphene_quaternion_t *q)
{
  float f[4];
  graphene_quaternion_to_float (q, f);
  g_print ("| %f %f %f %f |\n", f[0], f[1], f[2], f[3]);
}

void
graphene_matrix_get_translation (const graphene_matrix_t *m,
                                 graphene_vec3_t         *res)
{
  float f[16];
  graphene_matrix_to_float (m, f);
  graphene_vec3_init (res, f[12], f[13], f[14]);
}

void
graphene_matrix_get_scale (const graphene_matrix_t *m,
                           graphene_vec3_t         *res)
{
  float f[16];
  graphene_matrix_to_float (m, f);

  graphene_vec3_t sx, sy, sz;
  graphene_vec3_init (&sx, f[0], f[1], f[2]);
  graphene_vec3_init (&sy, f[4], f[5], f[6]);
  graphene_vec3_init (&sz, f[8], f[9], f[10]);

  graphene_vec3_init (res,
                      graphene_vec3_length (&sx),
                      graphene_vec3_length (&sy),
                      graphene_vec3_length (&sz));
}

void
graphene_matrix_get_rotation_matrix (const graphene_matrix_t *m,
                                     graphene_matrix_t *res)
{
  float f[16];
  graphene_matrix_to_float (m, f);

  graphene_vec3_t s_vec;
  graphene_matrix_get_scale (m, &s_vec);
  float s[3];
  graphene_vec3_to_float (&s_vec, s);

  float r[16] = {
    f[0] / s[0], f[1] / s[0], f[2]  / s[0], 0,
    f[4] / s[1], f[5] / s[1], f[6]  / s[1], 0,
    f[8] / s[2], f[9] / s[2], f[10] / s[2], 0,
    0,           0,           0,            1
  };
  graphene_matrix_init_from_float (res, r);
}

void
graphene_matrix_get_rotation_quaternion (const graphene_matrix_t *m,
                                         graphene_quaternion_t   *res)
{
  graphene_matrix_t rot_m;
  graphene_matrix_get_rotation_matrix (m, &rot_m);
  graphene_quaternion_init_from_matrix (res, &rot_m);
}

void
graphene_matrix_get_rotation_angles (const graphene_matrix_t *m,
                                     float                   *deg_x,
                                     float                   *deg_y,
                                     float                   *deg_z)
{
  graphene_quaternion_t q;
  graphene_matrix_get_rotation_quaternion (m, &q);
  graphene_quaternion_to_angles (&q, deg_x, deg_y, deg_z);
}

void
_print_translation (graphene_matrix_t *m)
{
  graphene_vec3_t t;
  graphene_matrix_get_translation (m, &t);

  float f[3];
  graphene_vec3_to_float (&t, f);

  g_print ("Translation: [%f %f %f]\n", f[0], f[1], f[2]);
}

void
_print_scale (graphene_matrix_t *m)
{
  graphene_vec3_t s;
  graphene_matrix_get_scale (m, &s);

  float f[3];
  graphene_vec3_to_float (&s, f);

  g_print ("Scale: [%f %f %f]\n", f[0], f[1], f[2]);
}

void
_print_rotation (graphene_matrix_t *m)
{
  float x, y, z;
  graphene_matrix_get_rotation_angles (m, &x, &y, &z);

  g_print ("Angles: [%f %f %f]\n", x, y, z);
}


int
main ()
{
  graphene_matrix_t mat;
  graphene_matrix_init_identity (&mat);

  g_print ("Identity:\n");
  graphene_matrix_print (&mat);
  _print_translation (&mat);
  _print_scale (&mat);
  _print_rotation (&mat);

  graphene_matrix_init_scale (&mat, 1, 2, 3);
  g_print ("Scale:\n");
  graphene_matrix_print (&mat);
  _print_translation (&mat);
  _print_scale (&mat);
  _print_rotation (&mat);

  graphene_point3d_t point = {
    .x = 1, .y = 2, .z = 3
  };
  graphene_matrix_init_translate (&mat, &point);
  g_print ("Translation:\n");
  graphene_matrix_print (&mat);
  _print_translation (&mat);
  _print_scale (&mat);
  _print_rotation (&mat);

  graphene_quaternion_t orientation;

  graphene_quaternion_init_from_angles (&orientation, 1, 2, 3);
  graphene_matrix_init_identity (&mat);
  graphene_matrix_rotate_quaternion (&mat, &orientation);
  g_print ("Rotation:\n");

  graphene_matrix_print (&mat);
  _print_translation (&mat);
  _print_scale (&mat);
  _print_rotation (&mat);

  g_print ("Rotation quat: ");
  graphene_quaternion_print (&orientation);
  graphene_quaternion_t q;
  graphene_matrix_get_rotation_quaternion (&mat, &q);
  g_print ("Result quat: ");
  graphene_quaternion_print (&q);

  return 0;
}
