/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/windows_patch.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "qtr/imgui_widgets.hpp"
#include "qtr/logger.hpp"
#include "qtr/render_widget.hpp"

namespace qtr
{

bool imgui_enum_selector(const std::string              &label,
                         int                            &value,
                         const std::vector<std::string> &options)
{
  // ImGui expects const char*[]
  std::vector<const char *> cstrs;
  cstrs.reserve(options.size());
  for (auto &s : options)
    cstrs.push_back(s.c_str());

  return ImGui::Combo(label.c_str(),
                      &value,
                      cstrs.data(),
                      static_cast<int>(cstrs.size()));
}

bool imgui_orientation_gizmo(float        &alpha_x,
                             float        &alpha_y,
                             const ImVec2 &center,
                             float         radius)
{
  bool changed = false;

  const float cos_ax = std::cos(alpha_x);
  const float sin_ax = std::sin(alpha_x);
  const float cos_ay = std::cos(alpha_y);
  const float sin_ay = std::sin(alpha_y);

  // camera basis vectors in world space
  const glm::vec3 forward(-cos_ax * sin_ay, -sin_ax, -cos_ax * cos_ay);
  const glm::vec3 right(cos_ay, 0.f, -sin_ay);
  const glm::vec3 up(-sin_ax * sin_ay, cos_ax, -sin_ax * cos_ay);

  struct AxisInfo
  {
    const char *label;
    const char *tooltip;
    glm::vec3   dir;
    ImU32       color;
    ImU32       color_dim;
    float       target_ax;
    float       target_ay;
    bool        is_positive;
  };

  const float half_pi = glm::half_pi<float>();
  const float pi = glm::pi<float>();

  // cardinal axes mapping according to issue specification:
  // X is East, -X is West, Y is North, -Y is South, +Z is Top
  // in OpenGL world coords: +Y is Up (Top), -Z is North, +Z is South, +X is East, -X is
  // West
  const std::array<AxisInfo, 5> axes = {
      // +X (East)
      AxisInfo{"E",
               "+X (East)",
               glm::vec3(1.f, 0.f, 0.f),
               IM_COL32(235, 75, 90, 255),
               IM_COL32(140, 45, 55, 180),
               0.f,
               half_pi,
               true},
      // -X (West)
      AxisInfo{"W",
               "-X (West)",
               glm::vec3(-1.f, 0.f, 0.f),
               IM_COL32(235, 75, 90, 255),
               IM_COL32(140, 45, 55, 180),
               0.f,
               -half_pi,
               false},
      // +Y (North: in OpenGL, North is -Z)
      AxisInfo{"N",
               "+Y (North)",
               glm::vec3(0.f, 0.f, -1.f),
               IM_COL32(120, 195, 60, 255),
               IM_COL32(70, 115, 35, 180),
               0.f,
               0.f,
               true},
      // -Y (South: in OpenGL, South is +Z)
      AxisInfo{"S",
               "-Y (South)",
               glm::vec3(0.f, 0.f, 1.f),
               IM_COL32(120, 195, 60, 255),
               IM_COL32(70, 115, 35, 180),
               0.f,
               pi,
               false},
      // +Z (Top: in OpenGL, Top is +Y)
      AxisInfo{"Top",
               "+Z (Top)",
               glm::vec3(0.f, 1.f, 0.f),
               IM_COL32(60, 140, 245, 255),
               IM_COL32(35, 80, 145, 180),
               0.99f * half_pi,
               0.f,
               true},
  };

  const float arm_len = radius * 0.72f;
  const float knob_r_pos = radius * 0.22f;
  const float knob_r_neg = radius * 0.17f;

  struct ProjectedAxis
  {
    int    index;
    ImVec2 pos_2d;
    float  depth;
    float  knob_r;
    bool   hovered;
  };

  ImGuiIO &io = ImGui::GetIO();
  ImVec2   mouse_pos = io.MousePos;

  std::array<ProjectedAxis, axes.size()> projected;
  int                          hovered_idx = -1;
  float                        max_hover_depth = -1e9f;

  for (size_t i = 0; i < axes.size(); ++i)
  {
    float sx = glm::dot(axes[i].dir, right);
    float sy = -glm::dot(axes[i].dir, up);
    float depth = glm::dot(axes[i].dir, -forward);

    ImVec2 p_2d(center.x + sx * arm_len, center.y + sy * arm_len);
    float  r_knob = axes[i].is_positive ? knob_r_pos : knob_r_neg;

    float dx = mouse_pos.x - p_2d.x;
    float dy = mouse_pos.y - p_2d.y;
    bool  is_hover = (dx * dx + dy * dy) <= (r_knob + 3.f) * (r_knob + 3.f);

    projected[i] = ProjectedAxis{static_cast<int>(i), p_2d, depth, r_knob, is_hover};

    if (is_hover && depth > max_hover_depth)
    {
      max_hover_depth = depth;
      hovered_idx = static_cast<int>(i);
    }
  }

  // update hover state to ensure only the foremost hovered knob is active
  for (size_t i = 0; i < projected.size(); ++i)
    projected[i].hovered = (static_cast<int>(i) == hovered_idx);

  // drag interaction
  static bool dragging = false;
  float       dist_to_center_sq = (mouse_pos.x - center.x) * (mouse_pos.x - center.x) +
                            (mouse_pos.y - center.y) * (mouse_pos.y - center.y);
  bool in_gizmo_disk = dist_to_center_sq <= (radius * 1.25f) * (radius * 1.25f);

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && in_gizmo_disk)
  {
    if (hovered_idx >= 0)
    {
      alpha_x = axes[hovered_idx].target_ax;
      alpha_y = axes[hovered_idx].target_ay;
      changed = true;
    }
    else
    {
      dragging = true;
    }
  }

  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    dragging = false;

