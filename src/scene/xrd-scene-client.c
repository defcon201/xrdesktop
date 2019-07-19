/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "xrd-scene-client.h"

#include <gmodule.h>

#include <gulkan.h>
#include <openvr-glib.h>

#include <signal.h>

#include "graphene-ext.h"

#include "xrd-scene-pointer-tip.h"
#include "xrd-scene-renderer.h"
#include "xrd-scene-desktop-cursor.h"

#define DEBUG_GEOMETRY 0

struct _XrdSceneClient
{
  XrdClient parent;

  XrdSceneDeviceManager *device_manager;

  graphene_matrix_t mat_head_pose;
  graphene_matrix_t mat_eye_pos[2];
  graphene_matrix_t mat_projection[2];

  float near;
  float far;

#if DEBUG_GEOMETRY
  XrdSceneVector *debug_vectors[4];
#endif

  XrdSceneBackground *background;
};

G_DEFINE_TYPE (XrdSceneClient, xrd_scene_client, XRD_TYPE_CLIENT)

static void xrd_scene_client_finalize (GObject *gobject);

void _init_device_model (XrdSceneClient      *self,
                         TrackedDeviceIndex_t device_id);
void _init_device_models (XrdSceneClient *self);


graphene_matrix_t _get_hmd_pose_matrix (EVREye eye);
graphene_matrix_t _get_view_projection_matrix (XrdSceneClient *self,
                                               EVREye eye);

graphene_matrix_t
_get_view_matrix (XrdSceneClient *self, EVREye eye);

void _update_matrices (XrdSceneClient *self);
void _update_device_poses (XrdSceneClient *self);
void _render_stereo (XrdSceneClient *self, VkCommandBuffer cmd_buffer);

