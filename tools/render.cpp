// Offline renderer for the Hibana engine. No JUCE, no audio device.
// Build:  g++ -O2 -std=c++17 -I../engine render.cpp -o hibana_render
// Usage:  ./hibana_render <output-dir>
// Writes one WAV per factory preset plus hibana_demo.wav (all presets back to back),
// and prints peak / RMS / sanity checks so regressions show up without listening.
#include "HibanaEngine.h"
#include "Presets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int kSampleRate = 48000;

struct Event { double time; int type; int note; float vel; };   // type: 1 on, 0 off

std::vector<Event> phrase()
{
    std::vector<Event> e;
    auto note = [&e] (double t, double len, int n, float v) { e.push_back ({ t, 1, n, v }); e.push_back ({ t + len, 0, n, 0 }); };
    note (0.00, 0.40, 48, 0.9f);
    note (0.50, 0.40, 55, 0.6f);
    note (1.00, 1.60, 60, 0.7f); note (1.00, 1.60, 63, 0.7f); note (1.00, 1.60, 67, 0.7f);
    note (2.80, 0.45, 72, 1.0f);
    note (3.30, 0.25, 70, 0.5f);
    std::sort (e.begin(), e.end(), [] (const Event& a, const Event& b) { return a.time < b.time; });
    return e;
}

std::vector<float> renderPreset (const hibana::Params& p, double seconds)
{
    hibana::Engine engine;
    engine.prepare (kSampleRate);
    engine.setParams (p);

    const int total = (int) (seconds * kSampleRate);
    std::vector<float> out ((size_t) total, 0.0f);
    int pos = 0;
    for (const auto& ev : phrase())
    {
        const int at = std::min (total, (int) (ev.time * kSampleRate));
        while (pos < at) { const int n = std::min (256, at - pos); engine.render (out.data() + pos, n); pos += n; }
        if (ev.type == 1) engine.noteOn (ev.note, ev.vel); else engine.noteOff (ev.note);
    }
    while (pos < total) { const int n = std::min (256, total - pos); engine.render (out.data() + pos, n); pos += n; }
    return out;
}

bool writeWav (const std::string& path, const std::vector<float>& s)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;
    const uint32_t dataBytes = (uint32_t) s.size() * 2, byteRate = kSampleRate * 2;
    const uint16_t fmt = 1, ch = 1, align = 2, bits = 16;
    const uint32_t riffSize = 36 + dataBytes, fmtSize = 16, rate = kSampleRate;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riffSize, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtSize, 4, 1, f); std::fwrite (&fmt, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&rate, 4, 1, f); std::fwrite (&byteRate, 4, 1, f); std::fwrite (&align, 2, 1, f);
    std::fwrite (&bits, 2, 1, f); std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (float x : s) { const int16_t v = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, x)) * 32767.0f); std::fwrite (&v, 2, 1, f); }
    std::fclose (f);
    return true;
}

bool report (const char* name, const std::vector<float>& s)
{
    double peak = 0, sumSq = 0, sum = 0; bool finite = true;
    for (float x : s) { finite &= std::isfinite (x); peak = std::max (peak, (double) std::fabs (x)); sumSq += x * x; sum += x; }
    const double rms = std::sqrt (sumSq / s.size()), dc = sum / s.size();
    const bool silentTail = [&] { double m = 0; for (size_t i = s.size() - 2400; i < s.size(); ++i) m = std::max (m, (double) std::fabs (s[i])); return m < 1e-3; }();
    const bool ok = finite && peak > 0.01 && peak <= 1.0 && std::fabs (dc) < 0.01;
    std::printf ("%-16s peak %6.1f dBFS  rms %6.1f dBFS  dc %+.4f  tail %s  %s\n", name,
                 20 * std::log10 (peak + 1e-12), 20 * std::log10 (rms + 1e-12), dc,
                 silentTail ? "silent" : "RINGING", ok ? "ok" : "CHECK");
    return ok;
}

// Hammer the voice allocator: far more notes than voices, overlapping, with the pedal.
bool stressTest()
{
    hibana::Engine e; e.prepare (kSampleRate);
    auto presets = hibana::factoryPresets(); e.setParams (presets[1].params);
    std::vector<float> buf (64);
    bool finite = true; double peak = 0;
    e.setSustainPedal (true);
    for (int i = 0; i < 200; ++i)
    {
        e.noteOn (36 + (i * 7) % 60, (float) ((i % 10) + 1) / 10.0f);
        e.render (buf.data(), 64);
        for (float x : buf) { finite &= std::isfinite (x); peak = std::max (peak, (double) std::fabs (x)); }
        if (i % 3 == 0) e.noteOff (36 + (i * 5) % 60);
    }
    e.setSustainPedal (false);
    e.allNotesOff();
    for (int i = 0; i < 3000; ++i) e.render (buf.data(), 64);   // ~4 s of release
    const bool ok = finite && peak <= 1.0 && e.activeVoiceCount() == 0;
    std::printf ("stress test      200 notes / 16 voices, peak %.2f, voices left %d  %s\n", peak, e.activeVoiceCount(), ok ? "ok" : "CHECK");
    return ok;
}
// Worst case for the filter: every mode at full resonance, full envelope sweep, loud chord.
bool resonanceTest()
{
    bool ok = true;
    const char* names[] = { "LP24", "LP12", "HP12", "BP12" };
    for (int mode = 0; mode < 4; ++mode)
    {
        hibana::Params p;
        p.filterMode = mode; p.resonance = 1.0f; p.cutoffHz = 60.0f; p.fEnvAmount = 1.0f;
        p.fAttack = 0.3f; p.fDecay = 0.3f; p.fSustain = 0.0f; p.gainDb = 6.0f;
        hibana::Engine e; e.prepare (kSampleRate); e.setParams (p);
        for (int n : { 36, 48, 55, 60, 64, 67, 72, 84 }) e.noteOn (n, 1.0f);
        std::vector<float> buf (kSampleRate);
        e.render (buf.data(), (int) buf.size());
        bool finite = true; double peak = 0;
        for (float x : buf) { finite &= std::isfinite (x); peak = std::max (peak, (double) std::fabs (x)); }
        std::printf ("resonance %s   finite %s, peak %.3f  %s\n", names[mode], finite ? "yes" : "NO", peak, (finite && peak <= 1.0) ? "ok" : "CHECK");
        ok &= finite && peak <= 1.0;
    }
    return ok;
}
} // namespace

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    const auto presets = hibana::factoryPresets();
    std::vector<float> demo;
    bool allOk = true;

    for (const auto& pr : presets)
    {
        auto s = renderPreset (pr.params, 5.0);
        allOk &= report (pr.name, s);
        std::string file = pr.name; for (auto& c : file) if (c == ' ') c = '_';
        writeWav (dir + "/" + file + ".wav", s);
        demo.insert (demo.end(), s.begin(), s.end());
    }
    writeWav (dir + "/hibana_demo.wav", demo);
    allOk &= stressTest();
    allOk &= resonanceTest();
    std::printf (allOk ? "ALL CHECKS PASSED\n" : "SOME CHECKS NEED ATTENTION\n");
    return allOk ? 0 : 1;
}
