#!/usr/bin/python3

import math

# For equilateral triangle, the far field value is 313 us. Listening distance
# makes nearly no difference.
#
# The effect rounds the delay lines to nearest full sample length, which are
# over 20 us apart at 50 kHz sample rates, so for higher precision, run at higher
# sample rates.
#
# That being said, 44100 and 48000 Hz sample rates both happen to work for equilateral
# triangle due to fortuitous sample lengths.
distance_speakers_m = 1.64 # (measured from the acoustic center position of speaker)
distance_listening_m = 1.64 # (measured from midpoint between ears to acoustic center position of speakers)
head_width_m = 0.215

# in an equilateral triangle, we have left speaker at (0, 0), right speaker at (distance_m, 0) and listener at 60 degree angle away from both
left = 0 + 0j
right = distance_speakers_m + 0j
# listener is in middle of the speakers, at j coordinate that creates the right distance value
listener_center = distance_speakers_m / 2 + (distance_listening_m ** 2 - (distance_speakers_m/2) ** 2) ** 0.5 * 1j

# when left plays to e.g. the right ear, then the sound is delayed by this distance
listener_right_ear = listener_center + head_width_m / 2
distance_left_to_contra_ear = abs(listener_right_ear - left) - abs(listener_right_ear - right)
time = distance_left_to_contra_ear / 343 * 1e6

print("if speakers are %.2f m apart and distance from center of head to both speakers is %.2f m, sound travel time difference is %.0f us" % (distance_speakers_m, distance_listening_m, time))
