/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/windows_patch.hpp"

#include <stdexcept>

#include <QMessageBox>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
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

RenderWidget::RenderWidget(const std::string &_title, QWidget *parent)
    : QOpenGLWidget(parent), title(_title)
{
  qtr::Logger::log()->trace("RenderWidget::RenderWidget");

  // NOTE: deliberately no setFormat() here. Requesting 3.3 core per-widget
  // makes initializeOpenGLFunctions() fail on every run on the Windows/NVIDIA
  // test machine, including single-viewport projects that worked before -- the
  // application already runs a QtWebEngine global shared context, and a
  // per-widget format that conflicts with it cannot be honoured. The driver's
  // default context does expose 3.3 core there. If a machine is ever found
  // where the default context is genuinely too old, the fix belongs in a
  // QSurfaceFormat::setDefaultFormat() call before QApplication, not here.

  this->setWindowTitle(this->title.c_str());
  this->setFocusPolicy(Qt::StrongFocus);
  this->setMouseTracking(true);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // force 60 FPS update
  this->connect(&frame_timer,
                &QTimer::timeout,
                [this]()
                {
                  if (this->need_update)
                  {
                    this->update();
                    this->need_update = false;
                  }
                });
  this->frame_timer.start(16);

  // init.
  this->timer.start(); // global timer
  this->reset_camera_position();

  // managers
  this->sp_shader_manager = std::make_unique<ShaderManager>();
  this->sp_texture_manager = std::make_unique<TextureManager>();
  this->sp_mesh_manager = std::make_unique<MeshManager>();

  // add meshes
  this->sp_mesh_manager->add_mesh(keys::mesh::plane);
  this->sp_mesh_manager->add_mesh(keys::mesh::hmap);
  this->sp_mesh_manager->add_mesh(keys::mesh::water);
  this->sp_mesh_manager->add_mesh(keys::mesh::path);
  this->sp_mesh_manager->add_instanced_mesh(keys::mesh::points);
  this->sp_mesh_manager->add_instanced_mesh(keys::mesh::rocks);
  this->sp_mesh_manager->add_instanced_mesh(keys::mesh::trees);
  this->sp_mesh_manager->add_instanced_mesh(keys::mesh::leaves);
  this->sp_mesh_manager->add_mesh(keys::mesh::skybox);

  // configure default render parameters
  this->sp_mesh_manager->get_render_params(keys::mesh::plane)->base_color = glm::vec3(
      0.2f,
      0.2f,
      0.2f);
  this->sp_mesh_manager->get_render_params(keys::mesh::path)->base_color = glm::vec3(
      1.0f,
      0.0f,
      1.0f);
  this->sp_mesh_manager->get_render_params(keys::mesh::water)->cast_shadow = false;
  this->sp_mesh_manager->get_render_params(keys::mesh::skybox)->cast_shadow = false;

  // add placeholder for each texture
  const std::vector<std::string> tex_names = {keys::tex::albedo,
                                              keys::tex::hmap,
                                              keys::tex::normal,
                                              keys::tex::shadow_map,
                                              keys::tex::depth,
                                              keys::tex::skybox};
  for (auto &s : tex_names)
    this->sp_texture_manager->add(s);
}

RenderWidget::~RenderWidget()
{
  if (this->context() && this->imgui_context)
  {
    // make THIS widget's GL context current before the ImGui backend
    // shutdown: it issues glDelete* calls on buffer/program names that only
    // exist in this widget's context. With another widget's context current,
    // those deletes destroy the OTHER widget's identically-numbered objects,
    // and its next ImGui draw segfaults (unbound element buffer -> index
    // offset read as a client pointer at ~NULL)
    this->makeCurrent();
    ImGui::SetCurrentContext(this->imgui_context);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext(this->imgui_context);
    this->imgui_context = nullptr;
    this->doneCurrent();
  }
}

