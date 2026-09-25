/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/windows_patch.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <unordered_map>

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

namespace
{

ImU32 with_alpha(ImU32 col, float alpha)
{
  const int a = static_cast<int>(std::clamp(alpha, 0.f, 1.f) *
                                 float((col >> IM_COL32_A_SHIFT) & 0xFF));
  return (col & ~IM_COL32_A_MASK) | (ImU32(a) << IM_COL32_A_SHIFT);
}

ImU32 mix_col(ImU32 a, ImU32 b, float t)
{
  const ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
  const ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
  return ImGui::ColorConvertFloat4ToU32(ImVec4(ca.x + (cb.x - ca.x) * t,
                                               ca.y + (cb.y - ca.y) * t,
                                               ca.z + (cb.z - ca.z) * t,
                                               ca.w + (cb.w - ca.w) * t));
}

// per ImGui context (one per renderer): hover fade and the click-to-align glide
struct GizmoState
{
  float hover_t = 0.f;
  bool  dragging = false;
  bool  gliding = false;
  float target_ax = 0.f;
  float target_ay = 0.f;
};

GizmoState &gizmo_state()
{
  static std::unordered_map<ImGuiContext *, GizmoState> states;
  return states[ImGui::GetCurrentContext()];
}

// shortest signed difference between two angles
float angle_delta(float from, float to)
{
  const float two_pi = glm::two_pi<float>();
  float       d = std::fmod(to - from, two_pi);
  if (d > glm::pi<float>())
    d -= two_pi;
  else if (d < -glm::pi<float>())
    d += two_pi;
  return d;
}

} // namespace

