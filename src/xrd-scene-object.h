/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_SCENE_OBJECT_H_
#define XRD_GLIB_SCENE_OBJECT_H_

#include <glib-object.h>
#include <gulkan-uniform-buffer.h>
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

  GulkanDevice *device;
};

XrdSceneObject *xrd_scene_object_new (void);

void
xrd_scene_object_set_scale (XrdSceneObject *self, float scale);

void
xrd_scene_object_set_position (XrdSceneObject     *self,
                               graphene_point3d_t *position);

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
                             GulkanDevice          *device,
                             VkDescriptorSetLayout *layout);

void
xrd_scene_object_update_descriptors_texture (XrdSceneObject *self,
                                             VkSampler       sampler,
                                             VkImageView     image_view);

void
xrd_scene_object_update_descriptors (XrdSceneObject *self);

G_END_DECLS

#endif /* XRD_GLIB_SCENE_OBJECT_H_ */
