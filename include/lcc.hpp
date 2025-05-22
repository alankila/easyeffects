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

  double delay_us = 360;
  double decay_db = -1.5;
  double center_db = -99;

 private:
  double delay_samples = 0;

  std::vector<float> data;
  unsigned int data_index = 0;

  float lowpass_coeff = 0;
  float left_lowpass_state = 0;
  float right_lowpass_state = 0;

  float highpass_coeff = 0;
  float left_highpass_state = 0;
  float right_highpass_state = 0;
};
