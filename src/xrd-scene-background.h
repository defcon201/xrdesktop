/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_BACKGROUND_H_
#define XRD_SCENE_BACKGROUND_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>

#include <openvr-glib.h>
#include <gulkan.h>

#include "xrd-scene-object.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_BACKGROUND xrd_scene_background_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneBackground, xrd_scene_background,
                      XRD, SCENE_BACKGROUND, XrdSceneObject)

struct _XrdSceneBackground
{
  XrdSceneObject parent;
  GulkanVertexBuffer *vertex_buffer;
};

XrdSceneBackground *xrd_scene_background_new (void);

gboolean
xrd_scene_background_initialize (XrdSceneBackground    *self,
                                 GulkanDevice          *device,
                                 VkDescriptorSetLayout *layout);

void
xrd_scene_background_render (XrdSceneBackground *self,
                             EVREye              eye,
                             VkPipeline          pipeline,
                             VkPipelineLayout    pipeline_layout,
                             VkCommandBuffer     cmd_buffer,
                             graphene_matrix_t  *vp);

G_END_DECLS

#endif /* XRD_SCENE_BACKGROUND_H_ */
