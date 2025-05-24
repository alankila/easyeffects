/*
 *  Copyright © 2017-2024 Antti S. Lankila  <alankila@bel.fi>
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

/**
 * Some standard filters from "Cookbook formulae for audio EQ biquad filter coefficients"
 * by Robert Bristow-Johnson  <rbj@audioimagination.com>, adapted for EasyEffects.
 */
class Biquad {
 private:
  float fa1 = 0;
  float fa2 = 0;
  float fb0 = 1;
  float fb1 = 0;
  float fb2 = 0;

  float x1 = 0;
  float x2 = 0;
  float y1 = 0;
  float y2 = 0;

  void set_coefficients(double a0, double a1, double a2, double b0, double b1, double b2) {
    fa1 = static_cast<float>(a1/a0);
    fa2 = static_cast<float>(a2/a0);
    fb0 = static_cast<float>(b0/a0);
    fb1 = static_cast<float>(b1/a0);
    fb2 = static_cast<float>(b2/a0);
  }

 public:
  void set_low_pass(double center_frequency, double sampling_frequency, double quality) {
    auto w0 = 2 * M_PI * center_frequency / sampling_frequency;
    auto alpha = std::sin(w0) / (2 * quality);

    auto b0 = (1 - std::cos(w0))/2;
    auto b1 =  1 - std::cos(w0);
    auto b2 = (1 - std::cos(w0))/2;
    auto a0 =  1 + alpha;
    auto a1 = -2 * std::cos(w0);
    auto a2 =  1 - alpha;
    set_coefficients(a0, a1, a2, b0, b1, b2);
  }

  void set_high_pass(double center_frequency, double sampling_frequency, double quality) {
    auto w0 = 2 * M_PI * center_frequency / sampling_frequency;
    auto alpha = std::sin(w0) / (2 * quality);
    auto b0 =  (1 + std::cos(w0))/2;
    auto b1 = -(1 + std::cos(w0));
    auto b2 =  (1 + std::cos(w0))/2;
    auto a0 =   1 + alpha;
    auto a1 =  -2 * std::cos(w0);
    auto a2 =   1 - alpha;
    set_coefficients(a0, a1, a2, b0, b1, b2);
  }

  void set_high_shelf(double center_frequency, double sampling_frequency, double db_gain, double quality) {
    auto w0 = 2 * M_PI * center_frequency / sampling_frequency;
    auto A = std::pow(10, db_gain / 40);
    auto alpha = std::sin(w0) / (2 * quality);

    auto b0 =      A * ((A + 1) + (A - 1) * std::cos(w0) + 2 * std::sqrt(A) * alpha);
    auto b1 = -2 * A * ((A - 1) + (A + 1) * std::cos(w0)                           ); 
    auto b2 =      A * ((A + 1) + (A - 1) * std::cos(w0) - 2 * std::sqrt(A) * alpha);
    auto a0 =           (A + 1) - (A - 1) * std::cos(w0) + 2 * std::sqrt(A) * alpha;
    auto a1 =  2 *     ((A - 1) - (A + 1) * std::cos(w0)                           );
    auto a2 =           (A + 1) - (A - 1) * std::cos(w0) - 2 * std::sqrt(A) * alpha;
    set_coefficients(a0, a1, a2, b0, b1, b2);
  }

  void set_peaking_band(double center_frequency, double sampling_frequency, double db_gain, double quality) {
    auto w0 = 2 * M_PI * center_frequency / sampling_frequency;
    auto A = std::pow(10, db_gain / 40);
    auto alpha = std::sin(w0) / (2 * quality);

    auto b0 =  1 + alpha*A;
    auto b1 = -2 * std::cos(w0);
    auto b2 =  1 - alpha*A;
    auto a0 =  1 + alpha/A;
    auto a1 = -2 * std::cos(w0);
    auto a2 =  1 - alpha/A;
    set_coefficients(a0, a1, a2, b0, b1, b2);
  }

  float process(float x0) {
    auto y0 = fb0 * x0 + fb1 * x1 + fb2 * x2 - y1 * fa1 - y2 * fa2;

    y2 = y1;
    y1 = y0;

    x2 = x1;
    x1 = x0;

    return y0;
  }
};

class FilterState {
 private:
  std::vector<float> data;
  size_t data_index = 0;

  Biquad f1;
  Biquad f2;
  Biquad f3;
  Biquad f4;
  Biquad f5;
  Biquad f6;
  Biquad f7;

 public:
  /**
   * Set up filtering line for specific delay and configure filters with sample rate.
   */
  void configure(double delay_us, double rate) {
    /* Configure delay line for the appropriate length (full sample precision only) */
    if (auto samples = static_cast<size_t>(std::round(delay_us / 1.0e6 * rate)); data.size() != samples) {
      data.resize(samples);
      data_index = 0;
    }

    /* 
     * Measured head shadow for 30 degree incident angle,
     * 2 fixed filters from me, REW optimized 5 PK filters:
     *
     * 1 True Manual HP_Q 140.0 0.00 0.710 
     * 2 True Auto PK 1306 1.70 1.000 1306 
     * 3 True Auto PK 1682 4.90 2.850 590.2 
     * 4 True Auto PK 2480 -22.20 1.114 2226 
     * 5 True Manual LP_Q 3000 0.00 1.00 
     * 6 True Auto PK 3525 1.80 7.006 503.1 
     * 7 True Auto PK 4048 7.50 2.928 1383 
     */
    f1.set_high_pass(140, rate, 0.710);
    f2.set_peaking_band(1306, rate, 1.7, 1.0);
    f3.set_peaking_band(1682, rate, 4.9, 2.85);
    f4.set_peaking_band(2480, rate, -22.2, 1.114);
    f5.set_low_pass(3000, rate, 1.0);
    f6.set_peaking_band(3525, rate, 1.8, 7.006);
    f7.set_peaking_band(4048, rate, 7.5, 2.928);
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
    sample = f1.process(sample);
    sample = f2.process(sample);
    sample = f3.process(sample);
    sample = f4.process(sample);
    sample = f5.process(sample);
    sample = f6.process(sample);
    sample = f7.process(sample);
    data[data_index] = sample;
    data_index = (data_index + 1) % data.size();
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

  bool phantom_center_only = false;
  float delay_us = 310;
  float decay_db = -4;

 private:
  FilterState a;
  FilterState b;
};
