/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/windows_patch.hpp"

#include <QFocusEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>

#include <imgui.h>

#include "qtr/render_widget.hpp"

namespace qtr
{

void RenderWidget::mousePressEvent(QMouseEvent *e)
{
  ImGuiIO &io = this->get_imgui_io();
  io.MousePos = ImVec2(float(e->position().x()), float(e->position().y()));

  if (e->button() == Qt::LeftButton)
    io.MouseDown[0] = true;
  if (e->button() == Qt::RightButton)
    io.MouseDown[1] = true;
  if (e->button() == Qt::MiddleButton)
    io.MouseDown[2] = true;

  this->need_update = true;
}

void RenderWidget::mouseReleaseEvent(QMouseEvent *e)
{
  ImGuiIO &io = this->get_imgui_io();
  io.MousePos = ImVec2(float(e->position().x()), float(e->position().y()));

  if (e->button() == Qt::LeftButton)
    io.MouseDown[0] = false;
  if (e->button() == Qt::RightButton)
    io.MouseDown[1] = false;
  if (e->button() == Qt::MiddleButton)
    io.MouseDown[2] = false;

  this->need_update = true;
}

void RenderWidget::mouseMoveEvent(QMouseEvent *e)
{
  ImGuiIO &io = this->get_imgui_io();
  io.MousePos = ImVec2(float(e->position().x()), float(e->position().y()));

  // Follow the buttons actually held. A press can reach this widget without
  // its release (a child overlay handling only the release, a popup closing
  // on it...), and a button left "down" here turns every later hover into a
  // camera drag.
  const Qt::MouseButtons held = e->buttons();
  io.MouseDown[0] = io.MouseDown[0] && (held & Qt::LeftButton);
  io.MouseDown[1] = io.MouseDown[1] && (held & Qt::RightButton);
  io.MouseDown[2] = io.MouseDown[2] && (held & Qt::MiddleButton);

  this->need_update = true;
}

void RenderWidget::leaveEvent(QEvent *event)
{
  // the pointer left without a grab, so no button is held over the view
  ImGuiIO &io = this->get_imgui_io();
  if (!QGuiApplication::mouseButtons())
    io.MouseDown[0] = io.MouseDown[1] = io.MouseDown[2] = false;
  QOpenGLWidget::leaveEvent(event);
}

void RenderWidget::wheelEvent(QWheelEvent *e)
{
  this->get_imgui_io().MouseWheel += e->angleDelta().y() / 120.0f;
  this->need_update = true;
}

void RenderWidget::keyPressEvent(QKeyEvent *e)
{
  this->pressed_keys.insert(e->key());
  this->need_update = true;
}

void RenderWidget::keyReleaseEvent(QKeyEvent *e)
{
  this->pressed_keys.erase(e->key());
  this->need_update = true;
}

void RenderWidget::focusOutEvent(QFocusEvent *event)
{
  this->pressed_keys.clear();
  QOpenGLWidget::focusOutEvent(event);
}

} // namespace qtr
