/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_CLIENT_H_
#define XRD_SCENE_CLIENT_H_

#include "xrd-client.h"

#include "openvr-context.h"
#include <glib-object.h>

#include <openvr-action-set.h>

#include <gulkan-client.h>
#include <gulkan-device.h>
#include <gulkan-instance.h>
#include <gulkan-texture.h>
#include <gulkan-frame-buffer.h>
#include <gulkan-vertex-buffer.h>
#include <gulkan-uniform-buffer.h>


#include "xrd-scene-device.h"
#include "xrd-scene-device-manager.h"
#include "xrd-scene-window.h"
#include "xrd-scene-pointer.h"
#include "xrd-scene-selection.h"
#include "xrd-scene-vector.h"
#include "xrd-scene-background.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_CLIENT xrd_scene_client_get_type ()
G_DECLARE_FINAL_TYPE (XrdSceneClient, xrd_scene_client,
                      XRD, SCENE_CLIENT, XrdClient)

XrdSceneClient *xrd_scene_client_new (void);

bool xrd_scene_client_initialize (XrdSceneClient *self);

void xrd_scene_client_render (XrdSceneClient *self);

void
xrd_scene_client_add_scene_window (XrdSceneClient *self,
                                   XrdSceneWindow *window);

/* Inheritance overwrites from XrdClient */

XrdOverlayWindow *
xrd_scene_client_add_window (XrdSceneClient *self,
                             const char     *title,
                             gpointer        native,
                             float           ppm,
                             gboolean        is_child,
                             gboolean        follow_head);

gboolean
xrd_scene_client_add_button (XrdSceneClient     *self,
                             XrdWindow         **button,
                             gchar              *label,
                             graphene_point3d_t *position,
                             GCallback           press_callback,
                             gpointer            press_callback_data);

GulkanClient *
xrd_scene_client_get_uploader (XrdSceneClient *self);

VkDescriptorSetLayout*
xrd_scene_client_get_descriptor_set_layout (XrdSceneClient *self);
G_END_DECLS

#endif /* XRD_SCENE_CLIENT_H_ */
