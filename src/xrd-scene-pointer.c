/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-pointer.h"

#include <gulkan.h>

#include "graphene-ext.h"
#include "xrd-pointer.h"

static void
xrd_scene_pointer_interface_init (XrdPointerInterface *iface);

struct _XrdScenePointer
{
  XrdSceneObject parent;
  GulkanVertexBuffer *vertex_buffer;
  float start_offset;
  float length;
  float default_length;

  XrdSceneSelection *selection;
};

G_DEFINE_TYPE_WITH_CODE (XrdScenePointer, xrd_scene_pointer, XRD_TYPE_SCENE_OBJECT,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_POINTER,
                                                xrd_scene_pointer_interface_init))

static void
xrd_scene_pointer_finalize (GObject *gobject);

static void
xrd_scene_pointer_class_init (XrdScenePointerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_pointer_finalize;
}

static void
xrd_scene_pointer_init (XrdScenePointer *self)
{
  self->vertex_buffer = gulkan_vertex_buffer_new ();

  self->start_offset = -0.02f;
  self->default_length = 40.0f;
  self->length = self->default_length;

  self->selection = xrd_scene_selection_new ();
}

XrdScenePointer *
xrd_scene_pointer_new (void)
{
  return (XrdScenePointer*) g_object_new (XRD_TYPE_SCENE_POINTER, 0);
}

static void
xrd_scene_pointer_finalize (GObject *gobject)
{
  XrdScenePointer *self = XRD_SCENE_POINTER (gobject);
  g_object_unref (self->vertex_buffer);
  g_object_unref (self->selection);
  G_OBJECT_CLASS (xrd_scene_pointer_parent_class)->finalize (gobject);
}


gboolean
xrd_scene_pointer_initialize (XrdScenePointer       *self,
                              GulkanDevice          *device,
                              VkDescriptorSetLayout *layout)
{
  gulkan_vertex_buffer_reset (self->vertex_buffer);

  graphene_vec4_t start;
  graphene_vec4_init (&start, 0, 0, self->start_offset, 1);

  graphene_matrix_t identity;
  graphene_matrix_init_identity (&identity);

  gulkan_geometry_append_ray (self->vertex_buffer,
                              &start, self->length, &identity);
  if (!gulkan_vertex_buffer_alloc_empty (self->vertex_buffer, device,
                                         k_unMaxTrackedDeviceCount))
    return FALSE;

  gulkan_vertex_buffer_map_array (self->vertex_buffer);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  if (!xrd_scene_object_initialize (obj, layout))
    return FALSE;

  xrd_scene_object_update_descriptors (obj);

  xrd_scene_selection_initialize (self->selection, device, layout);

  return TRUE;
}

void
xrd_scene_pointer_render (XrdScenePointer   *self,
                          EVREye             eye,
                          VkPipeline         pipeline,
                          VkPipeline         selection_pipeline,
                          VkPipelineLayout   pipeline_layout,
                          VkCommandBuffer    cmd_buffer,
                          graphene_matrix_t *vp)
{
  if (!gulkan_vertex_buffer_is_initialized (self->vertex_buffer))
    return;

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  xrd_scene_object_update_mvp_matrix (obj, eye, vp);

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  xrd_scene_selection_render (self->selection, eye,
                              selection_pipeline, pipeline_layout,
                              cmd_buffer, vp);


  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw (self->vertex_buffer, cmd_buffer);
}

void
xrd_scene_pointer_get_ray (XrdScenePointer *self,
                           graphene_ray_t  *res)
{
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);

  graphene_matrix_t *mat = &obj->model_matrix;

  graphene_vec4_t start;
  graphene_vec4_init (&start, 0, 0, self->start_offset, 1);
  graphene_matrix_transform_vec4 (mat, &start, &start);

  graphene_vec4_t end;
  graphene_vec4_init (&end, 0, 0, -self->length, 1);
  graphene_matrix_transform_vec4 (mat, &end, &end);

  graphene_vec4_t direction_vec4;
  graphene_vec4_subtract (&end, &start, &direction_vec4);

  graphene_point3d_t origin;
  graphene_vec3_t direction;

  graphene_vec3_t vec3_start;
  graphene_vec4_get_xyz (&start, &vec3_start);
  graphene_point3d_init_from_vec3 (&origin, &vec3_start);

  graphene_vec4_get_xyz (&direction_vec4, &direction);

  graphene_ray_init (res, &origin, &direction);
}

