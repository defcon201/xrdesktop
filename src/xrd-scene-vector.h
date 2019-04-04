/*
 * XrDesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_VECTOR_H_
#define XRD_SCENE_VECTOR_H_

#include <glib-object.h>

#include <gulkan-vertex-buffer.h>

#include "openvr-context.h"
#include <gulkan-uniform-buffer.h>

#include "xrd-scene-object.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_VECTOR xrd_scene_vector_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneVector, xrd_scene_vector,
                      XRD, SCENE_VECTOR, XrdSceneObject)

struct _XrdSceneVector
{
  XrdSceneObject parent;
  GulkanVertexBuffer *vertex_buffer;
};

XrdSceneVector *xrd_scene_vector_new (void);

gboolean
xrd_scene_vector_initialize (XrdSceneVector        *self,
                             GulkanDevice          *device,
                             VkDescriptorSetLayout *layout);

void
xrd_scene_vector_update (XrdSceneVector  *self,
                         graphene_vec4_t *start,
                         graphene_vec4_t *end,
                         graphene_vec3_t *color);

void
xrd_scene_vector_render (XrdSceneVector    *self,
                         EVREye             eye,
                         VkPipeline         pipeline,
                         VkPipelineLayout   pipeline_layout,
                         VkCommandBuffer    cmd_buffer,
                         graphene_matrix_t *vp);

void
xrd_scene_vector_update_from_ray (XrdSceneVector  *self,
                                  graphene_ray_t  *ray,
                                  graphene_vec3_t *color);

void
xrd_scene_vector_update_from_plane (XrdSceneVector   *self,
                                    graphene_plane_t *plane,
                                    graphene_vec3_t  *color);

G_END_DECLS

#endif /* XRD_SCENE_VECTOR_H_ */
