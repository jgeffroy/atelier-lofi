#include "RetroEditor.h"

namespace
{
const juce::Colour kBg     (0xff1f1d24);
const juce::Colour kPanel  (0xff2b2833);
const juce::Colour kAccent (0xffd9a441);
const juce::Colour kText   (0xffe8e2d6);
const char* kPageNames[] = { "Main", "Response", "Cassette", "Vinyl", "VHS", "Motion", "Noise & Damage", "Echo" };
}

RetroDeckEditor::RetroDeckEditor (RetroDeckProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), userPresets (p.apvts, "RetroDeck")
{
    getLookAndFeel().setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    getLookAndFeel().setColour (juce::Slider::thumbColourId, kText);

    titleLabel.setText ("RetroDeck", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, kAccent);
    addAndMakeVisible (titleLabel);

    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        factoryBox.addItem (processorRef.getProgramName (i), i + 1);
    factoryBox.setTextWhenNothingSelected ("Factory presets");
    factoryBox.onChange = [this] { processorRef.setCurrentProgram (factoryBox.getSelectedId() - 1); };
    addAndMakeVisible (factoryBox);
    addAndMakeVisible (userPresets);

    for (int i = 0; i < NumPages; ++i)
    {
        pageButtons[i].setButtonText (kPageNames[i]);
        pageButtons[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (pageButtons[i]);
    }
    statusLabel.setColour (juce::Label::textColourId, kText.withAlpha (0.75f));
    addAndMakeVisible (statusLabel);

    // ---- main
    add ("media",    "Media",        ComboK, PMain);
    add ("input",    "Input",        KnobK,  PMain);
    add ("drive",    "Saturation",   KnobK,  PMain);
    add ("output",   "Output",       KnobK,  PMain);
    add ("mix",      "Mix",          KnobK,  PMain);
    add ("mixMode",  "Mix Mode",     ComboK, PMain);
    add ("autoGain", "Auto Gain",    ToggleK, PMain);
    add ("width",    "Width",        KnobK,  PMain);
    add ("mono",     "Mono",         ToggleK, PMain);
    add ("lowCut",   "Low Cut",      KnobK,  PMain);
    add ("highCut",  "High Cut",     KnobK,  PMain);
    add ("tone",     "Tone",         KnobK,  PMain);
    add ("hiss",     "Hiss / Surface", KnobK, PMain);
    add ("wfAmount", "Wow & Flutter", KnobK, PMain);
    add ("age",      "Age",          KnobK,  PMain);
    add ("stop",     "Stop",         ToggleK, PMain);
    add ("stopFollow","Stop with Host", ToggleK, PMain);
    add ("respAmt",  "Response",     KnobK,  PMain);
    add ("satAmt",   "Saturation Amt", KnobK, PMain);
    add ("compAmt",  "Compression",  KnobK,  PMain);

    // ---- response / saturation / compression editor
    add ("respAmt",  "Response Amount", KnobK, PTone);
    add ("respRange","Response Range",  ComboK, PTone);
    add ("satAmt",   "Saturation Amount", KnobK, PTone);
    add ("satRange", "Saturation Range", ComboK, PTone);
    add ("compAmt",  "Compression Amount", KnobK, PTone);
    add ("compRange","Compression Range", ComboK, PTone);
    add ("hfSlope",  "HF Slope",        ComboK, PTone);
    add ("hfTrim",   "HF Roll-off Trim", KnobK, PTone);
    add ("lowTrim",  "Low Roll-off Trim", KnobK, PTone);
    add ("bumpFreq", "Head Bump Freq",  KnobK, PTone);
    add ("cBump",    "Head Bump Gain",  KnobK, PTone);
    add ("drive",    "Saturation Drive", KnobK, PTone);
    add ("cSatModel","Saturation Model", ComboK, PTone);
    add ("cHfComp",  "HF Compression",  KnobK, PTone);
    add ("secResp",  "Response On",     ToggleK, PTone);
    add ("secSat",   "Saturation On",   ToggleK, PTone);
    add ("secComp",  "Compression On",  ToggleK, PTone);

    // ---- cassette
    add ("cDeck",    "Deck",          ComboK, PCassette);
    add ("cTape",    "Tape",          ComboK, PCassette);
    add ("cSpeed",   "Speed",         ComboK, PCassette);
    add ("cBias",    "Bias",          KnobK,  PCassette);
    add ("cSatModel","Saturation Model", ComboK, PCassette);
    add ("cHfComp",  "HF Compression", KnobK, PCassette);
    add ("cAlc",     "Recorder ALC",  ComboK, PCassette);
    add ("cAlcAmt",  "ALC Amount",    KnobK,  PCassette);
    add ("cNr",      "Noise Reduction", ComboK, PCassette);
    add ("cNrMode",  "NR Mode",       ComboK, PCassette);
    add ("cNrTrack", "NR Mistracking", KnobK, PCassette);
    add ("cGen",     "Generations",   KnobK,  PCassette);
    add ("cAzimuth", "Azimuth",       KnobK,  PCassette);
    add ("cXtalk",   "Crosstalk",     KnobK,  PCassette);
    add ("cBump",    "Head Bump",     KnobK,  PCassette);
    add ("cAsperity","Modulation Noise", KnobK, PCassette);
    add ("cPrint",   "Print-Through", KnobK,  PCassette);
    add ("cPrintTime","Print Time",   KnobK,  PCassette);

    // ---- vinyl
    add ("vTable",   "Turntable",     ComboK, PVinyl);
    add ("vCart",    "Cartridge",     ComboK, PVinyl);
    add ("vRecord",  "Record",        ComboK, PVinyl);
    add ("vMonoBass","Mono Bass (cut)", ComboK, PVinyl);
    add ("vPos",     "Groove Position", KnobK, PVinyl);
    add ("vWear",    "Wear",          KnobK,  PVinyl);
    add ("vCutLimit","Cutting HF Limiter", KnobK, PVinyl);
    add ("vRumble",  "Rumble",        KnobK,  PVinyl);
    add ("vCrackle", "Crackle",       KnobK,  PVinyl);
    add ("vCrackleTone","Crackle Tone", KnobK, PVinyl);
    add ("vDust",    "Dust",          KnobK,  PVinyl);
    add ("vPops",    "Pops",          KnobK,  PVinyl);
    add ("vScratch", "Scratch",       KnobK,  PVinyl);
    add ("vWarp",    "Warp",          KnobK,  PVinyl);
    add ("vEcc",     "Off-Centre",    KnobK,  PVinyl);

    // ---- VHS
    add ("hFormat",  "Audio Track",   ComboK, PVhs);
    add ("hSpeed",   "Tape Speed",    ComboK, PVhs);
    add ("hStandard","Standard",      ComboK, PVhs);
    add ("hDolby",   "Linear Dolby B-type", ToggleK, PVhs);
    add ("hTracking","Tracking Errors", KnobK, PVhs);
    add ("hSwitch",  "Head Switching Buzz", KnobK, PVhs);
    add ("hBleed",   "Video Whine",   KnobK,  PVhs);
    add ("hCompTrack","Hi-Fi Compander Error", KnobK, PVhs);
    add ("hGen",     "Dub Generations", KnobK, PVhs);

    // ---- motion
    add ("wfAmount", "Wow & Flutter", KnobK,  PMotion);
    add ("wowDepth", "Wow Depth",     KnobK,  PMotion);
    add ("wowRate",  "Wow Rate",      KnobK,  PMotion);
    add ("wowSync",  "Wow Sync",      ComboK, PMotion);
    add ("flutDepth","Flutter Depth", KnobK,  PMotion);
    add ("flutRate", "Flutter Rate",  KnobK,  PMotion);
    add ("wfRandom", "Randomness",    KnobK,  PMotion);
    add ("fm",       "Wow > Flutter FM", KnobK, PMotion);
    add ("drift",    "Speed Drift",   KnobK,  PMotion);
    add ("snap",     "Sticky Snaps",  KnobK,  PMotion);
    add ("stopTime", "Stop Time",     KnobK,  PMotion);
    add ("startTime","Start Time",    KnobK,  PMotion);
    add ("stop",     "Stop",          ToggleK, PMotion);
    add ("stopFollow","Stop with Host", ToggleK, PMotion);

    // ---- noise & damage
    add ("hiss",     "Hiss / Surface", KnobK, PNoise);
    add ("hissGate", "Noise Only With Signal", ToggleK, PNoise);
    add ("hum",      "Hum",           KnobK,  PNoise);
    add ("humFreq",  "Hum Frequency", ComboK, PNoise);
    add ("motor",    "Motor Noise",   KnobK,  PNoise);
    add ("age",      "Age",           KnobK,  PNoise);
    add ("dropouts", "Dropouts",      KnobK,  PNoise);
    add ("dropRate", "Dropout Rate",  KnobK,  PNoise);
    add ("dropWidth","Dropout Stereo", KnobK, PNoise);
    add ("degrade",  "Degradation",   KnobK,  PNoise);
    add ("degradeRate","Degradation Rate", KnobK, PNoise);

    // ---- echo
    add ("echoType", "Echo",          ComboK, PEcho);
    add ("echoSync", "Echo Sync",     ComboK, PEcho);
    add ("echoTime", "Echo Time",     KnobK,  PEcho);
    add ("echoFb",   "Feedback",      KnobK,  PEcho);
    add ("echoLevel","Level",         KnobK,  PEcho);
    add ("echoHP",   "Low Cut",       KnobK,  PEcho);
    add ("echoLP",   "High Cut",      KnobK,  PEcho);

    showPage (PMain);
    setSize (960, 560);
    startTimerHz (15);
}

void RetroDeckEditor::add (const char* id, const char* label, Kind kind, int page)
{
    auto c = std::make_unique<Control>();
    c->id = id; c->label = label; c->kind = kind; c->page = page;

    if (kind == KnobK)
    {
        c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
        c->slider->setColour (juce::Slider::textBoxTextColourId, kText);
        c->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        c->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, id, *c->slider);
    }
    else if (kind == ToggleK)
    {
        c->toggle = std::make_unique<juce::ToggleButton> (label);
        c->toggle->setColour (juce::ToggleButton::textColourId, kText);
        c->toggle->setColour (juce::ToggleButton::tickColourId, kAccent);
        c->bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.apvts, id, *c->toggle);
    }
    else
    {
        c->combo = std::make_unique<juce::ComboBox>();
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (id)))
            c->combo->addItemList (choice->choices, 1);
        c->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processorRef.apvts, id, *c->combo);
    }

    if (kind != ToggleK)
    {
        c->labelComp.setText (label, juce::dontSendNotification);
        c->labelComp.setJustificationType (juce::Justification::centred);
        c->labelComp.setColour (juce::Label::textColourId, kText);
        addAndMakeVisible (c->labelComp);
    }
    addAndMakeVisible (*c->comp());
    controls.push_back (std::move (c));
}

