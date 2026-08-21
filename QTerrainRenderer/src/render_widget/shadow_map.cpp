/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/windows_patch.hpp"

#include "qtr/render_widget.hpp"

namespace qtr
{

void RenderWidget::render_shadow_map(const glm::mat4 &model,
                                     glm::mat4       &light_space_matrix)
{
  // shadow depth pass, camera at the light position
  this->camera_shadow_pass.position = this->light.position;
  this->camera_shadow_pass.near_plane = 0.f;
  this->camera_shadow_pass.far_plane = 100.f;

  float     ortho_size = 1.5f; // TODO hardcoded
  glm::mat4 light_projection = this->camera_shadow_pass.get_projection_matrix_ortho(
      ortho_size);
  glm::mat4 light_view = this->camera_shadow_pass.get_view_matrix();
  light_space_matrix = light_projection * light_view;

  QOpenGLShaderProgram *p_shader = this->sp_shader_manager->get("shadow_map_depth_pass")
                                       ->get();

  if (p_shader)
  {
    Texture *p_tex = this->sp_texture_manager->get(keys::tex::shadow_map);

    glViewport(0, 0, p_tex->get_width(), p_tex->get_height());
    glBindFramebuffer(GL_FRAMEBUFFER, this->fbo);

    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);

    p_shader->bind();
    p_shader->setUniformValue("light_space_matrix", toQMat(light_space_matrix));
    p_shader->setUniformValue("model", toQMat(model));

    auto *plane_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::plane);
    if (plane_drawable && plane_drawable->render_params.visible &&
        plane_drawable->render_params.cast_shadow)
      plane_drawable->draw(p_shader);

    auto *hmap_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::hmap);
    if (hmap_drawable && hmap_drawable->render_params.visible &&
        hmap_drawable->render_params.cast_shadow)
      hmap_drawable->draw(p_shader);

    // no water (cast_shadow is false on water by default)
    auto *water_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::water);
    if (water_drawable && water_drawable->render_params.visible &&
        water_drawable->render_params.cast_shadow)
      water_drawable->draw(p_shader);

    auto *rocks_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::rocks);
    if (rocks_drawable && rocks_drawable->render_params.visible &&
        rocks_drawable->render_params.cast_shadow)
      rocks_drawable->draw(p_shader);

    auto *leaves_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::leaves);
    if (leaves_drawable && leaves_drawable->render_params.visible &&
        leaves_drawable->render_params.cast_shadow)
      leaves_drawable->draw(p_shader);

    auto *trees_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::trees);
    if (trees_drawable && trees_drawable->render_params.visible &&
        trees_drawable->render_params.cast_shadow)
      trees_drawable->draw(p_shader);

    p_shader->release();

    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, this->defaultFramebufferObject());
  }
}

} // namespace qtr
