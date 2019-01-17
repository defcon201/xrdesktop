/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-window.h"
#include <gulkan-geometry.h>

G_DEFINE_TYPE (XrdSceneWindow, xrd_scene_window, XRD_TYPE_SCENE_OBJECT)

static void
xrd_scene_window_finalize (GObject *gobject);

static void
xrd_scene_window_class_init (XrdSceneWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_window_finalize;
}

static void
xrd_scene_window_init (XrdSceneWindow *self)
{
  self->vertex_buffer = gulkan_vertex_buffer_new ();
  self->sampler = VK_NULL_HANDLE;
}

XrdSceneWindow *
xrd_scene_window_new (void)
{
  return (XrdSceneWindow*) g_object_new (XRD_TYPE_SCENE_WINDOW, 0);
}

static void
xrd_scene_window_finalize (GObject *gobject)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (gobject);
  g_object_unref (self->texture);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);

  vkDestroySampler (obj->device->device, self->sampler, NULL);

  g_object_unref (self->vertex_buffer);

  G_OBJECT_CLASS (xrd_scene_window_parent_class)->finalize (gobject);
}

bool
xrd_scene_window_init_texture (XrdSceneWindow *self,
                               GulkanDevice   *device,
                               VkCommandBuffer cmd_buffer,
                               GdkPixbuf      *pixbuf)
{
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  obj->device = device;

  uint32_t mip_levels;

  self->texture = gulkan_texture_new_from_pixbuf_mipmapped (
      device, cmd_buffer, pixbuf,
      &mip_levels, VK_FORMAT_R8G8B8A8_UNORM);

  gulkan_texture_transfer_layout_mips (self->texture,
                                       device,
                                       cmd_buffer,
                                       mip_levels,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  VkSamplerCreateInfo sampler_info = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = 16.0f,
    .minLod = 0.0f,
    .maxLod = (float) mip_levels
  };

  vkCreateSampler (device->device, &sampler_info, NULL, &self->sampler);

  return true;
}

void _append_plane (GulkanVertexBuffer *vbo,
                    float x, float y, float z, float scale)
{
  graphene_matrix_t mat_scale;
  graphene_matrix_init_scale (&mat_scale, scale, scale, scale);

  graphene_point3d_t translation = { x, y, z };
  graphene_matrix_t mat_translation;
  graphene_matrix_init_translate (&mat_translation, &translation);

  graphene_matrix_t mat;
  graphene_matrix_multiply (&mat_scale, &mat_translation, &mat);

  gulkan_geometry_append_plane (vbo, &mat);
}

gboolean
xrd_scene_window_initialize (XrdSceneWindow        *self,
                             GulkanDevice          *device,
                             VkDescriptorSetLayout *layout)
{
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);

  _append_plane (self->vertex_buffer, 0, 0, 0, 1.0f);
  if (!gulkan_vertex_buffer_alloc_array (self->vertex_buffer, obj->device))
    return FALSE;

  if (!xrd_scene_object_initialize (obj, device, layout))
    return FALSE;

  xrd_scene_object_update_descriptors_texture (obj, self->sampler,
                                               self->texture->image_view);

  return TRUE;
}

void
xrd_scene_window_draw (XrdSceneWindow    *self,
                       EVREye             eye,
                       VkPipeline         pipeline,
                       VkPipelineLayout   pipeline_layout,
                       VkCommandBuffer    cmd_buffer,
                       graphene_matrix_t *vp)
{
  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  xrd_scene_object_update_mvp_matrix (obj, eye, vp);
  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw (self->vertex_buffer, cmd_buffer);
}
