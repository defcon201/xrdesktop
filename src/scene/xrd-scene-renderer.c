/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-renderer.h"

#include <graphene.h>

#include <gulkan.h>
#include <gxr.h>

#include "xrd-controller.h"
#include "xrd-scene-pointer.h"
#include "xrd-scene-pointer-tip.h"

#include "graphene-ext.h"

typedef struct {
  graphene_point3d_t position;
  graphene_point_t   uv;
} XrdSceneVertex;

typedef struct {
  float position[4];
  float color[4];
  float radius;
  float unused[3];
} XrdSceneLight;

typedef struct {
  XrdSceneLight lights[2];
  int active_lights;
} XrdSceneLights;

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

  gpointer scene_client;

  XrdSceneLights lights;
  GulkanUniformBuffer *lights_buffer;

  void
  (*render_eye) (uint32_t         eye,
                 VkCommandBuffer  cmd_buffer,
                 VkPipelineLayout pipeline_layout,
                 VkPipeline      *pipelines,
                 gpointer         data);

  void (*update_lights) (gpointer data);
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
  self->render_eye = NULL;
  self->scene_client = NULL;
  self->lights_buffer = gulkan_uniform_buffer_new ();

  self->lights.active_lights = 0;
  graphene_vec4_t position;
  graphene_vec4_init (&position, 0, 0, 0, 1);

  graphene_vec4_t color;
  graphene_vec4_init (&color,.078f, .471f, .675f, 1);

  for (uint32_t i = 0; i < 2; i++)
    {
      graphene_vec4_to_float (&position, self->lights.lights[i].position);
      graphene_vec4_to_float (&color, self->lights.lights[i].color);
      self->lights.lights[i].radius = 0.1f;
    }

  self->descriptor_set_layout = VK_NULL_HANDLE;
  self->pipeline_layout = VK_NULL_HANDLE;
  self->pipeline_cache = VK_NULL_HANDLE;

  for (uint32_t eye = 0; eye < 2; eye++)
    self->framebuffer[eye] = gulkan_frame_buffer_new();
}

static void
xrd_scene_renderer_finalize (GObject *gobject)
{
  XrdSceneRenderer *self = XRD_SCENE_RENDERER (gobject);

  VkDevice device = gulkan_client_get_device_handle (GULKAN_CLIENT (self));
  if (device != VK_NULL_HANDLE)
    vkDeviceWaitIdle (device);

  if (device != VK_NULL_HANDLE)
    {
      g_object_unref (self->lights_buffer);

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
    singleton = (XrdSceneRenderer*) g_object_new (XRD_TYPE_SCENE_RENDERER, 0);

  return singleton;
}

void xrd_scene_renderer_destroy_instance (void)
{
  g_clear_object (&singleton);
}

static bool
_init_framebuffers (XrdSceneRenderer *self, VkCommandBuffer cmd_buffer)
{
  OpenVRContext *context = openvr_context_get_instance ();

  if (openvr_context_is_valid (context))
    {
      context->system->GetRecommendedRenderTargetSize (&self->render_width,
                                                       &self->render_height);
    }
  else
    {
      g_warning ("Using default render target dimensions.\n");
      self->render_width = 1080;
      self->render_height = 1080;
    }

  self->render_width =
      (uint32_t) (self->super_sample_scale * (float) self->render_width);
  self->render_height =
      (uint32_t) (self->super_sample_scale * (float) self->render_height);

  for (uint32_t eye = 0; eye < 2; eye++)
    gulkan_frame_buffer_initialize (self->framebuffer[eye],
                                    gulkan_client_get_device (
                                      GULKAN_CLIENT (self)),
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
    "window", "window", "pointer", "pointer", "pointer", "device_model"
  };
  const char *stage_names[2] = {"vert", "frag"};

  for (int32_t i = 0; i < PIPELINE_COUNT; i++)
    for (int32_t j = 0; j < 2; j++)
      {
        char path[1024];
        sprintf (path, "/shaders/%s.%s.spv", shader_names[i], stage_names[j]);

        if (!gulkan_renderer_create_shader_module (
            gulkan_client_get_device_handle (GULKAN_CLIENT (self)), path,
           &self->shader_modules[i * 2 + j]))
          return false;
      }
  return true;
}

/* Create a descriptor set layout compatible with all shaders. */
static bool
_init_descriptor_layout (XrdSceneRenderer *self)
{
  VkDescriptorSetLayoutCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 4,
    .pBindings = (VkDescriptorSetLayoutBinding[]) {
      // mvp buffer
      {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
      },
      // Window and device texture
      {
        .binding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
      },
      // Window buffer
      {
        .binding = 2,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
      },
      // Lights buffer
      {
        .binding = 3,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
      },
    }
  };

  VkDevice device = gulkan_client_get_device_handle (GULKAN_CLIENT (self));
  VkResult res = vkCreateDescriptorSetLayout (device,
                                             &info, NULL,
                                             &self->descriptor_set_layout);
  vk_check_error ("vkCreateDescriptorSetLayout", res, false)

  return true;
}