bool imgui_orientation_gizmo(float        &alpha_x,
                             float        &alpha_y,
                             const ImVec2 &center,
                             float         radius,
                             ImFont       *font)
{
  bool        changed = false;
  GizmoState &st = gizmo_state();
  ImGuiIO    &io = ImGui::GetIO();
  const float dt = std::max(io.DeltaTime, 1e-4f);

  const float half_pi = glm::half_pi<float>();
  const float pi = glm::pi<float>();

  // --- clicking a knob glides the view there instead of jumping
  if (st.gliding)
  {
    const float k = 1.f - std::exp(-dt * 14.f); // frame-rate independent ease
    const float dx = st.target_ax - alpha_x;
    const float dy = angle_delta(alpha_y, st.target_ay);
    if (std::abs(dx) < 1e-3f && std::abs(dy) < 1e-3f)
    {
      alpha_x = st.target_ax;
      alpha_y = st.target_ay;
      st.gliding = false;
    }
    else
    {
      alpha_x += dx * k;
      alpha_y += dy * k;
    }
    changed = true;
  }

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
    float       target_ax;
    float       target_ay;
    bool        is_positive;
  };

  // cardinal axes: X is East, -X is West, Y is North, -Y is South, +Z is Top.
  // In OpenGL world coords: +Y is Up (Top), -Z is North, +Z is South, +X is
  // East, -X is West
  const ImU32 red = IM_COL32(236, 72, 92, 255);
  const ImU32 green = IM_COL32(128, 196, 62, 255);
  const ImU32 blue = IM_COL32(64, 146, 250, 255);

  const std::array<AxisInfo, 5> axes = {
      AxisInfo{"E", "+X (East)", glm::vec3(1.f, 0.f, 0.f), red, 0.f, half_pi, true},
      AxisInfo{"W", "-X (West)", glm::vec3(-1.f, 0.f, 0.f), red, 0.f, -half_pi, false},
      AxisInfo{"N", "+Y (North)", glm::vec3(0.f, 0.f, -1.f), green, 0.f, 0.f, true},
      AxisInfo{"S", "-Y (South)", glm::vec3(0.f, 0.f, 1.f), green, 0.f, pi, false},
      AxisInfo{"Top",
               "+Z (Top)",
               glm::vec3(0.f, 1.f, 0.f),
               blue,
               0.99f * half_pi,
               0.f,
               true},
  };

  const float arm_len = radius * 0.70f;
  const float knob_r_pos = radius * 0.215f;
  const float knob_r_neg = radius * 0.175f;

  struct ProjectedAxis
  {
    int    index;
    ImVec2 pos_2d;
    float  depth;
    float  knob_r;
    bool   hovered;
  };

  const ImVec2 mouse_pos = io.MousePos;

  std::array<ProjectedAxis, axes.size()> projected;
  int                                    hovered_idx = -1;
  float                                  max_hover_depth = -1e9f;

  for (size_t i = 0; i < axes.size(); ++i)
  {
    const float sx = glm::dot(axes[i].dir, right);
    const float sy = -glm::dot(axes[i].dir, up);
    const float depth = glm::dot(axes[i].dir, -forward);

    const ImVec2 p_2d(center.x + sx * arm_len, center.y + sy * arm_len);
    const float  r_knob = axes[i].is_positive ? knob_r_pos : knob_r_neg;

    const float dx = mouse_pos.x - p_2d.x;
    const float dy = mouse_pos.y - p_2d.y;
    const bool  is_hover = (dx * dx + dy * dy) <= (r_knob + 3.f) * (r_knob + 3.f);

    projected[i] = ProjectedAxis{static_cast<int>(i), p_2d, depth, r_knob, is_hover};

    if (is_hover && depth > max_hover_depth)
    {
      max_hover_depth = depth;
      hovered_idx = static_cast<int>(i);
    }
  }

  // only the foremost hovered knob is active
  for (size_t i = 0; i < projected.size(); ++i)
    projected[i].hovered = (static_cast<int>(i) == hovered_idx);

  // --- interaction
  const float dist_to_center_sq = (mouse_pos.x - center.x) * (mouse_pos.x - center.x) +
                                  (mouse_pos.y - center.y) * (mouse_pos.y - center.y);
  const bool in_gizmo_disk = dist_to_center_sq <= (radius * 1.25f) * (radius * 1.25f);

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && in_gizmo_disk)
  {
    if (hovered_idx >= 0)
    {
      st.target_ax = axes[hovered_idx].target_ax;
      st.target_ay = axes[hovered_idx].target_ay;
      st.gliding = true;
      changed = true;
    }
    else
    {
      st.dragging = true;
      st.gliding = false;
    }
  }

  // grabbing the view elsewhere takes over from a glide in progress
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !in_gizmo_disk)
    st.gliding = false;

  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    st.dragging = false;

  if (st.dragging && (io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f))
  {
    alpha_y -= io.MouseDelta.x * 0.008f;
    alpha_x += io.MouseDelta.y * 0.008f;
    alpha_x = glm::clamp(alpha_x, -0.99f * half_pi, 0.99f * half_pi);
    changed = true;
  }

  // background fades in while the pointer is over the gizmo (or dragging it)
  {
    const float target = (in_gizmo_disk || st.dragging) ? 1.f : 0.f;
    const float k = 1.f - std::exp(-dt * 12.f);
    const float before = st.hover_t;
    st.hover_t += (target - st.hover_t) * k;
    if (std::abs(target - st.hover_t) < 0.01f)
      st.hover_t = target;
    if (st.hover_t != before)
      changed = true; // keep frames coming until the fade settles
  }

  // back-to-front
  std::sort(projected.begin(),
            projected.end(),
            [](const ProjectedAxis &a, const ProjectedAxis &b)
            { return a.depth < b.depth; });

  // --- draw
  ImDrawList *dl = ImGui::GetForegroundDrawList();
  const int   segments = 64;
  const float h = st.hover_t;

  // disc: soft shadow, body, hairline rim
  const float disc_r = radius * 1.12f;
  dl->AddCircleFilled(ImVec2(center.x, center.y + 1.5f),
                      disc_r + 1.5f,
                      IM_COL32(0, 0, 0, int(40 + 30 * h)),
                      segments);
  dl->AddCircleFilled(center, disc_r, IM_COL32(26, 27, 30, int(120 + 70 * h)), segments);
  dl->AddCircle(center,
                disc_r,
                IM_COL32(255, 255, 255, int(18 + 22 * h)),
                segments,
                1.0f);

  // faint horizon ring
  dl->AddCircle(center,
                arm_len,
                IM_COL32(255, 255, 255, int(10 + 12 * h)),
                segments,
                1.0f);

  ImFont     *label_font = font ? font : ImGui::GetFont();
  const float base_size = font ? 11.5f : ImGui::GetFontSize();

  for (const auto &p : projected)
  {
    const auto &axis = axes[p.index];

    // 0 at the back, 1 facing the viewer: fades rather than flips
    const float facing = std::clamp(0.5f + 0.5f * p.depth, 0.f, 1.f);
    const float presence = 0.35f + 0.65f * facing;

    // stem, stopping at the knob's rim
    {
      const ImVec2 d(p.pos_2d.x - center.x, p.pos_2d.y - center.y);
      const float  len = std::sqrt(d.x * d.x + d.y * d.y);
      if (len > p.knob_r + 1.f)
      {
        const float  t = (len - p.knob_r) / len;
        const ImVec2 end(center.x + d.x * t, center.y + d.y * t);
        dl->AddLine(center,
                    end,
                    with_alpha(axis.color, (axis.is_positive ? 0.9f : 0.55f) * presence),
                    axis.is_positive ? 2.0f : 1.4f);
      }
    }

    const float r = p.knob_r + (p.hovered ? 1.5f : 0.f);

    if (axis.is_positive)
    {
      // solid knob, lit when hovered
      const ImU32 fill = p.hovered
                             ? mix_col(axis.color, IM_COL32(255, 255, 255, 255), 0.30f)
                             : mix_col(IM_COL32(40, 40, 44, 255), axis.color, presence);
      dl->AddCircleFilled(p.pos_2d, r, fill, segments);
      dl->AddCircle(p.pos_2d, r, IM_COL32(0, 0, 0, 90), segments, 1.0f);
    }
    else
    {
      // hollow knob: tinted glass with a coloured ring
      dl->AddCircleFilled(
          p.pos_2d,
          r,
          p.hovered
              ? with_alpha(axis.color, 0.55f)
              : mix_col(IM_COL32(30, 31, 34, 230), with_alpha(axis.color, 0.9f), 0.22f),
          segments);
      dl->AddCircle(p.pos_2d, r - 0.5f, with_alpha(axis.color, presence), segments, 1.5f);
    }

    // label, shrunk to fit inside its knob
    float       size = base_size;
    ImVec2      text_size = label_font->CalcTextSizeA(size, FLT_MAX, 0.f, axis.label);
    const float room = 2.f * r - 4.f;
    if (text_size.x > room && text_size.x > 0.f)
    {
      size *= room / text_size.x;
      text_size = label_font->CalcTextSizeA(size, FLT_MAX, 0.f, axis.label);
    }
    const ImVec2 text_pos(std::round(p.pos_2d.x - text_size.x * 0.5f),
                          std::round(p.pos_2d.y - text_size.y * 0.5f));

    ImU32 text_col;
    if (axis.is_positive)
      text_col = p.hovered ? IM_COL32(20, 20, 24, 255)
                           : IM_COL32(255, 255, 255, int(150 + 105 * facing));
    else
      text_col = p.hovered ? IM_COL32(255, 255, 255, 255)
                           : mix_col(IM_COL32(200, 200, 200, 255), axis.color, 0.45f);

    dl->AddText(label_font, size, text_pos, text_col, axis.label);
  }

  // pivot (no hover tooltips: the knobs' letters already say which axis is
  // which, and ImGui tooltips would clash with the host's)
  dl->AddCircleFilled(center, 2.2f, IM_COL32(220, 220, 225, int(150 + 80 * h)), 16);

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
