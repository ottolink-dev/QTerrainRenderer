/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include <glm/gtc/constants.hpp>

#include "qtr/keys.hpp"
#include "qtr/logger.hpp"
#include "qtr/primitives.hpp"
#include "qtr/render_widget.hpp"

namespace qtr
{

void set_points(RenderWidget             &renderer,
                const std::vector<float> &x,
                const std::vector<float> &y,
                const std::vector<float> &h)
{
  qtr::Logger::log()->trace("qtr::set_points");

  if (x.size() != y.size() || x.size() != h.size())
    throw std::invalid_argument("qtr::set_points: vector sizes do not match");

  std::vector<BaseInstance> instances;
  float                     scale = 0.01f;
  float                     rotation = 0.f;
  glm::vec3                 color = glm::vec3(0.f, 1.f, 0.f);

  for (size_t k = 0; k < x.size(); ++k)
  {
    float xs = 0.5f * renderer.get_hmap_wx() * (2.f * x[k] - 1.f);
    float hs = renderer.get_hmap_h0() + renderer.get_hmap_h() * h[k];
    float ys = 0.5f * renderer.get_hmap_wy() * (2.f * y[k] - 1.f);

    instances.push_back({glm::vec3(xs, hs, ys), scale, rotation, color});
  }

  auto sphere_mesh = std::make_shared<Mesh>();
  renderer.makeCurrent();
  generate_sphere(*sphere_mesh, 1.f);
  renderer.set_instanced_mesh(keys::mesh::points, sphere_mesh, instances);
  renderer.doneCurrent();
}

void set_path(RenderWidget             &renderer,
              const std::vector<float> &x,
              const std::vector<float> &y,
              const std::vector<float> &h,
              float                     width)
{
  qtr::Logger::log()->trace("qtr::set_path");

  if (x.size() != y.size() || x.size() != h.size())
    throw std::invalid_argument("qtr::set_path: vector sizes do not match");

  std::vector<glm::vec3> points;
  for (size_t k = 0; k < x.size(); ++k)
  {
    float xs = 0.5f * renderer.get_hmap_wx() * (2.f * x[k] - 1.f);
    float hs = renderer.get_hmap_h0() + renderer.get_hmap_h() * h[k];
    float ys = 0.5f * renderer.get_hmap_wy() * (2.f * y[k] - 1.f);

    points.push_back(glm::vec3(xs, hs, ys));
  }

  auto path_mesh = std::make_shared<Mesh>();
  renderer.makeCurrent();
  generate_path(*path_mesh, points, width);
  renderer.set_mesh(keys::mesh::path, path_mesh);
  renderer.doneCurrent();
}

void set_rocks(RenderWidget             &renderer,
               const std::vector<float> &x,
               const std::vector<float> &y,
               const std::vector<float> &h,
               const std::vector<float> &radius)
{
  qtr::Logger::log()->trace("qtr::set_rocks");

  if (x.size() != y.size() || x.size() != h.size() || x.size() != radius.size())
    throw std::invalid_argument("qtr::set_rocks: vector sizes do not match");

  std::vector<BaseInstance> instances;
  glm::vec3                 color = glm::vec3(0.f, 1.f, 0.f);

  for (size_t k = 0; k < x.size(); ++k)
  {
    float xs = 0.5f * renderer.get_hmap_wx() * (2.f * x[k] - 1.f);
    float hs = renderer.get_hmap_h0() + renderer.get_hmap_h() * h[k];
    float ys = 0.5f * renderer.get_hmap_wy() * (2.f * y[k] - 1.f);
    float rs = 2.f * radius[k];
    float rotation = (float)std::rand() / RAND_MAX * glm::two_pi<float>();

    instances.push_back({glm::vec3(xs, hs, ys), rs, rotation, color});
  }

  auto mesh = std::make_shared<Mesh>();
  renderer.makeCurrent();
  generate_rock(*mesh, 1.f, 0.3f, 0);
  renderer.set_instanced_mesh(keys::mesh::rocks, mesh, instances);
  renderer.doneCurrent();
}

void set_trees(RenderWidget             &renderer,
               const std::vector<float> &x,
               const std::vector<float> &y,
               const std::vector<float> &h,
               const std::vector<float> &radius)
{
  qtr::Logger::log()->trace("qtr::set_trees");

  if (x.size() != y.size() || x.size() != h.size() || x.size() != radius.size())
    throw std::invalid_argument("qtr::set_trees: vector sizes do not match");

  std::vector<BaseInstance> instances;
  glm::vec3                 color = glm::vec3(0.f, 1.f, 0.f);

  for (size_t k = 0; k < x.size(); ++k)
  {
    float xs = 0.5f * renderer.get_hmap_wx() * (2.f * x[k] - 1.f);
    float hs = renderer.get_hmap_h0() + renderer.get_hmap_h() * h[k];
    float ys = 0.5f * renderer.get_hmap_wy() * (2.f * y[k] - 1.f);
    float rs = 2.f * radius[k];
    float rotation = (float)std::rand() / RAND_MAX * glm::two_pi<float>();

    instances.push_back({glm::vec3(xs, hs, ys), rs, rotation, color});
  }

  auto  mesh = std::make_shared<Mesh>();
  float r = 1.f;
  renderer.makeCurrent();
  generate_tree(*mesh, r, 0.1f * r, 5.f * r, r, 5);
  renderer.set_instanced_mesh(keys::mesh::trees, mesh, instances);
  renderer.doneCurrent();
}

void set_leaves(RenderWidget             &renderer,
                const std::vector<float> &x,
                const std::vector<float> &y,
                const std::vector<float> &h,
                const std::vector<float> &radius)
{
  qtr::Logger::log()->trace("qtr::set_leaves");

  if (x.size() != y.size() || x.size() != h.size() || x.size() != radius.size())
    throw std::invalid_argument("qtr::set_leaves: vector sizes do not match");

  std::vector<BaseInstance> instances;
  glm::vec3                 color = glm::vec3(0.f, 1.f, 0.f);

  for (size_t k = 0; k < x.size(); ++k)
  {
    float xs = 0.5f * renderer.get_hmap_wx() * (2.f * x[k] - 1.f);
    float hs = renderer.get_hmap_h0() + renderer.get_hmap_h() * h[k];
    float ys = 0.5f * renderer.get_hmap_wy() * (2.f * y[k] - 1.f);
    float rs = 2.f * radius[k];
    float rotation = (float)std::rand() / RAND_MAX * glm::two_pi<float>();

    instances.push_back({glm::vec3(xs, hs, ys), rs, rotation, color});
  }

  auto  mesh = std::make_shared<Mesh>();
  float r = 1.f;
  renderer.makeCurrent();
  generate_grass_leaf_2sided(*mesh, glm::vec3(0.f, 0.f, 0.f), r, 0.1f * r);
  renderer.set_instanced_mesh(keys::mesh::leaves, mesh, instances);
  renderer.doneCurrent();
}

} // namespace qtr