static bool
_init_pipeline_layout (XrdSceneRenderer *self)
{
  VkPipelineLayoutCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &self->descriptor_set_layout,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges = NULL
  };

  VkResult res = vkCreatePipelineLayout (gulkan_client_get_device_handle (
                                           GULKAN_CLIENT (self)),
                                        &info, NULL, &self->pipeline_layout);
  vk_check_error ("vkCreatePipelineLayout", res, false)

  return true;
}

static bool
_init_pipeline_cache (XrdSceneRenderer *self)
{
  VkPipelineCacheCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
  };
  VkResult res = vkCreatePipelineCache (gulkan_client_get_device_handle (
                                          GULKAN_CLIENT (self)),
                                       &info, NULL, &self->pipeline_cache);
  vk_check_error ("vkCreatePipelineCache", res, false)

  return true;
}

typedef struct __attribute__((__packed__)) {
  VkPrimitiveTopology                           topology;
  uint32_t                                      stride;
  const VkVertexInputAttributeDescription      *attribs;
  uint32_t                                      attrib_count;
  const VkPipelineDepthStencilStateCreateInfo  *depth_stencil_state;
  const VkPipelineColorBlendAttachmentState    *blend_attachments;
  const VkPipelineRasterizationStateCreateInfo *rasterization_state;
} XrdPipelineConfig;

