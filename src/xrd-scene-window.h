/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_SCENE_WINDOW_H_
#define XRD_GLIB_SCENE_WINDOW_H_

#include <glib-object.h>

#include "openvr-context.h"

#include <gulkan-vertex-buffer.h>
#include <gulkan-texture.h>
#include <gulkan-uniform-buffer.h>

#include "xrd-scene-object.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_WINDOW xrd_scene_window_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneWindow, xrd_scene_window,
                      XRD, SCENE_WINDOW, XrdSceneObject)

struct _XrdSceneWindow
{
  XrdSceneObject parent;

  GulkanVertexBuffer *vertex_buffer;
  GulkanTexture *texture;
  VkSampler sampler;
};

XrdSceneWindow *xrd_scene_window_new (void);

bool
xrd_scene_window_init_texture (XrdSceneWindow *self,
                               GulkanDevice   *device,
                               VkCommandBuffer cmd_buffer,
                               GdkPixbuf      *pixbuf);

gboolean
xrd_scene_window_initialize (XrdSceneWindow        *self,
                             GulkanDevice          *device,
                             VkDescriptorSetLayout *layout);


void
xrd_scene_window_draw (XrdSceneWindow    *self,
                       EVREye             eye,
                       VkPipeline         pipeline,
                       VkPipelineLayout   pipeline_layout,
                       VkCommandBuffer    cmd_buffer,
                       graphene_matrix_t *vp);

G_END_DECLS

#endif /* XRD_GLIB_SCENE_WINDOW_H_ */
