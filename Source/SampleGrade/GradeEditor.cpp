#include "GradeEditor.h"

namespace
{
const juce::Colour kBg (0xff202326), kPanel (0xff2c3035), kAccent (0xffe0432f), kText (0xffe6e6e6);
}

SampleGradeEditor::SampleGradeEditor (SampleGradeProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), presetBar (p.apvts, "SampleGrade")
{
    title.setText ("SampleGrade", juce::dontSendNotification);
    title.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, kAccent);
    addAndMakeVisible (title);

    info.setText ("Grade rates derived from a 16 MHz clock (31.25 / 15.63 / 7.81 / 3.91 kHz); ADPCM models the ~4 bits per sample "
                  "implied by the memory size.", juce::dontSendNotification);
    info.setColour (juce::Label::textColourId, kText.withAlpha (0.6f));
    info.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (info);

    for (int i = 0; i < processorRef.getNumPrograms(); ++i) factoryBox.addItem (processorRef.getProgramName (i), i + 1);
    factoryBox.setTextWhenNothingSelected ("Factory presets");
    factoryBox.onChange = [this] { processorRef.setCurrentProgram (factoryBox.getSelectedId() - 1); };
    addAndMakeVisible (factoryBox);
    addAndMakeVisible (presetBar);

    static const char* names[4] = { "HI-FI", "STANDARD / LONG", "LO-FI 1", "LO-FI 2" };
    for (int i = 0; i < 4; ++i)
    {
        gradeButtons[(size_t) i].setButtonText (names[i]);
        gradeButtons[(size_t) i].onClick = [this, i]
        {
            if (auto* prm = processorRef.apvts.getParameter ("grade"))
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        };
        addAndMakeVisible (gradeButtons[(size_t) i]);
    }

    add ("grade",     "Sampling Grade", 2);
    add ("codec",     "Codec", 2);
    add ("decim",     "Decimation Filter", 2);
    add ("outFilter", "Output Filter", 2);
    add ("input",     "Input Level", 0);
    add ("outCut",    "Filter Cutoff", 0);
    add ("bits",      "Resolution", 0);
    add ("noise",     "Noise Floor", 0);
    add ("output",    "Output", 0);
    add ("mix",       "Mix", 0);
    add ("autoGain",  "Auto Gain", 1);
    add ("mono",      "Mono Sampling", 1);

    setSize (820, 420);
}

void SampleGradeEditor::add (const char* id, const char* text, int kind)
{
    auto it = std::make_unique<Item>();
    it->id = id; it->kind = kind;
    auto& apvts = processorRef.apvts;
    if (kind == 0)
    {
        it->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        it->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
        it->slider->setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        it->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        it->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *it->slider);
        addAndMakeVisible (*it->slider);
    }
    else if (kind == 1)
    {
        it->toggle = std::make_unique<juce::ToggleButton> (text);
        it->toggle->setColour (juce::ToggleButton::tickColourId, kAccent);
        it->bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, id, *it->toggle);
        addAndMakeVisible (*it->toggle);
    }
    else
    {
        it->combo = std::make_unique<juce::ComboBox>();
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id))) it->combo->addItemList (ch->choices, 1);
        it->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *it->combo);
        addAndMakeVisible (*it->combo);
    }
    if (kind != 1)
    {
        it->label.setText (text, juce::dontSendNotification);
        it->label.setJustificationType (juce::Justification::centred);
        it->label.setColour (juce::Label::textColourId, kText);
        addAndMakeVisible (it->label);
    }
    items.push_back (std::move (it));
}

void SampleGradeEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kPanel);
    g.fillRoundedRectangle (getLocalBounds().reduced (8).withTrimmedTop (104).toFloat(), 8.0f);
    const int cur = (int) std::lround (processorRef.apvts.getRawParameterValue ("grade")->load());
    for (int i = 0; i < 4; ++i)
        if (i == cur)
        {
            g.setColour (kAccent);
            g.fillRect (gradeButtons[(size_t) i].getBounds().withTop (gradeButtons[(size_t) i].getBottom()).withHeight (3));
        }
}

void SampleGradeEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (34);
    title.setBounds (top.removeFromLeft (170));
    presetBar.setBounds (top.removeFromRight (240).reduced (0, 4));
    top.removeFromRight (8);
    factoryBox.setBounds (top.removeFromRight (230).reduced (0, 4));

    auto row = r.removeFromTop (36);
    const int bw = row.getWidth() / 4;
    for (auto& b : gradeButtons) b.setBounds (row.removeFromLeft (bw).reduced (3, 2));
    info.setBounds (r.removeFromTop (22));
    r.removeFromTop (16);
    r = r.reduced (6);

    const int cols = 6, cellW = r.getWidth() / cols, cellH = 118;
    int n = 0;
    for (auto& it : items)
    {
        auto cell = juce::Rectangle<int> (r.getX() + (n % cols) * cellW, r.getY() + (n / cols) * cellH, cellW, cellH).reduced (5);
        ++n;
        if (it->kind == 0) { it->label.setBounds (cell.removeFromTop (16)); it->slider->setBounds (cell); }
        else if (it->kind == 2) { it->label.setBounds (cell.removeFromTop (16)); it->combo->setBounds (cell.removeFromTop (28)); }
        else it->toggle->setBounds (cell.removeFromTop (28).translated (0, 22));
    }
}
