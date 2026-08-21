/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/windows_patch.hpp"

#include <stdexcept>

#include <QOpenGLFunctions>

#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include "qtr/config.hpp"
#include "qtr/imgui_widgets.hpp"
#include "qtr/logger.hpp"
#include "qtr/mesh.hpp"
#include "qtr/primitives.hpp"
#include "qtr/render_widget.hpp"
#include "qtr/utils.hpp"

namespace qtr
{

void RenderWidget::render_scene_render_3d()
{
  if (QOpenGLContext::currentContext() != this->context())
    this->makeCurrent();

  // model
  bool flip_x = qtr::Config::get_config()->viewer3d.flip_x;
  bool flip_y = qtr::Config::get_config()->viewer3d.flip_y;

  float cx = flip_x ? -1.f : 1.f;
  float cy = flip_y ? -1.f : 1.f;

  glm::mat4 model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(cx, this->scale_h, cy));

  // shadow map
  glm::mat4 light_space_matrix;
  this->render_shadow_map(model, light_space_matrix);

  // projection - guard against zero height
  int h = this->height();
  int w = this->width();
  if (h <= 0 || w <= 0)
    return;

  float aspect_ratio = static_cast<float>(w) / static_cast<float>(h);

  glm::mat4 projection = this->camera.get_projection_matrix_perspective(aspect_ratio);

  // depth map
  this->render_depth_map(model, this->camera.get_view_matrix(), projection);

  // --- main lit pass

  this->setup_gl_state();

  QOpenGLShaderProgram *p_shader = this->sp_shader_manager->get("shadow_map_lit_pass")
                                       ->get();

  if (p_shader)
  {
    p_shader->bind();

    this->set_common_uniforms(*p_shader,
                              model,
                              projection,
                              this->camera.get_view_matrix(),
                              light_space_matrix);

    // base plane
    auto *plane_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::plane);
    if (plane_drawable && plane_drawable->render_params.visible)
    {
      p_shader->setUniformValue("base_color",
                                toQVec(plane_drawable->render_params.base_color));
      p_shader->setUniformValue("add_ambiant_occlusion", false);
      plane_drawable->draw(p_shader);
    }

    // points
    auto *points_drawable =
        this->sp_mesh_manager->get_drawable(keys::mesh::points);
    if (points_drawable && points_drawable->render_params.visible)
    {
      p_shader->setUniformValue("add_ambiant_occlusion", false);
      points_drawable->draw(p_shader);
    }

    // path
    auto *path_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::path);
    if (path_drawable && path_drawable->render_params.visible)
    {
      p_shader->setUniformValue("base_color",
                                toQVec(path_drawable->render_params.base_color));
      p_shader->setUniformValue("add_ambiant_occlusion", false);
      path_drawable->draw(p_shader);
    }

    // heightmap
    auto *hmap_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::hmap);
    if (hmap_drawable && hmap_drawable->render_params.visible)
    {
      p_shader->setUniformValue("base_color",
                                toQVec(hmap_drawable->render_params.base_color));
      p_shader->setUniformValue(
          "use_texture_albedo",
          true && !this->bypass_texture_albedo &&
              this->sp_texture_manager->get(keys::tex::albedo)->is_active());

      if (this->sp_texture_manager->get(keys::tex::normal)->is_active())
        p_shader->setUniformValue("normal_map_scaling", this->normal_map_scaling);

      p_shader->setUniformValue("add_ambiant_occlusion", this->add_ambiant_occlusion);
      hmap_drawable->draw(p_shader);

      p_shader->setUniformValue("normal_map_scaling", 0.f);
      p_shader->setUniformValue("use_texture_albedo", false);
    }

    auto *rocks_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::rocks);
    if (rocks_drawable && rocks_drawable->render_params.visible)
      rocks_drawable->draw(p_shader);

    auto *leaves_drawable =
        this->sp_mesh_manager->get_drawable(keys::mesh::leaves);
    if (leaves_drawable && leaves_drawable->render_params.visible)
      leaves_drawable->draw(p_shader);

    auto *trees_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::trees);
    if (trees_drawable && trees_drawable->render_params.visible)
      trees_drawable->draw(p_shader);

    auto *water_drawable = this->sp_mesh_manager->get_drawable(keys::mesh::water);
    if (water_drawable && water_drawable->render_params.visible)
    {
      p_shader->setUniformValue("spec_strength", this->water_spec_strength);

      p_shader->setUniformValue("add_ambiant_occlusion", false);
      p_shader->setUniformValue("use_texture_albedo", false);

      p_shader->setUniformValue("use_water_colors", true);

      p_shader->setUniformValue("color_shallow_water", toQVec(this->color_shallow_water));
      p_shader->setUniformValue("color_deep_water", toQVec(this->color_deep_water));

      p_shader->setUniformValue("water_color_depth", this->water_color_depth);

      p_shader->setUniformValue("add_water_foam", this->add_water_foam);
      p_shader->setUniformValue("foam_color", toQVec(this->foam_color));
      p_shader->setUniformValue("foam_depth", this->foam_depth);

      p_shader->setUniformValue("add_water_waves", this->add_water_waves);
      p_shader->setUniformValue("angle_spread_ratio", this->angle_spread_ratio);
      p_shader->setUniformValue("waves_alpha", this->waves_alpha);
      p_shader->setUniformValue("waves_kw", this->waves_kw);
      p_shader->setUniformValue("waves_amplitude", this->waves_amplitude);
      p_shader->setUniformValue("waves_normal_amplitude", this->waves_normal_amplitude);

      if (this->animate_waves)
        p_shader->setUniformValue("waves_speed", this->waves_speed);
      else
        p_shader->setUniformValue("waves_speed", 0.f);

      // either use the input mesh or use a simple plane surface as a
      // fallback
      if (water_drawable->is_active())
        water_drawable->draw(p_shader);
    }

    this->unbind_textures();

    p_shader->release();
  }
}

