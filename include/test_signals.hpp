/*
 *  Copyright © 2017-2025 Wellington Wallace
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

#include <pipewire/context.h>
#include <pipewire/filter.h>
#include <pipewire/proxy.h>
#include <spa/utils/hook.h>
#include <sys/types.h>
#include <random>
#include <vector>
#include "pipe_manager.hpp"

enum class TestSignalType { sine_wave, gaussian, pink };

class Pinkify {
 private:
  float x1 = 0;
  float x2 = 0;
  float x3 = 0;

  float y1 = 0;
  float y2 = 0;
  float y3 = 0;

 public:
  float process(float x0) {
    /* From julius o. smith, to convert white noise to pink noise:
     *    
     * B = [0.049922035 -0.095993537 0.050612699 -0.004408786];
     * A = [1 -2.494956002 2.017265875 -0.522189400];
     *
     * Note: a0 is 1, so other parameters are used as-is.
     */
    auto y0 = 0.049922035f * x0 - 0.095993537f * x1 + 0.050612699f * x2 - 0.004408786f * x3
      + 2.494956002f * y1 - 2.017265875f * y2 + 0.522189400f * y3;

    y3 = y2;
    y2 = y1;
    y1 = y0;

    x3 = x2;
    x2 = x1;
    x1 = x0;

    return y0;
  }
};

class TestSignals {
 public:
  TestSignals(PipeManager* pipe_manager);
  TestSignals(const TestSignals&) = delete;
  auto operator=(const TestSignals&) -> TestSignals& = delete;
  TestSignals(const TestSignals&&) = delete;
  auto operator=(const TestSignals&&) -> TestSignals& = delete;
  virtual ~TestSignals();

  struct data;

  struct port {
    struct data* data;
  };

  struct data {
    struct port* out_left = nullptr;
    struct port* out_right = nullptr;

    TestSignals* ts = nullptr;
  };

  pw_filter* filter = nullptr;

  pw_filter_state state = PW_FILTER_STATE_UNCONNECTED;

  uint n_samples = 0U;

  uint rate = 0U;

  bool create_left_channel = true;

  bool create_right_channel = true;

  bool can_get_node_id = false;

  float sine_phase = 0.0F;

  float sine_frequency = 1000.0F;

  Pinkify pinkify;

  TestSignalType signal_type = TestSignalType::sine_wave;

  void set_state(const bool& state);

  void set_frequency(const float& value);

  [[nodiscard]] auto get_node_id() const -> uint;

  void set_active(const bool& state) const;

  void set_signal_type(const TestSignalType& value);

  auto white_noise() -> float;

 private:
  PipeManager* pm = nullptr;

  spa_hook listener{};

  data pf_data = {};

  uint node_id = 0U;

  std::vector<pw_proxy*> list_proxies;

  std::random_device rd{};

  std::mt19937 random_generator;

  std::normal_distribution<float> normal_distribution{0.0F, 0.3F};
};
