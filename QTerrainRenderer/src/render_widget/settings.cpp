/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <algorithm>
#include <map>

#include "qtr/keys.hpp"
#include "qtr/render_widget.hpp"

// Key-based access to the render settings, so a host application can drive
// them from its own UI instead of the built-in ImGui panel. Each table maps a
// stable key to the member it stands for; the keys double as the vocabulary a
// host stores or shows, so renaming a member does not have to break them.

namespace qtr
{

namespace
{

// meshes whose visibility is exposed as "visible.<mesh>"
const std::vector<std::string> &visibility_meshes()
{
  static const std::vector<std::string> meshes = {keys::mesh::plane,
                                                  keys::mesh::hmap,
                                                  keys::mesh::water,
                                                  keys::mesh::points,
                                                  keys::mesh::path};
  return meshes;
}

std::vector<std::string> keys_of_map(const auto &map)
{
  std::vector<std::string> out;
  for (const auto &[key, _] : map)
    out.push_back(key);
  return out;
}

} // namespace

// Member tables. The members are private; RenderWidget declares this struct a
// friend so the tables can live here, next to the accessors that use them.
struct RenderWidgetSettingsTables
{
  static std::map<std::string, bool RenderWidget::*> bools()
  {
    return {{"show_orientation_gizmo", &RenderWidget::show_orientation_gizmo},
            {"normal_visualization", &RenderWidget::normal_visualization},
            {"wireframe_mode", &RenderWidget::wireframe_mode},
            {"keyboard_navigation_enabled", &RenderWidget::keyboard_navigation_enabled},
            {"bypass_texture_albedo", &RenderWidget::bypass_texture_albedo},
            {"apply_tonemap", &RenderWidget::apply_tonemap},
            {"auto_rotate_light", &RenderWidget::auto_rotate_light},
            {"bypass_shadow_map", &RenderWidget::bypass_shadow_map},
            {"add_ambiant_occlusion", &RenderWidget::add_ambiant_occlusion},
            {"add_water_foam", &RenderWidget::add_water_foam},
            {"add_water_waves", &RenderWidget::add_water_waves},
            {"animate_waves", &RenderWidget::animate_waves},
            {"show_skybox", &RenderWidget::show_skybox},
            {"add_fog", &RenderWidget::add_fog},
            {"fog_match_skybox", &RenderWidget::fog_match_skybox},
            {"add_atmospheric_scattering", &RenderWidget::add_atmospheric_scattering}};
  }

  static std::map<std::string, float RenderWidget::*> floats()
  {
    return {{"scale_h", &RenderWidget::scale_h},
            {"camera_move_speed", &RenderWidget::camera_move_speed},
            {"gamma_correction", &RenderWidget::gamma_correction},
            {"normal_map_scaling", &RenderWidget::normal_map_scaling},
            {"light_phi", &RenderWidget::light_phi},
            {"light_theta", &RenderWidget::light_theta},
            {"shadow_strength", &RenderWidget::shadow_strength},
            {"ambiant_occlusion_strength", &RenderWidget::ambiant_occlusion_strength},
            {"ambiant_occlusion_radius", &RenderWidget::ambiant_occlusion_radius},
            {"water_color_depth", &RenderWidget::water_color_depth},
            {"water_spec_strength", &RenderWidget::water_spec_strength},
            {"foam_depth", &RenderWidget::foam_depth},
            {"waves_kw", &RenderWidget::waves_kw},
            {"waves_amplitude", &RenderWidget::waves_amplitude},
            {"waves_normal_amplitude", &RenderWidget::waves_normal_amplitude},
            {"waves_alpha", &RenderWidget::waves_alpha},
            {"angle_spread_ratio", &RenderWidget::angle_spread_ratio},
            {"waves_speed", &RenderWidget::waves_speed},
            {"skybox_rotation", &RenderWidget::skybox_rotation},
            {"fog_density", &RenderWidget::fog_density},
            {"fog_height", &RenderWidget::fog_height},
            {"scattering_density", &RenderWidget::scattering_density},
            {"fog_strength", &RenderWidget::fog_strength},
            {"fog_scattering_ratio", &RenderWidget::fog_scattering_ratio}};
  }

