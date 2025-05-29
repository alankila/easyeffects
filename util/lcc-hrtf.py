#!/usr/bin/python3

import math

class Biquad:
  a1 = 0
  a2 = 0
  b0 = 1
  b1 = 0
  b2 = 0

  x2 = 0
  x1 = 0
  y1 = 0
  y2 = 0

  def set_coefficients(self, a0, a1, a2, b0, b1, b2):
    self.a1 = a1/a0
    self.a2 = a2/a0
    self.b0 = b0/a0
    self.b1 = b1/a0
    self.b2 = b2/a0

  def set_low_pass(self, center_frequency, sampling_frequency, quality):
    w0 = 2 * math.pi * center_frequency / sampling_frequency
    alpha = math.sin(w0) / (2 * quality)

    b0 = (1 - math.cos(w0))/2
    b1 =  1 - math.cos(w0)
    b2 = (1 - math.cos(w0))/2
    a0 =  1 + alpha
    a1 = -2 * math.cos(w0)
    a2 =  1 - alpha
    self.set_coefficients(a0, a1, a2, b0, b1, b2)

  def set_high_pass(self, center_frequency, sampling_frequency, quality):
    w0 = 2 * math.pi * center_frequency / sampling_frequency
    alpha = math.sin(w0) / (2 * quality)
    b0 =  (1 + math.cos(w0))/2
    b1 = -(1 + math.cos(w0))
    b2 =  (1 + math.cos(w0))/2
    a0 =   1 + alpha
    a1 =  -2 * math.cos(w0)
    a2 =   1 - alpha
    self.set_coefficients(a0, a1, a2, b0, b1, b2)

  def set_peaking_band(self, center_frequency, sampling_frequency, db_gain, quality):
    w0 = 2 * math.pi * center_frequency / sampling_frequency
    A = math.pow(10, db_gain / 40)
    alpha = math.sin(w0) / (2 * quality)

    b0 =  1 + alpha*A
    b1 = -2 * math.cos(w0)
    b2 =  1 - alpha*A
    a0 =  1 + alpha/A
    a1 = -2 * math.cos(w0)
    a2 =  1 - alpha/A
    self.set_coefficients(a0, a1, a2, b0, b1, b2)

  def set_allpass(self, center_frequency, sampling_frequency, quality):
    w0 = 2 * math.pi * center_frequency / sampling_frequency
    alpha = math.sin(w0) / (2 * quality)
    b0 =  1 - alpha
    b1 = -2 * math.cos(w0)
    b2 =  1 + alpha
    a0 =  1 + alpha
    a1 = -2 * math.cos(w0)
    a2 =  1 - alpha
    self.set_coefficients(a0, a1, a2, b0, b1, b2)

  def transfer(self, z):
    nom = self.b0 + self.b1 / z + self.b2 / (z*z)
    den =       1 + self.a1 / z + self.a2 / (z*z)
    return nom / den

  def process(self, x0):
    y0 = self.b0 * x0 + self.b1 * self.x1 + self.b2 * self.x2 - self.y1 * self.a1 - self.y2 * self.a2

    self.y2 = self.y1
    self.y1 = y0

    self.x2 = self.x1
    self.x1 = x0

    return y0

rate = 48000
f1 = Biquad()
f2 = Biquad()
f3 = Biquad()
f4 = Biquad()
f5 = Biquad()
f1.set_high_pass(300, rate, 0.710)
f2.set_peaking_band(1042, rate, -7.4, 1.798)
f3.set_peaking_band(2221, rate, -6.2, 3.140)
f4.set_low_pass(3000, rate, 1.0)
f5.set_peaking_band(3702, rate, -5.4, 3.108)
decay = -2

d1 = Biquad()
d1.set_allpass(870, rate, 0.22)

def transfer(o):
  o = math.cos(o) + math.sin(o) * 1j
  return f1.transfer(o) * f2.transfer(o) * f3.transfer(o) * f4.transfer(o) * f5.transfer(o) * 10 ** (decay / 20)

def transfer_allpass(o):
  o = math.cos(o) + math.sin(o) * 1j
  return -d1.transfer(o)

def process(sample):
  sample = f1.process(sample)
  sample = f2.process(sample)
  sample = f3.process(sample)
  sample = f4.process(sample)
  sample = f5.process(sample)
  sample *= 10 ** (decay / 20)
  return sample

def process_allpass(sample):
  return -d1.process(sample)

def dump_impulse():
  print(
"""* Impulse Response data saved by REW V5.31.3
* IR is not normalised
* IR window has not been applied
* IR is not the min phase version
* Source: /home/alankila/REW/validate-dsp.txt
* Dated: May 24, 2025 10:52:53 PM
* Measurement: validate-dsp
* Excitation: Comma/Tab/Space delimited data
* Response measured over: 20.0 to 19,052.7 Hz
1 // Peak value before normalisation
0 // Peak index
65536 // Response length
2.0833333333333333E-5 // Sample interval (seconds)
0 // Start time (seconds)
* Data start""")
  sample = 1
  for _ in range(0, 65536):
    output = process_allpass(sample)
    print("%.15g" % output)
    sample = 0

def dump_freq():
  hz = 20
  print("# frequency magnitude/db phase/deg phase_delay/us group_delay/ms processing_phase_mismatch/deg")

  error_avg = 0
  error_n = 1
  error_max_freq = 0
  error_max_phase = 0
  error_max = 0

  while hz < 20000:
    o = hz * 2 * math.pi
    o2 = o + 1e-5;

    result = transfer(o / rate)
    result2 = transfer(o2 / rate)

    magnitude = math.log(abs(result)) / math.log(10) * 20
    phase = math.atan2(result.imag, result.real) / math.pi * 180
    phase_delay_us = phase / 360 / hz * 1e6

    gd_difference = result2 / result
    group_delay_ms = -(math.atan2(gd_difference.imag, gd_difference.real)) / (o2 - o) * 1e3;

    # DSP with direct sound corrected with allpass filter compared to phase delay
    result_direct = transfer_allpass(o / rate) / result
    error_phase = math.atan2(result_direct.imag, result_direct.real) / math.pi * 180

    # Optimization target: weighted root mean square error_us
    weight = abs(result)
    weighted_error = error_phase ** 2 * weight
    error_avg += weighted_error
    error_n += weight
    if weighted_error > error_max:
      error_max = weighted_error
      error_max_phase = error_phase
      error_max_freq = hz

    print("%f %f %f %f %f %f" % (hz, magnitude, phase, phase_delay_us, group_delay_ms, error_phase))
    hz *= 1.02
  print("# weighted passband phase error rms: %f deg" % ((error_avg / error_n) ** 0.5))
  print("# worst case is %f Hz with %f deg" % (error_max_freq, error_max_phase));

dump_freq()
#dump_impulse()
