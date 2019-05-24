/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_SCENE_OBJECT_H_
#define XRD_GLIB_SCENE_OBJECT_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>
#include <gulkan.h>
#include <graphene.h>
#include <openvr_capi.h>

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_OBJECT xrd_scene_object_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneObject, xrd_scene_object,
                      XRD, SCENE_OBJECT, GObject)

struct _XrdSceneObject
{
  GObject parent;

  GulkanUniformBuffer *uniform_buffers[2];

  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_sets[2];

  graphene_matrix_t model_matrix;

  graphene_point3d_t position;
  float scale;
  graphene_quaternion_t orientation;

  gboolean visible;

  gboolean initialized;
};

XrdSceneObject *xrd_scene_object_new (void);

void
xrd_scene_object_set_scale (XrdSceneObject *self, float scale);

void
xrd_scene_object_set_position (XrdSceneObject     *self,
                               graphene_point3d_t *position);

void
xrd_scene_object_set_rotation_euler (XrdSceneObject   *self,
                                     graphene_euler_t *euler);

void
xrd_scene_object_update_mvp_matrix (XrdSceneObject    *self,
                                    EVREye             eye,
                                    graphene_matrix_t *vp);

void
xrd_scene_object_bind (XrdSceneObject    *self,
                       EVREye             eye,
                       VkCommandBuffer    cmd_buffer,
                       VkPipelineLayout   pipeline_layout);

gboolean
xrd_scene_object_initialize (XrdSceneObject        *self,
                             VkDescriptorSetLayout *layout);

void
xrd_scene_object_update_descriptors_texture (XrdSceneObject *self,
                                             VkSampler       sampler,
                                             VkImageView     image_view);

void
xrd_scene_object_update_descriptors (XrdSceneObject *self);

void
xrd_scene_object_set_transformation (XrdSceneObject    *self,
                                     graphene_matrix_t *mat);

graphene_matrix_t
xrd_scene_object_get_transformation (XrdSceneObject *self);

bool
xrd_scene_object_is_visible (XrdSceneObject *self);
void
xrd_scene_object_show (XrdSceneObject *self);

void
xrd_scene_object_hide (XrdSceneObject *self);

G_END_DECLS

#endif /* XRD_GLIB_SCENE_OBJECT_H_ */