void RetroDeckEditor::showPage (int page)
{
    currentPage = page;
    for (auto& c : controls)
    {
        const bool vis = c->page == page;
        c->comp()->setVisible (vis);
        c->labelComp.setVisible (vis && c->kind != ToggleK);
    }
    resized();
    updateEnabled();
}

void RetroDeckEditor::updateEnabled()
{
    const int media = (int) std::lround (processorRef.apvts.getRawParameterValue ("media")->load());
    const int pageMedia = currentPage == PCassette ? 0 : currentPage == PVinyl ? 1 : currentPage == PVhs ? 2 : -1;
    auto mul = [this] (const char* id) { static const char* t[4] = { "x1", "x2", "x4", "x8" };
                                         return juce::String (t[juce::jlimit (0, 3, (int) std::lround (processorRef.apvts.getRawParameterValue (id)->load()))]); };
    const bool hifi = (int) std::lround (processorRef.apvts.getRawParameterValue ("hFormat")->load()) == 2;
    const bool echoOn = processorRef.apvts.getRawParameterValue ("echoType")->load() > 0.5f;
    const bool synced = processorRef.apvts.getRawParameterValue ("echoSync")->load() > 0.5f;

    for (auto& c : controls)
    {
        if (c->page != currentPage) continue;
        bool en = pageMedia < 0 || pageMedia == media;
        const auto& id = c->id;
        if (id == "hDolby" || id == "hSpeed") en = en && ! hifi;
        if (id == "hCompTrack") en = en && hifi;
        if (c->page == PEcho && id != "echoType") en = echoOn;
        if (id == "echoTime") en = en && ! synced;
        if (id == "cAlcAmt") en = en && processorRef.apvts.getRawParameterValue ("cAlc")->load() > 0.5f;
        if (id == "cBump" || id == "cSatModel" || id == "cHfComp") en = en && media == 0;
        if (c->kind == KnobK && (id == "respAmt" || id == "satAmt" || id == "compAmt"))
        {
            const juce::String r = mul (id == "respAmt" ? "respRange" : id == "satAmt" ? "satRange" : "compRange");
            const juce::String base = id == "respAmt" ? (c->page == PMain ? "Response" : "Response Amount")
                                    : id == "satAmt"  ? (c->page == PMain ? "Saturation Amt" : "Saturation Amount")
                                                      : (c->page == PMain ? "Compression" : "Compression Amount");
            c->labelComp.setText (base + " (" + r + ")", juce::dontSendNotification);
        }
        c->comp()->setEnabled (en);
        c->comp()->setAlpha (en ? 1.0f : 0.35f);
        c->labelComp.setAlpha (en ? 1.0f : 0.35f);
    }

    static const char* mediaNames[] = { "Cassette", "Vinyl", "VHS" };
    juce::String status = "Active media: " + juce::String (mediaNames[juce::jlimit (0, 2, media)]);
    if (pageMedia >= 0 && pageMedia != media)
        status << "   (this page is inactive: select " << mediaNames[pageMedia] << " on the Main page)";
    const double speed = processorRef.getTransportSpeed();
    if (speed < 0.999) status << (speed <= 0.0 ? "   |  STOPPED" : "   |  speed " + juce::String (speed * 100.0, 0) + " %");
    statusLabel.setText (status, juce::dontSendNotification);

    for (int i = 0; i < NumPages; ++i)
    {
        const bool isMediaPage = (i == PCassette && media == 0) || (i == PVinyl && media == 1) || (i == PVhs && media == 2);
        pageButtons[i].setColour (juce::TextButton::buttonColourId,
                                  i == currentPage ? kAccent.darker (0.2f) : (isMediaPage ? juce::Colour (0xff4a4256) : juce::Colour (0xff35323d)));
        pageButtons[i].setColour (juce::TextButton::textColourOffId, i == currentPage ? juce::Colours::black : kText);
    }
}

