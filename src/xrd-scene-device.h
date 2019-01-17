/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_DEVICE_H_
#define XRD_SCENE_DEVICE_H_

#include <glib-object.h>


#include <graphene.h>
#include <openvr_capi.h>

#include <gulkan-uniform-buffer.h>

#include "xrd-scene-model.h"

#include "xrd-scene-object.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_DEVICE xrd_scene_device_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneDevice, xrd_scene_device,
                      XRD, SCENE_DEVICE, XrdSceneObject)

struct _XrdSceneDevice
{
  XrdSceneObject parent;

  XrdSceneModel *model;

  gboolean pose_valid;
  gboolean is_controller;
};

XrdSceneDevice *xrd_scene_device_new (void);

gboolean
xrd_scene_device_initialize (XrdSceneDevice        *self,
                             XrdSceneModel     *model,
                             GulkanDevice          *device,
                             VkDescriptorSetLayout *layout);

void
xrd_scene_device_draw (XrdSceneDevice    *self,
                       EVREye             eye,
                       VkCommandBuffer    cmd_buffer,
                       VkPipelineLayout   pipeline_layout,
                       graphene_matrix_t *mvp);

G_END_DECLS

#endif /* XRD_SCENE_DEVICE_H_ */
