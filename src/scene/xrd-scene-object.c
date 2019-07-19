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

typedef struct {
  float mvp[16];
  float mv[16];
  float m[16];
  bool receive_light;
} XrdSceneObjectTransformation;

typedef struct _XrdSceneObjectPrivate
{
  GObject parent;

  XrdSceneObjectTransformation transformation[2];
  GulkanUniformBuffer *uniform_buffers[2];

  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_sets[2];

  graphene_matrix_t model_matrix;

  graphene_point3d_t position;
  float scale;
  graphene_quaternion_t orientation;

  gboolean visible;

  gboolean initialized;
} XrdSceneObjectPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XrdSceneObject, xrd_scene_object, G_TYPE_OBJECT)

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
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);

  priv->descriptor_pool = VK_NULL_HANDLE;
  graphene_matrix_init_identity (&priv->model_matrix);
  priv->scale = 1.0f;
  for (uint32_t eye = 0; eye < 2; eye++)
    priv->uniform_buffers[eye] = gulkan_uniform_buffer_new ();
  priv->visible = TRUE;
  priv->initialized = FALSE;
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
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  if (!priv->initialized)
    return;

  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  VkDevice device = gulkan_client_get_device_handle (GULKAN_CLIENT (renderer));
  vkDestroyDescriptorPool (device, priv->descriptor_pool, NULL);

  for (uint32_t eye = 0; eye < 2; eye++)
    g_object_unref (priv->uniform_buffers[eye]);
}

static void
_update_model_matrix (XrdSceneObject *self)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  graphene_matrix_init_scale (&priv->model_matrix,
                              priv->scale, priv->scale, priv->scale);
  graphene_matrix_rotate_quaternion (&priv->model_matrix, &priv->orientation);
  graphene_matrix_translate (&priv->model_matrix, &priv->position);
}

void
xrd_scene_object_update_mvp_matrix (XrdSceneObject    *self,
                                    EVREye             eye,
                                    graphene_matrix_t *vp)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);

  graphene_matrix_t mvp;
  graphene_matrix_multiply (&priv->model_matrix, vp, &mvp);
  graphene_matrix_to_float (&mvp, priv->transformation[eye].mvp);

  priv->transformation[eye].receive_light = false;

  /* Update matrix in uniform buffer */
  gulkan_uniform_buffer_update_struct (priv->uniform_buffers[eye],
                                       (gpointer) &priv->transformation[eye]);
}

void
xrd_scene_object_update_transformation_buffer (XrdSceneObject    *self,
                                               EVREye             eye,
                                               graphene_matrix_t *view,
                                               graphene_matrix_t *projection)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);

  graphene_matrix_t vp;
  graphene_matrix_multiply (view, projection, &vp);

  graphene_matrix_to_float (&priv->model_matrix, priv->transformation[eye].m);

  graphene_matrix_t mv;
  graphene_matrix_multiply (&priv->model_matrix, view, &mv);
  graphene_matrix_to_float (&mv, priv->transformation[eye].mv);

  graphene_matrix_t mvp;
  graphene_matrix_multiply (&priv->model_matrix, &vp, &mvp);
  graphene_matrix_to_float (&mvp, priv->transformation[eye].mvp);

  priv->transformation[eye].receive_light = true;

  gulkan_uniform_buffer_update_struct (priv->uniform_buffers[eye],
                                       (gpointer) &priv->transformation[eye]);
}

void
xrd_scene_object_get_model_matrix (XrdSceneObject    *self,
                                   graphene_matrix_t *model_matrix)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  graphene_matrix_init_from_matrix (model_matrix, &priv->model_matrix);
}

void
xrd_scene_object_bind (XrdSceneObject    *self,
                       EVREye             eye,
                       VkCommandBuffer    cmd_buffer,
                       VkPipelineLayout   pipeline_layout)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  vkCmdBindDescriptorSets (
    cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1,
   &priv->descriptor_sets[eye], 0, NULL);
}

void
xrd_scene_object_set_scale (XrdSceneObject *self, float scale)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  priv->scale = scale;
  _update_model_matrix (self);
}

void
xrd_scene_object_set_position (XrdSceneObject     *self,
                               graphene_point3d_t *position)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  graphene_point3d_init_from_point (&priv->position, position);
  _update_model_matrix (self);
}

void
xrd_scene_object_get_position (XrdSceneObject     *self,
                               graphene_point3d_t *position)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  graphene_point3d_init_from_point (position, &priv->position);
}

