/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-selection.h"
#include "gulkan-geometry.h"
#include <gulkan-descriptor-set.h>

G_DEFINE_TYPE (XrdSceneSelection, xrd_scene_selection, XRD_TYPE_SCENE_OBJECT)

static void
xrd_scene_selection_finalize (GObject *gobject);

static void
xrd_scene_selection_class_init (XrdSceneSelectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_selection_finalize;
}

static void
xrd_scene_selection_init (XrdSceneSelection *self)
{
  self->vertex_buffer = gulkan_vertex_buffer_new ();
}

XrdSceneSelection *
xrd_scene_selection_new (void)
{
  return (XrdSceneSelection*) g_object_new (XRD_TYPE_SCENE_SELECTION, 0);
}

static void
xrd_scene_selection_finalize (GObject *gobject)
{
  XrdSceneSelection *self = XRD_SCENE_SELECTION (gobject);
  g_object_unref (self->vertex_buffer);
  G_OBJECT_CLASS (xrd_scene_selection_parent_class)->finalize (gobject);
}

void
_append_lines_quad (GulkanVertexBuffer *self,
                    graphene_vec3_t    *color)
{
  float padding = 0.05f;

  float scale = 1.0f + padding;

  float offset[2] = {
    -padding / 2.0f,
    -padding / 2.0f
  };

  graphene_vec4_t a, b, c, d;
  graphene_vec4_init (&a,     0 + offset[0],     0 + offset[1], 0, 1);
  graphene_vec4_init (&b, scale + offset[0],     0 + offset[1], 0, 1);
  graphene_vec4_init (&c, scale + offset[0], scale + offset[1], 0, 1);
  graphene_vec4_init (&d,     0 + offset[0], scale + offset[1], 0, 1);

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
xrd_scene_selection_initialize (XrdSceneSelection     *self,
                                GulkanDevice          *device,
                                VkDescriptorSetLayout *layout)
{
  gulkan_vertex_buffer_reset (self->vertex_buffer);

  graphene_vec4_t start;
  graphene_vec4_init (&start, 0, 0, -0.02f, 1);

  graphene_matrix_t identity;
  graphene_matrix_init_identity (&identity);

  graphene_vec3_t color;
  graphene_vec3_init (&color, .8f, .2f, .2f);

  _append_lines_quad (self->vertex_buffer, &color);

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
xrd_scene_selection_render (XrdSceneSelection *self,
                            EVREye             eye,
                            VkPipeline         pipeline,
                            VkPipelineLayout   pipeline_layout,
                            VkCommandBuffer    cmd_buffer,
                            graphene_matrix_t *vp)
{
  if (self->vertex_buffer->buffer == VK_NULL_HANDLE)
    return;

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  xrd_scene_object_update_mvp_matrix (obj, eye, vp);
  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw (self->vertex_buffer, cmd_buffer);
}
