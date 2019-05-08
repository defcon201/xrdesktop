/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-renderer.h"

#include <graphene.h>

#include <gulkan-frame-buffer.h>
#include <openvr-compositor.h>
#include <openvr-context.h>
#include <gulkan-renderer.h>

static bool use_validation = true;

typedef struct VertexDataScene
{
  graphene_point3d_t position;
  graphene_point_t   uv;
} VertexDataScene;

struct _XrdSceneRenderer
{
  GulkanClient parent;

  VkSampleCountFlagBits msaa_sample_count;
  float super_sample_scale;

  VkShaderModule shader_modules[PIPELINE_COUNT * 2];
  VkPipeline pipelines[PIPELINE_COUNT];
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipelineLayout pipeline_layout;
  VkPipelineCache pipeline_cache;

  GulkanFrameBuffer *framebuffer[2];

  uint32_t render_width;
  uint32_t render_height;
};

G_DEFINE_TYPE (XrdSceneRenderer, xrd_scene_renderer, GULKAN_TYPE_CLIENT)

static void
xrd_scene_renderer_finalize (GObject *gobject);

static void
xrd_scene_renderer_class_init (XrdSceneRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_renderer_finalize;
}

static void
xrd_scene_renderer_init (XrdSceneRenderer *self)
{
  self->msaa_sample_count = VK_SAMPLE_COUNT_4_BIT;
  self->super_sample_scale = 1.0f;

  self->descriptor_set_layout = VK_NULL_HANDLE;
  self->pipeline_layout = VK_NULL_HANDLE;
  self->pipeline_cache = VK_NULL_HANDLE;

  for (uint32_t eye = 0; eye < 2; eye++)
    self->framebuffer[eye] = gulkan_frame_buffer_new();
}

XrdSceneRenderer *
xrd_scene_renderer_new (void)
{
  return (XrdSceneRenderer*) g_object_new (XRD_TYPE_SCENE_RENDERER, 0);
}

static void
xrd_scene_renderer_finalize (GObject *gobject)
{
  XrdSceneRenderer *self = XRD_SCENE_RENDERER (gobject);

  VkDevice device = GULKAN_CLIENT (self)->device->device;
  if (device != VK_NULL_HANDLE)
    vkDeviceWaitIdle (device);

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

  G_OBJECT_CLASS (xrd_scene_renderer_parent_class)->finalize (gobject);
}

static XrdSceneRenderer *singleton = NULL;

XrdSceneRenderer *xrd_scene_renderer_get_instance (void)
{
  if (singleton == NULL)
    singleton = xrd_scene_renderer_new ();

  return singleton;
}

static bool
_init_vulkan_instance (XrdSceneRenderer *self)
{
  GSList *extensions = NULL;
  openvr_compositor_get_instance_extensions (&extensions);
  return gulkan_instance_create (GULKAN_CLIENT (self)->instance,
                                 use_validation, extensions);
}

static bool
_init_vulkan_device (XrdSceneRenderer *self)
{
  /* Query OpenVR for a physical device */
  uint64_t physical_device = 0;
  OpenVRContext *context = openvr_context_get_instance ();
  context->system->GetOutputDevice (
      &physical_device, ETextureType_TextureType_Vulkan,
      (struct VkInstance_T *) GULKAN_CLIENT (self)->instance->instance);

  GSList *extensions = NULL;
  openvr_compositor_get_device_extensions ((VkPhysicalDevice)physical_device,
                                           &extensions);

  return gulkan_device_create (GULKAN_CLIENT (self)->device,
                               GULKAN_CLIENT (self)->instance,
                               (VkPhysicalDevice)physical_device, extensions);
}

static bool
_init_framebuffers (XrdSceneRenderer *self, VkCommandBuffer cmd_buffer)
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
                                    GULKAN_CLIENT (self)->device,
                                    cmd_buffer,
                                    self->render_width, self->render_height,
                                    self->msaa_sample_count,
                                    VK_FORMAT_R8G8B8A8_UNORM);
  return true;
}

static bool
_init_shaders (XrdSceneRenderer *self)
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
            GULKAN_CLIENT (self)->device->device, path,
           &self->shader_modules[i * 2 + j]))
          return false;
      }
  return true;
}