  static std::map<std::string, glm::vec3 RenderWidget::*> colors()
  {
    return {{"color_shallow_water", &RenderWidget::color_shallow_water},
            {"color_deep_water", &RenderWidget::color_deep_water},
            {"skybox_color", &RenderWidget::skybox_color},
            {"fog_color", &RenderWidget::fog_color},
            {"rayleigh_color", &RenderWidget::rayleigh_color},
            {"mie_color", &RenderWidget::mie_color}};
  }
};

namespace
{

const std::map<std::string, bool RenderWidget::*> &bool_members()
{
  static const auto table = RenderWidgetSettingsTables::bools();
  return table;
}

const std::map<std::string, float RenderWidget::*> &float_members()
{
  static const auto table = RenderWidgetSettingsTables::floats();
  return table;
}

const std::map<std::string, glm::vec3 RenderWidget::*> &color_members()
{
  static const auto table = RenderWidgetSettingsTables::colors();
  return table;
}

} // namespace

// --- accessors

bool *RenderWidget::bool_setting(const std::string &key)
{
  if (auto it = bool_members().find(key); it != bool_members().end())
    return &(this->*(it->second));

  if (key == "2d.hillshading")
    return &this->viewer2d_settings.hillshading;

  if (key.rfind("visible.", 0) == 0)
  {
    auto *params = this->sp_mesh_manager->get_render_params(key.substr(8));
    return params ? &params->visible : nullptr;
  }

  return nullptr;
}

float *RenderWidget::float_setting(const std::string &key)
{
  if (auto it = float_members().find(key); it != float_members().end())
    return &(this->*(it->second));

  if (key == "fov")
    return &this->camera.fov;
  if (key == "2d.sun_azimuth")
    return &this->viewer2d_settings.sun_azimuth;
  if (key == "2d.sun_zenith")
    return &this->viewer2d_settings.sun_zenith;

  return nullptr;
}

glm::vec3 *RenderWidget::color_setting(const std::string &key)
{
  if (auto it = color_members().find(key); it != color_members().end())
    return &(this->*(it->second));
  return nullptr;
}

int RenderWidget::get_int_setting(const std::string &key) const
{
  if (key == "keyboard_layout")
    return static_cast<int>(this->keyboard_layout);
  if (key == "background_mode")
    return this->background_mode;
  if (key == "skybox_mode")
    return static_cast<int>(this->skybox_mode);
  if (key == "2d.colormap")
    return static_cast<int>(this->viewer2d_settings.cmap);
  if (key == "shadow_map_resolution")
    return this->shadow_map_resolution;
  return 0;
}

void RenderWidget::set_int_setting(const std::string &key, int value)
{
  if (key == "shadow_map_resolution")
  {
    this->set_shadow_map_resolution(value); // rebuilds the map, then redraws
    return;
  }

  if (key == "keyboard_layout")
    this->keyboard_layout = static_cast<KeyboardLayout>(value);
  else if (key == "background_mode")
    this->background_mode = std::clamp(value, 0, 1);
  else if (key == "skybox_mode")
    this->skybox_mode = static_cast<SkyboxMode>(value);
  else if (key == "2d.colormap")
    this->viewer2d_settings.cmap = static_cast<Viewer2DSettings::Colormap>(value);
  else
    return;

  this->settings_changed();
}

// --- key lists

const std::vector<std::string> &RenderWidget::bool_setting_keys()
{
  static const std::vector<std::string> keys = []()
  {
    auto out = keys_of_map(bool_members());
    out.push_back("2d.hillshading");
    for (const auto &mesh : visibility_meshes())
      out.push_back("visible." + mesh);
    return out;
  }();
  return keys;
}

const std::vector<std::string> &RenderWidget::float_setting_keys()
{
  static const std::vector<std::string> keys = []()
  {
    auto out = keys_of_map(float_members());
    out.insert(out.end(), {"fov", "2d.sun_azimuth", "2d.sun_zenith"});
    return out;
  }();
  return keys;
}

const std::vector<std::string> &RenderWidget::color_setting_keys()
{
  static const std::vector<std::string> keys = keys_of_map(color_members());
  return keys;
}

const std::vector<std::string> &RenderWidget::int_setting_keys()
{
  static const std::vector<std::string> keys = {"keyboard_layout",
                                                "skybox_mode",
                                                "2d.colormap",
                                                "shadow_map_resolution",
                                                "background_mode"};
  return keys;
}

// --- defaults

void RenderWidget::capture_setting_defaults()
{
  for (const auto &key : bool_setting_keys())
    if (bool *p = this->bool_setting(key))
      this->setting_defaults[key] = *p;

  for (const auto &key : float_setting_keys())
    if (float *p = this->float_setting(key))
      this->setting_defaults[key] = *p;

  for (const auto &key : color_setting_keys())
    if (glm::vec3 *p = this->color_setting(key))
      this->setting_defaults[key] = *p;

  for (const auto &key : int_setting_keys())
    this->setting_defaults[key] = this->get_int_setting(key);
}

std::any RenderWidget::default_setting(const std::string &key) const
{
  auto it = this->setting_defaults.find(key);
  return it == this->setting_defaults.end() ? std::any() : it->second;
}

// --- actions

void RenderWidget::settings_changed()
{
  this->need_update = true;
  this->update();
}

void RenderWidget::reset_camera()
{
  this->reset_camera_position();
  this->settings_changed();
}

void RenderWidget::reset_view_2d()
{
  this->viewer2d_settings.zoom = 0.8f;
  this->viewer2d_settings.offset = glm::vec2(0.f, 0.f);
  this->settings_changed();
}

void RenderWidget::set_settings_window_visible(bool visible)
{
  this->show_settings_window = visible;
  this->settings_changed();
}

void RenderWidget::set_overlay_insets(float top, float right)
{
  if (top == this->overlay_inset_top && right == this->overlay_inset_right)
    return;
  this->overlay_inset_top = top;
  this->overlay_inset_right = right;
  this->settings_changed();
}

} // namespace qtr