void RenderWidget::clear()
{
  qtr::Logger::log()->trace("RenderWidget::clear");

  // nothing was created if GL init never completed, and the calls below would
  // run through unresolved 3.3-core function pointers
  if (!this->initial_gl_done)
    return;

  this->makeCurrent();

  this->sp_mesh_manager->destroy_all();
  this->reset_textures();

  // reset_textures calls doneCurrent(), so re-acquire context for this widget
  this->makeCurrent();

  // Re-create permanent base meshes (plane and skybox cube)
  generate_plane(*this->sp_mesh_manager->get_mesh(keys::mesh::plane),
                 0.f,
                 -1e-3f,
                 0.f,
                 2000.f * this->hmap_wx,
                 2000.f * this->hmap_wx);

  Mesh *skybox_mesh = this->sp_mesh_manager->get_mesh(keys::mesh::skybox);
  if (skybox_mesh)
    generate_cube(*skybox_mesh, 0.f, 0.f, 0.f, 2.f, 2.f, 2.f);

  this->need_update = true;

  this->doneCurrent();
}

ImGuiIO &RenderWidget::get_imgui_io()
{
  ImGui::SetCurrentContext(this->imgui_context);
  return ImGui::GetIO();
}

bool RenderWidget::get_bypass_texture_albedo() const
{
  return this->bypass_texture_albedo;
}

bool RenderWidget::is_mesh_visible(const std::string &name) const
{
  return this->sp_mesh_manager->is_visible(name);
}

MeshManager &RenderWidget::get_mesh_manager() { return *this->sp_mesh_manager; }

Mesh &RenderWidget::get_water_mesh()
{
  return *this->sp_mesh_manager->get_mesh(keys::mesh::water);
}

