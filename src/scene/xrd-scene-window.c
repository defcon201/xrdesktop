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
                "initial-width-meters", (double) width_meters,
                "initial-height-meters", (double) height_meters,
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
                "initial-width-meters", (double) width_pixels / (double) ppm,
                "initial-height-meters", (double) height_pixels / (double) ppm,
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

  XrdSceneWindow *parent =
    XRD_SCENE_WINDOW (self->window_data.parent_window);

  if (parent != NULL)
    parent->window_data.child_window = NULL;

  /* TODO: a child window should not exist without a parent window anyway,
   * but it will be cleaned up already because the child window on the desktop
   * will most likely close already. */

  XrdSceneWindow *child = XRD_SCENE_WINDOW (self->window_data.child_window);
  if (child)
    child->window_data.parent_window = NULL;

  if (self->texture)
    g_object_unref (self->texture);

  G_OBJECT_CLASS (xrd_scene_window_parent_class)->finalize (gobject);
}

static void
_append_plane (GulkanVertexBuffer *vbo, float aspect_ratio)
{
  graphene_matrix_t mat_scale;
  graphene_matrix_init_scale (&mat_scale, aspect_ratio, 1.0f, 1.0f);

  graphene_point_t from = { .x = -0.5, .y = -0.5 };
  graphene_point_t to = { .x = 0.5, .y = 0.5 };

  gulkan_geometry_append_plane (vbo, &from, &to, &mat_scale);
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

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  if (!xrd_scene_object_is_visible (obj))
    return;

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  xrd_scene_object_update_mvp_matrix (obj, eye, vp);
  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw (self->vertex_buffer, cmd_buffer);
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

  if (self->window_data.child_window)
    xrd_window_update_child (window);

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

static gboolean
_get_transformation_no_scale (XrdWindow         *window,
                              graphene_matrix_t *mat)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  *mat = xrd_scene_object_get_transformation_no_scale (XRD_SCENE_OBJECT (self));
  return TRUE;
}

static void
_submit_texture (XrdWindow     *window,
                 GulkanClient  *client,
                 GulkanTexture *texture)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);

  if (texture == self->texture)
    {
      gchar *title;
      g_object_get (window, "title", &title, NULL);
      g_debug ("Texture %p was already submitted to window %p (%s).\n",
                 (void*) texture, (void*) window, title);
      return;
    }

  VkDevice device = gulkan_client_get_device_handle (client);

  uint32_t w = gulkan_texture_get_width (texture);
  uint32_t h = gulkan_texture_get_height (texture);

  float aspect_ratio = (float) w / (float) h;

  if (self->aspect_ratio != aspect_ratio)
    {
      self->aspect_ratio = aspect_ratio;
      gulkan_vertex_buffer_reset (self->vertex_buffer);
      _append_plane (self->vertex_buffer, self->aspect_ratio);
      gulkan_vertex_buffer_map_array (self->vertex_buffer);
    }

  if (self->texture)
    g_object_unref (self->texture);

  self->texture = texture;
  g_object_ref (self->texture);

  guint mip_levels = gulkan_texture_get_mip_levels (texture);

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

static void
_add_child (XrdWindow        *window,
            XrdWindow        *child,
            graphene_point_t *offset_center)
{
  (void) window;
  (void) child;
  (void) offset_center;
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

void
xrd_scene_window_set_width_meters (XrdSceneWindow *self,
                                   float           width_meters)
{
  float height_meters = width_meters / self->aspect_ratio;

  g_object_set (self,
                "initial-width-meters", (double) width_meters,
                "initial-height-meters", (double) height_meters,
                "scale", 1.0, /* Reset window scale */
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
  iface->get_transformation_no_scale = _get_transformation_no_scale;
  iface->submit_texture = _submit_texture;
  iface->poll_event = _poll_event;
  iface->add_child = _add_child;
  iface->set_color = _set_color;
  iface->set_flip_y = _set_flip_y;
  iface->show = (void (*)(XrdWindow*)) xrd_scene_object_show;
  iface->hide = (void (*)(XrdWindow*)) xrd_scene_object_hide;
  iface->is_visible = (gboolean (*)(XrdWindow*)) xrd_scene_object_is_visible;
  iface->get_data = _get_data;
}
