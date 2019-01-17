/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_CLIENT_H_
#define XRD_SCENE_CLIENT_H_

#include "openvr-context.h"
#include <glib-object.h>

#include <gulkan-client.h>
#include <gulkan-device.h>
#include <gulkan-instance.h>
#include <gulkan-texture.h>
#include <gulkan-frame-buffer.h>
#include <gulkan-vertex-buffer.h>
#include <gulkan-uniform-buffer.h>

#include "xrd-scene-device.h"
#include "xrd-scene-device-manager.h"
#include "xrd-scene-window.h"
#include "xrd-scene-pointer.h"

// Pipeline state objects
enum PipelineType
{
  PIPELINE_WINDOWS = 0,
  PIPELINE_POINTER,
  PIPELINE_DEVICE_MODELS,
  PIPELINE_COUNT
};

typedef struct VertexDataScene
{
  graphene_point3d_t position;
  graphene_point_t   uv;
} VertexDataScene;

G_BEGIN_DECLS

#define WINDOW_COUNT 4

#define XRD_TYPE_SCENE_CLIENT xrd_scene_client_get_type ()
G_DECLARE_FINAL_TYPE (XrdSceneClient, xrd_scene_client,
                      XRD, SCENE_CLIENT, GulkanClient)

struct _XrdSceneClient
{
  GulkanClient parent;

  VkSampleCountFlagBits msaa_sample_count;
  float super_sample_scale;

  XrdSceneDeviceManager *device_manager;

  float near_clip;
  float far_clip;

  XrdSceneWindow *windows[WINDOW_COUNT];

  VkShaderModule shader_modules[PIPELINE_COUNT * 2];
  VkPipeline pipelines[PIPELINE_COUNT];
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipelineLayout pipeline_layout;
  VkPipelineCache pipeline_cache;

  graphene_matrix_t mat_head_pose;
  graphene_matrix_t mat_eye_pos[2];
  graphene_matrix_t mat_projection[2];

  GulkanFrameBuffer *framebuffer[2];

  uint32_t render_width;
  uint32_t render_height;
};

XrdSceneClient *xrd_scene_client_new (void);

bool xrd_scene_client_initialize (XrdSceneClient *self);

void xrd_scene_client_render (XrdSceneClient *self);

G_END_DECLS

#endif /* XRD_SCENE_CLIENT_H_ */
