/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-device.h"
#include "openvr-context.h"
#include <gulkan-descriptor-set.h>

G_DEFINE_TYPE (XrdSceneDevice, xrd_scene_device, XRD_TYPE_SCENE_OBJECT)

static void
xrd_scene_device_finalize (GObject *gobject);

static void
xrd_scene_device_class_init (XrdSceneDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_device_finalize;
}

static void
xrd_scene_device_init (XrdSceneDevice *self)
{
  self->model = NULL;
  self->pose_valid = FALSE;
  self->is_controller = FALSE;
}

XrdSceneDevice *
xrd_scene_device_new (void)
{
  return (XrdSceneDevice*) g_object_new (XRD_TYPE_SCENE_DEVICE, 0);
}

static void
xrd_scene_device_finalize (GObject *gobject)
{
  XrdSceneDevice *self = XRD_SCENE_DEVICE (gobject);
  g_object_unref (self->model);
  G_OBJECT_CLASS (xrd_scene_device_parent_class)->finalize (gobject);
}

gboolean
xrd_scene_device_initialize (XrdSceneDevice        *self,
                             XrdSceneModel     *model,
                             GulkanDevice          *device,
                             VkDescriptorSetLayout *layout)
{
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);

  if (!xrd_scene_object_initialize (obj, device, layout))
    return FALSE;

  self->model = model;
  g_object_ref (self->model);

  xrd_scene_object_update_descriptors_texture (
    obj, self->model->sampler,
    self->model->texture->image_view);

  return TRUE;
}

void
xrd_scene_device_draw (XrdSceneDevice    *self,
                       EVREye             eye,
                       VkCommandBuffer    cmd_buffer,
                       VkPipelineLayout   pipeline_layout,
                       graphene_matrix_t *vp)
{
  if (!self->pose_valid)
    return;

  OpenVRContext *context = openvr_context_get_instance ();
  if (!context->system->IsInputAvailable () && self->is_controller)
    return;

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  xrd_scene_object_update_mvp_matrix (obj, eye, vp);
  xrd_scene_object_bind (obj, eye, cmd_buffer, pipeline_layout);
  gulkan_vertex_buffer_draw_indexed (self->model->vbo, cmd_buffer);
}