void
xrd_scene_object_set_rotation_euler (XrdSceneObject   *self,
                                     graphene_euler_t *euler)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  graphene_quaternion_init_from_euler (&priv->orientation, euler);
  _update_model_matrix (self);
}

gboolean
xrd_scene_object_initialize (XrdSceneObject        *self,
                             VkDescriptorSetLayout *layout)
{
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  GulkanDevice *device = gulkan_client_get_device (GULKAN_CLIENT (renderer));
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);

  /* Create uniform buffer to hold a matrix per eye */
  for (uint32_t eye = 0; eye < 2; eye++)
    if (!gulkan_uniform_buffer_allocate_and_map (priv->uniform_buffers[eye],
                                                 device, sizeof (XrdSceneObjectTransformation)))
      return FALSE;

  uint32_t set_count = 2;

  VkDescriptorPoolSize pool_sizes[] = {
    {
      .descriptorCount = set_count,
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    },
    {
      .descriptorCount = set_count,
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    },
    {
      .descriptorCount = set_count,
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    },
    {
      .descriptorCount = set_count,
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    }
  };


  if (!GULKAN_INIT_DECRIPTOR_POOL (device, pool_sizes,
                                   set_count, &priv->descriptor_pool))
     return FALSE;

  for (uint32_t eye = 0; eye < set_count; eye++)
    if (!gulkan_allocate_descritpor_set (device, priv->descriptor_pool,
                                         layout, 1,
                                         &priv->descriptor_sets[eye]))
      return FALSE;

  priv->initialized = TRUE;

  return TRUE;
}

void
xrd_scene_object_update_descriptors_texture (XrdSceneObject *self,
                                             VkSampler       sampler,
                                             VkImageView     image_view)
{
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  VkDevice device = gulkan_client_get_device_handle (GULKAN_CLIENT (renderer));
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);

  for (uint32_t eye = 0; eye < 2; eye++)
    {
      VkWriteDescriptorSet *write_descriptor_sets = (VkWriteDescriptorSet []) {
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = priv->descriptor_sets[eye],
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &(VkDescriptorBufferInfo) {
            .buffer = gulkan_uniform_buffer_get_handle (
                        priv->uniform_buffers[eye]),
            .offset = 0,
            .range = VK_WHOLE_SIZE
          },
          .pTexelBufferView = NULL
        },
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = priv->descriptor_sets[eye],
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
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);

  for (uint32_t eye = 0; eye < 2; eye++)
    {
      VkWriteDescriptorSet *write_descriptor_sets = (VkWriteDescriptorSet []) {
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = priv->descriptor_sets[eye],
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &(VkDescriptorBufferInfo) {
            .buffer = gulkan_uniform_buffer_get_handle (
                        priv->uniform_buffers[eye]),
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
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  graphene_ext_matrix_get_rotation_quaternion (mat, &priv->orientation);
  graphene_ext_matrix_get_translation_point3d (mat, &priv->position);

  // graphene_vec3_t scale;
  // graphene_matrix_get_scale (mat, &scale);

  _update_model_matrix (self);
}

/*
 * Set transformation without matrix decomposition and ability to rebuild
 * This will include scale as well.
 */

void
xrd_scene_object_set_transformation_direct (XrdSceneObject    *self,
                                            graphene_matrix_t *mat)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  graphene_matrix_init_from_matrix (&priv->model_matrix, mat);
}

graphene_matrix_t
xrd_scene_object_get_transformation (XrdSceneObject *self)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  return priv->model_matrix;
}

graphene_matrix_t
xrd_scene_object_get_transformation_no_scale (XrdSceneObject *self)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);

  graphene_matrix_t mat;
  graphene_matrix_init_identity (&mat);
  graphene_matrix_rotate_quaternion (&mat, &priv->orientation);
  graphene_matrix_translate (&mat, &priv->position);
  return mat;
}

gboolean
xrd_scene_object_is_visible (XrdSceneObject *self)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  return priv->visible;
}

void
xrd_scene_object_show (XrdSceneObject *self)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  priv->visible = true;
}

void
xrd_scene_object_hide (XrdSceneObject *self)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  priv->visible = false;
}

VkBuffer
xrd_scene_object_get_transformation_buffer (XrdSceneObject *self, uint32_t eye)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  return gulkan_uniform_buffer_get_handle (priv->uniform_buffers[eye]);
}

VkDescriptorSet
xrd_scene_object_get_descriptor_set (XrdSceneObject *self, uint32_t eye)
{
  XrdSceneObjectPrivate *priv = xrd_scene_object_get_instance_private (self);
  return priv->descriptor_sets[eye];
}
