/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gulkan.h>

#include "graphene-ext.h"
#include "xrd-scene-window.h"
#include "xrd-scene-renderer.h"

enum
{
  PROP_TITLE = 1,
  PROP_SCALE,
  PROP_NATIVE,
  PROP_TEXTURE_WIDTH,
  PROP_TEXTURE_HEIGHT,
  PROP_WIDTH_METERS,
  PROP_HEIGHT_METERS,
  N_PROPERTIES
};

static void
xrd_scene_window_window_interface_init (XrdWindowInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdSceneWindow, xrd_scene_window, XRD_TYPE_SCENE_OBJECT,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_WINDOW,
                                                xrd_scene_window_window_interface_init))

static void
xrd_scene_window_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (object);
  switch (property_id)
    {
    case PROP_TITLE:
      if (self->window_data.title)
        g_string_free (self->window_data.title, TRUE);
      self->window_data.title = g_string_new (g_value_get_string (value));
      break;
    case PROP_SCALE:
      self->window_data.scale = g_value_get_float (value);
      break;
    case PROP_NATIVE:
      self->window_data.native = g_value_get_pointer (value);
      break;
    case PROP_TEXTURE_WIDTH:
      self->window_data.texture_width = g_value_get_uint (value);
      break;
    case PROP_TEXTURE_HEIGHT:
      self->window_data.texture_height = g_value_get_uint (value);
      break;
    case PROP_WIDTH_METERS:
      self->window_data.initial_size_meters.x = g_value_get_float (value);
      break;
    case PROP_HEIGHT_METERS:
      self->window_data.initial_size_meters.y = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xrd_scene_window_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (object);

  switch (property_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, self->window_data.title->str);
      break;
    case PROP_SCALE:
      g_value_set_float (value, self->window_data.scale);
      break;
    case PROP_NATIVE:
      g_value_set_pointer (value, self->window_data.native);
      break;
    case PROP_TEXTURE_WIDTH:
      g_value_set_uint (value, self->window_data.texture_width);
      break;
    case PROP_TEXTURE_HEIGHT:
      g_value_set_uint (value, self->window_data.texture_height);
      break;
    case PROP_WIDTH_METERS:
      g_value_set_float (value, self->window_data.initial_size_meters.x);
      break;
    case PROP_HEIGHT_METERS:
      g_value_set_float (value, self->window_data.initial_size_meters.y);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xrd_scene_window_finalize (GObject *gobject);

static void
xrd_scene_window_class_init (XrdSceneWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_window_finalize;

  object_class->set_property = xrd_scene_window_set_property;
  object_class->get_property = xrd_scene_window_get_property;

  g_object_class_override_property (object_class, PROP_TITLE, "title");
  g_object_class_override_property (object_class, PROP_SCALE, "scale");
  g_object_class_override_property (object_class, PROP_NATIVE, "native");
  g_object_class_override_property (object_class, PROP_TEXTURE_WIDTH, "texture-width");
  g_object_class_override_property (object_class, PROP_TEXTURE_HEIGHT, "texture-height");
  g_object_class_override_property (object_class, PROP_WIDTH_METERS, "initial-width-meters");
  g_object_class_override_property (object_class, PROP_HEIGHT_METERS, "initial-height-meters");
}

static void
xrd_scene_window_init (XrdSceneWindow *self)
{
  self->vertex_buffer = gulkan_vertex_buffer_new ();
  self->sampler = VK_NULL_HANDLE;
  self->aspect_ratio = 1.0;
  self->texture = NULL;
}

XrdSceneWindow *
xrd_scene_window_new (const gchar *title)
{
  return (XrdSceneWindow*) g_object_new (XRD_TYPE_SCENE_WINDOW,
                                         "title", title, NULL);
}

XrdSceneWindow *
xrd_scene_window_new_from_meters (const gchar *title,
                                  float        width_meters,
                                  float        height_meters)
{
  XrdSceneWindow *window = xrd_scene_window_new (title);
  g_object_set (window,
                "initial-width-meters", width_meters,
                "initial-height-meters", height_meters,
                NULL);
  return window;
}

XrdSceneWindow *
xrd_scene_window_new_from_ppm (const gchar *title,
                               uint32_t     width_pixels,
                               uint32_t     height_pixels,
                               float        ppm)
{
  XrdSceneWindow *window = xrd_scene_window_new (title);
  g_object_set (window,
                "texture-width", width_pixels,
                "texture-height", height_pixels,
                "initial-width-meters", width_pixels / ppm,
                "initial-height-meters", height_pixels / ppm,
                NULL);
  return window;
}

XrdSceneWindow *
xrd_scene_window_new_from_native (const gchar *title,
                                  gpointer     native,
                                  uint32_t     width_pixels,
                                  uint32_t     height_pixels,
                                  float        ppm)
{
  XrdSceneWindow *window = xrd_scene_window_new_from_ppm (title,
                                                          width_pixels,
                                                          height_pixels,
                                                          ppm);
  g_object_set (window, "native", native, NULL);
  return window;
}


static void
xrd_scene_window_finalize (GObject *gobject)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (gobject);
  /* TODO: Ref texture when set, unref in examples */
  //g_object_unref (self->texture);

  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();

  vkDestroySampler (gulkan_client_get_device_handle (GULKAN_CLIENT (renderer)),
                    self->sampler, NULL);

  g_object_unref (self->vertex_buffer);

  G_OBJECT_CLASS (xrd_scene_window_parent_class)->finalize (gobject);
}

