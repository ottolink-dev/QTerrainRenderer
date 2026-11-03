/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <glm/vec4.hpp>

namespace qtr
{

/**
 * @brief Common visual style settings for QTerrainRenderer ImGui controls.
 *
 * Allows host applications (such as Hesiod) to inject unified theme colors
 * and metrics without coupling QTerrainRenderer to Qt or application types.
 */
struct UIStyle
{
  // --- Surfaces
  glm::vec4 bg_dark = glm::vec4(0.09f, 0.09f, 0.09f, 1.0f);
  glm::vec4 bg_light = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);

  // --- Inks
  glm::vec4 text = glm::vec4(0.90f, 0.90f, 0.90f, 1.0f);
  glm::vec4 text_disabled = glm::vec4(0.50f, 0.50f, 0.50f, 1.0f);

  // --- Accent & interactive states
  glm::vec4 accent = glm::vec4(0.369f, 0.506f, 0.675f, 1.0f);
  glm::vec4 accent_hover = glm::vec4(0.357f, 0.525f, 0.780f, 1.0f);
  glm::vec4 accent_active = glm::vec4(0.200f, 0.369f, 0.624f, 1.0f);

  // --- Borders
  glm::vec4 border = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);

  // --- Metrics (logical px)
  float window_rounding = 4.0f;
  float frame_rounding = 3.0f;
  float grab_rounding = 3.0f;
  float tab_rounding = 3.0f;
  float scrollbar_rounding = 3.0f;
};

} // namespace qtr
