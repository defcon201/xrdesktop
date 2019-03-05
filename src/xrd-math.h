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
                              float *inclination,
                              float *azimuth);

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
#endif /* XRD_MATH_H_ */
