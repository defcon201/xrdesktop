/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_SCENE_POINTER_H_
#define XRD_GLIB_SCENE_POINTER_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>

#include <gulkan.h>
#include <openvr-glib.h>

#include "xrd-scene-object.h"
#include "xrd-scene-window.h"
#include "xrd-scene-selection.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_POINTER xrd_scene_pointer_get_type()
G_DECLARE_FINAL_TYPE (XrdScenePointer, xrd_scene_pointer,
                      XRD, SCENE_POINTER, XrdSceneObject)

struct _XrdScenePointer
{
  XrdSceneObject parent;
  GulkanVertexBuffer *vertex_buffer;
  float start_offset;
  float length;
  float default_length;

  XrdSceneSelection *selection;
};

XrdScenePointer *xrd_scene_pointer_new (void);

gboolean
xrd_scene_pointer_initialize (XrdScenePointer       *self,
                              GulkanDevice          *device,
                              VkDescriptorSetLayout *layout);

void
xrd_scene_pointer_render (XrdScenePointer   *self,
                          EVREye             eye,
                          VkPipeline         pipeline,
                          VkPipelineLayout   pipeline_layout,
                          VkCommandBuffer    cmd_buffer,
                          graphene_matrix_t *vp);

void
xrd_scene_pointer_reset_length (XrdScenePointer *self);

void
xrd_scene_pointer_get_ray (XrdScenePointer *self,
                           graphene_ray_t  *res);

gboolean
xrd_scene_pointer_get_intersection (XrdScenePointer *pointer,
                                    XrdSceneWindow  *window,
                                    float           *distance,
                                    graphene_vec3_t *res);

G_END_DECLS

#endif /* XRD_GLIB_SCENE_POINTER_H_ */
