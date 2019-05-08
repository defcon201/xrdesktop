/*
 * xrddesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_RENDERER_H_
#define XRD_SCENE_RENDERER_H_

#include <glib-object.h>

#include <gulkan-client.h>

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_RENDERER xrd_scene_renderer_get_type()
G_DECLARE_FINAL_TYPE (XrdSceneRenderer, xrd_scene_renderer,
                      XRD, SCENE_RENDERER, GulkanClient)

struct _XrdSceneRenderer
{
  GulkanClient parent;
};

XrdSceneRenderer *xrd_scene_renderer_new (void);

XrdSceneRenderer *xrd_scene_renderer_instance (void);

G_END_DECLS

#endif /* XRD_SCENE_RENDERER_H_ */
