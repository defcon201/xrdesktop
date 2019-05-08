/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gulkan-geometry.h>

#include "graphene-ext.h"
#include "xrd-scene-window.h"

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
xrd_scene_window_window_interface_init (XrdWindowInterface *iface)
{
  iface->set_transformation =
      (void*) xrd_scene_window_set_transformation;
  iface->get_transformation =
      (void*) xrd_scene_window_get_transformation;
  iface->submit_texture = (void*)xrd_scene_window_submit_texture;
  iface->poll_event = (void*)xrd_scene_window_poll_event;
  iface->intersects = (void*)xrd_scene_window_intersects;
  iface->intersection_to_pixels =
      (void*)xrd_scene_window_intersection_to_pixels;
  iface->intersection_to_2d_offset_meter =
      (void*)xrd_scene_window_intersection_to_2d_offset_meter;
  iface->add_child = (void*)xrd_scene_window_add_child;
  iface->set_color = (void*)xrd_scene_window_set_color;
  iface->set_flip_y = (void*)xrd_scene_window_set_flip_y;
}

static void
xrd_scene_window_init (XrdSceneWindow *self)
{
  self->vertex_buffer = gulkan_vertex_buffer_new ();
  self->sampler = VK_NULL_HANDLE;
  self->aspect_ratio = 1.0;
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

  self->aspect_ratio = (float) gdk_pixbuf_get_width (pixbuf) /
                       (float) gdk_pixbuf_get_height (pixbuf);

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

void _append_plane (GulkanVertexBuffer *vbo, float aspect_ratio)
{
  graphene_matrix_t mat_scale;
  graphene_matrix_init_scale (&mat_scale, aspect_ratio, 1.0f, 1.0f);

  gulkan_geometry_append_plane (vbo, &mat_scale);
}

gboolean
xrd_scene_window_initialize (XrdSceneWindow        *self,
                             GulkanDevice          *device,
                             VkDescriptorSetLayout *layout)
{
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);

  _append_plane (self->vertex_buffer, self->aspect_ratio);
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

gboolean
xrd_scene_window_set_transformation (XrdSceneWindow    *self,
                                     graphene_matrix_t *mat)
{
  (void) self;
  (void) mat;
  g_warning ("stub: xrd_scene_window_set_transformation_matrix\n");
  return TRUE;
}

gboolean
xrd_scene_window_get_transformation (XrdSceneWindow    *self,
                                     graphene_matrix_t *mat)
{
  (void) self;
  (void) mat;
  g_warning ("stub: xrd_scene_window_get_transformation_matrix\n");
  return TRUE;
}

void
xrd_scene_window_submit_texture (XrdSceneWindow *self,
                                 GulkanClient   *client,
                                 GulkanTexture  *texture)
{
  (void) self;
  (void) client;
  (void) texture;
  g_warning ("stub: xrd_scene_window_submit_texture\n");
}

void
xrd_scene_window_poll_event (XrdSceneWindow *self)
{
  (void) self;
  g_warning ("stub: xrd_scene_window_poll_event\n");
}

gboolean
xrd_scene_window_intersects (XrdSceneWindow     *self,
                             graphene_matrix_t  *pointer_transformation_matrix,
                             graphene_point3d_t *intersection_point)
{
  (void) self;
  (void) pointer_transformation_matrix;
  (void) intersection_point;

  g_warning ("stub: xrd_scene_window_intersects\n");
  return TRUE;
}

gboolean
xrd_scene_window_intersection_to_pixels (XrdSceneWindow     *self,
                                         graphene_point3d_t *intersection_point,
                                         XrdPixelSize       *size_pixels,
                                         graphene_point_t   *window_coords)
{
  (void) self;
  (void) intersection_point;
  (void) size_pixels;
  (void) window_coords;

  g_warning ("stub: xrd_scene_window_intersection_to_pixels\n");
  return TRUE;
}

gboolean
xrd_scene_window_intersection_to_2d_offset_meter (XrdSceneWindow     *self,
                                                  graphene_point3d_t *intersection_point,
                                                  graphene_point_t   *offset_center)
{
  (void) self;
  (void) intersection_point;
  (void) offset_center;

  g_warning ("stub: xrd_scene_window_intersection_to_2d_offset_meter\n");
  return TRUE;
}

void
xrd_scene_window_add_child (XrdSceneWindow   *self,
                            XrdSceneWindow   *child,
                            graphene_point_t *offset_center)
{
  (void) self;
  (void) child;
  (void) offset_center;

  g_warning ("stub: xrd_scene_window_add_child\n");
}

void
xrd_scene_window_set_color (XrdSceneWindow  *self,
                            graphene_vec3_t *color)
{
  (void) self;
  (void) color;

  g_warning ("stub: xrd_scene_window_set_color\n");
}

void
xrd_scene_window_set_flip_y (XrdSceneWindow *self,
                             gboolean        flip_y)
{
  (void) self;
  (void) flip_y;

  g_warning ("stub: xrd_scene_window_set_flip_y\n");
}
