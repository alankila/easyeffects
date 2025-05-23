/*
 *  Copyright © 2025 Antti S. Lankila <alankila@bel.fi>
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

/* Interaural level difference parameters. These are inherently a compromise.
 * Midrange localization is most practical to achieve, but bass should not be canceled.
 * Any high frequency localization forces listening with head locked in a vice
 * due to wavelength. These will require much experimentation and possibly precisely
 * designed better filter shape than this rough stab. */
const auto ILD_HIGHPASS_HZ = 300.0f;
const auto ILD_LOWPASS_LIMIT_DB = -10.0f;
const auto ILD_LOWPASS_HZ = 3000.0f;

LCC::LCC(const std::string& tag,
         const std::string& schema,
         const std::string& schema_path,
         PipeManager* pipe_manager,
         PipelineType pipe_type)
    : PluginBase(tag, tags::plugin_name::lcc, tags::plugin_package::ee, schema, schema_path, pipe_manager, pipe_type) {
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

  /* The required buffer size for the stereo delay line. */
  auto samples = static_cast<size_t>(std::round(delay_us / 1.0e6 * rate));
  a.set_delay_length(samples);
  b.set_delay_length(samples);

  a.set_lowpass(ILD_LOWPASS_HZ / static_cast<float>(rate));
  b.set_lowpass(ILD_LOWPASS_HZ / static_cast<float>(rate));

  a.set_highpass(ILD_HIGHPASS_HZ / static_cast<float>(rate));
  b.set_highpass(ILD_HIGHPASS_HZ / static_cast<float>(rate));
}

/* Perform stereo crossfeed that cancels contralateral audio. */
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
  auto ild_lowpass_limit = static_cast<float>(std::pow(10, ILD_LOWPASS_LIMIT_DB / 20));
  for (size_t n = 0U; n < left_in.size(); n ++) {
    auto ao = left_in[n] - decay_gain * b.get_sample();
    auto bo = right_in[n] - decay_gain * a.get_sample();
    left_out[n] = ao;
    right_out[n] = bo;

    ao = a.highpass(ao);
    bo = b.highpass(bo);

    /* Lowpass with a maximum negative gain.
     * Literature suggests that head shadow is at most about -10 dB. */
    ao = a.lowpass(ao) * (1 - ild_lowpass_limit) + ao * ild_lowpass_limit;
    bo = b.lowpass(bo) * (1 - ild_lowpass_limit) + bo * ild_lowpass_limit;

    a.put_sample(ao);
    b.put_sample(bo);
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
