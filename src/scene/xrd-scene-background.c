/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-background.h"
#include <gulkan.h>
#include "graphene-ext.h"

G_DEFINE_TYPE (XrdSceneBackground, xrd_scene_background, XRD_TYPE_SCENE_OBJECT)

static void
xrd_scene_background_finalize (GObject *gobject);

static void
xrd_scene_background_class_init (XrdSceneBackgroundClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_background_finalize;
}

static void
xrd_scene_background_init (XrdSceneBackground *self)
{
  self->vertex_buffer = gulkan_vertex_buffer_new ();
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  obj->visible = FALSE;
}

XrdSceneBackground *
xrd_scene_background_new (void)
{
  return (XrdSceneBackground*) g_object_new (XRD_TYPE_SCENE_BACKGROUND, 0);
}

static void
xrd_scene_background_finalize (GObject *gobject)
{
  XrdSceneBackground *self = XRD_SCENE_BACKGROUND (gobject);
  g_object_unref (self->vertex_buffer);
  G_OBJECT_CLASS (xrd_scene_background_parent_class)->finalize (gobject);
}

void
_append_star (GulkanVertexBuffer *self,
              float               radius,
              float               y,
              uint32_t            sections,
              graphene_vec3_t    *color)
{
  graphene_vec4_t *points = g_malloc (sizeof(graphene_vec4_t) * sections);

  graphene_vec4_init (&points[0],  radius, y, 0, 1);
  graphene_vec4_init (&points[1], -radius, y, 0, 1);

  graphene_matrix_t rotation;
  graphene_matrix_init_identity (&rotation);
  graphene_matrix_rotate_y (&rotation, 360.0f / (float) sections);

  for (uint32_t i = 0; i < sections / 2 - 1; i++)
    {
      uint32_t j = i * 2;
      graphene_matrix_transform_vec4 (&rotation, &points[j],     &points[j + 2]);
      graphene_matrix_transform_vec4 (&rotation, &points[j + 1], &points[j + 3]);
    }

  for (uint32_t i = 0; i < sections; i++)
    gulkan_vertex_buffer_append_with_color (self, &points[i], color);

  g_free(points);
}

void
_append_circle (GulkanVertexBuffer *self,
                float               radius,
                float               y,
                uint32_t            edges,
                graphene_vec3_t    *color)
{
  graphene_vec4_t *points = g_malloc (sizeof(graphene_vec4_t) * edges * 2);

  graphene_vec4_init (&points[0], radius, y, 0, 1);

  graphene_matrix_t rotation;
  graphene_matrix_init_identity (&rotation);
  graphene_matrix_rotate_y (&rotation, 360.0f / (float) edges);

  for (uint32_t i = 0; i < edges; i++)
    {
      uint32_t j = i * 2;
      if (i != 0)
        graphene_vec4_init_from_vec4 (&points[j], &points[j - 1]);
      graphene_matrix_transform_vec4 (&rotation, &points[j], &points[j + 1]);
    }

  for (uint32_t i = 0; i < edges * 2; i++)
    gulkan_vertex_buffer_append_with_color (self, &points[i], color);

  g_free(points);
}

void
_append_floor (GulkanVertexBuffer *self,
               uint32_t            radius,
               float               y,
               graphene_vec3_t    *color)
{
  _append_star (self, (float) radius, y, 8, color);

  for (uint32_t i = 1; i <= radius; i++)
    _append_circle (self, (float) i, y, 128, color);
}

gboolean
xrd_scene_background_initialize (XrdSceneBackground    *self,
                                 GulkanDevice          *device,
                                 VkDescriptorSetLayout *layout)
{
  gulkan_vertex_buffer_reset (self->vertex_buffer);

  graphene_vec3_t color;
  graphene_vec3_init (&color, .6f, .6f, .6f);

  _append_floor (self->vertex_buffer, 20, 0.0f, &color);
  _append_floor (self->vertex_buffer, 20, 4.0f, &color);

  if (!gulkan_vertex_buffer_alloc_empty (self->vertex_buffer, device,
                                         k_unMaxTrackedDeviceCount))
    return FALSE;

  gulkan_vertex_buffer_map_array (self->vertex_buffer);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  if (!xrd_scene_object_initialize (obj, layout))
    return FALSE;

  xrd_scene_object_update_descriptors (obj);

  return TRUE;
}

void
xrd_scene_background_render (XrdSceneBackground *self,
                             EVREye              eye,
                             VkPipeline          pipeline,
                             VkPipelineLayout    pipeline_layout,
                             VkCommandBuffer     cmd_buffer,
                             graphene_matrix_t  *vp)
{
  if (!gulkan_vertex_buffer_is_initialized (self->vertex_buffer))
    return;

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  xrd_scene_object_update_mvp_matrix (obj, eye, vp);
  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw (self->vertex_buffer, cmd_buffer);
}
