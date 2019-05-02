/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "xrd-scene-client.h"

#include <gmodule.h>
#include <gulkan-texture.h>
#include <gulkan-renderer.h>
#include "gulkan-geometry.h"

#include "openvr-compositor.h"
#include "openvr-system.h"
#include "openvr-math.h"
#include "openvr-io.h"

#include <signal.h>

#include "graphene-ext.h"

static bool use_validation = true;

#define DEBUG_GEOMETRY 0

// Pipeline state objects
enum PipelineType
{
  PIPELINE_WINDOWS = 0,
  PIPELINE_POINTER,
  PIPELINE_DEVICE_MODELS,
  PIPELINE_COUNT
};

typedef struct VertexDataScene
{
  graphene_point3d_t position;
  graphene_point_t   uv;
} VertexDataScene;

struct _XrdSceneClient
{
  XrdClient parent;

  VkSampleCountFlagBits msaa_sample_count;
  float super_sample_scale;

  XrdSceneDeviceManager *device_manager;

  float near_clip;
  float far_clip;

  VkShaderModule shader_modules[PIPELINE_COUNT * 2];
  VkPipeline pipelines[PIPELINE_COUNT];
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipelineLayout pipeline_layout;
  VkPipelineCache pipeline_cache;

  graphene_matrix_t mat_head_pose;
  graphene_matrix_t mat_eye_pos[2];
  graphene_matrix_t mat_projection[2];

#if DEBUG_GEOMETRY
  XrdSceneVector *debug_vectors[4];
#endif

  GulkanFrameBuffer *framebuffer[2];

  uint32_t render_width;
  uint32_t render_height;

  XrdClientController controllers[2];

  GSList *windows;

  GHashTable *pointers; // int -> XrdScenePointer

  XrdSceneBackground *background;

  GulkanClient *gulkan_client;
};

G_DEFINE_TYPE (XrdSceneClient, xrd_scene_client, XRD_TYPE_CLIENT)

static void xrd_scene_client_finalize (GObject *gobject);

bool _init_vulkan (XrdSceneClient *self);
bool _init_vulkan_instance (XrdSceneClient *self);
bool _init_vulkan_device (XrdSceneClient *self);
void _init_device_model (XrdSceneClient *self,
                         TrackedDeviceIndex_t device_id);
void _init_device_models (XrdSceneClient *self);
bool _init_framebuffers (XrdSceneClient *self, VkCommandBuffer cmd_buffer);
bool _init_shaders (XrdSceneClient *self);
bool _init_graphics_pipelines (XrdSceneClient *self);
bool _init_pipeline_cache (XrdSceneClient *self);
bool _init_pipeline_layout (XrdSceneClient *self);
bool _init_descriptor_layout (XrdSceneClient *self);

graphene_matrix_t _get_hmd_pose_matrix (EVREye eye);
graphene_matrix_t _get_view_projection_matrix (XrdSceneClient *self,
                                               EVREye eye);

void _update_matrices (XrdSceneClient *self);
void _update_device_poses (XrdSceneClient *self);
void _render_stereo (XrdSceneClient *self, VkCommandBuffer cmd_buffer);

static void
_action_hand_pose_cb (OpenVRAction        *action,
                      OpenVRPoseEvent     *event,
                      XrdClientController *controller);

static void
xrd_scene_client_class_init (XrdSceneClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_client_finalize;

  XrdClientClass *xrd_client_class = XRD_CLIENT_CLASS (klass);
  xrd_client_class->add_window =
      (void*) xrd_scene_client_add_window;
  xrd_client_class->add_button =
      (void*) xrd_scene_client_add_button;
  xrd_client_class->get_uploader =
      (void*) xrd_scene_client_get_uploader;
}

void
_insert_at_key2 (GHashTable *table, uint32_t key, gpointer value)
{
  gint *keyp = g_new0 (gint, 1);
  *keyp = (gint) key;
  g_hash_table_insert (table, keyp, value);
}

