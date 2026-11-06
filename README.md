# Aurora

A pitch/formant voice shifter feeding a vintage-style reverb, built with JUCE as an
Audio Unit (plus VST3 and Standalone) for Logic Pro.

## Build (macOS)

You need Xcode (command line tools are enough) and CMake (`brew install cmake`).

```bash
cd Aurora
cmake -B build -G Xcode
cmake --build build --config Release
```

The first configure downloads JUCE (needs internet). After the build, the AU is copied to
`~/Library/Audio/Plug-Ins/Components/Aurora.component` and the VST3 to
`~/Library/Audio/Plug-Ins/VST3/`.

## Use in Logic

1. Open Logic. If it doesn't show up, go to Logic Pro > Settings > Plug-in Manager, select
   Aurora and click Reset & Rescan Selection.
2. On a track: Audio FX > Audio Units > Home Studio > Aurora.
3. To validate from Terminal: `auval -v aufx Auro Hsts`.
4. If Logic refuses it on Apple Silicon, ad-hoc sign it:
   `codesign --force --deep -s - ~/Library/Audio/Plug-Ins/Components/Aurora.component`

## Controls

**Shift**
- Pitch: -12 to +12 semitones (double-click to reset). Snap rounds it to whole semitones.
- Formant: moves vocal-style resonances down (bigger/darker) or up (smaller/brighter).
  This is an approximation using resonant filters rather than a true spectral-envelope shift.
- Mix: blend between your original voice and the shifted voice.

**Space**
- Decay, Size, Pre-Delay, Damp (high-frequency absorption), Low Cut, Mod (tail movement), Mix.
- Color: Warm (dark, slower build, more movement), Lush (balanced), Clear (bright, tighter).

**Footer**
- Routing: *Shift > Verb* shifts the voice then reverbs it. *Shift Tail* keeps the dry voice
  natural and shifts only the reverb tail (ghostly / shimmer-like).
- Output: -24 to +12 dB.

## Tests

`tests/dsp_test.cpp` checks the pitch shifter's accuracy and the reverb's stability and decay
time with no JUCE required:

```bash
cd tests && g++ -std=c++17 -O2 -I../Source dsp_test.cpp -o dsp_test && ./dsp_test
```

## Tuning

Wet loudness is set by `kWetTrim` in `Source/Core.h`. Reverb character lives in the `kColors`
table and the delay-length / early-reflection tables in the same file.