gboolean
xrd_scene_pointer_get_intersection (XrdScenePointer *pointer,
                                    XrdSceneWindow  *window,
                                    float           *distance,
                                    graphene_vec3_t *res)
{
  graphene_ray_t ray;
  xrd_scene_pointer_get_ray (pointer, &ray);

  graphene_plane_t plane;
  xrd_scene_window_get_plane (window, &plane);

  *distance = graphene_ray_get_distance_to_plane (&ray, &plane);
  if (*distance == INFINITY)
    return FALSE;

  graphene_ray_get_direction (&ray, res);
  graphene_vec3_scale (res, *distance, res);

  graphene_vec3_t origin;
  graphene_ray_get_origin_vec3 (&ray, &origin);
  graphene_vec3_add (&origin, res, res);

  graphene_matrix_t inverse;
  XrdSceneObject *window_obj = XRD_SCENE_OBJECT (window);
  graphene_matrix_inverse (&window_obj->model_matrix, &inverse);

  graphene_vec4_t intersection_vec4;
  graphene_vec4_init_from_vec3 (&intersection_vec4, res, 1.0f);

  graphene_vec4_t intersection_origin;
  graphene_matrix_transform_vec4 (&inverse,
                                  &intersection_vec4,
                                  &intersection_origin);

  float f[4];
  graphene_vec4_to_float (&intersection_origin, f);

  /* Test if we are in [0-aspect_ratio, 0-1] plane coordinates */
  if (f[0] >= -window->aspect_ratio / 2.0f
      && f[0] <= window->aspect_ratio / 2.0f
      && f[1] >= -0.5f && f[1] <= 0.5f)
    return TRUE;

  return FALSE;
}

static void
_move (XrdPointer        *pointer,
       graphene_matrix_t *transform)
{
  XrdScenePointer *self = XRD_SCENE_POINTER (pointer);
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  graphene_matrix_init_from_matrix (&obj->model_matrix, transform);
}

static void
_set_length (XrdPointer *pointer,
             float       length)
{
  XrdScenePointer *self = XRD_SCENE_POINTER (pointer);
  if (length == self->length)
    return;

  self->length = length;

  gulkan_vertex_buffer_reset (self->vertex_buffer);

  graphene_matrix_t identity;
  graphene_matrix_init_identity (&identity);

  graphene_vec4_t start;
  graphene_vec4_init (&start, 0, 0, self->start_offset, 1);

  gulkan_geometry_append_ray (self->vertex_buffer, &start, length, &identity);
  gulkan_vertex_buffer_map_array (self->vertex_buffer);
}

void
xrd_scene_pointer_reset_length (XrdScenePointer *self)
{
  _set_length (XRD_POINTER (self), self->default_length);
}

static void
_reset_length (XrdPointer *pointer)
{
  XrdScenePointer *self = XRD_SCENE_POINTER (pointer);
  self->length = self->default_length;
}

static float
_get_default_length (XrdPointer *pointer)
{
  XrdScenePointer *self = XRD_SCENE_POINTER (pointer);
  return self->default_length;
}

static void
xrd_scene_pointer_interface_init (XrdPointerInterface *iface)
{
  iface->move = _move;
  iface->set_length = _set_length;
  iface->get_default_length = _get_default_length;
  iface->reset_length = _reset_length;
}

XrdSceneSelection*
xrd_scene_pointer_get_selection (XrdScenePointer *self)
{
  return self->selection;
}
