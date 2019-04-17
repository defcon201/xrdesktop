/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-background.h"
#include "gulkan-geometry.h"
#include <gulkan-descriptor-set.h>

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
_append_lines (GulkanVertexBuffer *self,
               float               aspect_ratio,
               graphene_vec3_t    *color)
{
  float padding = 0.05f;

  float scale_x = aspect_ratio + padding;
  float scale_y = 1.0f + padding;

  float offset[2] = {
    -padding / 2.0f,
    -padding / 2.0f
  };

  graphene_vec4_t a, b, c, d;
  graphene_vec4_init (&a,       0 + offset[0],       0 + offset[1], 0, 1);
  graphene_vec4_init (&b, scale_x + offset[0],       0 + offset[1], 0, 1);
  graphene_vec4_init (&c, scale_x + offset[0], scale_y + offset[1], 0, 1);
  graphene_vec4_init (&d,       0 + offset[0], scale_y + offset[1], 0, 1);

  graphene_vec4_t points[8] = {
    a, b, b, c, c, d, d, a
  };

  for (uint32_t i = 0; i < G_N_ELEMENTS (points); i++)
    {
      gulkan_vertex_buffer_append_vec4 (self, &points[i]);
      gulkan_vertex_buffer_append_vec3 (self, color);
    }

  self->count += G_N_ELEMENTS (points);
}

gboolean
xrd_scene_background_initialize (XrdSceneBackground    *self,
                                 GulkanDevice          *device,
                                 VkDescriptorSetLayout *layout)
{
  gulkan_vertex_buffer_reset (self->vertex_buffer);

  graphene_vec3_t color;
  graphene_vec3_init (&color, .8f, .8f, .8f);

  _append_lines (self->vertex_buffer, 1.0f, &color);

  if (!gulkan_vertex_buffer_alloc_empty (self->vertex_buffer, device,
                                         k_unMaxTrackedDeviceCount))
    return FALSE;

  gulkan_vertex_buffer_map_array (self->vertex_buffer);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  if (!xrd_scene_object_initialize (obj, device, layout))
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
  if (self->vertex_buffer->buffer == VK_NULL_HANDLE)
    return;

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  xrd_scene_object_update_mvp_matrix (obj, eye, vp);
  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw (self->vertex_buffer, cmd_buffer);
}