void _append_plane (GulkanVertexBuffer *vbo, float aspect_ratio)
{
  graphene_matrix_t mat_scale;
  graphene_matrix_init_scale (&mat_scale, aspect_ratio, 1.0f, 1.0f);

  gulkan_geometry_append_plane (vbo, &mat_scale);
}

gboolean
xrd_scene_window_initialize (XrdSceneWindow *self)
{
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();

  _append_plane (self->vertex_buffer, self->aspect_ratio);
  if (!gulkan_vertex_buffer_alloc_array (self->vertex_buffer,
                                         gulkan_client_get_device (
                                           GULKAN_CLIENT (renderer))))
    return FALSE;

  VkDescriptorSetLayout *layout =
    xrd_scene_renderer_get_descriptor_set_layout (renderer);

  if (!xrd_scene_object_initialize (obj, layout))
    return FALSE;

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
  if (!self->texture)
    {
      g_warning ("Trying to draw window with no texture.\n");
      return;
    }

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  xrd_scene_object_update_mvp_matrix (obj, eye, vp);
  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw (self->vertex_buffer, cmd_buffer);
}

void
xrd_scene_window_get_normal (XrdSceneWindow  *self,
                             graphene_vec3_t *normal)
{
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);

  graphene_vec3_init (normal, 0, 0, 1);

  graphene_matrix_t rotation_matrix;
  graphene_matrix_get_rotation_matrix (&obj->model_matrix,
                                       &rotation_matrix);

  graphene_matrix_transform_vec3 (&rotation_matrix, normal, normal);
}

void
xrd_scene_window_get_plane (XrdSceneWindow   *self,
                            graphene_plane_t *res)
{
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  graphene_vec3_t normal;
  xrd_scene_window_get_normal (self, &normal);
  graphene_plane_init_from_point (res, &normal, &obj->position);
}

/* XrdWindow Interface functions */

static gboolean
_set_transformation (XrdWindow         *window,
                     graphene_matrix_t *mat)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  xrd_scene_object_set_transformation (XRD_SCENE_OBJECT (self), mat);

  float height_meters =
    xrd_window_get_current_height_meters (XRD_WINDOW (self));

  xrd_scene_object_set_scale (XRD_SCENE_OBJECT (self), height_meters);

  return TRUE;
}

static gboolean
_get_transformation (XrdWindow         *window,
                     graphene_matrix_t *mat)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  *mat = xrd_scene_object_get_transformation (XRD_SCENE_OBJECT (self));
  return TRUE;
}

