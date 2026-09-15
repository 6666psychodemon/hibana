#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using namespace juce;

String timeText (float s)
{
    if (s < 1.0f) return String (roundToInt (s * 1000.0f)) + " ms";
    return String (s, 2) + " s";
}

NormalisableRange<float> timeRange (float maxSeconds)
{
    NormalisableRange<float> r (0.0005f, maxSeconds);
    r.setSkewForCentre (0.35f);
    return r;
}

std::unique_ptr<AudioParameterFloat> floatParam (const char* id, const char* name, NormalisableRange<float> range,
                                                 float def, std::function<String (float, int)> toText)
{
    return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (std::move (toText)));
}

auto percent  = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
auto bipolar  = [] (float v, int) { const int p = roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + String (p) + "%"; };
auto seconds  = [] (float v, int) { return timeText (v); };
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout HibanaProcessor::createLayout()
{
    using namespace juce;
    const hibana::Params d;   // engine defaults = parameter defaults
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    // Oscillators
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::osc1Wave, 1 }, "Osc 1 wave",
                                                          StringArray { "Saw", "Square", "Triangle", "Sine" }, d.osc1Wave));
    ps.push_back (floatParam (pid::osc1PW, "Osc 1 pulse width", { 0.05f, 0.95f }, d.osc1PW, percent));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::osc2Wave, 1 }, "Osc 2 wave",
                                                          StringArray { "Saw", "Square", "Triangle", "Sine", "Noise" }, d.osc2Wave));
    ps.push_back (floatParam (pid::osc2PW, "Osc 2 pulse width", { 0.05f, 0.95f }, d.osc2PW, percent));
    ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { pid::osc2Semi, 1 }, "Osc 2 pitch", -24, 24, (int) d.osc2Semi,
                      AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + String (v) + " st"; })));
    ps.push_back (floatParam (pid::osc2Fine, "Osc 2 fine", { -50.0f, 50.0f }, d.osc2Fine,
                              [] (float v, int) { const int c = roundToInt (v); return (c > 0 ? "+" : "") + String (c) + " ct"; }));
    ps.push_back (floatParam (pid::oscMix, "Osc mix", { 0.0f, 1.0f }, d.oscMix,
                              [] (float v, int) { const int b = roundToInt (v * 100.0f); return String (100 - b) + " / " + String (b); }));

    // Filter
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::fltMode, 1 }, "Filter type",
                                                          StringArray { "Low-pass 24", "Low-pass 12", "High-pass 12", "Band-pass 12" }, d.filterMode));
    {
        NormalisableRange<float> r (20.0f, 20000.0f);
        r.setSkewForCentre (1000.0f);
        ps.push_back (floatParam (pid::cutoff, "Cutoff", r, d.cutoffHz,
                                  [] (float v, int) { return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 1) + " kHz"; }));
    }
    ps.push_back (floatParam (pid::reso,     "Resonance",       { 0.0f, 1.0f },  d.resonance,  percent));
    ps.push_back (floatParam (pid::keyTrack, "Key tracking",    { 0.0f, 1.0f },  d.keyTrack,   percent));
    ps.push_back (floatParam (pid::fEnvAmt,  "Filter envelope", { -1.0f, 1.0f }, d.fEnvAmount, bipolar));

    // Envelopes
    ps.push_back (floatParam (pid::fA, "Filter attack",  timeRange (10.0f), d.fAttack,  seconds));
    ps.push_back (floatParam (pid::fD, "Filter decay",   timeRange (10.0f), d.fDecay,   seconds));
    ps.push_back (floatParam (pid::fS, "Filter sustain", { 0.0f, 1.0f },    d.fSustain, percent));
    ps.push_back (floatParam (pid::fR, "Filter release", timeRange (10.0f), d.fRelease, seconds));
    ps.push_back (floatParam (pid::aA, "Amp attack",     timeRange (10.0f), d.aAttack,  seconds));
    ps.push_back (floatParam (pid::aD, "Amp decay",      timeRange (10.0f), d.aDecay,   seconds));
    ps.push_back (floatParam (pid::aS, "Amp sustain",    { 0.0f, 1.0f },    d.aSustain, percent));
    ps.push_back (floatParam (pid::aR, "Amp release",    timeRange (10.0f), d.aRelease, seconds));

    // Output
    ps.push_back (floatParam (pid::velSens, "Velocity", { 0.0f, 1.0f }, d.velSens, percent));
    ps.push_back (floatParam (pid::gain, "Volume", { -48.0f, 6.0f }, d.gainDb,
                              [] (float v, int) { return String (v, 1) + " dB"; }));

    return { ps.begin(), ps.end() };
}

