# pixla

The Pixla project is an 8-bit music style soundtracker using intracker synthesized sounds.

## Command column description

- `0xy` - Arpeggio, 4x speed playing note, note + x, note + y, note + 12
- `1xy` - Slide up, max 4 octaves up from note value then sliding stops
- `2xy` - Slide down, max 4 octaves down from note value then sliding stops
- `4xy` - Vibrato, x = speed, y = depth

## Keys
- `Left Alt + Z,X,C,V` - Mute channels 1-4
- `F1-F7` - Select octave
- `Space` - Stop/Edit
- `Right Ctrl` - Play Pattern
- `F9/F10` - Select instrument

## Instruments/Patches
- Attack/Decay/Sustain/Release configurable per patch
- Up to three wave segments per patch
- Each wave segment has the following properties
  - A waveform (Lowpass Pulse, Lowpass Saw, Pulse with PWM, White Noise)
  - A length in some time uint, or 0 to indicate this is the last segment
  - A starting duty cycle (for PWM)
  - Pulse width modulation speed
  - A note modifier keyword:
    - 00 no change
    - <00 subtract this from current note value, effectively adding the absolute value to the played note
    - >00 play the specified note regardless of the note in the tracker