void RenderWidget::initializeGL()
{
  qtr::Logger::log()->trace("RenderWidget::initializeGL");

  this->makeCurrent();

  const auto fail_init = [this](const char *why)
  {
    qtr::Logger::log()->critical(
        "RenderWidget::initializeGL: {} - 3D view disabled for this widget",
        why);
    this->gl_init_failed = true;

    // log what was actually obtained, not what was asked for: without this the
    // failure message says nothing about why resolution failed. Uses only
    // QOpenGLContext / QOpenGLFunctions (the ES2 subset, always resolvable) --
    // the 3.3-core table is what just failed.
    if (QOpenGLContext *ctx = this->context())
    {
      const QSurfaceFormat obtained = ctx->format();
      const char          *renderer = nullptr;

      if (QOpenGLFunctions *fns = ctx->functions())
      {
        fns->initializeOpenGLFunctions();
        renderer = reinterpret_cast<const char *>(fns->glGetString(GL_RENDERER));
      }

      qtr::Logger::log()->critical(
          "RenderWidget::initializeGL: obtained context was OpenGL {}.{} {} "
          "profile, renderer: {}",
          obtained.majorVersion(),
          obtained.minorVersion(),
          obtained.profile() == QSurfaceFormat::CoreProfile            ? "core"
          : obtained.profile() == QSurfaceFormat::CompatibilityProfile ? "compatibility"
                                                                       : "no",
          renderer ? renderer : "unknown");
    }

    // no ImGui context exists on this path: the input handlers dereference
    // it via get_imgui_io, so stop all input by disabling the widget
    this->setEnabled(false);

    // deferred: popping a modal dialog from inside initializeGL is unsafe
    QTimer::singleShot(0,
                       this,
                       [this]()
                       {
                         QMessageBox::critical(
                             this,
                             "3D view unavailable",
                             "The 3D view could not initialize OpenGL 3.3 "
                             "(core profile) on this system.\nSee the log for "
                             "details.");
                       });
  };

  if (!this->context() || !this->context()->isValid())
  {
    fail_init("no valid OpenGL context");
    return;
  }

  if (!this->initializeOpenGLFunctions())
  {
    fail_init("OpenGL 3.3 core functions could not be resolved");
    return;
  }

  // info level so it shows in release logs: the first thing needed when
  // debugging driver-specific issues (e.g. otto-link/Hesiod#537)
  {
    const QSurfaceFormat obtained = this->context()->format();
    const char          *vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));

    qtr::Logger::log()->info(
        "RenderWidget::initializeGL: OpenGL {}.{} {} profile, vendor: {}, "
        "renderer: {}",
        obtained.majorVersion(),
        obtained.minorVersion(),
        obtained.profile() == QSurfaceFormat::CoreProfile ? "core" : "compatibility",
        vendor ? vendor : "unknown",
        renderer ? renderer : "unknown");
  }

  // --- Shaders

  qtr::Logger::log()->trace("RenderWidget::initializeGL: setting up shaders...");

  this->sp_shader_manager->add_shader_from_code("diffuse_basic",
                                                diffuse_basic_vertex,
                                                diffuse_basic_frag);

  this->sp_shader_manager->add_shader_from_code("diffuse_phong",
                                                diffuse_basic_vertex,
                                                diffuse_phong_frag);

  this->sp_shader_manager->add_shader_from_code("diffuse_blinn_phong",
                                                diffuse_basic_vertex,
                                                diffuse_blinn_phong_frag);

  this->sp_shader_manager->add_shader_from_code("depth_map",
                                                depth_map_vertex,
                                                depth_map_frag);

  this->sp_shader_manager->add_shader_from_code("shadow_map_depth_pass",
                                                shadow_map_depth_pass_vertex,
                                                shadow_map_depth_pass_frag);

  this->sp_shader_manager->add_shader_from_code("shadow_map_lit_pass",
                                                shadow_map_lit_pass_vertex,
                                                shadow_map_lit_pass_frag);

  this->sp_shader_manager->add_shader_from_code("viewer2d_cmap",
                                                viewer2d_cmap_vertex,
                                                viewer2d_cmap_frag);

  this->sp_shader_manager->add_shader_from_code("skybox", skybox_vertex, skybox_frag);

  // --- Meshes

  // keep the plane square, use hmap_wx for both directions
  generate_plane(*this->sp_mesh_manager->get_mesh(keys::mesh::plane),
                 0.f,
                 -1e-3f,
                 0.f,
                 2000.f * this->hmap_wx,
                 2000.f * this->hmap_wx);

  // skybox unit cube
  generate_cube(*this->sp_mesh_manager->get_mesh(keys::mesh::skybox),
                0.f,
                0.f,
                0.f,
                2.f,
                2.f,
                2.f);

  // --- Textures

  // depth buffer
  {
    int depth_map_res = 512;

    this->sp_texture_manager->add_depth_texture(keys::tex::depth,
                                                depth_map_res,
                                                depth_map_res,
                                                false);

    // create framebuffer for depth map
    glGenFramebuffers(1, &this->fbo_depth);
    glBindFramebuffer(GL_FRAMEBUFFER, this->fbo_depth);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D,
                           this->sp_texture_manager->get(keys::tex::depth)->get_id(),
                           0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // shadow map texture and buffer
  {
    int shadow_map_res = 1024; // 2048;

    this->sp_texture_manager->add_depth_texture(keys::tex::shadow_map,
                                                shadow_map_res,
                                                shadow_map_res,
                                                true);

    // create framebuffer for shadow depth
    glGenFramebuffers(1, &this->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, this->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D,
                           this->sp_texture_manager->get(keys::tex::shadow_map)->get_id(),
                           0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // --- Pending textures (set before initializeGL)
  if (!this->pending_skybox_image.empty() && this->pending_skybox_width > 0)
  {
    qtr::Logger::log()->trace(
        "RenderWidget::initializeGL: uploading deferred skybox texture (width={})",
        this->pending_skybox_width);

    if (this->sp_texture_manager->get(keys::tex::skybox))
      this->sp_texture_manager->get(keys::tex::skybox)
          ->from_image_8bit_rgba(this->pending_skybox_image, this->pending_skybox_width);
    this->pending_skybox_image.clear();
    this->pending_skybox_width = 0;
  }

  for (auto &[name, tex_data] : this->pending_textures)
  {
    qtr::Logger::log()->trace(
        "RenderWidget::initializeGL: uploading deferred texture '{}' (width={})",
        name,
        tex_data.second);

    if (this->sp_texture_manager->get(name))
      this->sp_texture_manager->get(name)->from_image_8bit_rgba(tex_data.first,
                                                                tex_data.second);
  }
  this->pending_textures.clear();

  // --- ImGUI

  qtr::Logger::log()->trace("RenderWidget::initializeGL: setup ImGui context");

  // ImGui context
  IMGUI_CHECKVERSION();
  this->imgui_context = ImGui::CreateContext();
  ImGui::SetCurrentContext(this->imgui_context);
  ImGui::StyleColorsDark();
  imgui_set_blender_style();

  // OpenGL3 backend
  ImGui_ImplOpenGL3_Init("#version 330");

  this->doneCurrent();

  this->initial_gl_done = true;
}

void RenderWidget::reset_camera_position()
{
  // TODO use json
  this->target = glm::vec3(0.0f, 0.0f, 0.0f);
  this->pan_offset = glm::vec2(0.0f, 0.0f);
  this->distance = 5.0f;
  this->alpha_x = 35.f / 180.f * 3.14f;
  this->alpha_y = -25.f / 180.f * 3.14f;

  this->light_phi = -45.f / 180.f * 3.14f;
  this->light_theta = 30.f / 180.f * 3.14f;

  this->need_update = true;
}

void RenderWidget::reset_mesh(const std::string &name)
{
  this->makeCurrent();
  this->sp_mesh_manager->destroy(name);
  if (name == keys::mesh::hmap && this->sp_texture_manager->get(keys::tex::hmap))
    this->sp_texture_manager->get(keys::tex::hmap)->destroy();
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::reset_meshes()
{
  this->makeCurrent();
  this->sp_mesh_manager->destroy_all();
  if (this->sp_texture_manager->get(keys::tex::hmap))
    this->sp_texture_manager->get(keys::tex::hmap)->destroy();
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::reset_texture(const std::string &name)
{
  qtr::Logger::log()->trace("RenderWidget::reset_texture: {}", name);

  this->makeCurrent();
  if (this->sp_texture_manager->get(name))
    this->sp_texture_manager->get(name)->destroy();
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::reset_textures()
{
  qtr::Logger::log()->trace("RenderWidget::reset_textures");

  this->makeCurrent();

  // /!\ do not reset the depth maps
  const std::vector<std::string> tex_names = {keys::tex::albedo,
                                              keys::tex::hmap,
                                              keys::tex::normal};
  for (auto &s : tex_names)
    if (this->sp_texture_manager->get(s))
      this->sp_texture_manager->get(s)->destroy();

  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::resizeEvent(QResizeEvent *event)
{
  this->makeCurrent();
  QOpenGLWidget::resizeEvent(event);
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::resizeGL(int w, int h)
{
  // isValid() is not enough: when initializeOpenGLFunctions() fails the context
  // is still valid, only the versioned function table is unresolved, so
  // glViewport below would call through a null pointer. setEnabled(false) stops
  // input events but Qt keeps delivering resize/paint. Same guard as paintGL.
  if (!isValid() || !this->initial_gl_done)
    return;

  this->makeCurrent();
  this->glViewport(0, 0, w, h);
  if (this->imgui_context)
  {
    ImGui::SetCurrentContext(this->imgui_context);
    const float dpr = this->devicePixelRatioF();
    this->get_imgui_io().DisplaySize = ImVec2(float(this->width()),
                                              float(this->height()));
    this->get_imgui_io().DisplayFramebufferScale = ImVec2(dpr, dpr);
  }
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::set_aspect_ratio(float new_aspect_ratio)
{
  // wx is leading
  this->hmap_wy = this->hmap_wx / new_aspect_ratio;
}

void RenderWidget::set_bypass_texture_albedo(bool new_state)
{
  this->bypass_texture_albedo = new_state;
  this->need_update = true;
}

void RenderWidget::set_common_uniforms(QOpenGLShaderProgram &shader,
                                       const glm::mat4      &model,
                                       const glm::mat4      &projection,
                                       const glm::mat4      &view,
                                       const glm::mat4      &light_space)
{
  // Textures
  this->sp_texture_manager->bind_and_set(shader);

  // Time & matrices
  shader.setUniformValue("time", time);
  shader.setUniformValue("model", toQMat(model));
  shader.setUniformValue("view", toQMat(view));
  shader.setUniformValue("projection", toQMat(projection));
  shader.setUniformValue("light_space_matrix", toQMat(light_space));

  // Camera & light
  shader.setUniformValue("camera_pos", toQVec(camera.position));
  shader.setUniformValue("view_pos", toQVec(camera.position));
  shader.setUniformValue("light_pos", toQVec(light.position));

  // Screen & depth
  const float dpr = this->devicePixelRatioF();
  shader.setUniformValue("screen_size",
                         toQVec(glm::vec2(static_cast<float>(this->width()) * dpr,
                                          static_cast<float>(this->height()) * dpr)));
  shader.setUniformValue("near_plane", camera.near_plane);
  shader.setUniformValue("far_plane", camera.far_plane);

  // Shadow & AO
  shader.setUniformValue("bypass_shadow_map", bypass_shadow_map);
  shader.setUniformValue("shadow_strength", shadow_strength);
  shader.setUniformValue("add_ambiant_occlusion", add_ambiant_occlusion);
  shader.setUniformValue("ambiant_occlusion_strength", ambiant_occlusion_strength);
  shader.setUniformValue("ambiant_occlusion_radius", ambiant_occlusion_radius);

  // Rendering settings
  shader.setUniformValue("has_instances", false);
  shader.setUniformValue("scale_h", scale_h);
  shader.setUniformValue("hmap_h0", this->hmap_h0);
  shader.setUniformValue("hmap_h", this->hmap_h);
  shader.setUniformValue("normal_visualization", normal_visualization);
  shader.setUniformValue("normal_map_scaling", 0.f); // reset by default
  shader.setUniformValue("gamma_correction", gamma_correction);
  shader.setUniformValue("apply_tonemap", apply_tonemap);

  // Effects
  shader.setUniformValue("add_fog", add_fog);
  shader.setUniformValue("fog_color", toQVec(fog_color));
  shader.setUniformValue("fog_density", fog_density);
  shader.setUniformValue("fog_height", fog_height);
  shader.setUniformValue("fog_match_skybox", fog_match_skybox);
  shader.setUniformValue("skybox_mode", static_cast<int>(skybox_mode));
  shader.setUniformValue("skybox_color", toQVec(skybox_color));
  shader.setUniformValue("skybox_rotation", skybox_rotation);
  shader.setUniformValue(
      "has_skybox_texture",
      this->sp_texture_manager->get(keys::tex::skybox) &&
          this->sp_texture_manager->get(keys::tex::skybox)->is_active());
  shader.setUniformValue("add_atmospheric_scattering", add_atmospheric_scattering);
  shader.setUniformValue("scattering_density", scattering_density);
  shader.setUniformValue("rayleigh_color", toQVec(rayleigh_color));
  shader.setUniformValue("mie_color", toQVec(mie_color));
  shader.setUniformValue("fog_strength", fog_strength);
  shader.setUniformValue("fog_scattering_ratio", fog_scattering_ratio);

  // Reset per-object flags
  shader.setUniformValue("use_texture_albedo", false);
  shader.setUniformValue("use_water_colors", false);
  shader.setUniformValue("normal_map_scaling", 0.f);
  shader.setUniformValue("shininess", 32.f);
  shader.setUniformValue("spec_strength", 0.f);
}

void RenderWidget::set_heightmap_geometry(const std::vector<float> &data,
                                          int                       width,
                                          int                       height,
                                          bool                      add_skirt)
{
  qtr::Logger::log()->trace("RenderWidget::set_heightmap_geometry");

  if (this->initial_gl_done)
    this->makeCurrent();

  const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
  this->set_aspect_ratio(aspect_ratio);

  generate_heightmap(*this->sp_mesh_manager->get_mesh(keys::mesh::hmap),
                     data,
                     width,
                     height,
                     0.f,
                     this->hmap_h0,
                     0.f,
                     this->hmap_wx,
                     this->hmap_h,
                     this->hmap_wy,
                     add_skirt,
                     /* add_level */ 0.f,
                     /* exclude_below */ -FLT_MAX,
                     &this->hmap_hmin);

  // regenerate plane (keep the plane square, use hmap_wx for both
  // directions)
  generate_plane(*this->sp_mesh_manager->get_mesh(keys::mesh::plane),
                 0.f,
                 this->hmap_hmin * this->hmap_h - 1e-3f,
                 0.f,
                 2000.f * this->hmap_wx,
                 2000.f * this->hmap_wx);

  this->current_width = width;
  this->current_height = height;
  this->current_add_skirt_state = add_skirt;

  qtr::Logger::log()->trace("RenderWidget::set_heightmap_geometry: w x h = {} x {}",
                            width,
                            height);

  // also generate the heightmap texture /!\ texture of float, scaled
  // as the input, not scaled as what the OpenGL sees (there is an
  // additional this->hmap_h scaling for OpenGL)
  if (this->initial_gl_done && this->sp_texture_manager->get(keys::tex::hmap))
    this->sp_texture_manager->get(keys::tex::hmap)->from_float_vector(data, width);
  this->need_update = true;

  if (this->initial_gl_done)
    this->doneCurrent();
}

void RenderWidget::set_mesh(const std::string &name, std::shared_ptr<Mesh> sp_mesh)
{
  qtr::Logger::log()->trace("RenderWidget::set_mesh: {}", name);

  this->makeCurrent();
  this->sp_mesh_manager->set_mesh(name, sp_mesh);
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::set_instanced_mesh(const std::string               &name,
                                      std::shared_ptr<Mesh>            sp_mesh,
                                      const std::vector<BaseInstance> &instances)
{
  qtr::Logger::log()->trace("RenderWidget::set_instanced_mesh: {}", name);

  this->makeCurrent();
  this->sp_mesh_manager->set_instanced_mesh(name, sp_mesh, instances);
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::set_render_type(const RenderType &new_render_type)
{
  this->render_type = new_render_type;
}

void RenderWidget::set_mesh_visible(const std::string &name, bool visible)
{
  this->sp_mesh_manager->set_visible(name, visible);
  this->need_update = true;
}

void RenderWidget::set_texture(const std::string          &name,
                               const std::vector<uint8_t> &data,
                               int                         width)
{
  qtr::Logger::log()->trace("RenderWidget::set_texture: {}", name);

  this->need_update = true;

  if (!this->initial_gl_done)
  {
    qtr::Logger::log()->trace(
        "RenderWidget::set_texture: OpenGL not initialized yet, deferring texture '{}'",
        name);
    this->pending_textures[name] = {data, width};
    return;
  }

  this->makeCurrent();

  if (this->sp_texture_manager->get(name))
    this->sp_texture_manager->get(name)->from_image_8bit_rgba(data, width);

  this->doneCurrent();
}

void RenderWidget::set_water_geometry(const std::vector<float> &data,
                                      int                       width,
                                      int                       height,
                                      float                     exclude_below)
{
  qtr::Logger::log()->trace("RenderWidget::set_water_geometry");

  this->makeCurrent();

  bool  add_skirt = false;
  float add_level = 0.f;

  const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
  this->set_aspect_ratio(aspect_ratio);

  generate_heightmap(*this->sp_mesh_manager->get_mesh(keys::mesh::water),
                     data,
                     width,
                     height,
                     0.f,
                     this->hmap_h0,
                     0.f,
                     this->hmap_wx,
                     this->hmap_h,
                     this->hmap_wy,
                     add_skirt,
                     add_level,
                     exclude_below);

  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::setup_gl_state()
{
  const float dpr = this->devicePixelRatioF();
  glViewport(0,
             0,
             static_cast<GLsizei>(this->width() * dpr),
             static_cast<GLsizei>(this->height() * dpr));
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  glPolygonMode(GL_FRONT_AND_BACK, this->wireframe_mode ? GL_LINE : GL_FILL);
}

QSize RenderWidget::sizeHint() const { return QSize(QTR_CONFIG->widget.size_hint); }

void RenderWidget::unbind_textures() { this->sp_texture_manager->unbind(); }

void RenderWidget::update_camera()
{
  this->process_keyboard_input(this->dt);

  this->camera.set_position_angles(this->distance, this->alpha_x, this->alpha_y);

  this->camera.position += this->target;
  this->camera.target = this->target;

  if (this->auto_rotate_camera)
  {
    this->alpha_y += 0.5f * this->dt;
    this->need_update = true;
  }
}

void RenderWidget::update_light()
{
  this->light.set_position_spherical(this->light_distance,
                                     this->light_theta,
                                     this->light_phi);

  // actually works with a fixed sun: compensate for the elevation
  // scaling
  this->light.position.y /= this->scale_h;

  if (this->auto_rotate_light)
  {
    this->light_phi += 0.5f * this->dt;
    this->need_update = true;
  }
}

void RenderWidget::update_time()
{
  this->dt = static_cast<float>(this->timer.restart()) / 1000.0f;
  this->time += this->dt;
}

void RenderWidget::set_show_skybox(bool show)
{
  this->show_skybox = show;
  this->need_update = true;
}

void RenderWidget::set_skybox_mode(SkyboxMode mode)
{
  this->skybox_mode = mode;
  this->need_update = true;
}

void RenderWidget::set_skybox_color(const glm::vec3 &color)
{
  this->skybox_color = color;
  this->need_update = true;
}

void RenderWidget::set_skybox_rotation(float rotation_rad)
{
  this->skybox_rotation = rotation_rad;
  this->need_update = true;
}

void RenderWidget::set_fog_match_skybox(bool match)
{
  this->fog_match_skybox = match;
  this->need_update = true;
}

void RenderWidget::set_skybox_image(const std::vector<uint8_t> &data, int width)
{
  qtr::Logger::log()->trace("RenderWidget::set_skybox_image: width={}", width);

  this->skybox_mode = SkyboxMode::SKYBOX_IMAGE;
  this->need_update = true;

  if (!this->initial_gl_done)
  {
    qtr::Logger::log()->trace("RenderWidget::set_skybox_image: OpenGL not initialized "
                              "yet, deferring skybox upload");
    this->pending_skybox_image = data;
    this->pending_skybox_width = width;
    return;
  }

  this->makeCurrent();
  if (this->sp_texture_manager->get(keys::tex::skybox))
    this->sp_texture_manager->get(keys::tex::skybox)->from_image_8bit_rgba(data, width);
  this->doneCurrent();
}

void RenderWidget::set_show_orientation_gizmo(bool show)
{
  this->show_orientation_gizmo = show;
  this->need_update = true;
}

void RenderWidget::set_keyboard_navigation_enabled(bool enabled)
{
  this->keyboard_navigation_enabled = enabled;
  if (!this->keyboard_navigation_enabled)
    this->pressed_keys.clear();
  this->need_update = true;
}

void RenderWidget::set_keyboard_layout(KeyboardLayout layout)
{
  this->keyboard_layout = layout;
  this->need_update = true;
}

void RenderWidget::set_camera_move_speed(float speed)
{
  this->camera_move_speed = std::max(0.01f, speed);
  this->need_update = true;
}

void RenderWidget::process_keyboard_input(float delta_time)
{
  if (this->render_type != RenderType::RENDER_3D)
    return;

  if (!this->keyboard_navigation_enabled)
    return;

  if (this->pressed_keys.empty())
    return;

  ImGuiIO &io = this->get_imgui_io();
  if (io.WantCaptureKeyboard)
    return;

  // determine active keys for current layout
  int key_forward = (this->keyboard_layout == KeyboardLayout::WASD) ? Qt::Key_W
                                                                    : Qt::Key_Z;
  int key_backward = Qt::Key_S;
  int key_left = (this->keyboard_layout == KeyboardLayout::WASD) ? Qt::Key_A : Qt::Key_Q;
  int key_right = Qt::Key_D;
  int key_up = Qt::Key_E;
  int key_down = (this->keyboard_layout == KeyboardLayout::WASD) ? Qt::Key_Q : Qt::Key_A;

  float speed = this->camera_move_speed * delta_time;

  glm::vec3 forward_h(-sin(this->alpha_y), 0.f, -cos(this->alpha_y));
  glm::vec3 right_h(cos(this->alpha_y), 0.f, -sin(this->alpha_y));
  glm::vec3 up(0.f, 1.f, 0.f);

  if (this->pressed_keys.count(key_forward) || this->pressed_keys.count(Qt::Key_Up))
  {
    this->target += forward_h * speed;
    this->need_update = true;
  }
  if (this->pressed_keys.count(key_backward) || this->pressed_keys.count(Qt::Key_Down))
  {
    this->target -= forward_h * speed;
    this->need_update = true;
  }
  if (this->pressed_keys.count(key_left) || this->pressed_keys.count(Qt::Key_Left))
  {
    this->target -= right_h * speed;
    this->need_update = true;
  }
  if (this->pressed_keys.count(key_right) || this->pressed_keys.count(Qt::Key_Right))
  {
    this->target += right_h * speed;
    this->need_update = true;
  }
  if (this->pressed_keys.count(key_up))
  {
    this->target += up * speed;
    this->need_update = true;
  }
  if (this->pressed_keys.count(key_down))
  {
    this->target -= up * speed;
    this->need_update = true;
  }
}

} // namespace qtr