  if (dragging && (io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f))
  {
    alpha_y -= io.MouseDelta.x * 0.008f;
    alpha_x += io.MouseDelta.y * 0.008f;
    alpha_x = glm::clamp(alpha_x, -0.99f * half_pi, 0.99f * half_pi);
    changed = true;
  }

  // sort projected axes back-to-front (ascending depth)
  std::sort(projected.begin(),
            projected.end(),
            [](const ProjectedAxis &a, const ProjectedAxis &b)
            { return a.depth < b.depth; });

  // draw overlay
  ImDrawList *draw_list = ImGui::GetForegroundDrawList();

  // background disk
  draw_list->AddCircleFilled(center, radius * 1.15f, IM_COL32(20, 20, 20, 160));
  draw_list->AddCircle(center, radius * 1.15f, IM_COL32(60, 60, 60, 200), 0, 1.5f);

  // faint horizon guide circle
  draw_list->AddCircle(center, arm_len, IM_COL32(100, 100, 100, 40), 32, 1.0f);

  // draw axes and knobs in depth order
  for (const auto &p : projected)
  {
    const auto &axis = axes[p.index];
    bool        in_front = p.depth >= 0.f;

    // stem line from center to knob
    ImU32 line_col = in_front ? axis.color : axis.color_dim;
    float line_th = in_front ? 2.5f : 1.2f;
    draw_list->AddLine(center, p.pos_2d, line_col, line_th);

    // knob fill
    ImU32 fill_col;
    if (p.hovered)
      fill_col = IM_COL32(255, 255, 255, 240);
    else if (axis.is_positive)
      fill_col = in_front ? axis.color : axis.color_dim;
    else
      fill_col = in_front ? axis.color_dim : IM_COL32(40, 40, 40, 180);

    draw_list->AddCircleFilled(p.pos_2d, p.knob_r, fill_col);

    // knob border / outline
    ImU32 border_col = p.hovered ? IM_COL32(255, 255, 255, 255)
                                 : (in_front ? IM_COL32(255, 255, 255, 180)
                                             : IM_COL32(120, 120, 120, 120));
    draw_list->AddCircle(p.pos_2d, p.knob_r, border_col, 0, 1.2f);

    // label
    ImVec2 text_size = ImGui::CalcTextSize(axis.label);
    ImVec2 text_pos(p.pos_2d.x - text_size.x * 0.5f, p.pos_2d.y - text_size.y * 0.5f);
    ImU32 text_col = p.hovered ? IM_COL32(20, 20, 20, 255) : IM_COL32(255, 255, 255, 255);

    draw_list->AddText(text_pos, text_col, axis.label);
  }

  // center pivot point
  draw_list->AddCircleFilled(center, 3.0f, IM_COL32(200, 200, 200, 220));

  // tooltip on hover
  if (hovered_idx >= 0)
    ImGui::SetTooltip("%s", axes[hovered_idx].tooltip);

  return changed;
}