static bool
_init_graphics_pipelines (XrdSceneRenderer *self)
{
  XrdPipelineConfig config[PIPELINE_COUNT] = {
    // PIPELINE_WINDOWS
    {
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .stride = sizeof (XrdSceneVertex),
      .attribs = (VkVertexInputAttributeDescription []) {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof (XrdSceneVertex, uv)},
      },
      .attrib_count = 2,
      .depth_stencil_state = &(VkPipelineDepthStencilStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
      },
      .blend_attachments = &(VkPipelineColorBlendAttachmentState) {
        .blendEnable = VK_FALSE,
        .colorWriteMask = 0xf
      },
      .rasterization_state = &(VkPipelineRasterizationStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_FILL,
          .cullMode = VK_CULL_MODE_BACK_BIT,
          .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
          .lineWidth = 1.0f
      }
    },
    // PIPELINE_TIP
    {
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .stride = sizeof (XrdSceneVertex),
      .attribs = (VkVertexInputAttributeDescription []) {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof (XrdSceneVertex, uv)},
      },
      .attrib_count = 2,
      .depth_stencil_state = &(VkPipelineDepthStencilStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_FALSE,
          .depthWriteEnable = VK_FALSE
      },
      .blend_attachments = &(VkPipelineColorBlendAttachmentState) {
        .blendEnable = VK_TRUE,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                          VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT,
      },
      .rasterization_state = &(VkPipelineRasterizationStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_FILL,
          .cullMode = VK_CULL_MODE_BACK_BIT,
          .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
      }
    },
    // PIPELINE_POINTER
    {
      .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
      .stride = sizeof (float) * 6,
      .attribs = (VkVertexInputAttributeDescription []) {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof (float) * 3},
      },
      .depth_stencil_state = &(VkPipelineDepthStencilStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
      },
      .attrib_count = 2,
      .blend_attachments = &(VkPipelineColorBlendAttachmentState) {
        .blendEnable = VK_FALSE,
        .colorWriteMask = 0xf
      },
      .rasterization_state = &(VkPipelineRasterizationStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_LINE,
          .cullMode = VK_CULL_MODE_BACK_BIT,
          .lineWidth = 4.0f
      }
    },
    // PIPELINE_SELECTION
    {
      .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
      .stride = sizeof (float) * 6,
      .attribs = (VkVertexInputAttributeDescription []) {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof (float) * 3},
      },
      .depth_stencil_state = &(VkPipelineDepthStencilStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
      },
      .attrib_count = 2,
      .blend_attachments = &(VkPipelineColorBlendAttachmentState) {
        .blendEnable = VK_FALSE,
        .colorWriteMask = 0xf
      },
      .rasterization_state = &(VkPipelineRasterizationStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_LINE,
          .lineWidth = 2.0f
      }
    },
    // PIPELINE_BACKGROUND
    {
      .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
      .stride = sizeof (float) * 6,
      .attribs = (VkVertexInputAttributeDescription []) {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof (float) * 3},
      },
      .depth_stencil_state = &(VkPipelineDepthStencilStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
      },
      .attrib_count = 2,
      .blend_attachments = &(VkPipelineColorBlendAttachmentState) {
        .blendEnable = VK_FALSE,
        .colorWriteMask = 0xf
      },
      .rasterization_state = &(VkPipelineRasterizationStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_LINE,
          .lineWidth = 1.0f
      }
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
      .depth_stencil_state = &(VkPipelineDepthStencilStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
      },
      .attrib_count = 3,
      .blend_attachments = &(VkPipelineColorBlendAttachmentState) {
        .blendEnable = VK_FALSE,
        .colorWriteMask = 0xf
      },
      .rasterization_state = &(VkPipelineRasterizationStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_FILL,
          .cullMode = VK_CULL_MODE_BACK_BIT,
          .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
      }
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
        .pRasterizationState = config[i].rasterization_state,
        .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = self->msaa_sample_count,
          .minSampleShading = 0.0f,
          .pSampleMask = &(uint32_t) { 0xFFFFFFFF },
          .alphaToCoverageEnable = VK_FALSE
        },
        .pDepthStencilState = config[i].depth_stencil_state,
        .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
          .logicOpEnable = VK_FALSE,
          .attachmentCount = 1,
          .blendConstants = {0,0,0,0},
          .pAttachments = config[i].blend_attachments,
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
        .renderPass = gulkan_frame_buffer_get_render_pass (
          self->framebuffer[EVREye_Eye_Left]),
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

      VkResult res = vkCreateGraphicsPipelines (gulkan_client_get_device_handle
                                                  (GULKAN_CLIENT (self)),
                                                self->pipeline_cache, 1,
                                               &pipeline_info,
                                                NULL, &self->pipelines[i]);
      vk_check_error ("vkCreateGraphicsPipelines", res, false)
    }

  return true;
}


static bool
_init_vulkan (XrdSceneRenderer *self)
{
  GulkanCommandBuffer cmd_buffer;
  if (!gulkan_client_begin_cmd_buffer (GULKAN_CLIENT (self),
                                      &cmd_buffer))
    {
      g_printerr ("Could not begin command buffer.\n");
      return false;
    }

  _init_framebuffers (self, cmd_buffer.handle);

  if (!gulkan_client_submit_cmd_buffer (GULKAN_CLIENT (self), &cmd_buffer))
    {
      g_printerr ("Could not submit command buffer.\n");
      return false;
    }

  if (!_init_shaders (self))
    return false;

  GulkanDevice *device = gulkan_client_get_device (GULKAN_CLIENT (self));
  if (!gulkan_uniform_buffer_allocate_and_map (self->lights_buffer,
                                               device, sizeof (XrdSceneLights)))
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

bool
xrd_scene_renderer_init_vulkan_simple (XrdSceneRenderer *self)
{
  gulkan_client_init_vulkan (GULKAN_CLIENT (self), NULL, NULL);

  if (!_init_vulkan (self))
    return false;

  return true;
}

bool
xrd_scene_renderer_init_vulkan_openvr (XrdSceneRenderer *self)
{
  if (!openvr_compositor_gulkan_client_init (GULKAN_CLIENT (self)))
    return false;

  if (!_init_vulkan (self))
    return false;

  return true;
}

VkDescriptorSetLayout *
xrd_scene_renderer_get_descriptor_set_layout (XrdSceneRenderer *self)
{
  return &self->descriptor_set_layout;
}

static void
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

      if (self->render_eye)
        self->render_eye (eye, cmd_buffer, self->pipeline_layout,
                          self->pipelines, self->scene_client);

      vkCmdEndRenderPass (cmd_buffer);
    }
}

