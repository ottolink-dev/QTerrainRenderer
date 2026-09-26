/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>

namespace qtr
{

// small wrapper for convenience and safety
class Shader : protected QOpenGLFunctions_3_3_Core
{
public:
  Shader() = default;
  ~Shader();

  // Rule of 5: non-copyable, movable
  Shader(const Shader &) = delete;
  Shader &operator=(const Shader &) = delete;
  Shader(Shader &&) noexcept = default;
  Shader &operator=(Shader &&) noexcept = default;

  bool from_code(const std::string &vertex_code, const std::string &fragment_code);
  bool from_file(const std::string &vertex_path, const std::string &fragment_path);

  QOpenGLShaderProgram       *get();
  const QOpenGLShaderProgram *get() const;

private:
  void destroy();

  std::unique_ptr<QOpenGLShaderProgram> sp_program;
};

// --- Embedded shader sources

extern const std::string diffuse_basic_vertex;
extern const std::string diffuse_basic_frag;
extern const std::string diffuse_phong_frag;
extern const std::string diffuse_blinn_phong_frag;
extern const std::string depth_map_vertex;
extern const std::string depth_map_frag;
extern const std::string shadow_map_depth_pass_vertex;
extern const std::string shadow_map_depth_pass_frag;
extern const std::string shadow_map_lit_pass_vertex;
extern const std::string shadow_map_lit_pass_frag;
extern const std::string viewer2d_cmap_vertex;
extern const std::string viewer2d_cmap_frag;
extern const std::string skybox_vertex;
extern const std::string skybox_frag;

} // namespace qtr