#include "WowEditor.h"

namespace
{
const juce::Colour kBg (0xff1b2224), kPanel (0xff263033), kAccent (0xff5fc9b0), kText (0xffe4ece9);
}

WowLabEditor::WowLabEditor (WowLabProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), presetBar (p.apvts, "WowLab")
{
    title.setText ("WowLab", juce::dontSendNotification);
    title.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, kAccent);
    addAndMakeVisible (title);
    info.setColour (juce::Label::textColourId, kText.withAlpha (0.65f));
    info.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (info);

    for (int i = 0; i < processorRef.getNumPrograms(); ++i) factoryBox.addItem (processorRef.getProgramName (i), i + 1);
    factoryBox.setTextWhenNothingSelected ("Factory presets");
    factoryBox.onChange = [this] { processorRef.setCurrentProgram (factoryBox.getSelectedId() - 1); };
    addAndMakeVisible (factoryBox);
    addAndMakeVisible (presetBar);
    for (int i = 0; i < 2; ++i) { pageButtons[i].onClick = [this, i] { showPage (i); }; addAndMakeVisible (pageButtons[i]); }

    // main page: rate, shape, analog, mix
    add ("rateMode", "Rate Mode", 2, 0);
    add ("speed",    "Speed", 0, 0);
    add ("note",     "Note", 2, 0);
    add ("tempoSync","Tempo Sync", 2, 0);
    add ("bpm",      "BPM", 0, 0);
    add ("amount",   "Amount", 0, 0);
    add ("mult",     "Amount Multiplier", 2, 0);
    add ("shape",    "Shape (square/sine/tri)", 0, 0);
    add ("flutter",  "Flutter Amount", 0, 0);
    add ("wetDry",   "Wet/Dry", 0, 0);
    add ("mixMode",  "Mix Mode", 2, 0);
    add ("analogOn", "Analog", 1, 0);
    add ("model",    "Analog Mode", 2, 0);
    add ("color",    "Analog Color", 0, 0);
    add ("saturation","Analog Saturation", 0, 0);
    add ("bypass",   "Master Off", 1, 0);

    // advanced page: trim, misc, random, HQ
    add ("inGain",   "Input Gain", 0, 1);
    add ("outGain",  "Output Gain", 0, 1);
    add ("stereoPhase","Stereo Phase", 0, 1);
    add ("phaseOffset","Phase Offset", 0, 1);
    add ("wetDelay", "Wet Delay", 0, 1);
    add ("noise",    "Analog Noise Gain", 0, 1);
    add ("hq",       "HQ Mode", 1, 1);
    add ("rndMode",  "Random Mode", 2, 1);
    add ("rndSpeed", "Random Speed", 0, 1);
    add ("rndAmount","Random Amount", 0, 1);
    add ("rndSmooth","Random Smoothing", 0, 1);
    add ("rndSeed",  "Random Seed", 0, 1);

    showPage (0);
    setSize (900, 520);
    startTimerHz (30);
}

void WowLabEditor::add (const char* id, const char* text, int kind, int page)
{
    auto it = std::make_unique<Item>();
    it->id = id; it->kind = kind; it->page = page;
    auto& apvts = processorRef.apvts;
    if (kind == 0)
    {
        it->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        it->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
        it->slider->setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        it->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        it->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *it->slider);
    }
    else if (kind == 1)
    {
        it->toggle = std::make_unique<juce::ToggleButton> (text);
        it->toggle->setColour (juce::ToggleButton::tickColourId, kAccent);
        it->bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, id, *it->toggle);
    }
    else
    {
        it->combo = std::make_unique<juce::ComboBox>();
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id))) it->combo->addItemList (ch->choices, 1);
        it->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *it->combo);
    }
    addAndMakeVisible (*it->comp());
    if (kind != 1)
    {
        it->label.setText (text, juce::dontSendNotification);
        it->label.setJustificationType (juce::Justification::centred);
        it->label.setColour (juce::Label::textColourId, kText);
        addAndMakeVisible (it->label);
    }
    items.push_back (std::move (it));
}

void WowLabEditor::showPage (int page)
{
    currentPage = page;
    for (auto& it : items) { const bool v = it->page == page; it->comp()->setVisible (v); it->label.setVisible (v && it->kind != 1); }
    for (int i = 0; i < 2; ++i)
    {
        pageButtons[i].setColour (juce::TextButton::buttonColourId, i == page ? kAccent.darker (0.3f) : juce::Colour (0xff334044));
        pageButtons[i].setColour (juce::TextButton::textColourOffId, i == page ? juce::Colours::black : kText);
    }
    resized();
}