void
xrd_scene_renderer_update_lights (XrdSceneRenderer  *self,
                                  GList             *controllers)
{
  self->lights.active_lights = (int) g_list_length (controllers);
  if (self->lights.active_lights > 2)
    {
      g_warning ("Update lights received more than 2 controllers.\n");
      self->lights.active_lights = 2;
    }

  for (int i = 0; i < self->lights.active_lights; i++)
    {
      GList *l = g_list_nth (controllers, (guint) i);
      XrdController *controller = XRD_CONTROLLER (l->data);

      XrdScenePointerTip *scene_tip =
        XRD_SCENE_POINTER_TIP (xrd_controller_get_pointer_tip (controller));

      graphene_point3d_t tip_position;
      xrd_scene_object_get_position (XRD_SCENE_OBJECT (scene_tip), &tip_position);

      self->lights.lights[i].position[0] = tip_position.x;
      self->lights.lights[i].position[1] = tip_position.y;
      self->lights.lights[i].position[2] = tip_position.z;
    }

  gulkan_uniform_buffer_update_struct (self->lights_buffer,
                                       (gpointer) &self->lights);
}

static void
_draw (XrdSceneRenderer *self)
{
  GulkanCommandBuffer cmd_buffer;
  gulkan_client_begin_cmd_buffer (GULKAN_CLIENT (self), &cmd_buffer);

  self->update_lights (self->scene_client);

  _render_stereo (self, cmd_buffer.handle);

  vkEndCommandBuffer (cmd_buffer.handle);

  VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd_buffer.handle,
    .waitSemaphoreCount = 0,
    .pWaitSemaphores = NULL,
    .signalSemaphoreCount = 0
  };

  GulkanDevice *device = gulkan_client_get_device (GULKAN_CLIENT (self));
  VkDevice device_handle = gulkan_device_get_handle (device);
  vkQueueSubmit (gulkan_device_get_queue_handle (device), 1,
                &submit_info, cmd_buffer.fence);

  vkQueueWaitIdle (gulkan_device_get_queue_handle (device));

  vkFreeCommandBuffers (device_handle,
                        gulkan_client_get_command_pool (GULKAN_CLIENT (self)),
                        1, &cmd_buffer.handle);
  vkDestroyFence (device_handle, cmd_buffer.fence, NULL);
}

bool
xrd_scene_renderer_draw (XrdSceneRenderer *self)
{
  _draw (self);

  VkImage left =
    gulkan_frame_buffer_get_color_image (self->framebuffer[EVREye_Eye_Left]);

  VkImage right =
    gulkan_frame_buffer_get_color_image (self->framebuffer[EVREye_Eye_Right]);

  if (!openvr_compositor_submit (GULKAN_CLIENT(self),
                                 self->render_width,
                                 self->render_height,
                                 VK_FORMAT_R8G8B8A8_UNORM,
                                 self->msaa_sample_count,
                                 left, right))
    return false;

  return true;
}

void
xrd_scene_renderer_set_render_cb (XrdSceneRenderer *self,
                                  void (*render_eye) (uint32_t         eye,
                                                      VkCommandBuffer  cmd_buffer,
                                                      VkPipelineLayout pipeline_layout,
                                                      VkPipeline      *pipelines,
                                                      gpointer         data),
                                  gpointer scene_client)
{
  self->render_eye = render_eye;
  self->scene_client = scene_client;
}

void
xrd_scene_renderer_set_update_lights_cb (XrdSceneRenderer *self,
                                         void (*update_lights) (gpointer data),
                                         gpointer scene_client)
{
  self->update_lights = update_lights;
  self->scene_client = scene_client;
}

GulkanDevice*
xrd_scene_renderer_get_device ()
{
  XrdSceneRenderer *self = xrd_scene_renderer_get_instance ();
  return gulkan_client_get_device (GULKAN_CLIENT (self));
}

VkBuffer
xrd_scene_renderer_get_lights_buffer_handle (XrdSceneRenderer *self)
{
  return gulkan_uniform_buffer_get_handle (self->lights_buffer);
}
