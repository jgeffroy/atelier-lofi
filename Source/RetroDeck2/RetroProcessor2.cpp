#include "RetroProcessor2.h"
#include "RetroEditor2.h"

namespace
{
using PV = std::pair<const char*, float>;
struct Preset { const char* name; std::vector<PV> values; };

// Values are plain parameter values; anything not listed is reset to its default.
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list =
    {
        { "Init (Cassette, home deck)",     {} },
        { "Clean Chrome Glue",              { { "cDeck", 0 }, { "cTape", 1 }, { "drive", 25 }, { "wfAmount", 40 }, { "hiss", -12 } } },
        { "Portastudio Bounce",             { { "cDeck", 1 }, { "cTape", 1 }, { "cSpeed", 1 }, { "drive", 45 }, { "cNr", 1 }, { "cGen", 2 } } },
        { "Walkman Wobble",                 { { "cDeck", 3 }, { "cTape", 0 }, { "drive", 40 }, { "wfAmount", 160 }, { "wfRandom", 50 }, { "drift", 40 } } },
        { "Boombox Dub 1996",               { { "cDeck", 4 }, { "cTape", 0 }, { "drive", 55 }, { "cAlc", 1 }, { "cGen", 2 }, { "age", 20 } } },
        { "Dictaphone Memo",                { { "cDeck", 5 }, { "cTape", 0 }, { "drive", 60 }, { "cAlc", 2 }, { "cAlcAmt", 80 }, { "hiss", 6 } } },
        { "Lo-Fi Hip-Hop Tape",             { { "cDeck", 2 }, { "cTape", 0 }, { "drive", 50 }, { "wfAmount", 130 }, { "age", 30 }, { "dropouts", 30 }, { "hiss", 4 } } },
        { "Synthwave Metal Tape",           { { "cDeck", 0 }, { "cTape", 3 }, { "drive", 60 }, { "wfAmount", 70 }, { "width", 120 } } },
        { "NR Misuse Brightener",           { { "cDeck", 0 }, { "cTape", 1 }, { "cNr", 1 }, { "cNrMode", 1 }, { "drive", 20 }, { "wfAmount", 0 }, { "hiss", -48 } } },
        { "Old Mixtape (Age)",              { { "cTape", 0 }, { "age", 70 }, { "degrade", 40 }, { "dropouts", 40 }, { "cPrint", -42 }, { "cAzimuth", 35 } } },
        { "Third-Generation Dub",           { { "cTape", 0 }, { "cGen", 3 }, { "drive", 45 }, { "cNr", 1 }, { "cNrTrack", -3 } } },
        { "Drum Crush ALC",                 { { "cDeck", 4 }, { "cAlc", 2 }, { "cAlcAmt", 100 }, { "drive", 70 }, { "mix", 60 } } },
        { "Tape Stop Ready (host)",         { { "stopFollow", 1 }, { "stopTime", 1200 }, { "startTime", 600 } } },
        { "Tape Slapback Echo",             { { "cDeck", 1 }, { "echoType", 1 }, { "echoTime", 120 }, { "echoLevel", 45 } } },
        { "Seasick Warble",                 { { "cDeck", 3 }, { "wfAmount", 300 }, { "wowDepth", 300 }, { "wowRate", 0.4f }, { "fm", 60 }, { "snap", 40 } } },
        { "Vinyl: Clean LP",                { { "media", 1 }, { "vCrackle", 8 }, { "vDust", 10 }, { "vPops", 3 } } },
        { "Vinyl: Dusty Crate Digger",      { { "media", 1 }, { "vTable", 1 }, { "vCrackle", 45 }, { "vDust", 40 }, { "vPops", 25 }, { "vWear", 40 }, { "vPos", 70 } } },
        { "Vinyl: Scratched Record",        { { "media", 1 }, { "vScratch", 60 }, { "vCrackle", 30 }, { "vWarp", 30 } } },
        { "Vinyl: 1930s Shellac 78",        { { "media", 1 }, { "vTable", 3 }, { "vCart", 2 }, { "vRecord", 2 }, { "vCrackle", 70 }, { "vDust", 60 }, { "hiss", 6 } } },
        { "Vinyl: Suitcase Player",         { { "media", 1 }, { "vTable", 2 }, { "vCart", 2 }, { "vWarp", 45 }, { "vEcc", 40 }, { "vCrackle", 30 } } },
        { "Vinyl: Dubplate Cut (mono bass)",{ { "media", 1 }, { "vRecord", 3 }, { "vMonoBass", 2 }, { "vCutLimit", 40 }, { "vCrackle", 5 } } },
        { "Vinyl: Spin-Down Ready",         { { "media", 1 }, { "stopFollow", 1 }, { "stopTime", 2000 }, { "startTime", 900 } } },
        { "VHS: Hi-Fi Soundtrack",          { { "media", 2 }, { "hFormat", 2 }, { "hSwitch", 25 }, { "hTracking", 10 } } },
        { "VHS: Linear SP Mono",            { { "media", 2 }, { "hFormat", 0 }, { "hSpeed", 0 }, { "drive", 45 } } },
        { "VHS: EP Rental Tape",            { { "media", 2 }, { "hFormat", 1 }, { "hSpeed", 2 }, { "hTracking", 40 }, { "age", 40 }, { "hGen", 2 }, { "wfAmount", 160 } } },
        { "VHS: Tracking Nightmare",        { { "media", 2 }, { "hFormat", 2 }, { "hTracking", 70 }, { "hSwitch", 60 }, { "hCompTrack", -2 }, { "hBleed", -52 } } },
        { "VHS: NTSC Home Video",           { { "media", 2 }, { "hFormat", 2 }, { "hStandard", 1 }, { "hSwitch", 40 }, { "hBleed", -55 }, { "humFreq", 1 }, { "hum", -66 } } },
    };
    return list;
}
}

