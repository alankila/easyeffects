/*
 *  Copyright © 2017-2025 Wellington Wallace
 *  Crosstalk Canceller plugin developed by Antti S. Lankila <alankila@bel.fi>
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

#include "lcc.hpp"
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <algorithm>
#include <cstddef>
#include <mutex>
#include <span>
#include <string>
#include "pipe_manager.hpp"
#include "plugin_base.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

LCC::LCC(const std::string& tag,
         const std::string& schema,
         const std::string& schema_path,
         PipeManager* pipe_manager,
         PipelineType pipe_type)
    : PluginBase(tag, tags::plugin_name::lcc, tags::plugin_package::ee, schema, schema_path, pipe_manager, pipe_type) {

  delay_us = g_settings_get_double(settings, "delay-us");
  decay_db = g_settings_get_double(settings, "decay-db");
  phantom_center_only = g_settings_get_boolean(settings, "phantom-center-only");

  gconnections.push_back(g_signal_connect(settings, "changed::delay-us",
                                          G_CALLBACK(+[](GSettings* settings, char* key, gpointer user_data) {
                                            auto* self = static_cast<LCC*>(user_data);
                                            self->delay_us = g_settings_get_double(settings, key);
                                            self->setup();
                                          }),
                                          this));
  gconnections.push_back(g_signal_connect(settings, "changed::decay-db",
                                          G_CALLBACK(+[](GSettings* settings, char* key, gpointer user_data) {
                                            auto* self = static_cast<LCC*>(user_data);
                                            self->decay_db = g_settings_get_double(settings, key);
                                          }),
                                          this));
  gconnections.push_back(g_signal_connect(settings, "changed::phantom-center-only",
                                          G_CALLBACK(+[](GSettings* settings, char* key, gpointer user_data) {
                                            auto* self = static_cast<LCC*>(user_data);
                                            self->phantom_center_only = g_settings_get_boolean(settings, key);
                                          }),
                                          this));

  setup_input_output_gain();
}

LCC::~LCC() {
  if (connected_to_pw) {
    disconnect_from_pw();
  }

  util::debug(log_tag + name + " destroyed");
}

void LCC::setup() {
  std::scoped_lock<std::mutex> lock(data_mutex);

  a.configure(delay_us, rate);
  b.configure(delay_us, rate);
}

void LCC::process(std::span<float>& left_in,
                  std::span<float>& right_in,
                  std::span<float>& left_out,
                  std::span<float>& right_out) {
  std::scoped_lock<std::mutex> lock(data_mutex);

  if (bypass) {
    std::copy(left_in.begin(), left_in.end(), left_out.begin());
    std::copy(right_in.begin(), right_in.end(), right_out.begin());
    return;
  }

  if (input_gain != 1.0F) {
    apply_gain(left_in, right_in, input_gain);
  }

  auto decay_gain = static_cast<float>(std::pow(10, decay_db / 20));

  if (phantom_center_only) {
    for (size_t n = 0U; n < left_in.size(); n ++) {
      float middle = left_in[n] + right_in[n];
      float side = left_in[n] - right_in[n];
      auto mo = middle - decay_gain * a.get_sample();
      auto so = side;
      left_out[n] = (mo + so) * .5f;
      right_out[n] = (mo - so) * .5f;
      a.put_sample(mo);
      /* No delay line input from b */
    }
  } else {
    for (size_t n = 0U; n < left_in.size(); n ++) {
      auto ao = left_in[n] - decay_gain * b.get_sample();
      auto bo = right_in[n] - decay_gain * a.get_sample();
      left_out[n] = ao;
      right_out[n] = bo;
      a.put_sample(ao);
      b.put_sample(bo);
    }
  }

  if (output_gain != 1.0F) {
    apply_gain(left_out, right_out, output_gain);
  }

  if (post_messages) {
    get_peaks(left_in, right_in, left_out, right_out);

    if (send_notifications) {
      notify();
    }
  }
}

auto LCC::get_latency_seconds() -> float {
  return 0.0F;
}
