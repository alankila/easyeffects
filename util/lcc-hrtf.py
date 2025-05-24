#!/usr/bin/python3

import math

class Biquad:
  a1 = 0
  a2 = 0
  b0 = 1
  b1 = 0
  b2 = 0

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

  def transfer(self, omega):
    nom = self.b0 + self.b1 / omega + self.b2 / (omega*omega)
    den =       1 + self.a1 / omega + self.a2 / (omega*omega)
    return nom / den

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

hz = 20;
while (hz < 20000):
        angle = hz / rate * 2 * math.pi
        omega = math.cos(angle) + math.sin(angle) * 1j
        result = f1.transfer(omega) * f2.transfer(omega) * f3.transfer(omega) * f4.transfer(omega) * f5.transfer(omega) * 10 ** (decay / 20)

        magnitude = abs(result)
        phase = result / omega
        print("%f %f %f" % (hz, math.log(magnitude) / math.log(10) * 20, math.atan2(phase.imag, phase.real) / math.pi * 180))

        hz *= 1.1