/* Create a descriptor set layout compatible with all shaders. */
bool
_init_descriptor_layout (XrdSceneRenderer *self)
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

  VkResult res = vkCreateDescriptorSetLayout (GULKAN_CLIENT (self)->device->device,
                                             &info, NULL,
                                             &self->descriptor_set_layout);
  vk_check_error ("vkCreateDescriptorSetLayout", res);

  return true;
}

bool
_init_pipeline_layout (XrdSceneRenderer *self)
{
  VkPipelineLayoutCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &self->descriptor_set_layout,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges = NULL
  };

  VkResult res = vkCreatePipelineLayout (GULKAN_CLIENT (self)->device->device,
                                        &info, NULL, &self->pipeline_layout);
  vk_check_error ("vkCreatePipelineLayout", res);

  return true;
}

bool
_init_pipeline_cache (XrdSceneRenderer *self)
{
  VkPipelineCacheCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
  };
  VkResult res = vkCreatePipelineCache (GULKAN_CLIENT (self)->device->device,
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
_init_graphics_pipelines (XrdSceneRenderer *self)
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

      VkResult res = vkCreateGraphicsPipelines (GULKAN_CLIENT (self)->device->device,
                                                self->pipeline_cache, 1,
                                               &pipeline_info,
                                                NULL, &self->pipelines[i]);
      vk_check_error ("vkCreateGraphicsPipelines", res);
    }

  return true;
}

bool
xrd_scene_renderer_init_vulkan (XrdSceneRenderer *self)
{
  if (!_init_vulkan_instance (self))
    return false;

  if (!_init_vulkan_device (self))
    return false;

  if (!gulkan_client_init_command_pool (GULKAN_CLIENT (self)))
    {
      g_printerr ("Could not create command pool.\n");
      return false;
    }

  FencedCommandBuffer cmd_buffer;
  if (!gulkan_client_begin_res_cmd_buffer (GULKAN_CLIENT (self),
                                           &cmd_buffer))
    {
      g_printerr ("Could not begin command buffer.\n");
      return false;
    }

 _init_framebuffers (self, cmd_buffer.cmd_buffer);

  if (!gulkan_client_submit_res_cmd_buffer (GULKAN_CLIENT (self), &cmd_buffer))
    {
      g_printerr ("Could not submit command buffer.\n");
      return false;
    }

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

  return true;
}

VkDescriptorSetLayout *
xrd_scene_renderer_get_descriptor_set_layout (XrdSceneRenderer *self)
{
  return &self->descriptor_set_layout;
}

void
_render_stereo (XrdSceneRenderer *self, VkCommandBuffer cmd_buffer)
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

      // TODO: eye pass cb

      vkCmdEndRenderPass (cmd_buffer);
    }
}

void
xrd_scene_renderer_draw (XrdSceneRenderer *self)
{
  FencedCommandBuffer cmd_buffer;
  gulkan_client_begin_res_cmd_buffer (GULKAN_CLIENT (self), &cmd_buffer);

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

  vkQueueSubmit (GULKAN_CLIENT (self)->device->queue, 1,
                &submit_info, cmd_buffer.fence);

  vkQueueWaitIdle (GULKAN_CLIENT (self)->device->queue);

  vkFreeCommandBuffers (GULKAN_CLIENT (self)->device->device,
                        GULKAN_CLIENT (self)->command_pool,
                        1, &cmd_buffer.cmd_buffer);
  vkDestroyFence (GULKAN_CLIENT (self)->device->device, cmd_buffer.fence, NULL);

  GulkanDevice *device = GULKAN_CLIENT (self)->device;

  VRVulkanTextureData_t openvr_texture_data = {
    .m_nImage = (uint64_t)self->framebuffer[EVREye_Eye_Left]->color_image,
    .m_pDevice = (struct VkDevice_T *) device->device,
    .m_pPhysicalDevice = (struct VkPhysicalDevice_T *) device->physical_device,
    .m_pInstance = (struct VkInstance_T *) GULKAN_CLIENT (self)->instance->instance,
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
}
