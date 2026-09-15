// Hibana synth engine - milestone 1
// Pure C++17, no JUCE dependency, so it can be unit-rendered offline (tools/render.cpp)
// and wrapped by the plugin (Source/PluginProcessor.cpp).
//
// Signal path per voice:
//   osc1 + osc2 -> mix -> state-variable filter (LP24 / LP12 / HP12 / BP12) -> amp envelope
// Milestone 1 covers: 2 oscillators, mixer, filter + filter envelope, amp envelope,
// 16-voice polyphony, velocity, pitch bend, sustain pedal.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace hibana
{

enum OscWave    { WaveSaw = 0, WaveSquare, WaveTriangle, WaveSine, WaveNoise };
enum FilterMode { FilterLP24 = 0, FilterLP12, FilterHP12, FilterBP12 };

constexpr int   kMaxVoices      = 16;
constexpr float kBendRangeSemis = 2.0f;
constexpr float kFilterEnvOcts  = 7.0f;   // full envelope amount = +/- 7 octaves
constexpr float kPi             = 3.14159265358979f;
constexpr float kTwoPi          = 6.28318530717959f;

// All user-facing parameters in real units. A later milestone adds a mapping layer
// from Synth1-style 0..127 patch values (.sy1) onto these.
struct Params
{
    int   osc1Wave   = WaveSaw;
    float osc1PW     = 0.5f;     // 0.05 .. 0.95
    int   osc2Wave   = WaveSaw;
    float osc2PW     = 0.5f;
    float osc2Semi   = 0.0f;     // -24 .. +24
    float osc2Fine   = 0.0f;     // cents, -50 .. +50
    float oscMix     = 0.0f;     // 0 = osc1 only, 1 = osc2 only

    int   filterMode = FilterLP24;
    float cutoffHz   = 12000.0f;
    float resonance  = 0.0f;     // 0 .. 1
    float keyTrack   = 0.0f;     // 0 .. 1 (1 = cutoff follows the keyboard exactly)
    float fEnvAmount = 0.0f;     // -1 .. 1

    float fAttack = 0.002f, fDecay = 0.4f, fSustain = 0.0f, fRelease = 0.3f;   // seconds / level
    float aAttack = 0.002f, aDecay = 0.4f, aSustain = 1.0f, aRelease = 0.2f;

    float velSens = 0.5f;        // 0 .. 1
    float gainDb  = -6.0f;
};

// ---------------------------------------------------------------- helpers

inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Linear up to 0.8, then a smooth knee that approaches 1.0. Protects against
// resonant peaks and big chords without colouring normal levels.
inline float softClip (float x) noexcept
{
    const float a = std::fabs (x);
    if (a <= 0.8f) return x;
    const float shaped = 0.8f + 0.2f * fastTanh ((a - 0.8f) / 0.2f);
    return x > 0.0f ? shaped : -shaped;
}

inline float noteToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

// Polynomial band-limited step, removes most aliasing from hard edges.
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)        { t /= dt;               return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
    return 0.0f;
}

// ---------------------------------------------------------------- oscillator

struct Oscillator
{
    float    phase = 0.0f;
    uint32_t rng   = 0x9E3779B9u;

    float next (int wave, float dt, float pw) noexcept
    {
        const float t = phase;
        float out = 0.0f;

        switch (wave)
        {
            case WaveSaw:
                out = 2.0f * t - 1.0f - polyBlep (t, dt);
                break;

            case WaveSquare:
            {
                out = (t < pw) ? 1.0f : -1.0f;
                out += polyBlep (t, dt);
                float t2 = t - pw;
                if (t2 < 0.0f) t2 += 1.0f;
                out -= polyBlep (t2, dt);
                out -= (2.0f * pw - 1.0f);        // remove the DC offset of narrow pulses
                break;
            }

            case WaveTriangle:
                out = 1.0f - 4.0f * std::fabs (t - 0.5f);
                break;

            case WaveSine:
                out = std::sin (kTwoPi * t);
                break;

            case WaveNoise:
            default:
                rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                out = (float) (rng & 0xFFFFFF) / 8388608.0f - 1.0f;
                break;
        }

        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        return out;
    }
};

// ---------------------------------------------------------------- envelope

struct EnvCoefs
{
    float attack = 0, decay = 0, release = 0, sustain = 0;

    void set (float a, float d, float s, float r, float sr) noexcept
    {
        a = std::max (a, 0.0005f); d = std::max (d, 0.001f); r = std::max (r, 0.001f);
        // Attack charges toward 1.3 (analog-style overshoot target) and reaches 1.0 at time a.
        attack  = 1.0f - std::exp (-1.466f / (a * sr));
        // Decay / release: exponential, roughly -35 dB after the set time.
        decay   = std::exp (-4.0f / (d * sr));
        release = std::exp (-4.0f / (r * sr));
        sustain = std::clamp (s, 0.0f, 1.0f);
    }
};

