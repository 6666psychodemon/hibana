// Factory presets for milestone 1. Shared by the plugin (program list) and tools/render.cpp.
#pragma once

#include "HibanaEngine.h"
#include <vector>

namespace hibana
{

struct Preset
{
    const char* name;
    Params params;
};

inline std::vector<Preset> factoryPresets()
{
    std::vector<Preset> list;

    { Params p; list.push_back ({ "Init", p }); }

    { Params p;                                   // two detuned saws, slow filter swell
      p.osc1Wave = WaveSaw; p.osc2Wave = WaveSaw; p.osc2Fine = 9.0f; p.oscMix = 0.5f;
      p.filterMode = FilterLP24; p.cutoffHz = 1400.0f; p.resonance = 0.15f; p.keyTrack = 0.5f;
      p.fEnvAmount = 0.22f; p.fAttack = 1.2f; p.fDecay = 2.5f; p.fSustain = 0.5f; p.fRelease = 1.5f;
      p.aAttack = 0.7f; p.aDecay = 1.0f; p.aSustain = 0.9f; p.aRelease = 1.8f;
      p.velSens = 0.3f; p.gainDb = -5.0f;
      list.push_back ({ "Warm pad", p }); }

    { Params p;                                   // square + sub saw, punchy envelope
      p.osc1Wave = WaveSquare; p.osc2Wave = WaveSaw; p.osc2Semi = -12.0f; p.oscMix = 0.4f;
      p.filterMode = FilterLP24; p.cutoffHz = 260.0f; p.resonance = 0.35f; p.keyTrack = 0.3f;
      p.fEnvAmount = 0.5f; p.fAttack = 0.001f; p.fDecay = 0.25f; p.fSustain = 0.1f; p.fRelease = 0.1f;
      p.aAttack = 0.001f; p.aDecay = 0.5f; p.aSustain = 0.7f; p.aRelease = 0.08f;
      p.velSens = 0.7f; p.gainDb = -4.0f;
      list.push_back ({ "Square bass", p }); }

    { Params p;                                   // resonant pluck
      p.osc1Wave = WaveSaw; p.osc2Wave = WaveSquare; p.osc2Semi = 12.0f; p.oscMix = 0.3f;
      p.filterMode = FilterLP24; p.cutoffHz = 350.0f; p.resonance = 0.6f; p.keyTrack = 0.6f;
      p.fEnvAmount = 0.55f; p.fAttack = 0.001f; p.fDecay = 0.35f; p.fSustain = 0.0f; p.fRelease = 0.3f;
      p.aAttack = 0.001f; p.aDecay = 0.9f; p.aSustain = 0.0f; p.aRelease = 0.4f;
      p.velSens = 0.6f; p.gainDb = -1.0f;
      list.push_back ({ "Resonant pluck", p }); }

    { Params p;                                   // narrow pulse + triangle octave, 12 dB filter
      p.osc1Wave = WaveSquare; p.osc1PW = 0.3f; p.osc2Wave = WaveTriangle; p.osc2Semi = 12.0f; p.oscMix = 0.35f;
      p.filterMode = FilterLP12; p.cutoffHz = 2500.0f; p.resonance = 0.3f; p.keyTrack = 0.4f;
      p.fEnvAmount = 0.2f; p.fAttack = 0.02f; p.fDecay = 0.5f; p.fSustain = 0.4f; p.fRelease = 0.3f;
      p.aAttack = 0.01f; p.aDecay = 0.2f; p.aSustain = 0.9f; p.aRelease = 0.25f;
      p.velSens = 0.4f; p.gainDb = -6.0f;
      list.push_back ({ "Hollow lead", p }); }

    { Params p;                                   // bright octave saws through the high-pass
      p.osc1Wave = WaveSaw; p.osc2Wave = WaveSaw; p.osc2Semi = 12.0f; p.osc2Fine = -6.0f; p.oscMix = 0.5f;
      p.filterMode = FilterHP12; p.cutoffHz = 900.0f; p.resonance = 0.4f; p.keyTrack = 0.5f;
      p.aAttack = 0.001f; p.aDecay = 0.6f; p.aSustain = 0.3f; p.aRelease = 0.6f;
      p.velSens = 0.5f; p.gainDb = -2.0f;
      list.push_back ({ "Glass keys", p }); }

    { Params p;                                   // filtered noise wind
      p.osc2Wave = WaveNoise; p.oscMix = 1.0f;
      p.filterMode = FilterBP12; p.cutoffHz = 500.0f; p.resonance = 0.75f; p.keyTrack = 1.0f;
      p.fEnvAmount = 0.35f; p.fAttack = 1.5f; p.fDecay = 2.0f; p.fSustain = 0.2f; p.fRelease = 2.0f;
      p.aAttack = 1.0f; p.aDecay = 1.0f; p.aSustain = 0.8f; p.aRelease = 2.0f;
      p.velSens = 0.2f; p.gainDb = 0.0f;
      list.push_back ({ "Noise wind", p }); }

    return list;
}

} // namespace hibana
