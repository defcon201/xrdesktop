/*
 * XrDesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-vector.h"
#include "gulkan-geometry.h"
#include <gulkan-descriptor-set.h>

G_DEFINE_TYPE (XrdSceneVector, xrd_scene_vector, XRD_TYPE_SCENE_OBJECT)

static void
xrd_scene_vector_finalize (GObject *gobject);

static void
xrd_scene_vector_class_init (XrdSceneVectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_vector_finalize;
}

static void
xrd_scene_vector_init (XrdSceneVector *self)
{
  self->vertex_buffer = gulkan_vertex_buffer_new ();
}

XrdSceneVector *
xrd_scene_vector_new (void)
{
  return (XrdSceneVector*) g_object_new (XRD_TYPE_SCENE_VECTOR, 0);
}

static void
xrd_scene_vector_finalize (GObject *gobject)
{
  XrdSceneVector *self = XRD_SCENE_VECTOR (gobject);
  g_object_unref (self->vertex_buffer);
  G_OBJECT_CLASS (xrd_scene_vector_parent_class)->finalize (gobject);
}

void
_append_vector (GulkanVertexBuffer *buffer,
                graphene_vec4_t    *start,
                graphene_vec4_t    *end,
                graphene_vec3_t    *color)
{
  gulkan_vertex_buffer_append_vec4 (buffer, start);
  gulkan_vertex_buffer_append_vec3 (buffer, color);

  gulkan_vertex_buffer_append_vec4 (buffer, end);
  gulkan_vertex_buffer_append_vec3 (buffer, color);

  buffer->count += 2;
}

void
xrd_scene_vector_append_plane (GulkanVertexBuffer *buffer,
                               graphene_plane_t   *plane,
                               graphene_vec3_t    *color)
{
  graphene_vec3_t normal;
  graphene_plane_get_normal (plane, &normal);

  float constant = graphene_plane_get_constant (plane);

  graphene_vec3_scale (&normal, constant, &normal);

  graphene_vec4_t start;
  graphene_vec4_init (&start, 0, 0, 0, 1);

  gulkan_vertex_buffer_append_vec4 (buffer, &start);
  gulkan_vertex_buffer_append_vec3 (buffer, color);

  graphene_vec4_t end;
  graphene_vec4_init_from_vec3 (&end, &normal, 1);
  graphene_vec4_negate (&end, &end);

  gulkan_vertex_buffer_append_vec4 (buffer, &end);
  gulkan_vertex_buffer_append_vec3 (buffer, color);

  buffer->count += 2;
}

gboolean
xrd_scene_vector_initialize (XrdSceneVector        *self,
                             GulkanDevice          *device,
                             VkDescriptorSetLayout *layout)
{
  gulkan_vertex_buffer_reset (self->vertex_buffer);

  graphene_vec4_t start;
  graphene_vec4_init (&start, 0, 0, 0, 1);

  graphene_vec4_t end;
  graphene_vec4_init (&end, 0, 0, 1, 1);

  graphene_vec3_t color;
  graphene_vec3_init (&color, .8f, .2f, .2f);

  _append_vector (self->vertex_buffer, &start, &end, &color);

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
xrd_scene_vector_update (XrdSceneVector  *self,
                         graphene_vec4_t *start,
                         graphene_vec4_t *end,
                         graphene_vec3_t *color)
{
  gulkan_vertex_buffer_reset (self->vertex_buffer);
  _append_vector (self->vertex_buffer, start, end, color);
  gulkan_vertex_buffer_map_array (self->vertex_buffer);
}

void
xrd_scene_vector_update_from_ray (XrdSceneVector  *self,
                                  graphene_ray_t  *ray,
                                  graphene_vec3_t *color)
{
  graphene_vec4_t start, end;

  graphene_point3d_t origin;
  graphene_ray_get_origin (ray, &origin);

  graphene_vec3_t origin_vec3;
  graphene_point3d_to_vec3 (&origin, &origin_vec3);

  graphene_vec4_init_from_vec3 (&start, &origin_vec3, 1);

  graphene_vec3_t direction;
  graphene_ray_get_direction (ray, &direction);

  graphene_vec4_t direction_vec4;
  graphene_vec4_init_from_vec3 (&direction_vec4, &direction, 1);

  graphene_vec4_add (&start, &direction_vec4, &end);

  gulkan_vertex_buffer_reset (self->vertex_buffer);
  _append_vector (self->vertex_buffer, &start, &end, color);
  gulkan_vertex_buffer_map_array (self->vertex_buffer);
}

void
xrd_scene_vector_update_from_plane (XrdSceneVector   *self,
                                    graphene_plane_t *plane,
                                    graphene_vec3_t  *color)
{
  gulkan_vertex_buffer_reset (self->vertex_buffer);
  xrd_scene_vector_append_plane (self->vertex_buffer, plane, color);
  gulkan_vertex_buffer_map_array (self->vertex_buffer);
}

void
xrd_scene_vector_render (XrdSceneVector    *self,
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
