R""(
/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#version 330 core

layout(location = 0) in vec3 in_position;

out vec3 frag_dir;

uniform mat4  view;
uniform mat4  projection;
uniform float skybox_rotation; // rotation angle in radians around Y

void main()
{
  // Rotation matrix around Y axis
  float c = cos(skybox_rotation);
  float s = sin(skybox_rotation);
  mat3  rot_y = mat3(c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c);

  // Position is on unit cube, view direction is unrotated by skybox_rotation
  frag_dir = rot_y * in_position;

  // Remove translation from view matrix
  mat4 rot_view = mat4(mat3(view));
  vec4 clip_pos = projection * rot_view * vec4(in_position, 1.0);

  // Place at far plane (z = w produces NDC depth 1.0)
  gl_Position = clip_pos.xyww;
}
)""
