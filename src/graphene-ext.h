/*
 * Graphene Extensions
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GRAPHENE_EXT_H_
#define XRD_GRAPHENE_EXT_H_

#include <graphene.h>

void
graphene_quaternion_to_float (const graphene_quaternion_t *q,
                              float                       *dest);

void
graphene_quaternion_print (const graphene_quaternion_t *q);

void
graphene_matrix_get_translation (const graphene_matrix_t *m,
                                 graphene_vec3_t         *res);

void
graphene_matrix_get_scale (const graphene_matrix_t *m,
                           graphene_vec3_t         *res);

void
graphene_matrix_get_rotation_matrix (const graphene_matrix_t *m,
                                     graphene_matrix_t *res);

void
graphene_matrix_get_rotation_quaternion (const graphene_matrix_t *m,
                                         graphene_quaternion_t   *res);
void
graphene_matrix_get_rotation_angles (const graphene_matrix_t *m,
                                     float                   *deg_x,
                                     float                   *deg_y,
                                     float                   *deg_z);

void
graphene_point_scale (const graphene_point_t *p,
                      float                   factor,
                      graphene_point_t       *res);

void
graphene_ray_get_origin_vec4 (const graphene_ray_t *r,
                              float                 w,
                              graphene_vec4_t      *res);

void
graphene_ray_get_origin_vec3 (const graphene_ray_t *r,
                              graphene_vec3_t      *res);

void
graphene_ray_get_direction_vec4 (const graphene_ray_t *r,
                                 float                 w,
                                 graphene_vec4_t      *res);

void
graphene_vec4_print (const graphene_vec4_t *v);

void
graphene_vec3_print (const graphene_vec3_t *v);

#endif /* XRD_GRAPHENE_QUATERNION_H_ */