HibanaProcessor::HibanaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HibanaState", createLayout())
{
    auto get = [this] (const char* id) { auto* p = apvts.getRawParameterValue (id); jassert (p != nullptr); return p; };
    pp = { get (pid::osc1Wave), get (pid::osc1PW), get (pid::osc2Wave), get (pid::osc2PW), get (pid::osc2Semi),
           get (pid::osc2Fine), get (pid::oscMix), get (pid::fltMode), get (pid::cutoff), get (pid::reso),
           get (pid::keyTrack), get (pid::fEnvAmt), get (pid::fA), get (pid::fD), get (pid::fS), get (pid::fR),
           get (pid::aA), get (pid::aD), get (pid::aS), get (pid::aR), get (pid::velSens), get (pid::gain) };
}

void HibanaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mono.assign ((size_t) juce::jmax (samplesPerBlock, 512), 0.0f);
    engine.prepare (sampleRate);
    engine.setParams (readParams());
    keyboardState.reset();
}

bool HibanaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

hibana::Params HibanaProcessor::readParams() const
{
    hibana::Params p;
    p.osc1Wave   = (int) pp.osc1Wave->load();
    p.osc1PW     = pp.osc1PW->load();
    p.osc2Wave   = (int) pp.osc2Wave->load();
    p.osc2PW     = pp.osc2PW->load();
    p.osc2Semi   = pp.osc2Semi->load();
    p.osc2Fine   = pp.osc2Fine->load();
    p.oscMix     = pp.oscMix->load();
    p.filterMode = (int) pp.fltMode->load();
    p.cutoffHz   = pp.cutoff->load();
    p.resonance  = pp.reso->load();
    p.keyTrack   = pp.keyTrack->load();
    p.fEnvAmount = pp.fEnvAmt->load();
    p.fAttack = pp.fA->load(); p.fDecay = pp.fD->load(); p.fSustain = pp.fS->load(); p.fRelease = pp.fR->load();
    p.aAttack = pp.aA->load(); p.aDecay = pp.aD->load(); p.aSustain = pp.aS->load(); p.aRelease = pp.aR->load();
    p.velSens = pp.velSens->load();
    p.gainDb  = pp.gain->load();
    return p;
}

void HibanaProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                     engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())               engine.noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn())        engine.setSustainPedal (true);
    else if (m.isSustainPedalOff())       engine.setSustainPedal (false);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel())            engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
}

void HibanaProcessor::renderRange (juce::AudioBuffer<float>& buffer, int start, int end)
{
    while (start < end)
    {
        const int n = juce::jmin (end - start, (int) mono.size());
        engine.render (mono.data(), n);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom (ch, start, mono.data(), n);
        start += n;
    }
}

void HibanaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);   // merge on-screen keyboard
    engine.setParams (readParams());

    int pos = 0;
    for (const auto meta : midi)   // sample-accurate: render up to each event, then apply it
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        renderRange (buffer, pos, at);
        handleMidi (meta.getMessage());
        pos = at;
    }
    renderRange (buffer, pos, numSamples);
}

void HibanaProcessor::setParam (const char* id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void HibanaProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    const auto& p = presets[(size_t) index].params;
    setParam (pid::osc1Wave, (float) p.osc1Wave);  setParam (pid::osc1PW, p.osc1PW);
    setParam (pid::osc2Wave, (float) p.osc2Wave);  setParam (pid::osc2PW, p.osc2PW);
    setParam (pid::osc2Semi, p.osc2Semi);          setParam (pid::osc2Fine, p.osc2Fine);
    setParam (pid::oscMix, p.oscMix);
    setParam (pid::fltMode, (float) p.filterMode); setParam (pid::cutoff, p.cutoffHz);
    setParam (pid::reso, p.resonance);             setParam (pid::keyTrack, p.keyTrack);
    setParam (pid::fEnvAmt, p.fEnvAmount);
    setParam (pid::fA, p.fAttack); setParam (pid::fD, p.fDecay); setParam (pid::fS, p.fSustain); setParam (pid::fR, p.fRelease);
    setParam (pid::aA, p.aAttack); setParam (pid::aD, p.aDecay); setParam (pid::aS, p.aSustain); setParam (pid::aR, p.aRelease);
    setParam (pid::velSens, p.velSens);            setParam (pid::gain, p.gainDb);
}

const juce::String HibanaProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? juce::String (presets[(size_t) index].name) : juce::String();
}

void HibanaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HibanaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* HibanaProcessor::createEditor() { return new HibanaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HibanaProcessor(); }
