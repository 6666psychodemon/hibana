#pragma once

#include "PluginProcessor.h"

// Palette: aizome indigo panel, unbleached-cotton knobs and text,
// and one bold element: sparkler-gold value arcs ("hibana" = spark).
class HibanaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static inline const juce::Colour indigo  { 0xff1c2742 };
    static inline const juce::Colour section { 0xff243257 };
    static inline const juce::Colour line    { 0xff34466f };
    static inline const juce::Colour cotton  { 0xffece6d6 };
    static inline const juce::Colour gold    { 0xfff2b544 };

    HibanaLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

class HibanaEditor : public juce::AudioProcessorEditor
{
public:
    explicit HibanaEditor (HibanaProcessor&);
    ~HibanaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  name;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Rectangle<int> area;
    };

    Knob*   makeKnob (const char* paramId, const juce::String& label);
    Choice* makeChoice (const char* paramId);
    void    layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> row);

    HibanaProcessor& proc;
    HibanaLookAndFeel lnf;   // declared before the components so it outlives them
    juce::TooltipWindow tooltipWindow { this, 700 };

    std::vector<std::unique_ptr<Knob>>   knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<Section> sections;

    Choice *osc1Wave = nullptr, *osc2Wave = nullptr, *filterMode = nullptr;
    Knob *osc1PW = nullptr, *osc2Semi = nullptr, *osc2Fine = nullptr, *osc2PW = nullptr, *mix = nullptr,
         *cutoff = nullptr, *reso = nullptr, *fEnv = nullptr, *keyTrack = nullptr,
         *velocity = nullptr, *volume = nullptr,
         *fA = nullptr, *fD = nullptr, *fS = nullptr, *fR = nullptr,
         *aA = nullptr, *aD = nullptr, *aS = nullptr, *aR = nullptr;

    juce::ComboBox presetBox;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HibanaEditor)
};
