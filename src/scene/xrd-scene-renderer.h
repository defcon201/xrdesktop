/*
 * xrddesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_RENDERER_H_
#define XRD_SCENE_RENDERER_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>

#include <gulkan.h>

G_BEGIN_DECLS

enum PipelineType
{
  PIPELINE_WINDOWS = 0,
  PIPELINE_TIP,
  PIPELINE_POINTER,
  PIPELINE_SELECTION,
  PIPELINE_BACKGROUND,
  PIPELINE_DEVICE_MODELS,
  PIPELINE_COUNT
};

#define XRD_TYPE_SCENE_RENDERER xrd_scene_renderer_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneRenderer, xrd_scene_renderer,
                      XRD, SCENE_RENDERER, GulkanClient)

XrdSceneRenderer *xrd_scene_renderer_get_instance (void);

void xrd_scene_renderer_destroy_instance (void);

GulkanDevice*
xrd_scene_renderer_get_device (void);

bool
xrd_scene_renderer_init_vulkan_simple (XrdSceneRenderer *self);

bool
xrd_scene_renderer_init_vulkan_openvr (XrdSceneRenderer *self);

VkDescriptorSetLayout *
xrd_scene_renderer_get_descriptor_set_layout (XrdSceneRenderer *self);

bool
xrd_scene_renderer_draw (XrdSceneRenderer *self);

void
xrd_scene_renderer_set_render_cb (XrdSceneRenderer *self,
                                  void (*render_eye) (uint32_t         eye,
                                                      VkCommandBuffer  cmd_buffer,
                                                      VkPipelineLayout pipeline_layout,
                                                      VkPipeline      *pipelines,
                                                      gpointer         data),
                                  gpointer scene_client);

void
xrd_scene_renderer_set_update_lights_cb (XrdSceneRenderer *self,
                                         void (*update_lights) (gpointer data),
                                         gpointer scene_client);

VkBuffer
xrd_scene_renderer_get_lights_buffer_handle (XrdSceneRenderer *self);

void
xrd_scene_renderer_update_lights (XrdSceneRenderer  *self,
                                  GList             *controllers);

G_END_DECLS

#endif /* XRD_SCENE_RENDERER_H_ */
