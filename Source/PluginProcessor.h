#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "HibanaEngine.h"
#include "Presets.h"

// Parameter IDs, in one place so the processor, editor and future .sy1 importer agree.
namespace pid
{
    inline constexpr const char* osc1Wave = "osc1Wave";
    inline constexpr const char* osc1PW   = "osc1PW";
    inline constexpr const char* osc2Wave = "osc2Wave";
    inline constexpr const char* osc2PW   = "osc2PW";
    inline constexpr const char* osc2Semi = "osc2Semi";
    inline constexpr const char* osc2Fine = "osc2Fine";
    inline constexpr const char* oscMix   = "oscMix";
    inline constexpr const char* fltMode  = "filterMode";
    inline constexpr const char* cutoff   = "cutoff";
    inline constexpr const char* reso     = "resonance";
    inline constexpr const char* keyTrack = "keyTrack";
    inline constexpr const char* fEnvAmt  = "filterEnvAmount";
    inline constexpr const char* fA = "filterAttack";
    inline constexpr const char* fD = "filterDecay";
    inline constexpr const char* fS = "filterSustain";
    inline constexpr const char* fR = "filterRelease";
    inline constexpr const char* aA = "ampAttack";
    inline constexpr const char* aD = "ampDecay";
    inline constexpr const char* aS = "ampSustain";
    inline constexpr const char* aR = "ampRelease";
    inline constexpr const char* velSens = "velocitySens";
    inline constexpr const char* gain    = "gain";
}

class HibanaProcessor : public juce::AudioProcessor
{
public:
    HibanaProcessor();
    ~HibanaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) presets.size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // shared with the on-screen keyboard

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    hibana::Params readParams() const;
    void handleMidi (const juce::MidiMessage& m);
    void renderRange (juce::AudioBuffer<float>& buffer, int start, int end);
    void setParam (const char* id, float plainValue);

    hibana::Engine engine;
    std::vector<hibana::Preset> presets = hibana::factoryPresets();
    std::vector<float> mono;
    int currentProgram = 0;

    struct ParamPointers
    {
        std::atomic<float> *osc1Wave, *osc1PW, *osc2Wave, *osc2PW, *osc2Semi, *osc2Fine, *oscMix,
                           *fltMode, *cutoff, *reso, *keyTrack, *fEnvAmt,
                           *fA, *fD, *fS, *fR, *aA, *aD, *aS, *aR, *velSens, *gain;
    } pp {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HibanaProcessor)
};
