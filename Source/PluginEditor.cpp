#include "PluginEditor.h"

using LF = HibanaLookAndFeel;

namespace
{
juce::Font uiFont (float height, bool bold = false)
{
    auto opts = juce::FontOptions().withHeight (height);
    if (bold) opts = opts.withStyle ("Bold");
    return juce::Font (opts);
}

constexpr int kWidth = 950, kHeight = 424;
constexpr int kMargin = 14, kGap = 8;
constexpr int kTitleH = 26, kComboH = 24, kNameH = 16, kValueH = 16;
} // namespace

// ================================================================ look and feel

HibanaLookAndFeel::HibanaLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, indigo);

    setColour (Slider::textBoxTextColourId,       cotton.withAlpha (0.6f));
    setColour (Slider::textBoxOutlineColourId,    Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId,  gold.withAlpha (0.35f));
    setColour (Label::textColourId,               cotton);
    setColour (Label::textWhenEditingColourId,    cotton);
    setColour (TextEditor::textColourId,          cotton);
    setColour (TextEditor::backgroundColourId,    indigo);
    setColour (TextEditor::highlightColourId,     gold.withAlpha (0.35f));
    setColour (CaretComponent::caretColourId,     gold);

    setColour (ComboBox::backgroundColourId,      indigo);
    setColour (ComboBox::outlineColourId,         line);
    setColour (ComboBox::textColourId,           cotton);
    setColour (ComboBox::arrowColourId,          cotton.withAlpha (0.7f));
    setColour (ComboBox::focusedOutlineColourId,  gold);

    setColour (PopupMenu::backgroundColourId,            section);
    setColour (PopupMenu::textColourId,                  cotton);
    setColour (PopupMenu::highlightedBackgroundColourId, line);
    setColour (PopupMenu::highlightedTextColourId,       gold);

    setColour (MidiKeyboardComponent::whiteNoteColourId,         cotton);
    setColour (MidiKeyboardComponent::blackNoteColourId,         Colour (0xff141c30));
    setColour (MidiKeyboardComponent::keySeparatorLineColourId,  Colour (0xffb9b2a2));
    setColour (MidiKeyboardComponent::keyDownOverlayColourId,    gold.withAlpha (0.85f));
    setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, gold.withAlpha (0.25f));
    setColour (MidiKeyboardComponent::shadowColourId,            Colours::black.withAlpha (0.25f));
}

void HibanaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    using namespace juce;
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float arcR = radius - 2.0f, stroke = 3.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (line);
    g.strokePath (track, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));

    // Bipolar parameters (pitch, fine, envelope amount) light up from the centre.
    float from = startAngle;
    if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
        from = startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle);

    if (std::abs (angle - from) > 0.001f)
    {
        Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (s.isEnabled() ? gold : gold.withAlpha (0.3f));
        g.strokePath (value, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const float kr = arcR - 6.5f;
    g.setColour (cotton);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (Colour (0xffb9b2a2));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    Path pointer;
    pointer.addRoundedRectangle (-1.5f, -kr + 3.0f, 3.0f, kr * 0.55f, 1.5f);
    g.setColour (indigo);
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (c.x, c.y));
}

juce::Label* HibanaLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (11.5f));
    return l;
}

juce::Font HibanaLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (13.0f); }
juce::Font HibanaLookAndFeel::getPopupMenuFont()               { return uiFont (14.0f); }

// ================================================================ editor

HibanaEditor::HibanaEditor (HibanaProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    osc1Wave   = makeChoice (pid::osc1Wave);
    osc2Wave   = makeChoice (pid::osc2Wave);
    filterMode = makeChoice (pid::fltMode);

    osc1PW   = makeKnob (pid::osc1PW,   "Pulse width");
    osc2Semi = makeKnob (pid::osc2Semi, "Pitch");
    osc2Fine = makeKnob (pid::osc2Fine, "Fine");
    osc2PW   = makeKnob (pid::osc2PW,   "Pulse width");
    mix      = makeKnob (pid::oscMix,   "1 / 2");
    cutoff   = makeKnob (pid::cutoff,   "Cutoff");
    reso     = makeKnob (pid::reso,     "Resonance");
    fEnv     = makeKnob (pid::fEnvAmt,  "Envelope");
    keyTrack = makeKnob (pid::keyTrack, "Key track");
    velocity = makeKnob (pid::velSens,  "Velocity");
    volume   = makeKnob (pid::gain,     "Volume");
    fA = makeKnob (pid::fA, "Attack"); fD = makeKnob (pid::fD, "Decay");
    fS = makeKnob (pid::fS, "Sustain"); fR = makeKnob (pid::fR, "Release");
    aA = makeKnob (pid::aA, "Attack"); aD = makeKnob (pid::aD, "Decay");
    aS = makeKnob (pid::aS, "Sustain"); aR = makeKnob (pid::aR, "Release");

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.setTooltip ("Factory presets");
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (21.0f);
    keyboard.setScrollButtonsVisible (true);
    // Typing notes on the computer keyboard is handy in the standalone app,
    // but inside a DAW it would steal the host's shortcuts.
    const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    keyboard.setWantsKeyboardFocus (standalone);
    keyboard.setKeyPressBaseOctave (4);
    addAndMakeVisible (keyboard);

    setSize (kWidth, kHeight);
}

HibanaEditor::~HibanaEditor()
{
    setLookAndFeel (nullptr);
}

