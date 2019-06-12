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

typedef struct _XrdSceneWindowPrivate
{
  XrdSceneObject parent;

  GulkanVertexBuffer *vertex_buffer;
  VkSampler sampler;
  float aspect_ratio;

  gboolean flip_y;
  graphene_vec3_t color;

  XrdWindowData window_data;
} XrdSceneWindowPrivate;

G_DEFINE_TYPE_WITH_CODE (XrdSceneWindow, xrd_scene_window, XRD_TYPE_SCENE_OBJECT,
                         G_ADD_PRIVATE (XrdSceneWindow)
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_WINDOW,
                                                xrd_scene_window_window_interface_init))

static void
xrd_scene_window_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (object);
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);

  switch (property_id)
    {
    case PROP_TITLE:
      if (priv->window_data.title)
        g_string_free (priv->window_data.title, TRUE);
      priv->window_data.title = g_string_new (g_value_get_string (value));
      break;
    case PROP_SCALE:
      priv->window_data.scale = g_value_get_float (value);
      break;
    case PROP_NATIVE:
      priv->window_data.native = g_value_get_pointer (value);
      break;
    case PROP_TEXTURE_WIDTH:
      priv->window_data.texture_width = g_value_get_uint (value);
      break;
    case PROP_TEXTURE_HEIGHT:
      priv->window_data.texture_height = g_value_get_uint (value);
      break;
    case PROP_WIDTH_METERS:
      priv->window_data.initial_size_meters.x = g_value_get_float (value);
      break;
    case PROP_HEIGHT_METERS:
      priv->window_data.initial_size_meters.y = g_value_get_float (value);
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
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);

  switch (property_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->window_data.title->str);
      break;
    case PROP_SCALE:
      g_value_set_float (value, priv->window_data.scale);
      break;
    case PROP_NATIVE:
      g_value_set_pointer (value, priv->window_data.native);
      break;
    case PROP_TEXTURE_WIDTH:
      g_value_set_uint (value, priv->window_data.texture_width);
      break;
    case PROP_TEXTURE_HEIGHT:
      g_value_set_uint (value, priv->window_data.texture_height);
      break;
    case PROP_WIDTH_METERS:
      g_value_set_float (value, priv->window_data.initial_size_meters.x);
      break;
    case PROP_HEIGHT_METERS:
      g_value_set_float (value, priv->window_data.initial_size_meters.y);
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
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  priv->vertex_buffer = gulkan_vertex_buffer_new ();
  priv->sampler = VK_NULL_HANDLE;
  priv->aspect_ratio = 1.0;
  priv->window_data.texture = NULL;
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
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);

  /* TODO: Ref texture when set, unref in examples */
  //g_object_unref (self->texture);

  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();

  vkDestroySampler (gulkan_client_get_device_handle (GULKAN_CLIENT (renderer)),
                    priv->sampler, NULL);

  g_object_unref (priv->vertex_buffer);

  XrdSceneWindow *parent =
    XRD_SCENE_WINDOW (priv->window_data.parent_window);

  if (parent != NULL)
    {
      XrdSceneWindowPrivate *parent_priv =
        xrd_scene_window_get_instance_private (parent);
      parent_priv->window_data.child_window = NULL;
    }

  /* TODO: a child window should not exist without a parent window anyway,
   * but it will be cleaned up already because the child window on the desktop
   * will most likely close already. */

  XrdSceneWindow *child = XRD_SCENE_WINDOW (priv->window_data.child_window);
  if (child)
    {
      XrdSceneWindowPrivate *child_priv =
        xrd_scene_window_get_instance_private (child);
      child_priv->window_data.parent_window = NULL;
    }

  if (priv->window_data.texture)
    g_object_unref (priv->window_data.texture);

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
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);

  _append_plane (priv->vertex_buffer, priv->aspect_ratio);
  if (!gulkan_vertex_buffer_alloc_array (priv->vertex_buffer,
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
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  if (!priv->window_data.texture)
    {
      /* g_warning ("Trying to draw window with no texture.\n"); */
      return;
    }

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  if (!xrd_scene_object_is_visible (obj))
    return;

  vkCmdBindPipeline (cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  xrd_scene_object_update_mvp_matrix (obj, eye, vp);
  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw (priv->vertex_buffer, cmd_buffer);
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

  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  if (priv->window_data.child_window)
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

  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  if (texture == priv->window_data.texture)
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

  if (priv->aspect_ratio != aspect_ratio)
    {
      priv->aspect_ratio = aspect_ratio;
      gulkan_vertex_buffer_reset (priv->vertex_buffer);
      _append_plane (priv->vertex_buffer, priv->aspect_ratio);
      gulkan_vertex_buffer_map_array (priv->vertex_buffer);
    }

  if (priv->window_data.texture)
    g_object_unref (priv->window_data.texture);

  priv->window_data.texture = texture;
  g_object_ref (priv->window_data.texture);

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

  if (priv->sampler != VK_NULL_HANDLE)
    vkDestroySampler (device, priv->sampler, NULL);

  vkCreateSampler (device, &sampler_info, NULL, &priv->sampler);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  xrd_scene_object_update_descriptors_texture (obj, priv->sampler,
                                               gulkan_texture_get_image_view (
                                                 priv->window_data.texture));
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
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  graphene_vec3_init_from_vec3 (&priv->color, color);
}

static void
_select (XrdWindow *window)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  priv->window_data.selected = TRUE;

  g_print ("Scene window select STUB\n");
}

static void
_deselect (XrdWindow *window)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  priv->window_data.selected = FALSE;

  g_print ("Scene window deselect STUB\n");
}

static gboolean
_is_selected (XrdWindow *window)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  return priv->window_data.selected;
}

static void
_end_selection (XrdWindow *window)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  priv->window_data.selected = FALSE;

  g_print ("Scene window end selection STUB\n");
}

static void
_set_flip_y (XrdWindow *window,
             gboolean   flip_y)
{
  XrdSceneWindow *self = XRD_SCENE_WINDOW (window);
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  priv->flip_y = flip_y;
}

void
xrd_scene_window_set_width_meters (XrdSceneWindow *self,
                                   float           width_meters)
{
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  float height_meters = width_meters / priv->aspect_ratio;

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
  XrdSceneWindowPrivate *priv = xrd_scene_window_get_instance_private (self);
  return &priv->window_data;
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
  iface->select = _select;
  iface->deselect = _deselect;
  iface->is_selected = _is_selected;
  iface->end_selection = _end_selection;
  iface->set_flip_y = _set_flip_y;
  iface->show = (void (*)(XrdWindow*)) xrd_scene_object_show;
  iface->hide = (void (*)(XrdWindow*)) xrd_scene_object_hide;
  iface->is_visible = (gboolean (*)(XrdWindow*)) xrd_scene_object_is_visible;
  iface->get_data = _get_data;
}