static void
xrd_scene_client_init (XrdSceneClient *self)
{
  self->gulkan_client = gulkan_client_new ();
  self->msaa_sample_count = VK_SAMPLE_COUNT_4_BIT;
  self->super_sample_scale = 1.0f;

  self->descriptor_set_layout = VK_NULL_HANDLE;
  self->pipeline_layout = VK_NULL_HANDLE;
  self->pipeline_cache = VK_NULL_HANDLE;

  self->near_clip = 0.1f;
  self->far_clip = 30.0f;

  for (uint32_t eye = 0; eye < 2; eye++)
    self->framebuffer[eye] = gulkan_frame_buffer_new();

  self->device_manager = xrd_scene_device_manager_new ();

  self->background = xrd_scene_background_new ();

#if DEBUG_GEOMETRY
  for (uint32_t i = 0; i < G_N_ELEMENTS (self->debug_vectors); i++)
    self->debug_vectors[i] = xrd_scene_vector_new ();
#endif

  self->windows = NULL;

  self->pointers = g_hash_table_new_full (g_int_hash, g_int_equal,
                                          g_free, g_object_unref);

  for (uint32_t i = 0; i < 2; i++)
    {
      self->controllers[i].self = XRD_CLIENT (self);
      self->controllers[i].index = i;
    }
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

  VkDevice device = self->gulkan_client->device->device;

  if (device != VK_NULL_HANDLE)
    vkDeviceWaitIdle (device);

  OpenVRContext *context = openvr_context_get_instance ();
  g_object_unref (context);

  g_object_unref (self->device_manager);
  g_hash_table_unref (self->pointers);

  g_slist_free_full (self->windows, g_object_unref);

  g_object_unref (self->background);

#if DEBUG_GEOMETRY
  for (uint32_t i = 0; i < G_N_ELEMENTS (self->debug_vectors); i++)
    g_object_unref (self->debug_vectors[i]);
#endif

  if (device != VK_NULL_HANDLE)
    {
      for (uint32_t eye = 0; eye < 2; eye++)
        g_object_unref (self->framebuffer[eye]);

      vkDestroyPipelineLayout (device, self->pipeline_layout, NULL);
      vkDestroyDescriptorSetLayout (device, self->descriptor_set_layout, NULL);
      for (uint32_t i = 0; i < PIPELINE_COUNT; i++)
        vkDestroyPipeline (device, self->pipelines[i], NULL);

      for (uint32_t i = 0; i < G_N_ELEMENTS (self->shader_modules); i++)
        vkDestroyShaderModule (device, self->shader_modules[i], NULL);

      vkDestroyPipelineCache (device, self->pipeline_cache, NULL);
    }

  G_OBJECT_CLASS (xrd_scene_client_parent_class)->finalize (gobject);
}

bool
_init_openvr (XrdSceneClient *self)
{
  if (!openvr_context_is_installed ())
    {
      g_printerr ("VR Runtime not installed.\n");
      return false;
    }

  OpenVRContext *context = openvr_context_get_instance ();
  if (!openvr_context_init_scene (context))
    {
      g_printerr ("Could not init OpenVR.\n");
      return false;
    }

  if (!openvr_context_is_valid (context))
    {
      g_printerr ("Could not load OpenVR function pointers.\n");
      return false;
    }

  xrd_client_post_openvr_init (XRD_CLIENT (self));

  OpenVRActionSet *wm_actions = xrd_client_get_wm_actions (XRD_CLIENT (self));

  openvr_action_set_connect (wm_actions, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_left",
                             (GCallback) _action_hand_pose_cb,
                             &self->controllers[0]);
  openvr_action_set_connect (wm_actions, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_right",
                             (GCallback) _action_hand_pose_cb,
                             &self->controllers[1]);

  return true;
}

static void
_device_activate_cb (OpenVRContext          *context,
                     OpenVRDeviceIndexEvent *event,
                     gpointer               _self)
{
  (void) context;
  XrdSceneClient *self = (XrdSceneClient*) _self;
  g_print ("Device %d activated, initializing model.\n", event->index);
  _init_device_model (self, event->index);
}

