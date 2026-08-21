/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/mesh_manager.hpp"
#include "qtr/logger.hpp"

namespace qtr
{

MeshManager::~MeshManager()
{
  qtr::Logger::log()->trace("MeshManager::~MeshManager");
  this->clear();
}

Mesh &MeshManager::add_mesh(const std::string &name)
{
  auto  standard_drawable = std::make_unique<StandardDrawableMesh>();
  Mesh &ref = standard_drawable->mesh;
  this->drawables[name] = std::move(standard_drawable);
  return ref;
}

InstancedMesh<BaseInstance> &MeshManager::add_instanced_mesh(const std::string &name)
{
  auto instanced_drawable = std::make_unique<InstancedDrawableMesh<BaseInstance>>();
  InstancedMesh<BaseInstance> &ref = instanced_drawable->instanced_mesh;
  this->drawables[name] = std::move(instanced_drawable);
  return ref;
}

Mesh *MeshManager::get_mesh(const std::string &name)
{
  auto it = this->drawables.find(name);
  if (it != this->drawables.end())
  {
    auto *std_mesh = dynamic_cast<StandardDrawableMesh *>(it->second.get());
    if (std_mesh)
      return &std_mesh->mesh;
  }
  qtr::Logger::log()->error("MeshManager::get_mesh: mesh not found or wrong type: {}",
                            name);
  return nullptr;
}

InstancedMesh<BaseInstance> *MeshManager::get_instanced_mesh(const std::string &name)
{
  auto it = this->drawables.find(name);
  if (it != this->drawables.end())
  {
    auto *inst_mesh = dynamic_cast<InstancedDrawableMesh<BaseInstance> *>(
        it->second.get());
    if (inst_mesh)
      return &inst_mesh->instanced_mesh;
  }
  qtr::Logger::log()->error(
      "MeshManager::get_instanced_mesh: mesh not found or wrong type: {}",
      name);
  return nullptr;
}

IDrawableMesh *MeshManager::get_drawable(const std::string &name)
{
  auto it = this->drawables.find(name);
  if (it != this->drawables.end())
    return it->second.get();
  return nullptr;
}

MeshRenderParams *MeshManager::get_render_params(const std::string &name)
{
  auto *drawable = this->get_drawable(name);
  if (drawable)
    return &drawable->render_params;
  return nullptr;
}

bool MeshManager::is_visible(const std::string &name) const
{
  auto it = this->drawables.find(name);
  if (it != this->drawables.end())
    return it->second->render_params.visible;
  return false;
}

void MeshManager::set_visible(const std::string &name, bool visible)
{
  auto it = this->drawables.find(name);
  if (it != this->drawables.end())
    it->second->render_params.visible = visible;
}

void MeshManager::destroy(const std::string &name)
{
  auto it = this->drawables.find(name);
  if (it != this->drawables.end())
    it->second->destroy();
}

void MeshManager::destroy_all()
{
  for (auto &[_, drawable] : this->drawables)
  {
    if (drawable)
      drawable->destroy();
  }
}

void MeshManager::clear()
{
  this->destroy_all();
  this->drawables.clear();
}

} // namespace qtr
