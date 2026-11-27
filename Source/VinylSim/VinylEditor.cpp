#include "VinylEditor.h"

namespace
{
const juce::Colour kBg (0xff1e1b22), kPanel (0xff2a2630), kAccent (0xffe8b85a), kText (0xffece6f0);
}

VinylSimEditor::VinylSimEditor (VinylSimProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), presetBar (p.apvts, "VinylSim")
{
    title.setText ("VinylSim", juce::dontSendNotification);
    title.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, kAccent);
    addAndMakeVisible (title);
    info.setColour (juce::Label::textColourId, kText.withAlpha (0.7f));
    info.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (info);

    for (int i = 0; i < processorRef.getNumPrograms(); ++i) factoryBox.addItem (processorRef.getProgramName (i), i + 1);
    factoryBox.setTextWhenNothingSelected ("Factory presets");
    factoryBox.onChange = [this] { processorRef.setCurrentProgram (factoryBox.getSelectedId() - 1); };
    addAndMakeVisible (factoryBox);
    addAndMakeVisible (presetBar);
    for (int i = 0; i < 3; ++i) { pageButtons[i].onClick = [this, i] { showPage (i); }; addAndMakeVisible (pageButtons[i]); }

    // page 0: brilliance, cartridge, stylus, groove, output
    add ("brilliance", "Brilliance", 0, 0);
    add ("brilRange",  "Brilliance Range", 2, 0);
    add ("sheen",      "Sheen (tracing H2)", 0, 0);
    add ("sheenRange", "Sheen Range", 2, 0);
    add ("tipRes",     "Tip Resonance", 0, 0);
    add ("cart",       "Cartridge", 2, 0);
    add ("loadPf",     "Load Capacitance", 0, 0);
    add ("loadKohm",   "Load Resistance", 0, 0);
    add ("stylus",     "Stylus", 2, 0);
    add ("format",     "Record", 2, 0);
    add ("position",   "Groove Position", 0, 0);
    add ("grooveDb",   "Cutting Level", 0, 0);
    add ("wear",       "Groove Wear", 0, 0);
    add ("mix",        "Dry/Wet", 0, 0);
    add ("outGain",    "Output Gain", 0, 0);
    add ("autoGain",   "Auto Gain", 1, 0);
    add ("bypass",     "Master Off", 1, 0);

    // page 1: cutting and playback
    add ("cutLimit",   "Lathe HF Limiter", 0, 1);
    add ("cutRange",   "Limiter Range", 2, 1);
    add ("elliptical", "Elliptical EQ (0 = off)", 0, 1);
    add ("separation", "Channel Separation", 0, 1);
    add ("lowCut",     "Subsonic Filter", 0, 1);
    add ("warp",       "Warp", 0, 1);
    add ("warpRange",  "Warp Range", 2, 1);
    add ("flutter",    "Flutter", 0, 1);
    add ("inGain",     "Input Gain", 0, 1);

    // page 2: surface
    add ("crackle",    "Crackle", 0, 2);
    add ("dust",       "Dust", 0, 2);
    add ("pops",       "Pops", 0, 2);
    add ("hiss",       "Hiss", 0, 2);
    add ("rumble",     "Rumble", 0, 2);
    add ("surfRange",  "Surface Range", 2, 2);
    add ("seed",       "Surface Seed", 0, 2);

    showPage (0);
    setSize (980, 600);
    startTimerHz (20);
}

void VinylSimEditor::add (const char* id, const char* text, int kind, int page)
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

void VinylSimEditor::showPage (int page)
{
    currentPage = page;
    for (auto& it : items) { const bool v = it->page == page; it->comp()->setVisible (v); it->label.setVisible (v && it->kind != 1); }
    for (int i = 0; i < 3; ++i)
    {
        pageButtons[i].setColour (juce::TextButton::buttonColourId, i == page ? kAccent.darker (0.3f) : juce::Colour (0xff3a3442));
        pageButtons[i].setColour (juce::TextButton::textColourOffId, i == page ? juce::Colours::black : kText);
    }
    resized();
}

void VinylSimEditor::timerCallback()
{
    auto& P = processorRef;
    info.setText ("Cartridge resonance " + juce::String (P.cartHz.load() / 1000.0, 1) + " kHz"
                  + "   |  Brilliance peak +" + juce::String (P.brilDb.load(), 1) + " dB"
                  + "   |  Sheen x" + juce::String (P.sheenMult.load(), 1)
                  + "   |  Groove speed " + juce::String (P.grooveSpeed.load(), 2) + " m/s"
                  + "   |  Latency " + juce::String (P.latencyMs.load(), 1) + " ms",
                  juce::dontSendNotification);
    repaint (scope);
}

void VinylSimEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kPanel);
    g.fillRoundedRectangle (getLocalBounds().reduced (8).withTrimmedTop (196).toFloat(), 8.0f);

    if (scope.isEmpty()) return;
    g.setColour (kPanel);
    g.fillRoundedRectangle (scope.toFloat(), 6.0f);
    auto r = scope.toFloat().reduced (10.0f, 8.0f);
    const float lo = -24.0f, hi = 30.0f;
    auto yOf = [&] (float db) { return r.getBottom() - (juce::jlimit (lo, hi, db) - lo) / (hi - lo) * r.getHeight(); };
    auto xOf = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    g.setFont (juce::FontOptions (10.0f));
    for (float db : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f, 24.0f })
    {
        g.setColour (kText.withAlpha (db == 0.0f ? 0.35f : 0.1f));
        g.drawHorizontalLine ((int) yOf (db), r.getX(), r.getRight());
        g.setColour (kText.withAlpha (0.45f));
        g.drawText (juce::String ((int) db), (int) r.getX() - 2, (int) yOf (db) - 6, 24, 12, juce::Justification::left);
    }
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        g.setColour (kText.withAlpha (0.12f));
        g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom());
        g.setColour (kText.withAlpha (0.45f));
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) xOf (f) + 3, (int) r.getBottom() - 12, 30, 12, juce::Justification::left);
    }
    juce::Path path;
    for (int i = 0; i < VinylSimProcessor::kCurvePoints; ++i)
    {
        const float x = xOf (VinylSimProcessor::curveFreq (i)), y = yOf (processorRef.curve[(size_t) i].load());
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
    g.setColour (kAccent);
    g.strokePath (path, juce::PathStrokeType (2.0f));
    g.setColour (kText.withAlpha (0.55f));
    g.drawText ("Playback response (cartridge + tip + brilliance + groove losses)", r.toNearestInt().removeFromTop (14), juce::Justification::centredRight);
}

void VinylSimEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (34);
    title.setBounds (top.removeFromLeft (130));
    presetBar.setBounds (top.removeFromRight (240).reduced (0, 4));
    top.removeFromRight (8);
    factoryBox.setBounds (top.removeFromRight (230).reduced (0, 4));
    for (auto& b : pageButtons) b.setBounds (top.removeFromLeft (130).reduced (2, 4));
    scope = r.removeFromTop (118).reduced (0, 6);
    info.setBounds (r.removeFromTop (22));
    r.removeFromTop (16);
    r = r.reduced (6);
    const int cols = 9, cellW = r.getWidth() / cols, cellH = 118;
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