static void
_device_deactivate_cb (OpenVRContext          *context,
                       OpenVRDeviceIndexEvent *event,
                       gpointer               _self)
{
  (void) context;
  XrdSceneClient *self = (XrdSceneClient*) _self;
  g_print ("Device %d deactivated. Removing scene device.\n", event->index);
  xrd_scene_device_manager_remove (self->device_manager, event->index);
  g_hash_table_remove (self->pointers, &event->index);
}

static void
_action_hand_pose_cb (OpenVRAction        *action,
                      OpenVRPoseEvent     *event,
                      XrdClientController *controller)
{
  (void) action;
  XrdSceneClient *self = XRD_SCENE_CLIENT (controller->self);

  XrdScenePointer *pointer = g_hash_table_lookup (self->pointers,
                                                  &controller->index);
  XrdSceneObject *obj = XRD_SCENE_OBJECT (pointer);
  graphene_matrix_init_from_matrix (&obj->model_matrix, &event->pose);
}

void
_render_pointers (XrdSceneClient    *self,
                  EVREye             eye,
                  VkCommandBuffer    cmd_buffer,
                  VkPipeline         pipeline,
                  VkPipelineLayout   pipeline_layout,
                  graphene_matrix_t *vp)
{
  OpenVRContext *context = openvr_context_get_instance ();
  if (!context->system->IsInputAvailable ())
    return;

  GList *pointers = g_hash_table_get_values (self->pointers);
  for (GList *l = pointers; l; l = l->next)
    xrd_scene_pointer_render (l->data, eye, pipeline,
                              pipeline_layout, cmd_buffer, vp);
}

gboolean
_poll_events_cb (gpointer _self)
{
  XrdSceneClient *self = _self;
  OpenVRContext *context = openvr_context_get_instance ();
  openvr_context_poll_event (context);

  OpenVRActionSet *wm_actions = xrd_client_get_wm_actions (XRD_CLIENT (self));
  if (!openvr_action_set_poll (wm_actions))
    return FALSE;

  return TRUE;
}

bool
xrd_scene_client_initialize (XrdSceneClient *self)
{
  if (!_init_openvr (self))
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

  g_timeout_add (20, _poll_events_cb, self);

  return true;
}

bool
_init_vulkan (XrdSceneClient *self)
{
  if (!_init_vulkan_instance (self))
    return false;

  if (!_init_vulkan_device (self))
    return false;

  if (!gulkan_client_init_command_pool (self->gulkan_client))
    {
      g_printerr ("Could not create command pool.\n");
      return false;
    }

  FencedCommandBuffer cmd_buffer;
  if (!gulkan_client_begin_res_cmd_buffer (self->gulkan_client, &cmd_buffer))
    {
      g_printerr ("Could not begin command buffer.\n");
      return false;
    }

  _update_matrices (self);
  _init_framebuffers (self, cmd_buffer.cmd_buffer);

  if (!_init_shaders (self))
    return false;

  if (!_init_descriptor_layout (self))
    return false;
  if (!_init_pipeline_layout (self))
    return false;
  if (!_init_pipeline_cache (self))
    return false;
  if (!_init_graphics_pipelines (self))
    return false;

  _init_device_models (self);

  xrd_scene_background_initialize (self->background,
                                   self->gulkan_client->device,
                                   &self->descriptor_set_layout);

#if DEBUG_GEOMETRY
  for (uint32_t i = 0; i < G_N_ELEMENTS (self->debug_vectors); i++)
    xrd_scene_vector_initialize (self->debug_vectors[i],
                                 client->device,
                                &self->descriptor_set_layout);
#endif

  if (!gulkan_client_submit_res_cmd_buffer (self->gulkan_client, &cmd_buffer))
    {
      g_printerr ("Could not submit command buffer.\n");
      return false;
    }

  for (int i = 0; i < 2; i++)
    {
      XrdScenePointer *pointer = xrd_scene_pointer_new ();
      xrd_scene_pointer_initialize (pointer, self->gulkan_client->device,
                                    &self->descriptor_set_layout);
      _insert_at_key2 (self->pointers, i, pointer);
    }

  vkQueueWaitIdle (self->gulkan_client->device->queue);

  return true;
}

