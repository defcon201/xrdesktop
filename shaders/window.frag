/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#version 460
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 uv;

layout (binding = 1) uniform sampler2D image;
layout (binding = 2) uniform Shading {
  vec4 color;
  bool flip_y;
} ubo;


layout (location = 0) out vec4 out_color;

void main ()
{
  vec2 uv_b = ubo.flip_y ? vec2(uv.x, 1.0f - uv.y) : uv;
  out_color = texture (image, uv_b) * ubo.color;
}