struct Envelope
{
    enum Stage { Idle, Attack, Decay, Release };
    Stage stage = Idle;
    float level = 0.0f;

    void gateOn()  noexcept { stage = Attack; }
    void gateOff() noexcept { if (stage != Idle) stage = Release; }
    void reset()   noexcept { stage = Idle; level = 0.0f; }

    float next (const EnvCoefs& c) noexcept
    {
        switch (stage)
        {
            case Attack:
                level += (1.3f - level) * c.attack;
                if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                break;
            case Decay:      // also acts as sustain: converges on the sustain level
                level = c.sustain + (level - c.sustain) * c.decay;
                break;
            case Release:
                level *= c.release;
                if (level < 1.0e-4f) { level = 0.0f; stage = Idle; }
                break;
            case Idle:
            default:
                break;
        }
        return level;
    }
};

// ---------------------------------------------------------------- filter
// Topology-preserving state-variable filter (Simper / Cytomic). Stable under fast
// cutoff modulation, which matters for snappy filter envelopes.

struct SVF
{
    float ic1 = 0.0f, ic2 = 0.0f;
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    inline void process (float v0, float g, float k, float& lp, float& bp, float& hp) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
};

// ---------------------------------------------------------------- voice

struct Voice
{
    int      note      = -1;
    float    velocity  = 0.0f;
    bool     keyDown   = false;
    bool     sustained = false;
    uint64_t age       = 0;

    Oscillator osc1, osc2;
    Envelope   fenv, aenv;
    SVF        stageA, stageB;
    float      g = 0.1f;
    int        ctrlCountdown = 0;

    bool isActive() const noexcept { return aenv.stage != Envelope::Idle; }
};

// ---------------------------------------------------------------- engine