bool
_init_vulkan_instance (XrdSceneClient *self)
{
  GSList *extensions = NULL;
  openvr_compositor_get_instance_extensions (&extensions);
  return gulkan_instance_create (self->gulkan_client->instance,
                                 use_validation, extensions);
}

bool
_init_vulkan_device (XrdSceneClient *self)
{
  /* Query OpenVR for a physical device */
  uint64_t physical_device = 0;
  OpenVRContext *context = openvr_context_get_instance ();
  context->system->GetOutputDevice (
      &physical_device, ETextureType_TextureType_Vulkan,
      (struct VkInstance_T *) self->gulkan_client->instance->instance);

  GSList *extensions = NULL;
  openvr_compositor_get_device_extensions ((VkPhysicalDevice)physical_device,
                                           &extensions);

  return gulkan_device_create (self->gulkan_client->device,
                               self->gulkan_client->instance,
                               (VkPhysicalDevice)physical_device, extensions);
}

void
_init_device_model (XrdSceneClient *self,
                    TrackedDeviceIndex_t device_id)
{
  xrd_scene_device_manager_add (self->device_manager, self->gulkan_client,
                                device_id,
                               &self->descriptor_set_layout);
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
xrd_scene_client_add_scene_window (XrdSceneClient *self,
                                   XrdSceneWindow *window)
{
  self->windows = g_slist_append (self->windows, window);
}

void
_test_intersection (XrdSceneClient *self)
{
  GList *pointers = g_hash_table_get_values (self->pointers);
  for (GList *l = pointers; l; l = l->next)
    {
      XrdScenePointer *pointer = l->data;
      if (pointer == NULL)
        continue;

      float lowest_distance = FLT_MAX;
      XrdSceneWindow *selected_window = NULL;

      for (GSList *l = self->windows; l != NULL; l = l->next)
        {
          XrdSceneWindow *window = (XrdSceneWindow *) l->data;

          graphene_vec3_t intersection;
          float distance;
          bool intersects = xrd_scene_pointer_get_intersection (pointer,
                                                                window,
                                                                &distance,
                                                                &intersection);
          if (intersects && distance < lowest_distance)
            {
              selected_window = window;
              lowest_distance = distance;
            }
        }

      XrdSceneObject *selection_obj = XRD_SCENE_OBJECT (pointer->selection);
      if (selected_window != NULL)
        {
          XrdSceneObject *window_obj = XRD_SCENE_OBJECT (selected_window);
          graphene_matrix_init_from_matrix (&selection_obj->model_matrix,
                                            &window_obj->model_matrix);
          xrd_scene_selection_set_aspect_ratio (pointer->selection,
                                                selected_window->aspect_ratio);
          selection_obj->visible = TRUE;
          xrd_scene_pointer_set_length (pointer, lowest_distance);
        }
      else
        {
          selection_obj->visible = FALSE;
          xrd_scene_pointer_reset_length (pointer);
        }
    }
}

void
xrd_scene_client_render (XrdSceneClient *self)
{
  _test_intersection (self);

  FencedCommandBuffer cmd_buffer;
  gulkan_client_begin_res_cmd_buffer (self->gulkan_client, &cmd_buffer);

  _render_stereo (self, cmd_buffer.cmd_buffer);

  vkEndCommandBuffer (cmd_buffer.cmd_buffer);

  VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd_buffer.cmd_buffer,
    .waitSemaphoreCount = 0,
    .pWaitSemaphores = NULL,
    .signalSemaphoreCount = 0
  };

  vkQueueSubmit (self->gulkan_client->device->queue, 1,
                &submit_info, cmd_buffer.fence);

  vkQueueWaitIdle (self->gulkan_client->device->queue);

  vkFreeCommandBuffers (self->gulkan_client->device->device,
                        self->gulkan_client->command_pool,
                        1, &cmd_buffer.cmd_buffer);
  vkDestroyFence (self->gulkan_client->device->device, cmd_buffer.fence, NULL);

  GulkanDevice *device = self->gulkan_client->device;

  VRVulkanTextureData_t openvr_texture_data = {
    .m_nImage = (uint64_t)self->framebuffer[EVREye_Eye_Left]->color_image,
    .m_pDevice = (struct VkDevice_T *) device->device,
    .m_pPhysicalDevice = (struct VkPhysicalDevice_T *) device->physical_device,
    .m_pInstance = (struct VkInstance_T *) self->gulkan_client->instance->instance,
    .m_pQueue = (struct VkQueue_T *) device->queue,
    .m_nQueueFamilyIndex = device->queue_family_index,
    .m_nWidth = self->render_width,
    .m_nHeight = self->render_height,
    .m_nFormat = VK_FORMAT_R8G8B8A8_UNORM,
    .m_nSampleCount = self->msaa_sample_count
  };

  Texture_t texture = {
    &openvr_texture_data,
    ETextureType_TextureType_Vulkan,
    EColorSpace_ColorSpace_Auto
  };

  VRTextureBounds_t bounds = {
    .uMin = 0.0f,
    .uMax = 1.0f,
    .vMin = 0.0f,
    .vMax = 1.0f
  };

  OpenVRContext *context = openvr_context_get_instance ();
  context->compositor->Submit (EVREye_Eye_Left, &texture, &bounds,
                               EVRSubmitFlags_Submit_Default);

  openvr_texture_data.m_nImage =
    (uint64_t) self->framebuffer[EVREye_Eye_Right]->color_image;
  context->compositor->Submit (EVREye_Eye_Right, &texture, &bounds,
                               EVRSubmitFlags_Submit_Default);

  xrd_scene_device_manager_update_poses (self->device_manager,
                                        &self->mat_head_pose);
}

