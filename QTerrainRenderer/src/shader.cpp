/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <fstream>

#include "qtr/logger.hpp"
#include "qtr/shader.hpp"

namespace qtr
{

Shader::~Shader() { this->destroy(); }

bool Shader::from_code(const std::string &vertex_code, const std::string &fragment_code)
{
  // QOpenGLFunctions_3_3_Core::initializeOpenGLFunctions();

  this->destroy();
  this->sp_program = std::make_unique<QOpenGLShaderProgram>();

  if (!this->sp_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                 vertex_code.c_str()))
  {
    qtr::Logger::log()->error(
        "Shader::from_code: could not add vertex shader source code");
    qtr::Logger::log()->error("Shader::from_code: build log >>>");
    qtr::Logger::log()->error("{}", this->sp_program->log().toStdString());
    qtr::Logger::log()->error("Shader::from_code: <<< build log");
    return false;
  }

  if (!this->sp_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                 fragment_code.c_str()))
  {
    qtr::Logger::log()->error("Shader::from_code: could not add fragment source code");
    qtr::Logger::log()->error("Shader::from_code: build log >>>");
    qtr::Logger::log()->error("{}", this->sp_program->log().toStdString());
    qtr::Logger::log()->error("Shader::from_code: <<< build log");
    return false;
  }

  if (!this->sp_program->link())
  {
    qtr::Logger::log()->error("Shader::from_code: could not link shader program");
    qtr::Logger::log()->error("Shader::from_code: build log >>>");
    qtr::Logger::log()->error("{}", this->sp_program->log().toStdString());
    qtr::Logger::log()->error("Shader::from_code: <<< build log");
    return false;
  }

  return true;
}

bool Shader::from_file(const std::string &vertex_path, const std::string &fragment_path)
{
  this->destroy();
  this->sp_program = std::make_unique<QOpenGLShaderProgram>();

  if (!this->sp_program->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                                 QString::fromStdString(vertex_path)))
  {
    qtr::Logger::log()->error(
        "Shader::from_file: could not add vertex shader from file '{}'",
        vertex_path);
    qtr::Logger::log()->error("Shader::from_file: build log >>>");
    qtr::Logger::log()->error("{}", this->sp_program->log().toStdString());
    qtr::Logger::log()->error("Shader::from_file: <<< build log");
    return false;
  }

  if (!this->sp_program->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                                 QString::fromStdString(fragment_path)))
  {
    qtr::Logger::log()->error(
        "Shader::from_file: could not add fragment shader from file '{}'",
        fragment_path);
    qtr::Logger::log()->error("Shader::from_file: build log >>>");
    qtr::Logger::log()->error("{}", this->sp_program->log().toStdString());
    qtr::Logger::log()->error("Shader::from_file: <<< build log");
    return false;
  }

  if (!this->sp_program->link())
  {
    qtr::Logger::log()->error("Shader::from_file: could not link shader program");
    qtr::Logger::log()->error("Shader::from_file: build log >>>");
    qtr::Logger::log()->error("{}", this->sp_program->log().toStdString());
    qtr::Logger::log()->error("Shader::from_file: <<< build log");
    return false;
  }

  return true;
}

void Shader::destroy() { this->sp_program.reset(); }

QOpenGLShaderProgram *Shader::get() { return this->sp_program.get(); }

const QOpenGLShaderProgram *Shader::get() const { return this->sp_program.get(); }

} // namespace qtr
