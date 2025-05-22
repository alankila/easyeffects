/*
 *  Copyright © 2025 Antti S. Lankila <alankila@bel.fi>
 *  
 *  Inspired by work by Robert LiKamWa (@roblkw_asu) and Matthew Lane (@mattlane66),
 *  with guidance from Ralph Glasgal (https://www.ambiophonics.org/) with implementation at
 *  https://github.com/MeteorStudioASU/lcc
 *
 *  This version is a reimplementation from the published formulas.
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
const auto ILD_HIGHPASS_HZ = 400.0;
const auto ILD_LOWPASS_HZ = 2000.0;

LCC::LCC(const std::string& tag,
                     const std::string& schema,
                     const std::string& schema_path,
                     PipeManager* pipe_manager,
                     PipelineType pipe_type)
    : PluginBase(tag,
                 tags::plugin_name::lcc,
		 tags::plugin_package::ee,
                 schema,
                 schema_path,
                 pipe_manager,
                 pipe_type) {

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
  gconnections.push_back(g_signal_connect(settings, "changed::center-db",
                                          G_CALLBACK(+[](GSettings* settings, char* key, gpointer user_data) {
                                            auto* self = static_cast<LCC*>(user_data);
                                            self->center_db = g_settings_get_double(settings, key);
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

  /* Compute required buffer size for the stereo delay line. */
  delay_samples = this->delay_us / 1.0e6 * rate;
  if (data.size() != 2U * std::ceil(delay_samples)) {
    data.resize(2U * std::ceil(delay_samples));
    data_index = 0;
  }

  lowpass_coeff = std::exp(-2 * M_PI * ILD_LOWPASS_HZ / rate);
  left_lowpass_state = 0;
  right_lowpass_state = 0;

  highpass_coeff = std::exp(-2 * M_PI * ILD_HIGHPASS_HZ / rate);
  right_highpass_state = 0;
  left_highpass_state = 0;
}

/* Perform stereo crossfeed that cancels contralateral audio.
 *
 * The purpose is to create heightened sense of stereo width and
 * center channel clarity for a stereo playback situation where
 * channels are allowed to mix via air, e.g. left channel sound enters
 * right ear and vice versa. By constructing delayed copy of the sound of the
 * channel for the contralateral channel, we can digitally cancel bulk
 * of the sound.
 *
 * We also need to enhance the center channel some because it is highly
 * correlated in low frequencies, and we are likely to reduce its level
 * by sending the canceled copies out.
 *
 * This is the equation for channel A with contralateral channel B:
 *
 * A_out = A_in - decay_db * delayed_B_out;
 *
 * In addition to this, center channel is reinforced
 * outside the delay line
 *
 * A_out2 = A_out + center_db * (A_in + B_in) / 2
 */
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

  float decay_gain = std::pow(10, decay_db / 20);
  float center_gain = std::pow(10, center_db / 20);
  float overall_gain_comp = 1 / (1 + center_gain);

  size_t full_samples = std::ceil(delay_samples);
  float fract = full_samples - delay_samples;

  for (size_t n = 0U; n < left_in.size(); n ++) {
    size_t next_data_index = (data_index + 1) % full_samples;

    /* note: +1, we select the right delay line for left and vice versa. */
    float left  = left_in[n]  - decay_gain
      * (data[data_index * 2 + 1] * (1 - fract) + data[next_data_index * 2 + 1] * fract);
    float right = right_in[n] - decay_gain
      * (data[data_index * 2 + 0] * (1 - fract) + data[next_data_index * 2 + 0] * fract);

    /* head shadow is modeled by a crude 1st order filter. */
    left_lowpass_state  = left_lowpass_state  * lowpass_coeff + left  * (1 - lowpass_coeff);
    right_lowpass_state = right_lowpass_state * lowpass_coeff + right * (1 - lowpass_coeff);

    /* highpass is for not canceling the bass */
    left_highpass_state  = left_highpass_state  * highpass_coeff + left_lowpass_state  * (1 - highpass_coeff);
    right_highpass_state = right_highpass_state * highpass_coeff + right_lowpass_state * (1 - highpass_coeff);

    /* The lowpass state is input to the highpass filter. The highpass output is the difference between
     * the filter's input and its output (state). It is a lagging filter, and proper sign is
     * maintained by subtracting input - state.
     */
    data[data_index * 2 + 0] = left_lowpass_state  - left_highpass_state;
    data[data_index * 2 + 1] = right_lowpass_state - right_highpass_state;

    data_index = next_data_index;

    /* actual output is the original sound - delay line's feedback + the reinforced center channel */
    float center = (left_in[n] + right_in[n]) * 0.5f;
    left_out[n]  = (left  + center_gain * center) * overall_gain_comp;
    right_out[n] = (right + center_gain * center) * overall_gain_comp;
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