bool
_init_framebuffers (XrdSceneClient *self, VkCommandBuffer cmd_buffer)
{
  OpenVRContext *context = openvr_context_get_instance ();

  context->system->GetRecommendedRenderTargetSize (&self->render_width,
                                                   &self->render_height);
  self->render_width =
      (uint32_t) (self->super_sample_scale * (float) self->render_width);
  self->render_height =
      (uint32_t) (self->super_sample_scale * (float) self->render_height);

  for (uint32_t eye = 0; eye < 2; eye++)
    gulkan_frame_buffer_initialize (self->framebuffer[eye],
                                    self->gulkan_client->device,
                                    cmd_buffer,
                                    self->render_width, self->render_height,
                                    self->msaa_sample_count,
                                    VK_FORMAT_R8G8B8A8_UNORM);
  return true;
}

void
_update_matrices (XrdSceneClient *self)
{
  for (uint32_t eye = 0; eye < 2; eye++)
    {
      self->mat_projection[eye] =
        openvr_system_get_projection_matrix (eye,
                                             self->near_clip,
                                             self->far_clip);
      self->mat_eye_pos[eye] = _get_hmd_pose_matrix (eye);
    }
}

void
_render_stereo (XrdSceneClient *self, VkCommandBuffer cmd_buffer)
{
  VkViewport viewport = {
    0.0f, 0.0f,
    self->render_width, self->render_height,
    0.0f, 1.0f
  };
  vkCmdSetViewport (cmd_buffer, 0, 1, &viewport);
  VkRect2D scissor = {
    .offset = {0, 0},
    .extent = {self->render_width, self->render_height}
  };
  vkCmdSetScissor (cmd_buffer, 0, 1, &scissor);

  for (uint32_t eye = 0; eye < 2; eye++)
    {
      gulkan_frame_buffer_begin_pass (self->framebuffer[eye], cmd_buffer);

      graphene_matrix_t vp = _get_view_projection_matrix (self, eye);

      for (GSList *l = self->windows; l != NULL; l = l->next)
        {
          XrdSceneWindow *window = (XrdSceneWindow *) l->data;
          xrd_scene_window_draw (window, eye,
                                 self->pipelines[PIPELINE_WINDOWS],
                                 self->pipeline_layout,
                                 cmd_buffer, &vp);
        }

      _render_pointers (self, eye, cmd_buffer,
                        self->pipelines[PIPELINE_POINTER],
                        self->pipeline_layout, &vp);

      xrd_scene_device_manager_render (self->device_manager, eye, cmd_buffer,
                                       self->pipelines[PIPELINE_DEVICE_MODELS],
                                       self->pipeline_layout, &vp);

      xrd_scene_background_render (self->background, eye,
                                   self->pipelines[PIPELINE_POINTER],
                                   self->pipeline_layout, cmd_buffer, &vp);

#if DEBUG_GEOMETRY
      for (uint32_t i = 0; i < G_N_ELEMENTS (self->debug_vectors); i++)
        xrd_scene_vector_render (self->debug_vectors[i], eye,
                                 self->pipelines[PIPELINE_POINTER],
                                 self->pipeline_layout,
                                 cmd_buffer,
                                &vp);
#endif

      vkCmdEndRenderPass (cmd_buffer);
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

bool
_init_shaders (XrdSceneClient *self)
{
  const char *shader_names[PIPELINE_COUNT] = {
    "window", "pointer", "device_model"
  };
  const char *stage_names[2] = {"vert", "frag"};

  for (int32_t i = 0; i < PIPELINE_COUNT; i++)
    for (int32_t j = 0; j < 2; j++)
      {
        char path[1024];
        sprintf (path, "/shaders/%s.%s.spv", shader_names[i], stage_names[j]);

        if (!gulkan_renderer_create_shader_module (
            self->gulkan_client->device->device, path,
           &self->shader_modules[i * 2 + j]))
          return false;
      }
  return true;
}

/* Create a descriptor set layout compatible with all shaders. */
bool
_init_descriptor_layout (XrdSceneClient *self)
{
  VkDescriptorSetLayoutCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 2,
    .pBindings = (VkDescriptorSetLayoutBinding[]) {
      {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
      },
      {
        .binding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
      }
    }
  };

  VkResult res = vkCreateDescriptorSetLayout (self->gulkan_client->device->device,
                                             &info, NULL,
                                             &self->descriptor_set_layout);
  vk_check_error ("vkCreateDescriptorSetLayout", res);

  return true;
}

bool
_init_pipeline_layout (XrdSceneClient *self)
{
  VkPipelineLayoutCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &self->descriptor_set_layout,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges = NULL
  };

  VkResult res = vkCreatePipelineLayout (self->gulkan_client->device->device,
                                        &info, NULL, &self->pipeline_layout);
  vk_check_error ("vkCreatePipelineLayout", res);

  return true;
}

