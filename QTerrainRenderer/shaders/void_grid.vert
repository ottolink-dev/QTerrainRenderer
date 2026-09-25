/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#version 330 core

layout(location = 0) in vec3 in_position;

out vec3 world_pos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
  vec4 w = model * vec4(in_position, 1.0);
  world_pos = w.xyz;
  gl_Position = projection * view * w;
}
