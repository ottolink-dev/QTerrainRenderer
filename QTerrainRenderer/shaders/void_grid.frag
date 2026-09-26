/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#version 330 core

// "Void" background: a subtle, unlit grid at the terrain's lowest level, the
// terrain's footprint outlined, and brighter corner marks at its four corners.
// Nothing here is lit or shadowed: it is drawn over a black clear.

in vec3 world_pos;

out vec4 frag_color;

uniform vec2  terrain_half; // footprint half extents (x, z), world units
uniform float cell;         // minor grid spacing, world units
uniform vec3  camera_pos;

// antialiased grid lines, ~width_px pixels wide whatever the distance
float grid(vec2 p, float spacing, float width_px)
{
  vec2 coord = p / spacing;
  vec2 d = max(fwidth(coord), vec2(1e-6));
  vec2 g = abs(fract(coord - 0.5) - 0.5) / d;
  return 1.0 - clamp(min(g.x, g.y) / width_px, 0.0, 1.0);
}

void main()
{
  vec2  p = world_pos.xz;
  float extent = max(terrain_half.x, terrain_half.y);

  // grid: minor and major lines, dissolving into the void away from the
  // terrain and at grazing distances (where lines would only shimmer)
  float minor = grid(p, cell, 1.0);
  float major = grid(p, cell * 4.0, 1.25);

  float radial = 1.0 - smoothstep(1.4 * extent, 5.0 * extent, length(p));
  float far = 1.0 - smoothstep(4.0 * extent, 12.0 * extent, length(camera_pos - world_pos));

  float alpha = max(minor * 0.09, major * 0.16) * radial * far;
  vec3  color = vec3(0.62);

  // footprint frame, just outside the terrain (at the footprint itself the
  // terrain's own rim would cover it): signed distance to it, in pixels
  vec2  frame_half = terrain_half + vec2(0.05 * extent);
  vec2  q = abs(p) - frame_half;
  float sd = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
  float px = abs(sd) / max(fwidth(sd), 1e-6);

  // a faint outline all round...
  float outline = 1.0 - clamp(px / 1.1, 0.0, 1.0);
  alpha = max(alpha, outline * 0.22);

  // ...and distinct corner marks: brighter and a little thicker along the
  // last stretch of each side before a corner
  float arm = 0.16 * min(frame_half.x, frame_half.y);
  vec2  from_corner = frame_half - abs(p);
  bool  near_corner = from_corner.x < arm && from_corner.y < arm;
  if (near_corner)
  {
    float mark = 1.0 - clamp((px - 0.6) / 1.2, 0.0, 1.0);
    if (mark > 0.0)
    {
      color = mix(color, vec3(0.78), mark);
      alpha = max(alpha, mark * 0.85);
    }
  }

  if (alpha < 0.003)
    discard;

  frag_color = vec4(color, alpha);
}
