/*
 * xrddesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_RENDERER_H_
#define XRD_SCENE_RENDERER_H_

#include <glib-object.h>

#include <gulkan-client.h>

G_BEGIN_DECLS

enum PipelineType
{
  PIPELINE_WINDOWS = 0,
  PIPELINE_POINTER,
  PIPELINE_DEVICE_MODELS,
  PIPELINE_COUNT
};

#define XRD_TYPE_SCENE_RENDERER xrd_scene_renderer_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneRenderer, xrd_scene_renderer,
                      XRD, SCENE_RENDERER, GulkanClient)

XrdSceneRenderer *xrd_scene_renderer_new (void);

XrdSceneRenderer *xrd_scene_renderer_get_instance (void);

bool
xrd_scene_renderer_init_vulkan (XrdSceneRenderer *self);

VkDescriptorSetLayout *
xrd_scene_renderer_get_descriptor_set_layout (XrdSceneRenderer *self);

void
xrd_scene_renderer_draw (XrdSceneRenderer *self);

void
xrd_scene_renderer_set_render_cb (XrdSceneRenderer *self,
                                  void (*render_eye) (uint32_t         eye,
                                                      VkCommandBuffer  cmd_buffer,
                                                      VkPipelineLayout pipeline_layout,
                                                      VkPipeline      *pipelines,
                                                      gpointer         data),
                                  gpointer data);

G_END_DECLS

#endif /* XRD_SCENE_RENDERER_H_ */
