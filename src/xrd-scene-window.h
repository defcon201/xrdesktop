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

  gboolean flip_y;
  graphene_vec3_t color;

  XrdWindowData window_data;
};

XrdSceneWindow *xrd_scene_window_new (const gchar *title);

XrdSceneWindow *
xrd_scene_window_new_from_meters (const gchar *title,
                                  float        width_meters,
                                  float        height_meters);

XrdSceneWindow *
xrd_scene_window_new_from_ppm (const gchar *title,
                               uint32_t     width_pixels,
                               uint32_t     height_pixels,
                               float        ppm);

XrdSceneWindow *
xrd_scene_window_new_from_native (const gchar *title,
                                  gpointer     native,
                                  uint32_t     width_pixels,
                                  uint32_t     height_pixels,
                                  float        ppm);

bool
xrd_scene_window_init_texture (XrdSceneWindow *self,
                               GdkPixbuf      *pixbuf);

gboolean
xrd_scene_window_initialize (XrdSceneWindow *self);

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

void
xrd_scene_window_set_width_meters (XrdSceneWindow *self,
                                   float           width_meters);


G_END_DECLS

#endif /* XRD_GLIB_SCENE_WINDOW_H_ */
