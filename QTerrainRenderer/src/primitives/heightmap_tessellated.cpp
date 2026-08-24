/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "hmm/src/heightmap.h"
#include "hmm/src/triangulator.h"

#include "qtr/logger.hpp"
#include "qtr/mesh.hpp"
#include "qtr/primitives.hpp"

namespace qtr
{

void generate_heightmap_tessellated(Mesh                     &mesh,
                                    const std::vector<float> &data,
                                    int                       width,
                                    int                       height,
                                    float                     x,
                                    float                     y,
                                    float                     z,
                                    float                     lx,
                                    float                     ly,
                                    float                     lz,
                                    float                     max_error,
                                    int                       max_triangles,
                                    int                       max_points,
                                    float                    *p_hmin)
{
  if (data.empty() || width <= 1 || height <= 1)
    return;

  // Triangulator in hmm expects (width, height, data)
  auto         p_hmap = std::make_shared<Heightmap>(width, height, data);
  Triangulator tri(p_hmap);

  qtr::Logger::log()->trace(
      "generate_heightmap_tessellated: run triangulator (max_error={})",
      max_error);
  tri.Run(max_error, max_triangles, max_points);

  const auto &tri_points = tri.Points(1.0f);
  const auto &tri_indices = tri.Triangles();

  const size_t num_points = tri_points.size();
  const size_t num_tris = tri_indices.size();

  std::vector<Vertex> vertices;
  vertices.reserve(num_points);

  const float hx = lx * 0.5f;
  const float hz = lz * 0.5f;
  const float inv_w = 1.0f / float(width - 1);
  const float inv_h = 1.0f / float(height - 1);

  float hmin = std::numeric_limits<float>::max();

  for (const auto &pt : tri_points)
  {
    // pt.x is in [0, width - 1], pt.y is (height - 1 - y_orig), pt.z is raw height
    const float raw_h = pt.z;
    if (raw_h < hmin)
      hmin = raw_h;

    const float u = pt.x * inv_w;
    const float v = float((height - 1) - pt.y) * inv_h;

    const float xpos = x - hx + u * lx;
    const float zpos = z - hz + v * lz;
    const float ypos = y + raw_h * ly;

    vertices.emplace_back(glm::vec3(xpos, ypos, zpos),
                          glm::vec3(0.0f, 1.0f, 0.0f),
                          glm::vec2(u, v));
  }

  if (p_hmin)
    *p_hmin = hmin;

  std::vector<uint> indices;
  indices.reserve(num_tris * 3);

  for (const auto &t : tri_indices)
  {
    indices.push_back(static_cast<uint>(t.x));
    indices.push_back(static_cast<uint>(t.y));
    indices.push_back(static_cast<uint>(t.z));
  }

  // Compute normals
  for (size_t k = 0; k < indices.size(); k += 3)
  {
    auto &v0 = vertices[indices[k + 0]];
    auto &v1 = vertices[indices[k + 1]];
    auto &v2 = vertices[indices[k + 2]];

    glm::vec3 n = glm::cross(v1.position - v0.position, v2.position - v0.position);
    float     len = glm::length(n);
    if (len > 1e-6f)
    {
      n /= len;
      v0.normal += n;
      v1.normal += n;
      v2.normal += n;
    }
  }

  for (auto &v : vertices)
  {
    float len = glm::length(v.normal);
    if (len > 1e-6f)
      v.normal /= len;
    else
      v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  mesh.create(std::move(vertices), std::move(indices), /* store_cpu_copy */ true);
}

} // namespace qtr
