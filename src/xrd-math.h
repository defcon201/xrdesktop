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

bool
xrd_math_matrix_equals (graphene_matrix_t *a,
                        graphene_matrix_t *b);

float
xrd_math_point_matrix_distance (graphene_point3d_t *intersection_point,
                                graphene_matrix_t  *pose);

#endif /* XRD_MATH_H_ */