static void
_submit_texture (XrdWindow     *window,
                 GulkanClient  *client,
                 GulkanTexture *texture)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  VkDevice device = gulkan_client_get_device_handle (client);

  float aspect_ratio = (float) gulkan_texture_get_width (texture) /
    (float) gulkan_texture_get_height (texture);

  if (self->aspect_ratio != aspect_ratio)
    {
      self->aspect_ratio = aspect_ratio;
      gulkan_vertex_buffer_reset (self->vertex_buffer);
      _append_plane (self->vertex_buffer, self->aspect_ratio);
      gulkan_vertex_buffer_map_array (self->vertex_buffer);
    }

  self->texture = texture;

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
    .maxLod = (float) gulkan_texture_get_mip_levels (texture)
  };

  if (self->sampler != VK_NULL_HANDLE)
    vkDestroySampler (device, self->sampler, NULL);

  vkCreateSampler (device, &sampler_info, NULL, &self->sampler);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  xrd_scene_object_update_descriptors_texture (obj, self->sampler,
                                               gulkan_texture_get_image_view (
                                                 self->texture));
}

static void
_poll_event (XrdWindow *self)
{
  (void) self;
}

/* TODO: Use pointer class in interface */
static gboolean
_intersects (XrdWindow          *window,
             graphene_matrix_t  *mat,
             graphene_point3d_t *intersection_point)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);

  /* TODO: Don't Hardcode pointer props */
  float start_offset = -0.02f;
  float length = 40.0f;

  /* Get ray */
  graphene_vec4_t start;
  graphene_vec4_init (&start, 0, 0, start_offset, 1);
  graphene_matrix_transform_vec4 (mat, &start, &start);

  graphene_vec4_t end;
  graphene_vec4_init (&end, 0, 0, -length, 1);
  graphene_matrix_transform_vec4 (mat, &end, &end);

  graphene_vec4_t direction_vec4;
  graphene_vec4_subtract (&end, &start, &direction_vec4);

  graphene_point3d_t origin;
  graphene_vec3_t direction;

  graphene_vec3_t vec3_start;
  graphene_vec4_get_xyz (&start, &vec3_start);
  graphene_point3d_init_from_vec3 (&origin, &vec3_start);

  graphene_vec4_get_xyz (&direction_vec4, &direction);

  graphene_ray_t ray;
  graphene_ray_init (&ray, &origin, &direction);

  /* Get intersection */

  graphene_plane_t plane;
  xrd_scene_window_get_plane (self, &plane);

  float distance = graphene_ray_get_distance_to_plane (&ray, &plane);
  if (distance == INFINITY)
    return FALSE;

  graphene_vec3_t intersection_vec;
  graphene_ray_get_direction (&ray, &intersection_vec);
  graphene_vec3_scale (&intersection_vec, distance, &intersection_vec);

  graphene_vec3_t intersetion_origin;
  graphene_ray_get_origin_vec3 (&ray, &intersetion_origin);
  graphene_vec3_add (&intersetion_origin, &intersection_vec, &intersection_vec);

  graphene_matrix_t inverse;
  XrdSceneObject *window_obj = XRD_SCENE_OBJECT (self);
  graphene_matrix_inverse (&window_obj->model_matrix, &inverse);

  graphene_vec4_t intersection_vec4;
  graphene_vec4_init_from_vec3 (&intersection_vec4, &intersection_vec, 1.0f);

  graphene_vec4_t intersection_origin;
  graphene_matrix_transform_vec4 (&inverse,
                                  &intersection_vec4,
                                  &intersection_origin);

  float f[4];
  graphene_vec4_to_float (&intersection_origin, f);

  graphene_point3d_init_from_vec3 (intersection_point, &intersection_vec);

  /* Test if we are in [0-aspect_ratio, 0-1] plane coordinates */
  if (f[0] >= 0 && f[0] <= self->aspect_ratio && f[1] >= 0 && f[1] <= 1.0f)
    return TRUE;

  return FALSE;
}

