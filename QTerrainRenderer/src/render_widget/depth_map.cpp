/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/windows_patch.hpp"

#include "qtr/render_widget.hpp"

namespace qtr
{

void RenderWidget::render_depth_map(const glm::mat4 &model,
                                    const glm::mat4 &view,
                                    const glm::mat4 &projection)
{
  QOpenGLShaderProgram *p_shader = this->sp_shader_manager->get("depth_map")->get();

  if (p_shader)
  {
    Texture *p_tex = this->sp_texture_manager->get(keys::tex::depth);

    glViewport(0, 0, p_tex->get_width(), p_tex->get_height());
    glBindFramebuffer(GL_FRAMEBUFFER, this->fbo_depth);

    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    p_shader->bind();
    p_shader->setUniformValue("model", toQMat(model));
    p_shader->setUniformValue("view", toQMat(view));
    p_shader->setUniformValue("projection", toQMat(projection));

    auto *plane_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::plane);
    if (plane_drawable && plane_drawable->render_params.visible &&
        plane_drawable->render_params.depth_pass)
      plane_drawable->draw(p_shader);

    auto *hmap_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::hmap);
    if (hmap_drawable && hmap_drawable->render_params.visible &&
        hmap_drawable->render_params.depth_pass)
      hmap_drawable->draw(p_shader);

    auto *water_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::water);
    if (water_drawable && water_drawable->render_params.visible &&
        water_drawable->render_params.depth_pass)
      water_drawable->draw(p_shader);

    auto *leaves_drawable =
        this->sp_mesh_manager->get_drawable(keys::mesh::leaves);
    if (leaves_drawable && leaves_drawable->render_params.visible &&
        leaves_drawable->render_params.depth_pass)
      leaves_drawable->draw(p_shader);

    auto *rocks_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::rocks);
    if (rocks_drawable && rocks_drawable->render_params.visible &&
        rocks_drawable->render_params.depth_pass)
      rocks_drawable->draw(p_shader);

    auto *trees_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::trees);
    if (trees_drawable && trees_drawable->render_params.visible &&
        trees_drawable->render_params.depth_pass)
      trees_drawable->draw(p_shader);

    p_shader->release();

    glBindFramebuffer(GL_FRAMEBUFFER, this->defaultFramebufferObject());
  }
}

} // namespace qtr
