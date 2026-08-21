/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QOpenGLShaderProgram>
#include <glm/glm.hpp>

#include "qtr/instanced_mesh.hpp"
#include "qtr/mesh.hpp"

namespace qtr
{

struct MeshRenderParams
{
  bool      visible = true;
  bool      cast_shadow = true;
  bool      depth_pass = true;
  bool      add_ambient_occlusion = false;
  glm::vec3 base_color = glm::vec3(1.0f);
};

// Abstract drawable wrapper interface
class IDrawableMesh
{
public:
  virtual ~IDrawableMesh() = default;
  virtual void draw(QOpenGLShaderProgram *p_shader) = 0;
  virtual void destroy() = 0;
  virtual bool is_active() const = 0;

  MeshRenderParams render_params;
};

class StandardDrawableMesh : public IDrawableMesh
{
public:
  Mesh mesh;

  void draw(QOpenGLShaderProgram * /*p_shader*/) override { mesh.draw(); }

  void destroy() override { mesh.destroy(); }

  bool is_active() const override { return mesh.is_active(); }
};

template <typename InstanceT = BaseInstance>
class InstancedDrawableMesh : public IDrawableMesh
{
public:
  InstancedMesh<InstanceT> instanced_mesh;

  void draw(QOpenGLShaderProgram *p_shader) override { instanced_mesh.draw(p_shader); }

  void destroy() override { instanced_mesh.destroy(); }

  bool is_active() const override { return instanced_mesh.is_active(); }
};

class MeshManager
{
public:
  MeshManager() = default;
  ~MeshManager();

  // Mesh registration
  Mesh                        &add_mesh(const std::string &name);
  InstancedMesh<BaseInstance> &add_instanced_mesh(const std::string &name);

  // Accessors
  Mesh                        *get_mesh(const std::string &name);
  InstancedMesh<BaseInstance> *get_instanced_mesh(const std::string &name);
  IDrawableMesh               *get_drawable(const std::string &name);

  // Parameter access
  MeshRenderParams *get_render_params(const std::string &name);
  bool              is_visible(const std::string &name) const;
  void              set_visible(const std::string &name, bool visible);

  // Lifecycle
  void destroy(const std::string &name);
  void destroy_all();
  void clear();

  const std::map<std::string, std::unique_ptr<IDrawableMesh>> &get_drawables() const
  {
    return drawables;
  }

private:
  std::map<std::string, std::unique_ptr<IDrawableMesh>> drawables;
};

} // namespace qtr