bool
_init_pipeline_cache (XrdSceneClient *self)
{
  VkPipelineCacheCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
  };
  VkResult res = vkCreatePipelineCache (self->gulkan_client->device->device,
                                       &info, NULL, &self->pipeline_cache);
  vk_check_error ("vkCreatePipelineCache", res);

  return true;
}

typedef struct __attribute__((__packed__)) PipelineConfig {
  VkPrimitiveTopology                      topology;
  uint32_t                                 stride;
  const VkVertexInputAttributeDescription* attribs;
  uint32_t                                 attrib_count;
} PipelineConfig;

bool
_init_graphics_pipelines (XrdSceneClient *self)
{
  PipelineConfig config[PIPELINE_COUNT] = {
    // PIPELINE_WINDOWS
    {
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .stride = sizeof (VertexDataScene),
      .attribs = (VkVertexInputAttributeDescription []) {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof (VertexDataScene, uv)},
      },
      .attrib_count = 2
    },
    // PIPELINE_POINTER
    {
      .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
      .stride = sizeof (float) * 6,
      .attribs = (VkVertexInputAttributeDescription []) {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof (float) * 3},
      },
      .attrib_count = 2
    },
    // PIPELINE_DEVICE_MODELS
    {
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .stride = sizeof (RenderModel_Vertex_t),
      .attribs = (VkVertexInputAttributeDescription []) {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof (RenderModel_Vertex_t, vNormal)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof (RenderModel_Vertex_t, rfTextureCoord)},
      },
      .attrib_count = 3
    }
  };

  for (uint32_t i = 0; i < PIPELINE_COUNT; i++)
    {
      VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .layout = self->pipeline_layout,
        .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
          .pVertexAttributeDescriptions = config[i].attribs,
          .vertexBindingDescriptionCount = 1,
          .pVertexBindingDescriptions = &(VkVertexInputBindingDescription) {
            .binding = 0,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            .stride = config[i].stride
          },
          .vertexAttributeDescriptionCount = config[i].attrib_count
        },
        .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
          .topology = config[i].topology,
          .primitiveRestartEnable = VK_FALSE
        },
        .pViewportState = &(VkPipelineViewportStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
          .viewportCount = 1,
          .scissorCount = 1
        },
        .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_FILL,
          .cullMode = VK_CULL_MODE_BACK_BIT,
          .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
          .lineWidth = 1.0f
        },
        .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = self->msaa_sample_count,
          .minSampleShading = 0.0f,
          .pSampleMask = &(uint32_t) { 0xFFFFFFFF },
          .alphaToCoverageEnable = VK_FALSE
        },
        .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_TRUE ,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
          .depthBoundsTestEnable = VK_FALSE,
          .stencilTestEnable = VK_FALSE,
          .minDepthBounds = 0.0f,
          .maxDepthBounds = 0.0f
        },
        .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
          .logicOpEnable = VK_FALSE,
          .logicOp = VK_LOGIC_OP_COPY,
          .attachmentCount = 1,
          .blendConstants = {0,0,0,0},
          .pAttachments = &(VkPipelineColorBlendAttachmentState) {
            .blendEnable = VK_FALSE,
            .colorWriteMask = 0xf
          },
        },
        .stageCount = 2,
        .pStages = (VkPipelineShaderStageCreateInfo []) {
          {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = self->shader_modules[i * 2],
            .pName = "main"
          },
          {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = self->shader_modules[i * 2 + 1],
            .pName = "main"
          }
        },
        .renderPass = self->framebuffer[EVREye_Eye_Left]->render_pass,
        .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
          .dynamicStateCount = 2,
          .pDynamicStates = (VkDynamicState[]) {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
          }
        },
        .subpass = VK_NULL_HANDLE
      };

      VkResult res = vkCreateGraphicsPipelines (self->gulkan_client->device->device,
                                                self->pipeline_cache, 1,
                                               &pipeline_info,
                                                NULL, &self->pipelines[i]);
      vk_check_error ("vkCreateGraphicsPipelines", res);
    }

  return true;
}

/* Inheritance overwrites from XrdClient */

XrdOverlayWindow *
xrd_scene_client_add_window (XrdSceneClient *self,
                             const char     *title,
                             gpointer        native,
                             float           ppm,
                             gboolean        is_child,
                             gboolean        follow_head)
{
  (void) self;
  (void) title;
  (void) native;
  (void) ppm;
  (void) is_child;
  (void) follow_head;

  g_warning ("stub: xrd_scene_client_add_window\n");
  return NULL;
}

gboolean
xrd_scene_client_add_button (XrdSceneClient     *self,
                             XrdWindow         **button,
                             gchar              *label,
                             graphene_point3d_t *position,
                             GCallback           press_callback,
                             gpointer            press_callback_data)
{
  (void) self;
  (void) button;
  (void) label;
  (void) position;
  (void) press_callback;
  (void) press_callback_data;

  g_warning ("stub: xrd_scene_client_add_button\n");
  return TRUE;
}

GulkanClient *
xrd_scene_client_get_uploader (XrdSceneClient *self)
{
  return self->gulkan_client;
}

VkDescriptorSetLayout*
xrd_scene_client_get_descriptor_set_layout (XrdSceneClient *self)
{
  return &self->descriptor_set_layout;
}

