/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-object.h"
#include <gulkan.h>

#include "xrd-scene-renderer.h"
#include "graphene-ext.h"

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
  self->visible = TRUE;
  self->initialized = FALSE;
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
  if (!self->initialized)
    return;

  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  VkDevice device = gulkan_client_get_device_handle (GULKAN_CLIENT (renderer));
  vkDestroyDescriptorPool (device, self->descriptor_pool, NULL);

  for (uint32_t eye = 0; eye < 2; eye++)
    g_object_unref (self->uniform_buffers[eye]);
}

void
_update_model_matrix (XrdSceneObject *self)
{
  graphene_matrix_init_scale (&self->model_matrix,
                              self->scale, self->scale, self->scale);
  graphene_matrix_rotate_quaternion (&self->model_matrix, &self->orientation);
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
  gulkan_uniform_buffer_update_matrix (self->uniform_buffers[eye], &mvp);
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

void
xrd_scene_object_set_rotation_euler (XrdSceneObject   *self,
                                     graphene_euler_t *euler)
{
  graphene_quaternion_init_from_euler (&self->orientation, euler);
  _update_model_matrix (self);
}

gboolean
xrd_scene_object_initialize (XrdSceneObject        *self,
                             VkDescriptorSetLayout *layout)
{
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  GulkanDevice *device = gulkan_client_get_device (GULKAN_CLIENT (renderer));

  /* Create uniform buffer to hold a matrix per eye */
  for (uint32_t eye = 0; eye < 2; eye++)
    gulkan_uniform_buffer_allocate_and_map (self->uniform_buffers[eye],
                                            device, sizeof (float) * 16);

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


  if (!GULKAN_INIT_DECRIPTOR_POOL (device, pool_sizes,
                                   set_count, &self->descriptor_pool))
     return FALSE;

  for (uint32_t eye = 0; eye < set_count; eye++)
    if (!gulkan_allocate_descritpor_set (device, self->descriptor_pool,
                                         layout, 1,
                                         &self->descriptor_sets[eye]))
      return FALSE;

  self->initialized = TRUE;

  return TRUE;
}

void
xrd_scene_object_update_descriptors_texture (XrdSceneObject *self,
                                             VkSampler       sampler,
                                             VkImageView     image_view)
{
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  VkDevice device = gulkan_client_get_device_handle (GULKAN_CLIENT (renderer));

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
            .buffer = gulkan_uniform_buffer_get_handle (
                        self->uniform_buffers[eye]),
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

      vkUpdateDescriptorSets (device, 2, write_descriptor_sets, 0, NULL);
    }
}

void
xrd_scene_object_update_descriptors (XrdSceneObject *self)
{
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  VkDevice device = gulkan_client_get_device_handle (GULKAN_CLIENT (renderer));

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
            .buffer = gulkan_uniform_buffer_get_handle (
                        self->uniform_buffers[eye]),
            .offset = 0,
            .range = VK_WHOLE_SIZE
          },
          .pTexelBufferView = NULL
        }
      };

      vkUpdateDescriptorSets (device, 1, write_descriptor_sets, 0, NULL);
    }
}

void
xrd_scene_object_set_transformation (XrdSceneObject    *self,
                                     graphene_matrix_t *mat)
{
  graphene_matrix_get_rotation_quaternion (mat, &self->orientation);
  graphene_matrix_get_translation_point3d (mat, &self->position);

  // graphene_vec3_t scale;
  // graphene_matrix_get_scale (mat, &scale);

  _update_model_matrix (self);
}

graphene_matrix_t
xrd_scene_object_get_transformation (XrdSceneObject *self)
{
  return self->model_matrix;
}

bool
xrd_scene_object_is_visible (XrdSceneObject *self)
{
  return self->visible;
}

void
xrd_scene_object_show (XrdSceneObject *self)
{
  self->visible = true;
}

void
xrd_scene_object_hide (XrdSceneObject *self)
{
  self->visible = false;
}