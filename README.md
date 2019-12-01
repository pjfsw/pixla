# pixla

The Pixla project is an 8-bit music style soundtracker using intracker synthesized sounds.

## Command column description

- `0xy` - Arpeggio, 4x speed playing note, note + x, note + y, note + 12
- `1xy` - Slide up, max 4 octaves up from note value then sliding stops
- `2xy` - Slide down, max 4 octaves down from note value then sliding stops
- `3xy` - Tone portamento, only xy == 00 supported (change pitch)
- `4xy` - Vibrato, x = speed, y = depth
- `7xy` - Tremolo, x = speed, y = depth
- `Bxx` - Jump to song position (0-255).
- `Cxx` - Channel volume 00-FF. Is reset to maximum value upon note trigger (but not on tone portamento)
- `Dxx` - Pattern break, set position to row xx in next song position
- `Exx` - Set global volume 00-FF. Will remain until a new value is set or song is restarted.
- `Fxx` - Set tempo, in 2 * BPM values (i.e. 0x40 = 64 = 128 BPM)

## Keys

- `Shift + F5,F6,F7,F8` - Mute channels 1-4
- `F1, F2` - Decrease/increase octave in editor
- `Shift + F1, F2` - Transpose track down/up
- `Alt + F1, F2` - Transpose pattern down/up
- `Space` - Stop/Edit
- `Right Ctrl` - Play Pattern
- `F9, F10` - Select instrument
- `Alt + F9, F10` - Decrease or increase song BPM (set on song start, can be overridden with F command)
- `Half / Shift + Half` - Increase/Decrease stepping
- `Alt + Left, Right` - Decrease/Increase pattern at position
- `Alt + Up, Down` - Decrease/Increase song position
- `Alt + Shift + Down` - Increase song position, if at the end, add another position
- `Alt + Home` - Move to beginning of song
- `Alt + End` - Move to end of song
- `Alt + Backspace` - Remove previous song pattern
- `Alt + Delete`- Remove current song pattern
- `Alt + Insert`- Insert a pattern at position
- `Shift + F9, F10` - Activate/deactivate instrument editor mode
- `Shift + X` - Cut track
- `Shift + C` - Copy track
- `Shift + V` - Paste track
- `Alt + X` - Cut pattern
- `Alt + C` - Copy pattern
- `Alt + V` - Paste pattern
- `Ctrl + O` - Open song
- `Ctrl + S` - Save song as
- `Ctrl + B` - Render WAV output
- `F12` - Save current song

### Instrument editor mode

- `Up, Down` - Navigate through instrument settings
- `Left, Right` - Alter current instrument setting

### Edit mode

- `Backspace` - Delete previous row
- `Insert` - Insert row at current position
- `Del` - Delete row at cursor
- `Return` - Clear note or command at cursor
- `Shift + Return` - Clear note and command at cursor
- `Ctrl + Z` - Undo changes in pattern (undo buffer resets when a new pattern is edited)
- `Ctrl + Y` - Redo changes in pattern (undo buffer resets when a new pattern is edited)

## Instruments/Patches
- Attack/Decay/Sustain/Release configurable per patch
- Up to three wave segments per patch
- Each wave segment has the following properties
  - A waveform (Lowpass Pulse, Lowpass Saw, Pulse with PWM, White Noise, Triangle, Ring Modulated Pulse)
  - A length in some time uint, or 0 to indicate this is the last segment
  - A starting duty cycle (for PWM)
  - Pulse width modulation speed
  - A note modifier keyword:
    - = 00 no change
    - < 00 subtract this from current note value, effectively adding the absolute value to the played note
    - \> 00 play the specified note regardless of the note in the tracker
  - A relative volume, values 0 = MAX/default, 1-127 = relative value
  - Filter
  - Ring modulation carrier frequency or 0 to use channel 0 frequency (when wave = Ring modulation)
