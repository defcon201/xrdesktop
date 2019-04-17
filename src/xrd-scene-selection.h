/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_SELECTION_H_
#define XRD_SCENE_SELECTION_H_

#include <glib-object.h>

#include <gulkan-vertex-buffer.h>

#include "openvr-context.h"
#include <gulkan-uniform-buffer.h>

#include "xrd-scene-object.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_SELECTION xrd_scene_selection_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneSelection, xrd_scene_selection,
                      XRD, SCENE_SELECTION, XrdSceneObject)

struct _XrdSceneSelection
{
  XrdSceneObject parent;
  GulkanVertexBuffer *vertex_buffer;
};

XrdSceneSelection *xrd_scene_selection_new (void);

gboolean
xrd_scene_selection_initialize (XrdSceneSelection     *self,
                                GulkanDevice          *device,
                                VkDescriptorSetLayout *layout);

void
xrd_scene_selection_render (XrdSceneSelection *self,
                            EVREye             eye,
                            VkPipeline         pipeline,
                            VkPipelineLayout   pipeline_layout,
                            VkCommandBuffer    cmd_buffer,
                            graphene_matrix_t *vp);

void
xrd_scene_selection_set_aspect_ratio (XrdSceneSelection *self,
                                      float              aspect_ratio);

G_END_DECLS

#endif /* XRD_SCENE_SELECTION_H_ */
