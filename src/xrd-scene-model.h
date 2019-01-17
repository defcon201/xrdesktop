/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_MODEL_H_
#define XRD_SCENE_MODEL_H_

#include <glib-object.h>

#include <gulkan-texture.h>
#include "gulkan-vertex-buffer.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_MODEL xrd_scene_model_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneModel, xrd_scene_model,
                      XRD, SCENE_MODEL, GObject)

struct _XrdSceneModel
{
  GObject parent;

  GulkanDevice *device;

  GulkanTexture *texture;
  GulkanVertexBuffer *vbo;
  VkSampler sampler;
};

XrdSceneModel *xrd_scene_model_new (void);

gboolean
xrd_scene_model_load (XrdSceneModel *self,
                          GulkanDevice             *device,
                          VkCommandBuffer           cmd_buffer,
                          const char               *model_name);

G_END_DECLS

#endif /* XRD_SCENE_MODEL_H_ */
