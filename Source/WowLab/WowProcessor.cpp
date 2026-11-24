#include "WowProcessor.h"
#include "WowEditor.h"

namespace
{
const juce::StringArray kNotes { "1", "1/2 D", "1 T", "1/2", "1/4 D", "1/2 T", "1/4", "1/8 D", "1/4 T", "1/8",
                                 "1/16 D", "1/8 T", "1/16", "1/32 D", "1/16 T", "1/32" };
const double kNoteBeats[16] = { 4.0, 3.0, 8.0 / 3.0, 2.0, 1.5, 4.0 / 3.0, 1.0, 0.75, 2.0 / 3.0, 0.5,
                                0.375, 1.0 / 3.0, 0.25, 0.1875, 1.0 / 6.0, 0.125 };
const juce::StringArray kMults { "1x", "2x", "4x", "8x", "16x", "32x", "64x", "128x", "256x" };

using PV = std::pair<const char*, float>;
struct Preset { const char* name; std::vector<PV> values; };
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list =
    {
        { "Init",                          {} },
        { "Subtle Tape 15 IPS",            { { "amount", 10 } } },
        { "7.5 IPS Warmth",                { { "model", 1 }, { "amount", 20 }, { "saturation", 120 } } },
        { "Cassette Warble",               { { "model", 2 }, { "amount", 35 }, { "mult", 1 }, { "speed", 0.8f } } },
        { "Broken Walkman",                { { "model", 2 }, { "amount", 60 }, { "mult", 2 }, { "flutter", 70 }, { "rndMode", 6 }, { "rndAmount", 120 } } },
        { "Seasick Pads",                  { { "amount", 70 }, { "mult", 3 }, { "speed", 0.25f }, { "flutter", 10 } } },
        { "Vinyl Warp (33 rpm)",           { { "speed", 0.555f }, { "amount", 50 }, { "mult", 2 }, { "flutter", 0 }, { "shape", 30 } } },
        { "Vibrato 1/8 (synced)",          { { "rateMode", 1 }, { "note", 9 }, { "amount", 40 }, { "mult", 2 }, { "flutter", 0 }, { "analogOn", 0 } } },
        { "Chorus (parallel)",             { { "mixMode", 1 }, { "wetDry", 50 }, { "amount", 60 }, { "mult", 3 }, { "speed", 0.7f }, { "stereoPhase", 90 }, { "wetDelay", 12 } } },
        { "Tape Flanger (parallel)",       { { "mixMode", 1 }, { "wetDry", 50 }, { "amount", 40 }, { "mult", 4 }, { "speed", 0.15f }, { "wetDelay", 2 }, { "flutter", 0 } } },
        { "Stereo Widener",                { { "amount", 30 }, { "mult", 2 }, { "stereoPhase", 180 }, { "analogOn", 0 } } },
        { "Random Drift",                  { { "amount", 30 }, { "mult", 2 }, { "rndMode", 1 }, { "rndSpeed", 0.5f }, { "rndAmount", 150 } } },
        { "Lo-Fi Keys",                    { { "model", 2 }, { "amount", 45 }, { "mult", 2 }, { "color", 150 }, { "saturation", 140 }, { "noise", 0 } } },
        { "Square Wobble (synced 1/4)",    { { "rateMode", 1 }, { "note", 6 }, { "shape", -80 }, { "amount", 50 }, { "mult", 3 }, { "flutter", 0 } } },
        { "Tape Echo Throw (parallel)",    { { "mixMode", 1 }, { "wetDry", 40 }, { "wetDelay", 320 }, { "amount", 25 }, { "model", 1 } } },
        { "HQ Mastering Wobble",           { { "hq", 1 }, { "amount", 8 }, { "analogOn", 0 } } },
        { "Extreme Pitch Chaos",           { { "amount", 80 }, { "mult", 6 }, { "rndMode", 6 }, { "rndSpeed", 3 }, { "rndSmooth", 40 } } },
    };
    return list;
}
}

