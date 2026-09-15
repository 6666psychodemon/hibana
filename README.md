# Hibana

A free two-oscillator subtractive synth in the spirit of Synth1 (Daichi Laboratory).
Clean-room reimplementation: built from public documentation and listening, not from
Synth1's code. Builds as VST3, AU and a standalone app for current macOS
(Apple Silicon and Intel).

## Get a Mac build without installing anything (GitHub Actions)

1. Create a new repository on GitHub and upload everything in this folder
   (keep the hidden `.github` folder: it holds the build recipe).
2. Open the repository's **Actions** tab. The "Build macOS" run starts on every
   push; you can also start it by hand with "Run workflow".
3. When it finishes (about 10 minutes), download the **Hibana-macOS** artifact
   at the bottom of the run page. Unzip it, then unzip `Hibana-macOS.zip` inside.
4. In Terminal: `bash ~/Downloads/Hibana-macOS/install.sh`
   (adjust the path if you unzipped elsewhere).
5. Rescan plugins in your DAW. Logic and GarageBand use the AU; most other
   DAWs use the VST3. `Hibana.app` in your Applications folder runs without a DAW.

The run also tests the build: the engine test harness, Apple's `auval` for the AU,
and `pluginval` at maximum strictness for the VST3. Their logs are in the run's
steps.

## Build on your own Mac

Install Xcode (App Store) and CMake (`brew install cmake`), then:

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel --target Hibana_VST3 Hibana_AU Hibana_Standalone

Results land in `build/Hibana_artefacts/Release/`. Local builds need no
quarantine removal; copy them into `~/Library/Audio/Plug-Ins/VST3` and
`~/Library/Audio/Plug-Ins/Components`.

## Layout

    engine/HibanaEngine.h   the synth (plain C++, no JUCE): oscillators, filter, envelopes, voices
    engine/Presets.h        factory presets
    Source/                 JUCE plugin wrapper and interface
    tools/render.cpp        offline renderer + tests: writes a WAV per preset, checks levels,
                            voice stealing and filter stability
    scripts/install.sh      installer used by the downloaded build

The engine has no JUCE dependency, so sound changes can be tested by rendering
WAVs (`./build/hibana_render out-dir`) without opening a DAW.

## Roadmap

- [x] Milestone 1: 2 oscillators, mixer, 4 filter types, filter and amp envelopes,
      16 voices, velocity, pitch bend, sustain pedal, presets, build pipeline
- [ ] Milestone 2: FM, ring modulation, oscillator sync, osc 1 detune, sub oscillator
- [ ] Milestone 3: 2 LFOs, arpeggiator, portamento and legato, mod wheel routing
- [ ] Milestone 4: effects (chorus/flanger, tempo delay, distortion, EQ)
- [ ] Milestone 5: `.sy1` import and calibration against recordings of the original

## Licence note

Hibana is built on JUCE 8, which is available under AGPLv3 or a commercial JUCE
licence. Personal use is unrestricted. Sharing builds publicly means either
publishing the source under AGPLv3 or holding a JUCE licence (a free Starter
tier exists for small revenue).
