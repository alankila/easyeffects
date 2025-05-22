/*
 *  Copyright © 2017-2024 Wellington Wallace
 *
 *  This file is part of Easy Effects.
 *
 *  Easy Effects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Easy Effects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Easy Effects. If not, see <https://www.gnu.org/licenses/>.
 */

#include "lcc_preset.hpp"
#include <gio/gio.h>
#include <nlohmann/json_fwd.hpp>
#include "plugin_preset_base.hpp"
#include "preset_type.hpp"
#include "tags_plugin_name.hpp"
#include "tags_schema.hpp"
#include "util.hpp"

LCCPreset::LCCPreset(PresetType preset_type, const int& index)
    : PluginPresetBase(tags::schema::lcc::id,
                       tags::schema::lcc::input_path,
                       tags::schema::lcc::output_path,
                       preset_type,
                       index) {
  instance_name.assign(tags::plugin_name::lcc).append("#").append(util::to_string(index));
}

void LCCPreset::save(nlohmann::json& json) {
  json[section][instance_name]["bypass"] = g_settings_get_boolean(settings, "bypass") != 0;

  json[section][instance_name]["input-gain"] = g_settings_get_double(settings, "input-gain");

  json[section][instance_name]["output-gain"] = g_settings_get_double(settings, "output-gain");

  json[section][instance_name]["delay-us"] = g_settings_get_double(settings, "delay-us");

  json[section][instance_name]["decay-db"] = g_settings_get_double(settings, "decay-db");

  json[section][instance_name]["center-db"] = g_settings_get_double(settings, "center-db");
}

void LCCPreset::load(const nlohmann::json& json) {
  update_key<bool>(json.at(section).at(instance_name), settings, "bypass", "bypass");

  update_key<double>(json.at(section).at(instance_name), settings, "input-gain", "input-gain");

  update_key<double>(json.at(section).at(instance_name), settings, "output-gain", "output-gain");

  update_key<double>(json.at(section).at(instance_name), settings, "delay-us", "delay-us");

  update_key<double>(json.at(section).at(instance_name), settings, "decay-db", "decay-db");

  update_key<double>(json.at(section).at(instance_name), settings, "center-db", "center-db");
}
