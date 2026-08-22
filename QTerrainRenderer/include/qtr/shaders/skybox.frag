R""(
/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#version 330 core

in vec3 frag_dir;

out vec4 frag_color;

// Skybox uniforms
uniform int       skybox_mode; // 0: Uniform Color, 1: Image
uniform vec3      skybox_color;
uniform sampler2D texture_skybox;
uniform bool      has_skybox_texture;

// Post-processing
uniform float gamma_correction;
uniform bool  apply_tonemap;

const float PI = 3.14159265358979323846;

vec2 dir_to_equirectangular_uv(vec3 dir)
{
  vec3  d = normalize(dir);
  float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;
  float v = 0.5 - asin(clamp(d.y, -1.0, 1.0)) / PI;
  return vec2(u, v);
}

vec3 tonemap_ACES(vec3 x)
{
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return (x * (a * x + b)) / (x * (c * x + d) + e);
}

void main()
{
  vec3 col = skybox_color;

  if (skybox_mode == 1 && has_skybox_texture)
  {
    vec2 uv = dir_to_equirectangular_uv(frag_dir);
    col = texture(texture_skybox, uv).rgb;
  }

  col.x = pow(max(col.x, 0.0), 1.0 / gamma_correction);
  col.y = pow(max(col.y, 0.0), 1.0 / gamma_correction);
  col.z = pow(max(col.z, 0.0), 1.0 / gamma_correction);

  if (apply_tonemap)
    col = tonemap_ACES(col);

  frag_color = vec4(col, 1.0);
}
)""
