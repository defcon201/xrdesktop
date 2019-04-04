/*
 * XR Desktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_MATH_H_
#define XRD_MATH_H_

#include <graphene.h>
#include <glib.h>

#define PI   ((float) 3.1415926535)
#define DEG_TO_RAD(x) ( (x) * 2.0 * PI / 360.0 )
#define RAD_TO_DEG(x) ( (x) * 360.0 / ( 2.0 * PI ) )

bool
xrd_math_matrix_equals (graphene_matrix_t *a,
                        graphene_matrix_t *b);

float
xrd_math_point_matrix_distance (graphene_point3d_t *intersection_point,
                                graphene_matrix_t  *pose);


void
xrd_math_get_frustum_angles (float *left, float *right,
                             float *top, float *bottom);

void
xrd_math_get_rotation_angles (graphene_vec3_t *direction,
                              float *azimuth,
                              float *inclination);

void
xrd_math_matrix_set_translation_point (graphene_matrix_t  *matrix,
                                       graphene_point3d_t *point);

void
xrd_math_matrix_set_translation_vec (graphene_matrix_t  *matrix,
                                     graphene_vec3_t *vec);
void
xrd_math_matrix_get_translation_vec (graphene_matrix_t *matrix,
                                     graphene_vec3_t   *vec);
void
xrd_math_matrix_get_translation_point (graphene_matrix_t  *matrix,
                                       graphene_point3d_t *point);

void
graphene_point_scale (const graphene_point_t *p,
                      float                   factor,
                      graphene_point_t       *res);

/** intersect_lines_2d:
 * 2 lines are given by 2 consecutive (x,y) points each.
 *
 * Returns 1 if the lines intersect, otherwise 0. In addition, if the lines
 * intersect, the intersection point is stored in the floats i_x and i_y.
 * Based on an algorithm in Andre LeMothe's
 * "Tricks of the Windows Game Programming Gurus".
 * Implementation from https://stackoverflow.com/a/1968345 */
gboolean
xrd_math_intersect_lines_2d (float p0_x, float p0_y, float p1_x, float p1_y,
                             float p2_x, float p2_y, float p3_x, float p3_y,
                             float *i_x, float *i_y);

/** xrd_math_clamp_towards_center:
 * Given x and y limits, clamp x and y values to those limits in a way that
 * lets both go towards zero until an x or y limit is reached.
 */
gboolean
xrd_math_clamp_towards_zero_2d (float x_min, float x_max,
                                float y_min, float y_max,
                                float x, float y,
                                float *x_clamped, float *y_clamped);

/** xrd_math_sphere_to_3d_coords:
 * Converts azimuth, inclination and distance to a 3D point on the surface of
 * the sphere induced by the origin (0,0,0) and the `distance`.
 * The referemce direction for azimuth and inclination is the -z axis.
 */
void
xrd_math_sphere_to_3d_coords (float azimuth,
                              float inclination,
                              float distance,
                              graphene_point3d_t *point);

#endif /* XRD_MATH_H_ */