void RenderWidget::render_ui_render_3d()
{
  // must precede any ImGui call: destroying another RenderWidget leaves the
  // global current context null (its destructor destroys the context it
  // selected), and GetIO() dereferences the global
  ImGui::SetCurrentContext(this->imgui_context);

  {
    const float dpr = this->devicePixelRatioF();
    ImGuiIO    &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(this->width()) * dpr, float(this->height()) * dpr);
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();

  // --- Overlay: FPS ---
  bool changed = false;

  changed |= imgui_viewer_main_menubar(*this);

  ImGui::SetNextWindowBgAlpha(0.95f);
  ImGui::Begin("Render settings");

  // --- View & Camera ---
  ImGui::SeparatorText("View");
  changed |= ImGui::Checkbox("Normal visualization", &this->normal_visualization);
  ImGui::SameLine();
  changed |= ImGui::Checkbox("Wireframe", &this->wireframe_mode);
  changed |= ImGui::SliderFloat("Height scale", &this->scale_h, 0.f, 2.f);
  changed |= ImGui::SliderAngle("FOV", &this->camera.fov, 10.f, 180.f);
  changed |= ImGui::Checkbox("Auto rotate cam.", &this->auto_rotate_camera);

  if (ImGui::Button("Reset Camera"))
  {
    this->reset_camera_position();
    this->need_update = true;
  }

  // --- Rendering Toggles ---
  ImGui::SeparatorText("Render Options");

  if (ImGui::BeginTable("#CheckGrid", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    changed |= ImGui::Checkbox(
        "Plane",
        &this->sp_mesh_manager->get_render_params(keys::mesh::plane)->visible);
    //
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    changed |= ImGui::Checkbox(
        "Terrain",
        &this->sp_mesh_manager->get_render_params(keys::mesh::hmap)->visible);
    ImGui::TableNextColumn();
    changed |= ImGui::Checkbox(
        "Water##render",
        &this->sp_mesh_manager->get_render_params(keys::mesh::water)->visible);
    //
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    changed |= ImGui::Checkbox(
        "Points",
        &this->sp_mesh_manager->get_render_params(keys::mesh::points)->visible);
    ImGui::SameLine();
    ImGui::TableNextColumn();
    changed |= ImGui::Checkbox(
        "Path",
        &this->sp_mesh_manager->get_render_params(keys::mesh::path)->visible);
    //
    // ImGui::TableNextRow();
    // ImGui::TableNextColumn();
    // changed |= ImGui::Checkbox("Rocks",
    // &this->sp_mesh_manager->get_render_params(RenderWidget::MESH_ROCKS)->visible);
    // ImGui::TableNextColumn();
    // changed |= ImGui::Checkbox("Trees",
    // &this->sp_mesh_manager->get_render_params(RenderWidget::MESH_TREES)->visible);

    ImGui::EndTable();
  }

  // --- Materials ---
  if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImGui::Text("Albedo");
    changed |= ImGui::SliderFloat("Gamma", &this->gamma_correction, 0.01f, 4.f);
    changed |= ImGui::Checkbox("Bypass albedo", &this->bypass_texture_albedo);
    changed |= ImGui::Checkbox("Tonemap", &this->apply_tonemap);

    ImGui::Text("Normal Map");
    changed |= ImGui::SliderFloat("Scaling", &this->normal_map_scaling, 0.f, 2.f);
  }

  // --- Lighting ---
  if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
  {
    changed |= ImGui::SliderAngle("Azimuth", &this->light_phi, -180.f, 180.f);
    changed |= ImGui::SliderAngle("Zenith", &this->light_theta, 0.f, 90.f);
    changed |= ImGui::Checkbox("Auto rotate", &this->auto_rotate_light);

    ImGui::Text("Shadow Map");
    changed |= ImGui::Checkbox("Bypass", &this->bypass_shadow_map);
    changed |= ImGui::SliderFloat("Strength", &this->shadow_strength, 0.f, 1.f);

    if (ImGui::TreeNode("Ambient Occlusion"))
    {
      changed |= ImGui::Checkbox("Enable AO", &this->add_ambiant_occlusion);
      changed |= ImGui::SliderFloat("Strength",
                                    &this->ambiant_occlusion_strength,
                                    0.f,
                                    1.f);
      changed |= ImGui::SliderFloat("Radius", &this->ambiant_occlusion_radius, 0.f, 0.5f);
      ImGui::TreePop();
    }
  }

  // --- Water ---
  if (ImGui::CollapsingHeader("Water", ImGuiTreeNodeFlags_DefaultOpen))
  {
    changed |= ImGui::SliderFloat("Color depth", &this->water_color_depth, 0.f, 0.2f);
    changed |= ImGui::SliderFloat("Specularity", &this->water_spec_strength, 0.f, 1.f);
    changed |= imgui_show_water_preset_selector(this->color_shallow_water,
                                                this->color_deep_water);

    ImGui::Separator();

    changed |= ImGui::Checkbox("Foam", &this->add_water_foam);
    if (this->add_water_foam)
      changed |= ImGui::SliderFloat("Foam depth", &this->foam_depth, 0.f, 0.1f);

    changed |= ImGui::Checkbox("Waves", &this->add_water_waves);
    if (this->add_water_waves)
    {
      changed |= ImGui::SliderFloat("Wavenumber", &this->waves_kw, 0.f, 2048.f);
      changed |= ImGui::SliderFloat("Amplitude", &this->waves_amplitude, 0.f, 0.1f);
      changed |= ImGui::SliderFloat("Normal amplitude",
                                    &this->waves_normal_amplitude,
                                    0.f,
                                    0.1f);
      changed |= ImGui::SliderAngle("Angle", &this->waves_alpha, -180.f, 180.f);
      changed |= ImGui::SliderFloat("Angle spread", &this->angle_spread_ratio, 0.f, 0.1f);
      changed |= ImGui::Checkbox("Animate", &this->animate_waves);
      if (this->animate_waves)
        changed |= ImGui::SliderFloat("Speed", &this->waves_speed, 0.f, 1.f);
    }
  }

  // --- Atmosphere ---
  if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
  {
    changed |= ImGui::Checkbox("Fog", &this->add_fog);
    changed |= ImGui::SliderFloat("Density##fog", &this->fog_density, 0.f, 100.f);
    changed |= ImGui::SliderFloat("Height##fog", &this->fog_height, 0.f, 1.f);
    changed |= ImGui::ColorEdit3("Color##fog", glm::value_ptr(this->fog_color));

    changed |= ImGui::Checkbox("Scattering", &this->add_atmospheric_scattering);
    changed |= ImGui::SliderFloat("Density##scat", &this->scattering_density, 0.f, 1.f);
    changed |= ImGui::SliderFloat("Fog strength##scat", &this->fog_strength, 0.f, 1.f);
    changed |= ImGui::SliderFloat("Scattering ratio##scat",
                                  &this->fog_scattering_ratio,
                                  0.f,
                                  1.f);
    changed |= ImGui::ColorEdit3("Rayleigh color", glm::value_ptr(this->rayleigh_color));
    changed |= ImGui::ColorEdit3("Mie color", glm::value_ptr(this->mie_color));
  }

  // --- Mouse controls overlay ---
  if (QTR_CONFIG->viewer3d.show_mouse_control)
  {
    // Offset from top-right corner
    const ImVec2 padding(20.0f, 20.0f);

    // Get main viewport
    ImGuiViewport *viewport = ImGui::GetMainViewport();

    // Position at top-right
    ImVec2 pos = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - padding.x,
                        viewport->WorkPos.y + padding.y);

    // Window flags for overlay
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    // Remove frame/border
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    // Set exact screen position
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);

    if (ImGui::Begin("MouseControlsOverlay", nullptr, flags))
    {
      ImGui::Text("LMB: Rotate");
      ImGui::Text("Wheel: Zoom");
      ImGui::Text("RMB: Pan");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  // --- End main window ---
  this->need_update |= changed;
  ImGui::End();

  // --- IO / camera control ---
  ImGuiIO &io = this->get_imgui_io();

  if (!io.WantCaptureMouse) // only outside the ImGUI window
  {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
      this->alpha_y -= io.MouseDelta.x * 0.005f;
      this->alpha_x += io.MouseDelta.y * 0.005f;
      this->alpha_x = glm::clamp(this->alpha_x,
                                 -0.99f * glm::half_pi<float>(),
                                 0.99f * glm::half_pi<float>());
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
      this->pan_offset.x -= io.MouseDelta.x * 0.001f * this->distance;
      this->pan_offset.y += io.MouseDelta.y * 0.001f * this->distance;
    }

    if (io.MouseWheel != 0.0f)
    {
      this->distance *= (1.0f - io.MouseWheel * 0.1f);
      this->distance = glm::clamp(this->distance, 0.f, 50.0f);
    }
  }
  else
  {
    // to force frame update for ImGUI submenus...
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
        ImGui::IsMouseReleased(ImGuiMouseButton_Right))
      this->need_update = true;
  }

  // --- Render ---
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace qtr