RetroDeck2Processor::RetroDeck2Processor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RETRODECK2", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout RetroDeck2Processor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto F = [&p] (const char* id, const char* name, float lo, float hi, float step, float skew, float def, const char* unit)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, Range (lo, hi, step, skew), def,
                                                                  juce::AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto B = [&p] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def)); };
    auto C = [&p] (const char* id, const char* name, const juce::StringArray& items, int def)
    { p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    auto I = [&p] (const char* id, const char* name, int lo, int hi, int def)
    { p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, def)); };

    // main
    C ("media",    "Media", { "Cassette", "Vinyl", "VHS" }, 0);
    F ("input",    "Input",      -24.0f, 24.0f, 0.1f, 1.0f, 0.0f, "dB");
    F ("output",   "Output",     -24.0f, 24.0f, 0.1f, 1.0f, 0.0f, "dB");
    B ("autoGain", "Auto Gain", true);
    F ("mix",      "Mix",          0.0f, 100.0f, 0.1f, 1.0f, 100.0f, "%");
    F ("drive",    "Saturation",   0.0f, 100.0f, 0.1f, 1.0f, 30.0f, "%");
    F ("width",    "Width",        0.0f, 200.0f, 0.1f, 1.0f, 100.0f, "%");
    B ("mono",     "Mono", false);
    F ("lowCut",   "Low Cut",     10.0f, 500.0f, 1.0f, 0.4f, 10.0f, "Hz");
    F ("highCut",  "High Cut",  2000.0f, 22000.0f, 1.0f, 0.4f, 22000.0f, "Hz");
    F ("tone",     "Tone",      -100.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    C ("mixMode",  "Mix Mode", { "Morph (no phasing)", "Parallel, aligned", "Parallel, classic" }, 0);

    // stage amounts with range multipliers, and the response editor
    const juce::StringArray ranges { "x1", "x2", "x4", "x8" };
    F ("respAmt",  "Response Amount",    0.0f, 100.0f, 0.1f, 1.0f, 100.0f, "%");
    C ("respRange","Response Range", ranges, 0);
    F ("satAmt",   "Saturation Amount",  0.0f, 100.0f, 0.1f, 1.0f, 100.0f, "%");
    C ("satRange", "Saturation Range", ranges, 0);
    F ("compAmt",  "Compression Amount", 0.0f, 100.0f, 0.1f, 1.0f, 100.0f, "%");
    C ("compRange","Compression Range", ranges, 0);
    F ("hfTrim",   "HF Roll-off Trim",  -2.0f, 2.0f, 0.01f, 1.0f, 0.0f, "oct");
    C ("hfSlope",  "HF Slope", { "12 dB/oct", "24 dB/oct", "36 dB/oct" }, 1);
    F ("bumpFreq", "Head Bump Frequency", 50.0f, 200.0f, 0.1f, 1.0f, 100.0f, "%");
    F ("lowTrim",  "Low Roll-off Trim", -2.0f, 2.0f, 0.01f, 1.0f, 0.0f, "oct");

    B ("secResp",  "Response", true);
    B ("secSat",   "Saturation On", true);
    B ("secComp",  "Compression", true);

    // cassette
    C ("cDeck",    "Deck",  { "Studio 3-Head", "Portastudio 4-Track", "Home Stereo", "Walkman", "Boombox", "Dictaphone" }, 2);
    C ("cTape",    "Tape",  { "Type I (Ferric)", "Type II (Chrome)", "Type III (FeCr)", "Type IV (Metal)" }, 1);
    C ("cSpeed",   "Speed", { "1 7/8 ips", "3 3/4 ips" }, 0);
    F ("cBias",    "Bias",      -100.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    C ("cSatModel","Saturation Model", { "Hysteresis (magnetic)", "Smooth", "Hard" }, 0);
    F ("cHfComp",  "HF Compression", 0.0f, 100.0f, 0.1f, 1.0f, 50.0f, "%");
    C ("cAlc",     "Recorder ALC", { "Off", "Gentle", "Crush" }, 0);
    F ("cAlcAmt",  "ALC Amount",   0.0f, 100.0f, 0.1f, 1.0f, 50.0f, "%");
    C ("cNr",      "Noise Reduction", { "Off", "B-type", "C-type", "dbx-type" }, 0);
    C ("cNrMode",  "NR Mode", { "Encode + Decode", "Encode only", "Decode only" }, 0);
    F ("cNrTrack", "NR Mistracking", -10.0f, 10.0f, 0.1f, 1.0f, 0.0f, "dB");
    I ("cGen",     "Generations", 1, 4, 1);
    F ("cAzimuth", "Azimuth",   -100.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    F ("cXtalk",   "Crosstalk",  -20.0f, 20.0f, 0.1f, 1.0f, 0.0f, "dB");
    F ("cPrint",   "Print-Through", -80.0f, -20.0f, 0.1f, 1.0f, -80.0f, "dB");
    F ("cPrintTime","Print Time",  0.3f, 3.0f, 0.01f, 1.0f, 1.4f, "s");
    F ("cBump",    "Head Bump",   -6.0f, 6.0f, 0.1f, 1.0f, 0.0f, "dB");
    F ("cAsperity","Modulation Noise", 0.0f, 100.0f, 0.1f, 1.0f, 30.0f, "%");

    // vinyl
    C ("vTable",   "Turntable", { "Studio Deck", "DJ Direct-Drive", "Portable Suitcase", "Gramophone" }, 0);
    C ("vCart",    "Cartridge", { "Moving Coil", "Moving Magnet", "Ceramic" }, 1);
    C ("vRecord",  "Record",    { "LP 33", "Single 45", "Shellac 78", "Lacquer Dubplate" }, 0);
    C ("vStylus",  "Stylus",    { "Conical (15 um)", "Elliptical (7 um)", "Worn (28 um)" }, 0);
    F ("vPos",     "Groove Position", 0.0f, 100.0f, 0.1f, 1.0f, 30.0f, "%");
    F ("vWear",    "Wear",       0.0f, 100.0f, 0.1f, 1.0f, 10.0f, "%");
    F ("vCrackle", "Crackle",    0.0f, 100.0f, 0.1f, 1.0f, 20.0f, "%");
    F ("vPops",    "Pops",       0.0f, 100.0f, 0.1f, 1.0f, 10.0f, "%");
    F ("vDust",    "Dust",       0.0f, 100.0f, 0.1f, 1.0f, 20.0f, "%");
    F ("vScratch", "Scratch",    0.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    F ("vWarp",    "Warp",       0.0f, 100.0f, 0.1f, 1.0f, 10.0f, "%");
    F ("vEcc",     "Off-Centre", 0.0f, 100.0f, 0.1f, 1.0f, 10.0f, "%");
    F ("vRumble",  "Rumble",   -90.0f, -30.0f, 0.1f, 1.0f, -70.0f, "dB");
    C ("vMonoBass","Mono Bass (cut)", { "Off", "80 Hz", "150 Hz", "300 Hz" }, 0);
    F ("vCutLimit","Cutting HF Limiter", 0.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    F ("vCrackleTone","Crackle Tone", 0.0f, 100.0f, 0.1f, 1.0f, 50.0f, "%");

    // VHS
    C ("hFormat",  "Audio Track", { "Linear Mono", "Linear Stereo", "Hi-Fi Stereo" }, 2);
    C ("hSpeed",   "Tape Speed",  { "SP", "LP", "EP" }, 0);
    C ("hStandard","Standard",    { "PAL (50 Hz)", "NTSC (59.94 Hz)" }, 0);
    B ("hDolby",   "Linear Dolby B-type", true);
    F ("hTracking","Tracking Errors", 0.0f, 100.0f, 0.1f, 1.0f, 10.0f, "%");
    F ("hSwitch",  "Head Switching Buzz", 0.0f, 100.0f, 0.1f, 1.0f, 20.0f, "%");
    F ("hBleed",   "Video Whine", -90.0f, -30.0f, 0.1f, 1.0f, -90.0f, "dB");
    F ("hCompTrack","Hi-Fi Compander Error", -10.0f, 10.0f, 0.1f, 1.0f, 0.0f, "dB");
    I ("hGen",     "Dub Generations", 1, 4, 1);

    // motion
    F ("wfAmount", "Wow & Flutter", 0.0f, 300.0f, 0.1f, 1.0f, 100.0f, "%");
    F ("wowDepth", "Wow Depth",     0.0f, 400.0f, 0.1f, 1.0f, 100.0f, "%");
    F ("wowRate",  "Wow Rate",      0.1f, 6.0f, 0.01f, 0.5f, 0.6f, "Hz");
    C ("wowSync",  "Wow Sync", { "Free", "2 bars", "1 bar", "1/2", "1/4", "1/8" }, 0);
    F ("flutDepth","Flutter Depth", 0.0f, 400.0f, 0.1f, 1.0f, 100.0f, "%");
    F ("flutRate", "Flutter Rate",  4.0f, 25.0f, 0.01f, 1.0f, 10.0f, "Hz");
    F ("wfRandom", "Randomness",    0.0f, 100.0f, 0.1f, 1.0f, 30.0f, "%");
    F ("drift",    "Speed Drift",   0.0f, 100.0f, 0.1f, 1.0f, 20.0f, "%");
    F ("snap",     "Sticky Snaps",  0.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    F ("fm",       "Wow > Flutter FM", 0.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    B ("stop",     "Stop", false);
    B ("stopFollow","Stop with Host", false);
    F ("stopTime", "Stop Time",    50.0f, 5000.0f, 1.0f, 0.5f, 800.0f, "ms");
    F ("startTime","Start Time",   50.0f, 5000.0f, 1.0f, 0.5f, 400.0f, "ms");

    // noise and damage
    F ("hiss",     "Hiss / Surface", -48.0f, 24.0f, 0.1f, 1.0f, 0.0f, "dB");
    B ("hissGate", "Noise Only With Signal", false);
    F ("hum",      "Hum",       -90.0f, -30.0f, 0.1f, 1.0f, -90.0f, "dB");
    C ("humFreq",  "Hum Frequency", { "50 Hz", "60 Hz" }, 0);
    F ("motor",    "Motor Noise", -90.0f, -30.0f, 0.1f, 1.0f, -90.0f, "dB");
    F ("age",      "Age",         0.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    F ("dropouts", "Dropouts",    0.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    F ("dropRate", "Dropout Rate", 0.1f, 10.0f, 0.01f, 0.5f, 1.0f, "/s");
    F ("dropWidth","Dropout Stereo", 0.0f, 100.0f, 0.1f, 1.0f, 30.0f, "%");
    F ("degrade",  "Degradation", 0.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    F ("degradeRate","Degradation Rate", 0.2f, 4.0f, 0.01f, 1.0f, 1.0f, "Hz");

    // echo
    C ("echoType", "Echo", { "Off", "Slap", "Feedback", "Ping-Pong" }, 0);
    F ("echoTime", "Echo Time",   10.0f, 2000.0f, 0.1f, 0.4f, 180.0f, "ms");
    C ("echoSync", "Echo Sync", { "Free", "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" }, 0);
    F ("echoFb",   "Echo Feedback", 0.0f, 100.0f, 0.1f, 1.0f, 35.0f, "%");
    F ("echoLevel","Echo Level",    0.0f, 100.0f, 0.1f, 1.0f, 35.0f, "%");
    F ("echoHP",   "Echo Low Cut", 20.0f, 2000.0f, 1.0f, 0.4f, 150.0f, "Hz");
    F ("echoLP",   "Echo High Cut", 1000.0f, 20000.0f, 1.0f, 0.4f, 6000.0f, "Hz");

    return { p.begin(), p.end() };
}

bool RetroDeck2Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == layouts.getMainInputChannelSet();
}

retro2::Params RetroDeck2Processor::buildParams (double bpm) const
{
    retro2::Params q;
    auto pc = [this] (const char* id) { return (double) get (id) * 0.01; };
    auto ch = [this] (const char* id) { return (int) std::lround (get (id)); };
    auto on = [this] (const char* id) { return get (id) > 0.5f; };

    q.media = ch ("media");
    q.inGainDb = get ("input"); q.outGainDb = get ("output"); q.autoGain = on ("autoGain"); q.mix = pc ("mix");
    q.drive = pc ("drive"); q.width = pc ("width"); q.mono = on ("mono");
    q.lowCut = get ("lowCut"); q.highCut = get ("highCut"); q.tone = pc ("tone");
    q.secResp = on ("secResp"); q.secSat = on ("secSat"); q.secComp = on ("secComp");
    static const double rangeMul[4] = { 1.0, 2.0, 4.0, 8.0 };
    q.respAmt = pc ("respAmt") * rangeMul[juce::jlimit (0, 3, ch ("respRange"))];
    q.satAmt  = pc ("satAmt")  * rangeMul[juce::jlimit (0, 3, ch ("satRange"))];
    q.compAmt = pc ("compAmt") * rangeMul[juce::jlimit (0, 3, ch ("compRange"))];
    q.hfTrimOct = get ("hfTrim"); q.hfSlope = ch ("hfSlope"); q.bumpMul = pc ("bumpFreq"); q.lowTrimOct = get ("lowTrim");
    q.mixMode = ch ("mixMode");

    q.cDeck = ch ("cDeck"); q.cTape = ch ("cTape"); q.cSpeed = ch ("cSpeed"); q.cBias = pc ("cBias");
    q.cSatModel = ch ("cSatModel"); q.cHfComp = pc ("cHfComp"); q.cAlc = ch ("cAlc"); q.cAlcAmt = pc ("cAlcAmt");
    q.cNr = ch ("cNr"); q.cNrMode = ch ("cNrMode"); q.cNrTrack = get ("cNrTrack"); q.cGen = ch ("cGen");
    q.cAzimuth = pc ("cAzimuth"); q.cXtalkOffset = get ("cXtalk"); q.cPrintDb = get ("cPrint");
    q.cPrintTime = get ("cPrintTime"); q.cBumpDb = get ("cBump"); q.cAsperity = pc ("cAsperity");

    q.vTable = ch ("vTable"); q.vCart = ch ("vCart"); q.vRecord = ch ("vRecord"); q.vStylus = ch ("vStylus"); q.vPos = pc ("vPos");
    q.vWear = pc ("vWear"); q.vCrackle = pc ("vCrackle"); q.vPops = pc ("vPops"); q.vDust = pc ("vDust");
    q.vScratch = pc ("vScratch"); q.vWarp = pc ("vWarp"); q.vEcc = pc ("vEcc"); q.vRumbleDb = get ("vRumble");
    q.vMonoBass = ch ("vMonoBass"); q.vCutLimit = pc ("vCutLimit"); q.vCrackleTone = pc ("vCrackleTone");

    q.hFormat = ch ("hFormat"); q.hSpeed = ch ("hSpeed"); q.hStandard = ch ("hStandard"); q.hDolby = on ("hDolby");
    q.hTracking = pc ("hTracking"); q.hSwitch = pc ("hSwitch"); q.hBleedDb = get ("hBleed");
    q.hCompTrack = get ("hCompTrack"); q.hGen = ch ("hGen");

    q.wfAmount = pc ("wfAmount"); q.wowDepth = pc ("wowDepth"); q.wowRate = get ("wowRate");
    q.flutDepth = pc ("flutDepth"); q.flutRate = get ("flutRate"); q.wfRandom = pc ("wfRandom");
    q.drift = pc ("drift"); q.snap = pc ("snap"); q.fm = pc ("fm");
    static const double syncBeats[6] = { 0.0, 8.0, 4.0, 2.0, 1.0, 0.5 };
    const int ws = juce::jlimit (0, 5, ch ("wowSync"));
    q.wowSyncHz = ws == 0 ? 0.0 : bpm / 60.0 / syncBeats[ws];
    q.stopTime = get ("stopTime") * 0.001; q.startTime = get ("startTime") * 0.001;

    q.hissDb = get ("hiss"); q.hissGate = on ("hissGate"); q.humDb = get ("hum");
    q.humHz = ch ("humFreq") == 1 ? 60.0 : 50.0; q.motorDb = get ("motor"); q.age = pc ("age");
    q.dropouts = pc ("dropouts"); q.dropRate = get ("dropRate"); q.dropWidth = pc ("dropWidth");
    q.degrade = pc ("degrade"); q.degradeRate = get ("degradeRate");

    q.echoType = ch ("echoType");
    static const double echoBeats[10] = { 0.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };
    const int es = juce::jlimit (0, 9, ch ("echoSync"));
    q.echoMs = es == 0 ? (double) get ("echoTime") : std::min (2000.0, echoBeats[es] * 60000.0 / bpm);
    q.echoFb = pc ("echoFb"); q.echoLevel = pc ("echoLevel"); q.echoHP = get ("echoHP"); q.echoLP = get ("echoLP");

    // Morph mode: one signal path only. The Mix control scales the intensity of every stage instead of
    // blending two signals, so intermediate values can never cause comb filtering or phasing.
    if (q.mixMode == 0)
    {
        const double m = juce::jlimit (0.0, 1.0, q.mix);
        q.mix = 1.0;
        const double mDb = 20.0 * std::log10 (std::max (m, 1e-4));
        auto dbScale = [mDb] (double db, double offDb) { return db <= offDb + 0.1 ? offDb : std::max (offDb, db + mDb); };
        auto logMorph = [m] (double flat, double v) { return std::exp (std::log (flat) + m * (std::log (v) - std::log (flat))); };

        q.respAmt *= m; q.satAmt *= m; q.compAmt *= m;
        q.cAlcAmt *= m; q.cBias *= m; q.cAzimuth *= m; q.cAsperity *= m; q.cNrTrack *= m;
        if (m < 0.05) q.cNr = 0;
        q.cGen = 1 + (int) std::lround ((q.cGen - 1) * m);
        q.hGen = 1 + (int) std::lround ((q.hGen - 1) * m);
        q.cXtalkOffset += mDb; q.cPrintDb = dbScale (q.cPrintDb, -80.0);
        q.hissDb = q.hissDb <= -47.9 ? -48.0 : std::max (-48.0, q.hissDb + mDb);
        q.humDb = dbScale (q.humDb, -90.0); q.motorDb = dbScale (q.motorDb, -90.0);
        q.vRumbleDb = dbScale (q.vRumbleDb, -90.0); q.hBleedDb = dbScale (q.hBleedDb, -90.0);
        q.vCrackle *= m; q.vPops *= m; q.vDust *= m; q.vScratch *= m; q.vWarp *= m; q.vEcc *= m;
        q.vWear *= m; q.vPos *= m; q.vCutLimit *= m;
        q.hTracking *= m; q.hSwitch *= m; q.hCompTrack *= m;
        q.wfAmount *= m; q.drift *= m; q.snap *= m;
        q.age *= m; q.dropouts *= m; q.degrade *= m; q.echoLevel *= m;
        q.width = 1.0 + (q.width - 1.0) * m; q.mono = q.mono && m >= 0.5; q.tone *= m;
        q.lowCut = logMorph (10.0, q.lowCut); q.highCut = logMorph (22000.0, q.highCut);
    }
    return q;
}

void RetroDeck2Processor::prepareToPlay (double sampleRate, int)
{
    setLatencySamples (retro2::Params::kLatency);   // transport offset + stylus look-ahead, compensated by the host
    engine.prepare (sampleRate);
    engine.setParams (buildParams (120.0));
    engine.reset();
}

void RetroDeck2Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    double bpm = 120.0;
    bool playing = true;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) if (*b > 20.0 && *b < 400.0) bpm = *b;
            playing = pos->getIsPlaying();
        }

    engine.setParams (buildParams (bpm));
    const bool stopReq = get ("stop") > 0.5f || (get ("stopFollow") > 0.5f && ! playing);

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float inPk = 0.0f, outPk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float a = l[i], b = r ? r[i] : a;
        inPk = juce::jmax (inPk, std::abs (a), std::abs (b));
        float ol, orr;
        engine.process (a, b, ol, orr, stopReq);
        l[i] = ol;
        if (r) r[i] = orr;
        outPk = juce::jmax (outPk, std::abs (ol), std::abs (orr));
    }
    const float decay = std::exp (-(float) n / (0.25f * (float) getSampleRate()));
    inMeter = juce::jmax (inPk, inMeter * decay);
    outMeter = juce::jmax (outPk, outMeter * decay);
    inputDb.store (juce::Decibels::gainToDecibels (inMeter, -90.0f));
    outputDb.store (juce::Decibels::gainToDecibels (outMeter, -90.0f));
    speedShown.store (engine.currentSpeed());
}

int RetroDeck2Processor::getNumPrograms() { return (int) presets().size(); }

const juce::String RetroDeck2Processor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String (presets()[(size_t) index].name) : juce::String();
}

void RetroDeck2Processor::setPlain (const juce::String& id, float v)
{
    if (auto* prm = apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void RetroDeck2Processor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& kv : presets()[(size_t) index].values)
        setPlain (kv.first, kv.second);
}

void RetroDeck2Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RetroDeck2Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* RetroDeck2Processor::createEditor() { return new RetroDeck2Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RetroDeck2Processor(); }