class Engine
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = (float) sampleRate;
        for (auto& v : voices) { v = Voice{}; }
        for (int i = 0; i < kMaxVoices; ++i) voices[(size_t) i].osc2.rng ^= (uint32_t) (i * 7919 + 1);
        smoothedGain = targetGain();
        setParams (params);
    }

    void setParams (const Params& p) noexcept
    {
        params = p;
        fCoefs.set (p.fAttack, p.fDecay, p.fSustain, p.fRelease, sr);
        aCoefs.set (p.aAttack, p.aDecay, p.aSustain, p.aRelease, sr);
    }

    void noteOn (int note, float velocity) noexcept
    {
        Voice* v = findVoiceFor (note);
        const bool wasIdle = ! v->isActive();

        v->note = note;
        v->velocity = std::clamp (velocity, 0.0f, 1.0f);
        v->keyDown = true;
        v->sustained = false;
        v->age = ++ageCounter;

        if (wasIdle)
        {
            v->osc1.phase = 0.0f;
            v->osc2.phase = 0.0f;
            v->stageA.reset(); v->stageB.reset();
            v->fenv.reset(); v->aenv.reset();
        }
        // Stolen / retriggered voices keep their envelope level and phase: no clicks.
        v->ctrlCountdown = 0;
        v->fenv.gateOn();
        v->aenv.gateOn();
    }

    void noteOff (int note) noexcept
    {
        for (auto& v : voices)
        {
            if (v.note == note && v.keyDown && v.isActive())
            {
                v.keyDown = false;
                if (sustainPedal) v.sustained = true;
                else              { v.fenv.gateOff(); v.aenv.gateOff(); }
            }
        }
    }

    void setSustainPedal (bool down) noexcept
    {
        sustainPedal = down;
        if (! down)
            for (auto& v : voices)
                if (v.sustained) { v.sustained = false; v.fenv.gateOff(); v.aenv.gateOff(); }
    }

    void allNotesOff() noexcept
    {
        sustainPedal = false;
        for (auto& v : voices) { v.keyDown = false; v.sustained = false; v.fenv.gateOff(); v.aenv.gateOff(); }
    }

    void setPitchBend (float normalised) noexcept   // -1 .. 1
    {
        bendSemis = std::clamp (normalised, -1.0f, 1.0f) * kBendRangeSemis;
    }

    int activeVoiceCount() const noexcept
    {
        int n = 0;
        for (auto& v : voices) n += v.isActive() ? 1 : 0;
        return n;
    }

    // Writes (does not add) numSamples of mono output.
    void render (float* out, int numSamples) noexcept
    {
        std::fill (out, out + numSamples, 0.0f);

        const Params& p = params;
        const float mix2 = std::clamp (p.oscMix, 0.0f, 1.0f);
        const float mix1 = 1.0f - mix2;
        const float res  = std::clamp (p.resonance, 0.0f, 1.0f);
        const float kRes = 1.414f - 1.354f * res;    // 12 dB modes: Butterworth .. near self-oscillation
        const float kB   = 0.765f - 0.705f * res;    // 24 dB mode: resonant stage of a 4-pole Butterworth
        const float lpComp = 1.0f - 0.45f * res;     // tame the resonant peak a little
        const float nyqLimit = 0.45f * sr;
        const float pw1 = std::clamp (p.osc1PW, 0.05f, 0.95f);
        const float pw2 = std::clamp (p.osc2PW, 0.05f, 0.95f);

        for (auto& v : voices)
        {
            if (! v.isActive()) continue;

            const float baseNote = (float) v.note + bendSemis;
            const float dt1 = std::min (noteToHz (baseNote) / sr, 0.45f);
            const float dt2 = std::min (noteToHz (baseNote + p.osc2Semi + p.osc2Fine * 0.01f) / sr, 0.45f);
            const float velGain = (1.0f - p.velSens) + p.velSens * v.velocity;
            const float keyOcts = p.keyTrack * ((float) v.note - 60.0f) / 12.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float fe = v.fenv.next (fCoefs);
                const float ae = v.aenv.next (aCoefs);

                if (v.ctrlCountdown-- <= 0)     // update filter coefficient every 8 samples
                {
                    v.ctrlCountdown = 7;
                    const float octs = p.fEnvAmount * kFilterEnvOcts * fe + keyOcts;
                    const float fc = std::clamp (p.cutoffHz * std::exp2 (octs), 20.0f, nyqLimit);
                    v.g = std::tan (kPi * fc / sr);
                }

                const float o1 = v.osc1.next (p.osc1Wave, dt1, pw1);
                const float o2 = v.osc2.next (p.osc2Wave, dt2, pw2);
                const float x = fastTanh (mix1 * o1 + mix2 * o2);   // gentle input drive

                float lp, bp, hp, y;
                switch (p.filterMode)
                {
                    case FilterLP24:
                    {
                        float lpA, bpA, hpA;
                        v.stageA.process (x, v.g, 1.848f, lpA, bpA, hpA);                 // Butterworth stage
                        v.stageB.process (lpA, v.g, kB, lp, bp, hp);
                        y = lp * lpComp;
                        break;
                    }
                    case FilterLP12: v.stageA.process (x, v.g, kRes, lp, bp, hp); y = lp * lpComp; break;
                    case FilterHP12: v.stageA.process (x, v.g, kRes, lp, bp, hp); y = hp * lpComp; break;
                    case FilterBP12:
                    default:         v.stageA.process (x, v.g, kRes, lp, bp, hp); y = bp * kRes; break;  // constant peak gain
                }

                out[i] += y * ae * velGain * 0.5f;

                if (! v.isActive()) { v.note = -1; break; }
            }
        }

        // master: DC blocker (~10 Hz), smoothed gain, safety clip
        const float target = targetGain();
        const float smooth = 1.0f - std::exp (-1.0f / (0.02f * sr));
        const float dcR = 1.0f - kTwoPi * 10.0f / sr;
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = out[i];
            const float y = x - dcX1 + dcR * dcY1;
            dcX1 = x; dcY1 = y;
            smoothedGain += (target - smoothedGain) * smooth;
            out[i] = softClip (y * smoothedGain);
        }
    }

private:
    float targetGain() const noexcept { return std::pow (10.0f, params.gainDb / 20.0f); }

    Voice* findVoiceFor (int note) noexcept
    {
        // 1. same note already sounding -> retrigger it (no stacked duplicates)
        for (auto& v : voices) if (v.isActive() && v.note == note) return &v;
        // 2. a free voice
        for (auto& v : voices) if (! v.isActive()) return &v;
        // 3. steal the oldest released voice, else the oldest overall
        Voice* best = nullptr;
        for (auto& v : voices)
            if (! v.keyDown && ! v.sustained && (best == nullptr || v.age < best->age)) best = &v;
        if (best != nullptr) return best;
        best = &voices[0];
        for (auto& v : voices) if (v.age < best->age) best = &v;
        return best;
    }

    std::array<Voice, kMaxVoices> voices {};
    Params   params {};
    EnvCoefs fCoefs, aCoefs;
    float    sr = 48000.0f;
    float    bendSemis = 0.0f;
    float    smoothedGain = 0.5f;
    float    dcX1 = 0.0f, dcY1 = 0.0f;
    bool     sustainPedal = false;
    uint64_t ageCounter = 0;
};

} // namespace hibana
