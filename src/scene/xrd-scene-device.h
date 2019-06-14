/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_DEVICE_H_
#define XRD_SCENE_DEVICE_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>


#include <graphene.h>

#include <gulkan.h>

#include "xrd-scene-model.h"

#include "xrd-scene-object.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_DEVICE xrd_scene_device_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneDevice, xrd_scene_device,
                      XRD, SCENE_DEVICE, XrdSceneObject)

XrdSceneDevice *xrd_scene_device_new (void);

gboolean
xrd_scene_device_initialize (XrdSceneDevice        *self,
                             XrdSceneModel         *model,
                             VkDescriptorSetLayout *layout);

void
xrd_scene_device_draw (XrdSceneDevice    *self,
                       EVREye             eye,
                       VkCommandBuffer    cmd_buffer,
                       VkPipelineLayout   pipeline_layout,
                       graphene_matrix_t *mvp);

void
xrd_scene_device_set_is_controller (XrdSceneDevice *self, bool is_controller);

void
xrd_scene_device_set_is_pose_valid (XrdSceneDevice *self, bool valid);

G_END_DECLS

#endif /* XRD_SCENE_DEVICE_H_ */