WowLabProcessor::WowLabProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WOWLAB", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout WowLabProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto F = [&p] (const char* id, const char* name, float lo, float hi, float step, float skew, float def, const char* unit)
    { p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, Range (lo, hi, step, skew), def,
                                                                juce::AudioParameterFloatAttributes().withLabel (unit))); };
    auto C = [&p] (const char* id, const char* name, const juce::StringArray& items, int def)
    { p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    auto B = [&p] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def)); };

    // rate
    C ("rateMode", "Rate Mode", { "Hz", "BPM" }, 0);
    F ("speed",    "Speed", 0.1f, 20.0f, 0.01f, 0.35f, 0.55f, "Hz");
    C ("note",     "Note", kNotes, 6);
    F ("bpm",      "BPM", 30.0f, 300.0f, 0.1f, 1.0f, 120.0f, "");
    C ("tempoSync","Tempo Sync", { "Manual", "Synced" }, 1);
    // shape
    F ("amount",   "Amount", 0.0f, 100.0f, 0.1f, 1.0f, 15.0f, "%");
    C ("mult",     "Amount Multiplier", kMults, 0);
    F ("shape",    "Shape", -100.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    F ("flutter",  "Flutter Amount", 0.0f, 100.0f, 0.1f, 1.0f, 50.0f, "%");
    F ("wetDry",   "Wet/Dry", 0.0f, 100.0f, 0.1f, 1.0f, 100.0f, "%");
    C ("mixMode",  "Mix Mode", { "Morph (no phasing)", "Parallel (chorus / flange)" }, 0);
    // analog
    B ("analogOn", "Analog", true);
    F ("color",    "Analog Color", 0.0f, 200.0f, 0.1f, 1.0f, 100.0f, "%");
    F ("saturation","Analog Saturation", 0.0f, 200.0f, 0.1f, 1.0f, 100.0f, "%");
    C ("model",    "Analog Mode", { "15 IPS", "7.5 IPS", "Cassette" }, 0);
    F ("noise",    "Analog Noise Gain", -96.0f, 24.0f, 0.1f, 1.0f, -6.0f, "dB");
    // trim & misc
    F ("inGain",   "Input Gain", -24.0f, 12.0f, 0.1f, 1.0f, 0.0f, "dB");
    F ("outGain",  "Output Gain", -24.0f, 12.0f, 0.1f, 1.0f, 0.0f, "dB");
    F ("stereoPhase","Stereo Phase", 0.0f, 360.0f, 0.1f, 1.0f, 0.0f, "deg");
    F ("phaseOffset","Phase Offset", 0.0f, 360.0f, 0.1f, 1.0f, 0.0f, "deg");
    F ("wetDelay", "Wet Delay", 0.0f, 500.0f, 0.1f, 0.5f, 0.0f, "ms");
    // random
    C ("rndMode",  "Random Mode", { "Off", "Speed", "Amount", "Shape", "Stereo Phase", "Phase Offset", "Amount + Speed", "Wet Delay" }, 0);
    F ("rndSpeed", "Random Speed", 0.1f, 10.0f, 0.01f, 0.5f, 1.0f, "Hz");
    F ("rndAmount","Random Amount", 0.0f, 200.0f, 0.1f, 1.0f, 100.0f, "%");
    F ("rndSmooth","Random Smoothing", 25.0f, 200.0f, 0.1f, 1.0f, 100.0f, "%");
    p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "rndSeed", 1 }, "Random Seed", 0, 9999, 0));
    // quality / bypass
    B ("hq",       "HQ Mode", false);
    B ("bypass",   "Master Off", false);
    return { p.begin(), p.end() };
}

bool WowLabProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == layouts.getMainInputChannelSet();
}

wow::Params WowLabProcessor::buildParams (double hostBpm) const
{
    wow::Params q;
    const bool bpmMode = get ("rateMode") > 0.5f;
    const double bpm = get ("tempoSync") > 0.5f ? hostBpm : (double) get ("bpm");
    const double beats = kNoteBeats[juce::jlimit (0, 15, (int) std::lround (get ("note")))];
    q.synced = bpmMode;
    q.cycleBeats = beats;
    q.speedHz = bpmMode ? bpm / 60.0 / beats : (double) get ("speed");
    q.amount = get ("amount") * 0.01;
    q.amountMult = std::pow (2.0, (double) juce::jlimit (0, 8, (int) std::lround (get ("mult"))));
    q.shape = get ("shape") * 0.01;
    q.flutterBal = get ("flutter") * 0.01;
    q.mix = get ("wetDry") * 0.01;
    q.mixMode = (int) std::lround (get ("mixMode"));
    q.analogOn = get ("analogOn") > 0.5f;
    q.color = get ("color") * 0.01; q.saturation = get ("saturation") * 0.01;
    q.model = (int) std::lround (get ("model")); q.noiseGainDb = get ("noise");
    q.inGainDb = get ("inGain"); q.outGainDb = get ("outGain");
    q.stereoPhaseDeg = get ("stereoPhase"); q.phaseOffsetDeg = get ("phaseOffset"); q.wetDelayMs = get ("wetDelay");
    q.rndMode = (int) std::lround (get ("rndMode")); q.rndSpeed = get ("rndSpeed");
    q.rndAmount = get ("rndAmount") * 0.01; q.rndSmooth = get ("rndSmooth") * 0.01;
    q.rndSeed = (int) std::lround (get ("rndSeed"));
    q.hq = get ("hq") > 0.5f; q.bypass = get ("bypass") > 0.5f;
    return q;
}

void WowLabProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setParams (buildParams (120.0));
    engine.reset();
    reportedLatency = engine.latencySamples();
    setLatencySamples (reportedLatency);
}

void WowLabProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    double bpm = get ("bpm"), ppq = 0.0; bool playing = false; long long samplePos = -1;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) if (*b > 20.0 && *b < 400.0 && get ("tempoSync") > 0.5f) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            if (auto t = pos->getTimeInSamples()) samplePos = *t;
            playing = pos->getIsPlaying();
        }
    lastBpm.store (bpm);

    engine.setParams (buildParams (bpm));
    if (engine.latencySamples() != reportedLatency)
    {
        reportedLatency = engine.latencySamples();
        setLatencySamples (reportedLatency);
    }
    engine.setTransport (playing, ppq, bpm, samplePos);

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        double a = l[i], b = r ? r[i] : l[i];
        engine.process (a, b);
        l[i] = (float) a;
        if (r) r[i] = (float) b;
    }
}

int WowLabProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String WowLabProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (presets()[(size_t) i].name) : juce::String();
}
void WowLabProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& kv : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (kv.first))
            prm->setValueNotifyingHost (prm->convertTo0to1 (kv.second));
}

void WowLabProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}
void WowLabProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* WowLabProcessor::createEditor() { return new WowLabEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WowLabProcessor(); }
