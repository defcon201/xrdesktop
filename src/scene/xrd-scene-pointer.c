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

  XrdPointerData data;

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

  xrd_pointer_init (XRD_POINTER (self));

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
  graphene_vec4_init (&start, 0, 0, self->data.start_offset, 1);

  graphene_matrix_t identity;
  graphene_matrix_init_identity (&identity);

  gulkan_geometry_append_ray (self->vertex_buffer,
                              &start, self->data.length, &identity);
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
  if (!xrd_scene_object_is_visible (obj))
    return;

  xrd_scene_object_update_mvp_matrix (obj, eye, vp);

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  xrd_scene_selection_render (self->selection, eye,
                              selection_pipeline, pipeline_layout,
                              cmd_buffer, vp);

  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw (self->vertex_buffer, cmd_buffer);
}

static gboolean
_get_intersection (XrdPointer      *pointer,
                   XrdWindow       *window,
                   float           *distance,
                   graphene_vec3_t *res)
{
  graphene_ray_t ray;
  xrd_pointer_get_ray (pointer, &ray);

  XrdSceneWindow *scene_window = XRD_SCENE_WINDOW (window);
  graphene_plane_t plane;
  xrd_scene_window_get_plane (scene_window, &plane);

  *distance = graphene_ray_get_distance_to_plane (&ray, &plane);
  if (*distance == INFINITY)
    return FALSE;

  graphene_ray_get_direction (&ray, res);
  graphene_vec3_scale (res, *distance, res);

  graphene_vec3_t origin;
  graphene_ray_get_origin_vec3 (&ray, &origin);
  graphene_vec3_add (&origin, res, res);

  graphene_matrix_t inverse;
  XrdSceneObject *window_obj = XRD_SCENE_OBJECT (scene_window);
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
  if (f[0] >= -scene_window->aspect_ratio / 2.0f
      && f[0] <= scene_window->aspect_ratio / 2.0f
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
  gulkan_vertex_buffer_reset (self->vertex_buffer);

  graphene_matrix_t identity;
  graphene_matrix_init_identity (&identity);

  graphene_vec4_t start;
  graphene_vec4_init (&start, 0, 0, self->data.start_offset, 1);

  gulkan_geometry_append_ray (self->vertex_buffer, &start, length, &identity);
  gulkan_vertex_buffer_map_array (self->vertex_buffer);
}

static XrdPointerData*
_get_data (XrdPointer *pointer)
{
  XrdScenePointer *self = XRD_SCENE_POINTER (pointer);
  return &self->data;
}

static void
_set_transformation (XrdPointer        *pointer,
                     graphene_matrix_t *matrix)
{
  XrdScenePointer *self = XRD_SCENE_POINTER (pointer);
  xrd_scene_object_set_transformation (XRD_SCENE_OBJECT (self), matrix);
}

static void
_get_transformation (XrdPointer        *pointer,
                     graphene_matrix_t *matrix)
{
  XrdScenePointer *self = XRD_SCENE_POINTER (pointer);
  graphene_matrix_t transformation =
    xrd_scene_object_get_transformation (XRD_SCENE_OBJECT (self));
  graphene_matrix_init_from_matrix (matrix, &transformation);
}

static void
_set_selected_window (XrdPointer *pointer,
                      XrdWindow  *window)
{
  XrdScenePointer *self = XRD_SCENE_POINTER (pointer);
  XrdSceneObject *selection_obj = XRD_SCENE_OBJECT (self->selection);
  if (window == NULL)
    {
      selection_obj->visible = FALSE;
      return;
    }

  XrdSceneWindow *scene_window = XRD_SCENE_WINDOW (window);
  XrdSceneObject *window_obj = XRD_SCENE_OBJECT (scene_window);
  graphene_matrix_init_from_matrix (&selection_obj->model_matrix,
                                    &window_obj->model_matrix);
  xrd_scene_selection_set_aspect_ratio (self->selection,
                                        scene_window->aspect_ratio);
  selection_obj->visible = TRUE;
}

static void
xrd_scene_pointer_interface_init (XrdPointerInterface *iface)
{
  iface->move = _move;
  iface->set_length = _set_length;
  iface->get_data = _get_data;
  iface->set_transformation = _set_transformation;
  iface->get_transformation = _get_transformation;
  iface->get_intersection = _get_intersection;
  iface->set_selected_window = _set_selected_window;
}

