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

#pragma once

#include <span>
#include <string>
#include <vector>
#include "pipe_manager.hpp"
#include "plugin_base.hpp"

class FilterState {
 private:
  std::vector<float> data;
  size_t data_index = 0;

  float lowpass_coeff = 0;
  float lowpass_state = 0;

  float highpass_coeff = 0;
  float highpass_state = 0;

 public:
  /**
   * Configure internal delay line length
   *
   * @param size number of samples to keep
   */
  void set_delay_length(size_t size) {
    if (data.size() != size) {
      data.resize(size);
      data_index = 0;
    }
  }

  /**
   * Retrieve sample from the delay line that was set
   * some number of samples ago.
   *
   * @return historical sample from delay line
   */
  float get_sample() {
    return data[data_index];
  }

  /**
   * Put a new sample into the delay line
   *
   * @param sample the sample to store
   */
  void put_sample(float sample) {
    data[data_index] = sample;
    data_index = (data_index + 1) % data.size();
  }

  /**
   * Configure lowpass filter
   *
   * @param f the fractional sample rate (fs = 1)
   */
  void set_lowpass(float f) {
    lowpass_coeff = static_cast<float>(std::exp(-2 * M_PI * f));
  }

  /**
   * Perform lowpass filtering and return the sample
   * 
   * @param sample the sample to input
   * @return lowpassed sample
   */
  float lowpass(float sample) {
    lowpass_state = lowpass_state * lowpass_coeff + sample * (1 - lowpass_coeff);
    return lowpass_state;
  }

  /**
   * Configure highpass filter
   *
   * @param f the fractional sample rate (fs = 1)
   */
  void set_highpass(float f) {
    highpass_coeff = static_cast<float>(std::exp(-2 * M_PI * f));
  }

  /**
   * Perform highpass filtering and return the sample
   * 
   * @param sample the sample to input
   * @return highpassed sample
   */
  float highpass(float sample) {
    highpass_state = highpass_state * highpass_coeff + sample * (1 - highpass_coeff);
    return sample - highpass_state;
  }
};

class LCC : public PluginBase {
 public:
  LCC(const std::string& tag,
            const std::string& schema,
            const std::string& schema_path,
            PipeManager* pipe_manager,
            PipelineType pipe_type);
  LCC(const LCC&) = delete;
  auto operator=(const LCC&) -> LCC& = delete;
  LCC(const LCC&&) = delete;
  auto operator=(const LCC&&) -> LCC& = delete;
  ~LCC() override;

  void setup() override;

  void process(std::span<float>& left_in,
               std::span<float>& right_in,
               std::span<float>& left_out,
               std::span<float>& right_out) override;

  auto get_latency_seconds() -> float override;

  float delay_us = 360;
  float decay_db = -1.5;

 private:
  FilterState a;
  FilterState b;
};