void RetroDeckEditor::timerCallback()
{
    updateEnabled();
    const float in = processorRef.getInputLevelDb(), out = processorRef.getOutputLevelDb();
    if (std::abs (in - shownIn) > 0.3f || std::abs (out - shownOut) > 0.3f)
    {
        shownIn = in; shownOut = out;
        repaint (meterArea);
    }
}

void RetroDeckEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kPanel);
    g.fillRoundedRectangle (getLocalBounds().reduced (8).withTrimmedTop (118).toFloat(), 8.0f);

    auto drawMeter = [&g] (juce::Rectangle<float> r, float db, const juce::String& name)
    {
        g.setColour (kText.withAlpha (0.7f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (name, r.removeFromLeft (30.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff3a3644));
        g.fillRoundedRectangle (r, 2.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (db > -1.0f ? juce::Colour (0xffe0503a) : kAccent);
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * norm), 2.0f);
    };
    if (! meterArea.isEmpty())
    {
        auto m = meterArea.toFloat();
        drawMeter (m.removeFromTop (m.getHeight() * 0.5f).reduced (0, 2), shownIn, "IN");
        drawMeter (m.reduced (0, 2), shownOut, "OUT");
    }
}

void RetroDeckEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto header = area.removeFromTop (34);
    titleLabel.setBounds (header.removeFromLeft (150));
    userPresets.setBounds (header.removeFromRight (250).reduced (0, 4));
    header.removeFromRight (8);
    factoryBox.setBounds (header.removeFromRight (240).reduced (0, 4));
    header.removeFromRight (12);
    meterArea = header.reduced (4, 4);

    auto bar = area.removeFromTop (34);
    for (int i = 0; i < NumPages; ++i)
        pageButtons[i].setBounds (bar.removeFromLeft (i == PNoise ? 124 : 96).reduced (2, 3));

    statusLabel.setBounds (area.removeFromTop (26).reduced (4, 2));
    area.removeFromTop (16);
    area = area.reduced (6);

    const int cols = 8;
    const int cellW = area.getWidth() / cols;
    const int cellH = 116;
    int n = 0;
    for (auto& c : controls)
    {
        if (c->page != currentPage) continue;
        const int col = n % cols, row = n / cols;
        auto cell = juce::Rectangle<int> (area.getX() + col * cellW, area.getY() + row * cellH, cellW, cellH).reduced (4);
        ++n;
        if (c->kind == KnobK)
        {
            c->labelComp.setBounds (cell.removeFromTop (16));
            c->slider->setBounds (cell);
        }
        else if (c->kind == ComboK)
        {
            c->labelComp.setBounds (cell.removeFromTop (16));
            c->combo->setBounds (cell.removeFromTop (28).reduced (2, 0));
        }
        else
            c->toggle->setBounds (cell.removeFromTop (28).translated (0, 22));
    }
}