static void
xrd_scene_client_init (XrdSceneClient *self)
{
  xrd_client_set_upload_layout (XRD_CLIENT (self),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  self->device_manager = xrd_scene_device_manager_new ();

  self->background = xrd_scene_background_new ();

  self->near = 0.1f;
  self->far = 30.0f;

#if DEBUG_GEOMETRY
  for (uint32_t i = 0; i < G_N_ELEMENTS (self->debug_vectors); i++)
    self->debug_vectors[i] = xrd_scene_vector_new ();
#endif
}

XrdSceneClient *
xrd_scene_client_new (void)
{
  return (XrdSceneClient *)g_object_new (XRD_TYPE_SCENE_CLIENT, 0);
}

static void
xrd_scene_client_finalize (GObject *gobject)
{
  XrdSceneClient *self = XRD_SCENE_CLIENT (gobject);

  g_object_unref (self->device_manager);

  g_object_unref (self->background);

#if DEBUG_GEOMETRY
  for (uint32_t i = 0; i < G_N_ELEMENTS (self->debug_vectors); i++)
    g_object_unref (self->debug_vectors[i]);
#endif

  G_OBJECT_CLASS (xrd_scene_client_parent_class)->finalize (gobject);

  xrd_scene_renderer_destroy_instance ();
}

static bool
_init_openvr ()
{
  OpenVRContext *context = openvr_context_get_instance ();
  if (!openvr_context_initialize (context, OPENVR_APP_SCENE))
    {
      g_printerr ("Could not init OpenVR.\n");
      return false;
    }
  return true;
}

static void
_device_activate_cb (OpenVRContext          *context,
                     OpenVRDeviceIndexEvent *event,
                     gpointer               _self)
{
  (void) context;
  XrdSceneClient *self = (XrdSceneClient*) _self;
  g_print ("Device %lu activated, initializing model.\n",
           event->controller_handle);
  _init_device_model (self, (TrackedDeviceIndex_t) event->controller_handle);
}

static void
_device_deactivate_cb (OpenVRContext          *context,
                       OpenVRDeviceIndexEvent *event,
                       gpointer               _self)
{
  (void) context;
  XrdSceneClient *self = (XrdSceneClient*) _self;
  g_print ("Device %lu deactivated. Removing scene device.\n",
           event->controller_handle);
  xrd_scene_device_manager_remove (self->device_manager,
                                   (TrackedDeviceIndex_t) event->controller_handle);
  /* TODO: Remove pointer in client */
  // g_hash_table_remove (self->pointers, &event->index);
}

static void
_render_pointers (XrdSceneClient    *self,
                  EVREye             eye,
                  VkCommandBuffer    cmd_buffer,
                  VkPipeline        *pipelines,
                  VkPipelineLayout   pipeline_layout,
                  graphene_matrix_t *vp)
{
  OpenVRContext *context = openvr_context_get_instance ();
  if (!context->system->IsInputAvailable ())
    return;

  GList *controllers =
    g_hash_table_get_values (xrd_client_get_controllers (XRD_CLIENT (self)));
  for (GList *l = controllers; l; l = l->next)
    {
      XrdController *controller = XRD_CONTROLLER (l->data);

      XrdScenePointer *pointer =
        XRD_SCENE_POINTER (xrd_controller_get_pointer (controller));
      xrd_scene_pointer_render (pointer, eye,
                                pipelines[PIPELINE_POINTER],
                                pipelines[PIPELINE_SELECTION],
                                pipeline_layout, cmd_buffer, vp);
    }
  g_list_free (controllers);
}

/*
 * Since we are using world space positons for the lights, this only needs
 * to be run once for both eyes
 */
static void
_update_lights_cb (gpointer _self)
{
  XrdSceneClient *self = XRD_SCENE_CLIENT (_self);

  GList *controllers =
    g_hash_table_get_values (xrd_client_get_controllers (XRD_CLIENT (self)));

  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  xrd_scene_renderer_update_lights (renderer, controllers);

  g_list_free (controllers);
}

static void
_render_eye_cb (uint32_t         eye,
                VkCommandBuffer  cmd_buffer,
                VkPipelineLayout pipeline_layout,
                VkPipeline      *pipelines,
                gpointer         _self)
{
  XrdSceneClient *self = XRD_SCENE_CLIENT (_self);

  graphene_matrix_t vp = _get_view_projection_matrix (self, eye);
  graphene_matrix_t view = _get_view_matrix (self, eye);

  XrdWindowManager *manager = xrd_client_get_manager (XRD_CLIENT (self));

  xrd_scene_background_render (self->background, eye,
                               pipelines[PIPELINE_BACKGROUND],
                               pipeline_layout, cmd_buffer, &vp);

  for (GSList *l = xrd_window_manager_get_windows (manager);
       l != NULL; l = l->next)
    {
      xrd_scene_window_draw_phong (XRD_SCENE_WINDOW (l->data), eye,
                                   pipelines[PIPELINE_WINDOWS],
                                   pipeline_layout,
                                   cmd_buffer, &view,
                                  &self->mat_projection[eye]);
    }

  for (GSList *l = xrd_window_manager_get_buttons (manager);
       l != NULL; l = l->next)
    {
      xrd_scene_window_draw_phong (XRD_SCENE_WINDOW (l->data), eye,
                                   pipelines[PIPELINE_WINDOWS],
                                   pipeline_layout,
                                   cmd_buffer, &view,
                                   &self->mat_projection[eye]);
    }

  _render_pointers (self, eye, cmd_buffer, pipelines, pipeline_layout, &vp);

  xrd_scene_device_manager_render (self->device_manager, eye, cmd_buffer,
                                   pipelines[PIPELINE_DEVICE_MODELS],
                                   pipeline_layout, &vp);

  GList *controllers =
    g_hash_table_get_values (xrd_client_get_controllers (XRD_CLIENT (self)));
  for (GList *l = controllers; l; l = l->next)
    {
      XrdController *controller = XRD_CONTROLLER (l->data);
      XrdScenePointerTip *scene_tip =
        XRD_SCENE_POINTER_TIP (xrd_controller_get_pointer_tip (controller));
      xrd_scene_window_draw (XRD_SCENE_WINDOW (scene_tip), eye,
                             pipelines[PIPELINE_TIP],
                             pipeline_layout,
                             cmd_buffer, &vp);
    }
  g_list_free (controllers);

  XrdDesktopCursor *cursor = xrd_client_get_desktop_cursor (XRD_CLIENT (self));
  XrdSceneDesktopCursor *scene_cursor = XRD_SCENE_DESKTOP_CURSOR (cursor);
  xrd_scene_window_draw (XRD_SCENE_WINDOW (scene_cursor), eye,
                         pipelines[PIPELINE_TIP],
                         pipeline_layout,
                         cmd_buffer, &vp);

#if DEBUG_GEOMETRY
  for (uint32_t i = 0; i < G_N_ELEMENTS (self->debug_vectors); i++)
    xrd_scene_vector_render (self->debug_vectors[i], eye,
                             pipelines[PIPELINE_POINTER],
                             pipeline_layout,
                             cmd_buffer,
                            &vp);
#endif
}

static bool
_init_vulkan (XrdSceneClient *self)
{
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();

  if (!xrd_scene_renderer_init_vulkan_openvr (renderer))
    return false;

  _update_matrices (self);
  _init_device_models (self);

  GulkanDevice *device = gulkan_client_get_device (GULKAN_CLIENT (renderer));

  VkDescriptorSetLayout *descriptor_set_layout =
    xrd_scene_renderer_get_descriptor_set_layout (renderer);

  xrd_scene_background_initialize (self->background,
                                   device,
                                   descriptor_set_layout);

#if DEBUG_GEOMETRY
  for (uint32_t i = 0; i < G_N_ELEMENTS (self->debug_vectors); i++)
    xrd_scene_vector_initialize (self->debug_vectors[i], device,
                                 descriptor_set_layout);
#endif

  XrdDesktopCursor *cursor =
    XRD_DESKTOP_CURSOR (xrd_scene_desktop_cursor_new ());
  xrd_client_set_desktop_cursor (XRD_CLIENT (self), cursor);

  vkQueueWaitIdle (gulkan_device_get_queue_handle (device));

  xrd_scene_renderer_set_render_cb (renderer, _render_eye_cb, self);
  xrd_scene_renderer_set_update_lights_cb (renderer, _update_lights_cb, self);

  return true;
}

static void
_init_controller (XrdClient     *client,
                  XrdController *controller)
{
  (void) client;

  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  GulkanDevice *device = gulkan_client_get_device (GULKAN_CLIENT (renderer));
  VkDescriptorSetLayout *descriptor_set_layout =
    xrd_scene_renderer_get_descriptor_set_layout (renderer);

  XrdScenePointer *pointer = xrd_scene_pointer_new ();
  xrd_scene_pointer_initialize (pointer, device,
                                descriptor_set_layout);
  xrd_controller_set_pointer (controller, XRD_POINTER (pointer));

  XrdScenePointerTip *pointer_tip = xrd_scene_pointer_tip_new ();
  xrd_controller_set_pointer_tip (controller, XRD_POINTER_TIP (pointer_tip));
}

bool
xrd_scene_client_initialize (XrdSceneClient *self)
{
  if (!_init_openvr ())
    {
      g_printerr ("Could not init OpenVR.\n");
      return false;
    }

  if (!_init_vulkan (self))
    {
      g_print ("Could not init Vulkan.\n");
      return false;
    }

  OpenVRContext *context = openvr_context_get_instance ();
  g_signal_connect (context, "device-activate-event",
                    (GCallback) _device_activate_cb, self);
  g_signal_connect (context, "device-deactivate-event",
                    (GCallback) _device_deactivate_cb, self);

  xrd_client_post_openvr_init (XRD_CLIENT (self));

  return true;
}

void
_init_device_model (XrdSceneClient      *self,
                    TrackedDeviceIndex_t device_id)
{
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  VkDescriptorSetLayout *descriptor_set_layout =
    xrd_scene_renderer_get_descriptor_set_layout (renderer);

  xrd_scene_device_manager_add (self->device_manager, GULKAN_CLIENT (renderer),
                                device_id, descriptor_set_layout);
}

void
_init_device_models (XrdSceneClient *self)
{
  for (TrackedDeviceIndex_t i = k_unTrackedDeviceIndex_Hmd + 1;
       i < k_unMaxTrackedDeviceCount; i++)
    {
      OpenVRContext *context = openvr_context_get_instance ();
      if (!context->system->IsTrackedDeviceConnected (i))
        continue;

      _init_device_model (self, i);
    }
}

void
xrd_scene_client_render (XrdSceneClient *self)
{
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  xrd_scene_renderer_draw (renderer);

  xrd_scene_device_manager_update_poses (self->device_manager,
                                        &self->mat_head_pose);
}

void
_update_matrices (XrdSceneClient *self)
{
  for (uint32_t eye = 0; eye < 2; eye++)
    {
      self->mat_projection[eye] =
        openvr_system_get_projection_matrix (eye, self->near, self->far);
      self->mat_eye_pos[eye] = _get_hmd_pose_matrix (eye);
    }
}

graphene_matrix_t
_get_hmd_pose_matrix (EVREye eye)
{
  graphene_matrix_t mat = openvr_system_get_eye_to_head_transform (eye);
  graphene_matrix_inverse (&mat, &mat);
  return mat;
}

graphene_matrix_t
_get_view_projection_matrix (XrdSceneClient *self, EVREye eye)
{
  graphene_matrix_t mat;
  graphene_matrix_init_from_matrix (&mat, &self->mat_head_pose);
  graphene_matrix_multiply (&mat, &self->mat_eye_pos[eye], &mat);
  graphene_matrix_multiply (&mat, &self->mat_projection[eye], &mat);
  return mat;
}

graphene_matrix_t
_get_view_matrix (XrdSceneClient *self, EVREye eye)
{
  graphene_matrix_t mat;
  graphene_matrix_init_from_matrix (&mat, &self->mat_head_pose);
  graphene_matrix_multiply (&mat, &self->mat_eye_pos[eye], &mat);
  return mat;
}

static GulkanClient *
_get_uploader (XrdClient *client)
{
  (void) client;
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  return GULKAN_CLIENT (renderer);
}

VkDescriptorSetLayout*
xrd_scene_client_get_descriptor_set_layout ()
{
  XrdSceneRenderer *renderer = xrd_scene_renderer_get_instance ();
  VkDescriptorSetLayout *descriptor_set_layout =
    xrd_scene_renderer_get_descriptor_set_layout (renderer);
  return descriptor_set_layout;
}

static void
xrd_scene_client_class_init (XrdSceneClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_client_finalize;

  XrdClientClass *xrd_client_class = XRD_CLIENT_CLASS (klass);
  xrd_client_class->get_uploader = _get_uploader;
  xrd_client_class->init_controller = _init_controller;
}