static gboolean
_intersection_to_pixels (XrdWindow          *window,
                         graphene_point3d_t *intersection_point,
                         XrdPixelSize       *size_pixels,
                         graphene_point_t   *window_coords)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);

  /* transform intersection point to origin */
  graphene_matrix_t transform =
    xrd_scene_object_get_transformation (XRD_SCENE_OBJECT (self));

  graphene_matrix_t inverse_transform;
  graphene_matrix_inverse (&transform, &inverse_transform);

  graphene_point3d_t intersection_origin;
  graphene_matrix_transform_point3d (&inverse_transform,
                                      intersection_point,
                                     &intersection_origin);

  graphene_vec2_t position_2d_vec;
  graphene_vec2_init (&position_2d_vec,
                      intersection_origin.x,
                      intersection_origin.y);

  /* normalize coordinates to [0 - 1, 0 - 1] */
  graphene_vec2_t size_meters;

  XrdWindow *xrd_window = XRD_WINDOW (self);

  graphene_vec2_init (&size_meters,
                      xrd_window_get_current_width_meters (xrd_window),
                      xrd_window_get_current_height_meters (xrd_window));

  graphene_vec2_divide (&position_2d_vec, &size_meters, &position_2d_vec);

  /* move origin from cetner to corner of overlay */
  graphene_vec2_t center_normalized;
  graphene_vec2_init (&center_normalized, 0.5f, 0.5f);

  graphene_vec2_add (&position_2d_vec, &center_normalized, &position_2d_vec);

  /* invert y axis */
  graphene_vec2_init (&position_2d_vec,
                      graphene_vec2_get_x (&position_2d_vec),
                      1.0f - graphene_vec2_get_y (&position_2d_vec));

  /* scale to pixel coordinates */
  graphene_vec2_t size_pixels_vec;
  graphene_vec2_init (&size_pixels_vec,
                      size_pixels->width,
                      size_pixels->height);

  graphene_vec2_multiply (&position_2d_vec, &size_pixels_vec, &position_2d_vec);

  /* return point_t */
  graphene_point_init_from_vec2 (window_coords, &position_2d_vec);

  return TRUE;
}

static gboolean
_intersection_to_2d_offset_meter (XrdWindow          *window,
                                  graphene_point3d_t *intersection_point,
                                  graphene_point_t   *offset_center)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);

  graphene_matrix_t transform =
    xrd_scene_object_get_transformation (XRD_SCENE_OBJECT (self));

  graphene_matrix_t inverse_transform;
  graphene_matrix_inverse (&transform, &inverse_transform);

  graphene_point3d_t intersection_origin;
  graphene_matrix_transform_point3d (&inverse_transform,
                                      intersection_point,
                                     &intersection_origin);

  graphene_point_init (offset_center,
                      intersection_origin.x,
                      intersection_origin.y);
  return TRUE;
}

static void
_add_child (XrdWindow        *window,
            XrdWindow        *child,
            graphene_point_t *offset_center)
{
  (void) window;
  (void) child;
  (void) offset_center;

  g_warning ("stub: xrd_scene_window_add_child\n");
}

static void
_set_color (XrdWindow       *window,
            graphene_vec3_t *color)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  graphene_vec3_init_from_vec3 (&self->color, color);
}

static void
_set_flip_y (XrdWindow *window,
             gboolean   flip_y)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  self->flip_y = flip_y;
}

static void
_set_hidden (XrdWindow *window,
             gboolean   hidden)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  obj->visible = !hidden;
}

static gboolean
_get_hidden (XrdWindow *window)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  return !obj->visible;
}

void
xrd_scene_window_set_width_meters (XrdSceneWindow *self,
                                   float           width_meters)
{
  float height_meters = width_meters / self->aspect_ratio;

  g_object_set (self,
                "initial-width-meters", width_meters,
                "initial-height-meters", height_meters,
                "scale", 1.0f, /* Reset window scale */
                NULL);

  xrd_scene_object_set_scale (XRD_SCENE_OBJECT (self), height_meters);
}

static XrdWindowData*
_get_data (XrdWindow *window)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  return &self->window_data;
}

static void
xrd_scene_window_window_interface_init (XrdWindowInterface *iface)
{
  iface->set_transformation = _set_transformation;
  iface->get_transformation = _get_transformation;
  iface->submit_texture = _submit_texture;
  iface->poll_event = _poll_event;
  iface->intersects = _intersects;
  iface->intersection_to_pixels = _intersection_to_pixels;
  iface->intersection_to_2d_offset_meter = _intersection_to_2d_offset_meter;
  iface->add_child = _add_child;
  iface->set_color = _set_color;
  iface->set_flip_y = _set_flip_y;
  iface->set_hidden = _set_hidden;
  iface->get_hidden = _get_hidden;
  iface->get_data = _get_data;
}
