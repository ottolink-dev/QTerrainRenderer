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

// Fog uniforms
uniform bool  add_fog;
uniform vec3  fog_color;
uniform float fog_density;
uniform float fog_height;
uniform bool  fog_match_skybox;

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

  if (add_fog)
  {
    vec3 d = normalize(frag_dir);
    vec3 effective_fog_color = fog_color;
    if (fog_match_skybox)
    {
      if (skybox_mode == 1 && has_skybox_texture)
      {
        float u_horiz = atan(d.z, d.x) / (2.0 * PI) + 0.5;
        effective_fog_color = texture(texture_skybox, vec2(u_horiz, 0.5)).rgb;
      }
      else
      {
        effective_fog_color = skybox_color;
      }
    }

    float h_fog = max(fog_height, 0.005);
    float horizon_factor = 0.0;
    if (d.y <= 0.0)
    {
      horizon_factor = 1.0;
    }
    else
    {
      horizon_factor = exp(-d.y / (h_fog * 1.5));
    }
    horizon_factor *= clamp(fog_density * 0.1, 0.0, 1.0);
    col = mix(col, effective_fog_color, horizon_factor);
  }

  col.x = pow(max(col.x, 0.0), 1.0 / gamma_correction);
  col.y = pow(max(col.y, 0.0), 1.0 / gamma_correction);
  col.z = pow(max(col.z, 0.0), 1.0 / gamma_correction);

  if (apply_tonemap)
    col = tonemap_ACES(col);

  frag_color = vec4(col, 1.0);
}
)""
