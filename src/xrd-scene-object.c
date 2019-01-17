/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-object.h"
#include <gulkan-descriptor-set.h>

G_DEFINE_TYPE (XrdSceneObject, xrd_scene_object, G_TYPE_OBJECT)

static void
xrd_scene_object_finalize (GObject *gobject);

static void
xrd_scene_object_class_init (XrdSceneObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_object_finalize;
}

static void
xrd_scene_object_init (XrdSceneObject *self)
{
  self->descriptor_pool = VK_NULL_HANDLE;
  graphene_matrix_init_identity (&self->model_matrix);
  self->scale = 1.0f;
  for (uint32_t eye = 0; eye < 2; eye++)
    self->uniform_buffers[eye] = gulkan_uniform_buffer_new ();
}

XrdSceneObject *
xrd_scene_object_new (void)
{
  return (XrdSceneObject*) g_object_new (XRD_TYPE_SCENE_OBJECT, 0);
}

static void
xrd_scene_object_finalize (GObject *gobject)
{
  XrdSceneObject *self = XRD_SCENE_OBJECT (gobject);
  vkDestroyDescriptorPool (self->device->device, self->descriptor_pool, NULL);

  for (uint32_t eye = 0; eye < 2; eye++)
    g_object_unref (self->uniform_buffers[eye]);
}

void
_update_model_matrix (XrdSceneObject *self)
{
  graphene_matrix_init_scale (&self->model_matrix,
                              self->scale, self->scale, self->scale);
  graphene_matrix_translate (&self->model_matrix, &self->position);
}

void
xrd_scene_object_update_mvp_matrix (XrdSceneObject    *self,
                                    EVREye             eye,
                                    graphene_matrix_t *vp)
{
 graphene_matrix_t mvp;
  graphene_matrix_multiply (&self->model_matrix, vp, &mvp);

  /* Update matrix in uniform buffer */
  graphene_matrix_to_float (&mvp, self->uniform_buffers[eye]->data);
}

void
xrd_scene_object_bind (XrdSceneObject    *self,
                       EVREye             eye,
                       VkCommandBuffer    cmd_buffer,
                       VkPipelineLayout   pipeline_layout)
{
  vkCmdBindDescriptorSets (
    cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1,
   &self->descriptor_sets[eye], 0, NULL);
}

void
xrd_scene_object_set_scale (XrdSceneObject *self, float scale)
{
  self->scale = scale;
  _update_model_matrix (self);
}

void
xrd_scene_object_set_position (XrdSceneObject     *self,
                               graphene_point3d_t *position)
{
  graphene_point3d_init_from_point (&self->position, position);
  _update_model_matrix (self);
}

gboolean
xrd_scene_object_initialize (XrdSceneObject        *self,
                             GulkanDevice          *device,
                             VkDescriptorSetLayout *layout)
{
  self->device = device;

  /* Create uniform buffer to hold a matrix per eye */
  for (uint32_t eye = 0; eye < 2; eye++)
    gulkan_uniform_buffer_allocate_and_map (self->uniform_buffers[eye],
                                            self->device, sizeof (float) * 16);

  uint32_t set_count = 2;

  VkDescriptorPoolSize pool_sizes[] = {
    {
      .descriptorCount = set_count,
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    },
    {
      .descriptorCount = set_count,
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    }
  };


  if (!GULKAN_INIT_DECRIPTOR_POOL (self->device, pool_sizes,
                                   set_count, &self->descriptor_pool))
     return FALSE;

  for (uint32_t eye = 0; eye < set_count; eye++)
    if (!gulkan_allocate_descritpor_set (self->device, self->descriptor_pool,
                                         layout, 1,
                                         &self->descriptor_sets[eye]))
      return FALSE;

  return TRUE;
}

void
xrd_scene_object_update_descriptors_texture (XrdSceneObject *self,
                                             VkSampler       sampler,
                                             VkImageView     image_view)
{
  for (uint32_t eye = 0; eye < 2; eye++)
    {
      VkWriteDescriptorSet *write_descriptor_sets = (VkWriteDescriptorSet []) {
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[eye],
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &(VkDescriptorBufferInfo) {
            .buffer = self->uniform_buffers[eye]->buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
          },
          .pTexelBufferView = NULL
        },
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[eye],
          .dstBinding = 1,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &(VkDescriptorImageInfo) {
            .sampler = sampler,
            .imageView = image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
          },
          .pBufferInfo = NULL,
          .pTexelBufferView = NULL
        }
      };

      vkUpdateDescriptorSets (self->device->device,
                              2, write_descriptor_sets, 0, NULL);
    }
}

void
xrd_scene_object_update_descriptors (XrdSceneObject *self)
{
  for (uint32_t eye = 0; eye < 2; eye++)
    {
      VkWriteDescriptorSet *write_descriptor_sets = (VkWriteDescriptorSet []) {
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[eye],
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &(VkDescriptorBufferInfo) {
            .buffer = self->uniform_buffers[eye]->buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
          },
          .pTexelBufferView = NULL
        }
      };

      vkUpdateDescriptorSets (self->device->device,
                              1, write_descriptor_sets, 0, NULL);
    }
}