void WowLabEditor::timerCallback()
{
    auto raw = [this] (const char* id) { return processorRef.apvts.getRawParameterValue (id)->load(); };
    const bool bpmMode = raw ("rateMode") > 0.5f, manual = raw ("tempoSync") < 0.5f, analog = raw ("analogOn") > 0.5f;
    for (auto& it : items)
    {
        bool en = true;
        if (it->id == "speed") en = ! bpmMode;
        if (it->id == "note" || it->id == "tempoSync") en = bpmMode;
        if (it->id == "bpm") en = bpmMode && manual;
        if (it->id == "model" || it->id == "color" || it->id == "saturation" || it->id == "noise") en = analog;
        if (it->id == "rndSpeed" || it->id == "rndAmount" || it->id == "rndSmooth" || it->id == "rndSeed") en = raw ("rndMode") > 0.5f;
        it->comp()->setEnabled (en);
        it->comp()->setAlpha (en ? 1.0f : 0.35f);
        it->label.setAlpha (en ? 1.0f : 0.35f);
    }
    const bool morph = raw ("mixMode") < 0.5f;
    info.setText (juce::String (morph ? "Morph: Wet/Dry scales the effect on a single path (no phasing)." :
                                        "Parallel: Wet/Dry blends with the dry signal (phasing / chorus by design).")
                  + "   Tempo " + juce::String (processorRef.getCurrentBpm(), 1) + " BPM"
                  + (raw ("hq") > 0.5f ? "   |  HQ on (17 samples latency)" : ""), juce::dontSendNotification);
    scopePhase += 0.03f;
    repaint (scope);
}

void WowLabEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kPanel);
    g.fillRoundedRectangle (getLocalBounds().reduced (8).withTrimmedTop (150).toFloat(), 8.0f);

    // modulation shape preview
    if (! scope.isEmpty())
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (scope.toFloat(), 6.0f);
        auto raw = [this] (const char* id) { return processorRef.apvts.getRawParameterValue (id)->load(); };
        const float shape = raw ("shape") * 0.01f, fl = raw ("flutter") * 0.01f;
        const float depth = juce::jlimit (0.05f, 1.0f, raw ("amount") * 0.01f * std::sqrt (std::pow (2.0f, std::round (raw ("mult")))) / 4.0f + 0.05f);
        juce::Path path;
        auto r = scope.toFloat().reduced (8.0f);
        for (int i = 0; i <= 200; ++i)
        {
            const float t = (float) i / 200.0f, ph = 2.0f * t + scopePhase * 0.2f;
            auto wv = [shape] (float x)
            {
                const float s = std::sin (juce::MathConstants<float>::twoPi * x);
                if (shape < 0.0f) return (1.0f + shape) * s + (-shape) * std::tanh (4.0f * s) / std::tanh (4.0f);
                return (1.0f - shape) * s + shape * (2.0f / juce::MathConstants<float>::pi) * std::asin (s);
            };
            const float v = (1.0f - fl) * wv (ph) + fl * 0.35f * wv (ph * 8.0f);
            const float y = r.getCentreY() - v * depth * r.getHeight() * 0.45f;
            if (i == 0) path.startNewSubPath (r.getX(), y); else path.lineTo (r.getX() + t * r.getWidth(), y);
        }
        g.setColour (kAccent);
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }
}

void WowLabEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (34);
    title.setBounds (top.removeFromLeft (140));
    presetBar.setBounds (top.removeFromRight (240).reduced (0, 4));
    top.removeFromRight (8);
    factoryBox.setBounds (top.removeFromRight (240).reduced (0, 4));
    for (auto& b : pageButtons) b.setBounds (top.removeFromLeft (100).reduced (2, 4));
    scope = r.removeFromTop (70).reduced (0, 6);
    info.setBounds (r.removeFromTop (22));
    r.removeFromTop (16);
    r = r.reduced (6);
    const int cols = 8, cellW = r.getWidth() / cols, cellH = 118;
    int n = 0;
    for (auto& it : items)
    {
        if (it->page != currentPage) continue;
        auto cell = juce::Rectangle<int> (r.getX() + (n % cols) * cellW, r.getY() + (n / cols) * cellH, cellW, cellH).reduced (4);
        ++n;
        if (it->kind == 0) { it->label.setBounds (cell.removeFromTop (16)); it->slider->setBounds (cell); }
        else if (it->kind == 2) { it->label.setBounds (cell.removeFromTop (16)); it->combo->setBounds (cell.removeFromTop (28)); }
        else it->toggle->setBounds (cell.removeFromTop (28).translated (0, 22));
    }
}