HibanaEditor::Knob* HibanaEditor::makeKnob (const char* paramId, const juce::String& label)
{
    auto k = std::make_unique<Knob>();
    // Add to the editor first so the value box is created with Hibana's look and feel.
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->name);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxTextColourId, LF::cotton.withAlpha (0.6f));
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, kValueH);
    k->slider.setMouseDragSensitivity (220);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);

    if (auto* param = proc.apvts.getParameter (paramId))
    {
        k->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        k->slider.setTooltip (param->getName (64));
    }

    k->name.setText (label, juce::dontSendNotification);
    k->name.setFont (uiFont (12.5f));
    k->name.setColour (juce::Label::textColourId, LF::cotton.withAlpha (0.8f));
    k->name.setJustificationType (juce::Justification::centred);
    k->name.setInterceptsMouseClicks (false, false);

    knobs.push_back (std::move (k));
    return knobs.back().get();
}

HibanaEditor::Choice* HibanaEditor::makeChoice (const char* paramId)
{
    auto c = std::make_unique<Choice>();
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        c->box.addItemList (param->choices, 1);
    c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, c->box);
    addAndMakeVisible (c->box);
    choices.push_back (std::move (c));
    return choices.back().get();
}

void HibanaEditor::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> row)
{
    if (row.size() == 0) return;
    const int w = area.getWidth() / (int) row.size();
    for (auto* k : row)
    {
        auto col = area.removeFromLeft (w);
        k->name.setBounds (col.removeFromTop (kNameH));
        k->slider.setBounds (col.reduced (4, 0));
    }
}

void HibanaEditor::resized()
{
    sections.clear();
    auto area = getLocalBounds().reduced (kMargin);

    // top bar: wordmark is painted, preset menu on the right
    auto top = area.removeFromTop (34);
    presetBox.setBounds (top.removeFromRight (220).withSizeKeepingCentre (220, 26));
    area.removeFromTop (kGap);

    keyboard.setBounds (area.removeFromBottom (72));
    area.removeFromBottom (kGap + 2);

    // Row 1 follows the signal: oscillators -> mixer -> filter -> output
    auto row1 = area.removeFromTop (148);
    area.removeFromTop (kGap);
    auto row2 = area;

    const float weights[] = { 1.6f, 3.0f, 1.1f, 4.0f, 2.0f };
    const float unit = (float) (row1.getWidth() - 4 * kGap) / 11.7f;
    auto take = [&] (juce::Rectangle<int>& r, float wgt, bool last)
    {
        auto s = last ? r : r.removeFromLeft (juce::roundToInt (unit * wgt));
        if (! last) r.removeFromLeft (kGap);
        return s;
    };

    auto body = [] (juce::Rectangle<int> s) { return s.reduced (8, 6).withTrimmedTop (kTitleH - 6); };

    auto sOsc1 = take (row1, weights[0], false);
    auto sOsc2 = take (row1, weights[1], false);
    auto sMix  = take (row1, weights[2], false);
    auto sFlt  = take (row1, weights[3], false);
    auto sOut  = take (row1, weights[4], true);

    sections.push_back ({ "Oscillator 1", sOsc1 });
    sections.push_back ({ "Oscillator 2", sOsc2 });
    sections.push_back ({ "Mix", sMix });
    sections.push_back ({ "Filter", sFlt });
    sections.push_back ({ "Output", sOut });

    auto withCombo = [&] (juce::Rectangle<int> s, Choice* c)
    {
        auto b = body (s);
        auto comboRow = b.removeFromTop (kComboH);
        if (c != nullptr) c->box.setBounds (comboRow.withWidth (juce::jmin (comboRow.getWidth(), 150)));
        b.removeFromTop (4);
        return b;   // every row-1 section reserves the combo row so all knobs line up
    };

    layoutKnobs (withCombo (sOsc1, osc1Wave),   { osc1PW });
    layoutKnobs (withCombo (sOsc2, osc2Wave),   { osc2Semi, osc2Fine, osc2PW });
    layoutKnobs (withCombo (sMix, nullptr),     { mix });
    layoutKnobs (withCombo (sFlt, filterMode),  { cutoff, reso, fEnv, keyTrack });
    layoutKnobs (withCombo (sOut, nullptr),     { velocity, volume });

    // Row 2: the two envelopes
    auto sFEnv = row2.removeFromLeft ((row2.getWidth() - kGap) / 2);
    row2.removeFromLeft (kGap);
    auto sAEnv = row2;
    sections.push_back ({ "Filter envelope", sFEnv });
    sections.push_back ({ "Amp envelope", sAEnv });
    layoutKnobs (body (sFEnv).reduced (10, 0), { fA, fD, fS, fR });
    layoutKnobs (body (sAEnv).reduced (10, 0), { aA, aD, aS, aR });

    repaint();
}

void HibanaEditor::paint (juce::Graphics& g)
{
    g.fillAll (LF::indigo);

    // wordmark + build version
    auto top = getLocalBounds().reduced (kMargin).removeFromTop (34);
    g.setColour (LF::cotton);
    g.setFont (uiFont (24.0f, true));
    g.drawText ("Hibana", top.removeFromLeft (110), juce::Justification::centredLeft);
    g.setColour (LF::cotton.withAlpha (0.45f));
    g.setFont (uiFont (12.0f));
    g.drawText ("v" JucePlugin_VersionString, top.removeFromLeft (80).translated (0, 4), juce::Justification::centredLeft);

    for (const auto& s : sections)
    {
        const auto r = s.area.toFloat();
        g.setColour (LF::section);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (LF::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);

        g.setColour (LF::cotton.withAlpha (0.92f));
        g.setFont (uiFont (13.5f, true));
        g.drawText (s.title, s.area.reduced (10, 6).removeFromTop (kTitleH - 8), juce::Justification::centredLeft);
    }
}
