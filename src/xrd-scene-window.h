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
#include "xrd-window.h"

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
  float aspect_ratio;

  XrdWindowData window_data;
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

void
xrd_scene_window_get_normal (XrdSceneWindow  *self,
                             graphene_vec3_t *normal);

void
xrd_scene_window_get_plane (XrdSceneWindow   *self,
                            graphene_plane_t *res);

/* XrdWindow Interface functions */

gboolean
xrd_scene_window_set_transformation (XrdSceneWindow    *self,
                                     graphene_matrix_t *mat);

gboolean
xrd_scene_window_get_transformation (XrdSceneWindow    *self,
                                     graphene_matrix_t *mat);

void
xrd_scene_window_submit_texture (XrdSceneWindow *self,
                                 GulkanClient   *client,
                                 GulkanTexture  *texture);

void
xrd_scene_window_poll_event (XrdSceneWindow *self);

gboolean
xrd_scene_window_intersects (XrdSceneWindow     *self,
                             graphene_matrix_t  *pointer_transformation_matrix,
                             graphene_point3d_t *intersection_point);

gboolean
xrd_scene_window_intersection_to_pixels (XrdSceneWindow     *self,
                                         graphene_point3d_t *intersection_point,
                                         XrdPixelSize       *size_pixels,
                                         graphene_point_t   *window_coords);

gboolean
xrd_scene_window_intersection_to_2d_offset_meter (XrdSceneWindow     *self,
                                                  graphene_point3d_t *intersection_point,
                                                  graphene_point_t   *offset_center);

void
xrd_scene_window_add_child (XrdSceneWindow   *self,
                            XrdSceneWindow   *child,
                            graphene_point_t *offset_center);

void
xrd_scene_window_set_color (XrdSceneWindow  *self,
                            graphene_vec3_t *color);

void
xrd_scene_window_set_flip_y (XrdSceneWindow *self,
                             gboolean        flip_y);

G_END_DECLS

#endif /* XRD_GLIB_SCENE_WINDOW_H_ */
