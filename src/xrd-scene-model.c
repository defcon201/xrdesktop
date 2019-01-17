/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-model.h"
#include "openvr-context.h"

G_DEFINE_TYPE (XrdSceneModel, xrd_scene_model, G_TYPE_OBJECT)

static void
xrd_scene_model_finalize (GObject *gobject);

static void
xrd_scene_model_class_init (XrdSceneModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_model_finalize;
}

static void
xrd_scene_model_init (XrdSceneModel *self)
{
  self->sampler = VK_NULL_HANDLE;
  self->vbo = gulkan_vertex_buffer_new ();
}

XrdSceneModel *
xrd_scene_model_new (void)
{
  return (XrdSceneModel*) g_object_new (XRD_TYPE_SCENE_MODEL, 0);
}

static void
xrd_scene_model_finalize (GObject *gobject)
{
  XrdSceneModel *self = XRD_SCENE_MODEL (gobject);
  g_object_unref (self->vbo);
  g_object_unref (self->texture);

  if (self->sampler != VK_NULL_HANDLE)
    vkDestroySampler (self->vbo->device, self->sampler, NULL);
}

bool
_load_openvr_mesh (RenderModel_t **model,
                   const char     *name)
{
  EVRRenderModelError error;
  OpenVRContext *context = openvr_context_get_instance ();

  do
    {
      error = context->model->LoadRenderModel_Async ((char *) g_strdup (name),
                                                     model);
      /* Treat async loading synchronously.. */
      usleep (1000);
    }
  while (error == EVRRenderModelError_VRRenderModelError_Loading);

  if (error != EVRRenderModelError_VRRenderModelError_None)
    {
      g_printerr ("Unable to load model %s - %s\n", name,
                  context->model->GetRenderModelErrorNameFromEnum (error));
      return FALSE;
    }

  return TRUE;
}

bool
_load_openvr_texture (TextureID_t                id,
                      RenderModel_TextureMap_t **texture)
{
  EVRRenderModelError error;
  OpenVRContext *context = openvr_context_get_instance ();

  do
    {
      error = context->model->LoadTexture_Async (id, texture);
      /* Treat async loading synchronously.. */
      usleep (1000);
    }
  while (error == EVRRenderModelError_VRRenderModelError_Loading);

  if (error != EVRRenderModelError_VRRenderModelError_None)
    {
      g_printerr ("Unable to load OpenVR texture id: %d\n", id);
      return FALSE;
    }

  return TRUE;
}

gboolean
_load_mesh (XrdSceneModel *self,
            GulkanDevice             *device,
            RenderModel_t            *vr_model)
{
  if (!gulkan_vertex_buffer_alloc_data (
      self->vbo, device, vr_model->rVertexData,
      sizeof (RenderModel_Vertex_t) * vr_model->unVertexCount))
    return FALSE;

  if (!gulkan_vertex_buffer_alloc_index_data (
      self->vbo, device, vr_model->rIndexData,
      sizeof (uint16_t) * vr_model->unTriangleCount * 3))
    return FALSE;

  self->vbo->count = vr_model->unTriangleCount * 3;

  return TRUE;
}

gboolean
_load_texture (XrdSceneModel *self,
               GulkanDevice             *device,
               VkCommandBuffer           cmd_buffer,
               RenderModel_TextureMap_t *texture)
{
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data (
      texture->rubTextureMapData, GDK_COLORSPACE_RGB, TRUE, 8,
      texture->unWidth, texture->unHeight,
      4 * texture->unWidth, NULL, NULL);

  uint32_t num_mipmaps;
  self->texture =
    gulkan_texture_new_from_pixbuf_mipmapped (device, cmd_buffer,
                                              pixbuf, &num_mipmaps,
                                              VK_FORMAT_R8G8B8A8_UNORM);

  gulkan_texture_transfer_layout_mips (
      self->texture, device, cmd_buffer, num_mipmaps,
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
    .maxLod = (float) num_mipmaps,
  };

  vkCreateSampler (device->device, &sampler_info, NULL, &self->sampler);

  return TRUE;
}


gboolean
xrd_scene_model_load (XrdSceneModel *self,
                          GulkanDevice      *device,
                          VkCommandBuffer    cmd_buffer,
                          const char        *model_name)
{
  RenderModel_t *vr_model;
  if (!_load_openvr_mesh (&vr_model, model_name))
    return FALSE;

  OpenVRContext *context = openvr_context_get_instance ();

  RenderModel_TextureMap_t *vr_diffuse_texture;
  if (!_load_openvr_texture (vr_model->diffuseTextureId, &vr_diffuse_texture))
    {
      context->model->FreeRenderModel (vr_model);
      return FALSE;
    }

  if (!_load_mesh (self, device, vr_model))
    return FALSE;

  if (!_load_texture (self, device, cmd_buffer, vr_diffuse_texture))
    return FALSE;

  context->model->FreeRenderModel (vr_model);
  context->model->FreeTexture (vr_diffuse_texture);

  return TRUE;
}
