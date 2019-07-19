/*
 * gulkan
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable

const float shininess = 16.0f;
const float ambient = 0.5f;

layout (location = 0) in vec4 world_frag_position;
layout (location = 1) in vec4 view_frag_position;
layout (location = 2) in vec2 uv;

layout (binding = 0) uniform Transformation {
  mat4 mvp;
  mat4 mv;
  mat4 m;
  bool receive_light;
} transformation;

layout (binding = 1) uniform sampler2D image;
layout (binding = 2) uniform Shading {
  vec4 color;
  bool flip_y;
} shading;

struct Light {
  vec4 position;
  vec4 color;
  float radius;
  float unused[3];
};

layout (std430, binding = 3) uniform Lights
{
  Light lights[2];
  int active_lights;
} lights;

layout (location = 0) out vec4 out_color;

const float intensity = 2.0f;
const vec3 light_color_max = vec3(1.0f, 1.0f, 1.0f);

// "Lighten only" blending
vec3 lighten (vec3 a, vec3 b)
{
  vec3 c;
  c.r = max (a.r, b.r);
  c.g = max (a.g, b.g);
  c.b = max (a.b, b.b);
  return c;
}

void main ()
{
  vec2 uv_b = shading.flip_y ? vec2 (uv.x, 1.0f - uv.y) : uv;
  vec4 texture_color = texture (image, uv_b);

  vec4 base_diffuse = mix (texture_color * shading.color, texture_color, 0.5f);

  if (!transformation.receive_light)
    {
      out_color = base_diffuse;
      return;
    }

  vec3 lit = vec3 (0);

  float frag_distance = length (view_frag_position.xyz);

  for (int i = 0; i < lights.active_lights; i++)
    {
      vec3 L = lights.lights[i].position.xyz - world_frag_position.xyz;
      float d = length (L);

      float radius = lights.lights[i].radius * frag_distance;

      float atten = intensity / ((d / radius) + 1.0);
      vec3 light_gradient = mix (lights.lights[i].color.xyz,
                                 light_color_max,
                                 atten * 0.5f);
      lit += light_gradient * base_diffuse.rgb * atten;
    }

  out_color = vec4 (lighten (lit, base_diffuse.rgb), 1.0f);
}