void imgui_set_blender_style()
{
  ImGuiStyle &style = ImGui::GetStyle();

  // ===== Colors =====
  ImVec4 blender_blue = ImVec4(0.369f, 0.506f, 0.675f, 1.f);
  ImVec4 blender_blue_hover = ImVec4(0.357f, 0.525f, 0.780f, 1.0f);
  ImVec4 blender_blue_active = ImVec4(0.200f, 0.369f, 0.624f, 1.0f);

  ImVec4 bg_dark = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
  ImVec4 bg_light = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
  ImVec4 text_color = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
  ImVec4 text_disabled = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

  style.Colors[ImGuiCol_Text] = text_color;
  style.Colors[ImGuiCol_TextDisabled] = text_disabled;
  style.Colors[ImGuiCol_WindowBg] = bg_dark;
  style.Colors[ImGuiCol_ChildBg] = bg_light;
  style.Colors[ImGuiCol_PopupBg] = bg_light;
  style.Colors[ImGuiCol_Border] = bg_light;
  style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  style.Colors[ImGuiCol_FrameBg] = bg_light;
  style.Colors[ImGuiCol_FrameBgHovered] = blender_blue_hover;
  style.Colors[ImGuiCol_FrameBgActive] = blender_blue_active;
  style.Colors[ImGuiCol_TitleBg] = bg_dark;
  style.Colors[ImGuiCol_TitleBgActive] = bg_dark;
  style.Colors[ImGuiCol_TitleBgCollapsed] = bg_dark;

  // Buttons
  style.Colors[ImGuiCol_Button] = blender_blue;
  style.Colors[ImGuiCol_ButtonHovered] = blender_blue_hover;
  style.Colors[ImGuiCol_ButtonActive] = blender_blue_active;

  // Headers / collapsing menus
  style.Colors[ImGuiCol_Header] = blender_blue;
  style.Colors[ImGuiCol_HeaderHovered] = blender_blue_hover;
  style.Colors[ImGuiCol_HeaderActive] = blender_blue_active;

  // Tabs
  style.Colors[ImGuiCol_Tab] = blender_blue;
  style.Colors[ImGuiCol_TabHovered] = blender_blue_hover;
  style.Colors[ImGuiCol_TabActive] = blender_blue_active;
  style.Colors[ImGuiCol_TabUnfocused] = bg_light;
  style.Colors[ImGuiCol_TabUnfocusedActive] = blender_blue;

  // Sliders / Progress Bars
  style.Colors[ImGuiCol_SliderGrab] = blender_blue;
  style.Colors[ImGuiCol_SliderGrabActive] = blender_blue_active;
  style.Colors[ImGuiCol_PlotHistogram] = blender_blue;
  style.Colors[ImGuiCol_PlotHistogramHovered] = blender_blue_hover;

  // Checkboxes / Radio buttons
  style.Colors[ImGuiCol_CheckMark] = blender_blue;

  // ===== Style metrics =====
  style.WindowRounding = 4.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 3.0f;
  style.ScrollbarRounding = 3.0f;
  style.WindowPadding = ImVec2(8, 8);
  style.FramePadding = ImVec2(6, 4);
  style.ItemSpacing = ImVec2(6, 4);
  style.ItemInnerSpacing = ImVec2(4, 4);
  style.IndentSpacing = 16.0f;
  style.ScrollbarSize = 12.0f;
  style.GrabMinSize = 8.0f;

  // Tweak other colors for consistent Blender look
  style.Colors[ImGuiCol_CheckMark] = blender_blue;
  style.Colors[ImGuiCol_Separator] = bg_light;
  style.Colors[ImGuiCol_SeparatorHovered] = blender_blue_hover;
  style.Colors[ImGuiCol_SeparatorActive] = blender_blue_active;
}

bool imgui_show_water_preset_selector(glm::vec3 &shallow, glm::vec3 &deep)
{
  bool ret = false;

  // Build list of preset names
  static std::vector<const char *> preset_names;
  if (preset_names.empty())
  {
    for (auto &kv : water_colors)
      preset_names.push_back(kv.first.c_str());
  }

  // Find current index
  int current_index = 0;
  for (size_t i = 0; i < preset_names.size(); i++)
  {
    if (current_water_preset == preset_names[i])
    {
      current_index = static_cast<int>(i);
      break;
    }
  }

  // Combo box
  if (ImGui::Combo("Water Preset",
                   &current_index,
                   preset_names.data(),
                   (int)preset_names.size()))
  {
    // Update selected preset
    current_water_preset = preset_names[current_index];
    ret = true;
  }

  // Display colors of the selected preset
  auto &preset = water_colors[current_water_preset];
  shallow = preset.first;
  deep = preset.second;

  ImGui::Indent();

  ImGui::Text("Shallow color:");
  ImGui::ColorButton("##shallow_color",
                     ImVec4(shallow.r, shallow.g, shallow.b, 1.0f),
                     0,
                     ImVec2(50, 20));
  ImGui::SameLine();
  ImGui::Text("(%.2f, %.2f, %.2f)", shallow.r, shallow.g, shallow.b);

  ImGui::Text("Deep color:");
  ImGui::ColorButton("##deep_color",
                     ImVec4(deep.r, deep.g, deep.b, 1.0f),
                     0,
                     ImVec2(50, 20));
  ImGui::SameLine();
  ImGui::Text("(%.2f, %.2f, %.2f)", deep.r, deep.g, deep.b);

  ImGui::Unindent();

  return ret;
}

bool imgui_viewer_main_menubar(RenderWidget &render_widget)
{
  bool changed = false;

  ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.f, 0.f, 0.f, 0.1f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.1f));

  if (ImGui::BeginMainMenuBar())
  {
    if (ImGui::BeginMenu("Viewer Type"))
    {
      if (ImGui::MenuItem("2D viewer"))
        render_widget.set_render_type(RenderType::RENDER_2D);
      if (ImGui::MenuItem("3D renderer"))
        render_widget.set_render_type(RenderType::RENDER_3D);
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  ImGui::PopStyleColor(2);

  return changed;
}

} // namespace qtr
