#include "GradeProcessor.h"
#include "GradeEditor.h"

namespace
{
using PV = std::pair<const char*, float>;
struct Preset { const char* name; std::vector<PV> values; };
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list =
    {
        { "Init (Standard / Long)",      {} },
        { "Hi-Fi Grade",                 { { "grade", 0 } } },
        { "Standard / Long Grade",       { { "grade", 1 } } },
        { "Lo-Fi 1 Grade",               { { "grade", 2 } } },
        { "Lo-Fi 2 Grade",               { { "grade", 3 } } },
        { "Crusty Drums (Lo-Fi 1 hot)",  { { "grade", 2 }, { "input", 6 }, { "decim", 2 } } },
        { "Dusty Loop (Standard, raw)",  { { "grade", 1 }, { "decim", 2 }, { "outCut", 9000 } } },
        { "Telephone Grit (Lo-Fi 2)",    { { "grade", 3 }, { "decim", 2 }, { "outFilter", 1 } } },
        { "Clean Lo-Fi (no codec)",      { { "grade", 2 }, { "codec", 0 }, { "decim", 0 }, { "outFilter", 1 } } },
        { "Metallic Images (filter off)",{ { "grade", 2 }, { "outFilter", 2 } } },
        { "Broken Codec (3-bit)",        { { "grade", 1 }, { "codec", 2 } } },
        { "12-bit Feel",                 { { "grade", 0 }, { "bits", 12 } } },
        { "Parallel Grit 50%",           { { "grade", 2 }, { "mix", 50 }, { "input", 4 } } },
        { "Later SP Lo-Fi 11 kHz",       { { "grade", 6 }, { "codec", 0 } } },
        { "Later SP Long 22 kHz",        { { "grade", 5 }, { "codec", 0 } } },
    };
    return list;
}
}

SampleGradeProcessor::SampleGradeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SAMPLEGRADE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SampleGradeProcessor::createParameterLayout()
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

    juce::StringArray grades;
    for (int i = 0; i < grade::kNumGrades; ++i) grades.add (grade::kGrades[i].name);
    C ("grade",     "Sampling Grade", grades, 1);
    C ("codec",     "Codec", { "Linear 16-bit", "ADPCM 4-bit", "ADPCM 3-bit" }, 1);
    C ("decim",     "Decimation Filter", { "Clean (steep)", "Simple (2-pole)", "Raw (aliasing)" }, 1);
    C ("outFilter", "Output Filter", { "Fixed (hardware style)", "Tracks the grade", "Off" }, 0);
    F ("outCut",    "Output Filter Cutoff", 3000.0f, 20000.0f, 1.0f, 0.4f, 13000.0f, "Hz");
    F ("bits",      "Resolution", 4.0f, 16.0f, 1.0f, 1.0f, 16.0f, "bits");
    F ("input",     "Input Level", -24.0f, 24.0f, 0.1f, 1.0f, 0.0f, "dB");
    B ("autoGain",  "Auto Gain", true);
    F ("noise",     "Noise Floor", -120.0f, -50.0f, 0.1f, 1.0f, -86.0f, "dB");
    B ("mono",      "Mono Sampling", false);
    F ("output",    "Output", -24.0f, 12.0f, 0.1f, 1.0f, 0.0f, "dB");
    F ("mix",       "Mix", 0.0f, 100.0f, 0.1f, 1.0f, 100.0f, "%");
    return { p.begin(), p.end() };
}

bool SampleGradeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == layouts.getMainInputChannelSet();
}

grade::Params SampleGradeProcessor::buildParams() const
{
    grade::Params q;
    q.grade = (int) std::lround (get ("grade"));
    q.codec = (int) std::lround (get ("codec"));
    q.decim = (int) std::lround (get ("decim"));
    q.outFilter = (int) std::lround (get ("outFilter"));
    q.outCutHz = get ("outCut");
    q.bitsOverride = get ("bits");
    q.inGainDb = get ("input");
    q.noiseDb = get ("noise");
    q.mono = get ("mono") > 0.5f;
    return q;
}

void SampleGradeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int block = juce::jmax (16, samplesPerBlock);
    oversampling.initProcessing ((size_t) block);
    oversampling.reset();
    const int latency = (int) std::ceil (oversampling.getLatencyInSamples());
    setLatencySamples (latency);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) block, 2 };
    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) latency);
    dryBuffer.setSize (2, block);
    engine.prepare (sampleRate * 4.0);
    engine.setParams (buildParams());
    engine.reset();
}

void SampleGradeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    engine.setParams (buildParams());

    if (dryBuffer.getNumSamples() < n) dryBuffer.setSize (2, n, false, false, true);
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (c, buffer.getSample (c, i));
            dryBuffer.setSample (c, i, dryDelay.popSample (c));
        }

    juce::dsp::AudioBlock<float> block (buffer);
    auto sub = block.getSubsetChannelBlock (0, (size_t) numCh).getSubBlock (0, (size_t) n);
    auto up = oversampling.processSamplesUp (sub);
    float* L = up.getChannelPointer (0);
    float* R = numCh > 1 ? up.getChannelPointer (1) : nullptr;
    for (size_t i = 0; i < up.getNumSamples(); ++i)
    {
        double l = L[i], r = R ? R[i] : L[i];
        engine.process (l, r);
        L[i] = (float) l;
        if (R) R[i] = (float) r;
    }
    oversampling.processSamplesDown (sub);

    const float comp = (get ("autoGain") > 0.5f ? juce::Decibels::decibelsToGain (-get ("input")) : 1.0f)
                       * juce::Decibels::decibelsToGain (get ("output"));
    const float mix = get ("mix") * 0.01f;
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < n; ++i)
            buffer.setSample (c, i, dryBuffer.getSample (c, i) * (1.0f - mix) + buffer.getSample (c, i) * comp * mix);
}

int SampleGradeProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String SampleGradeProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (presets()[(size_t) i].name) : juce::String();
}
void SampleGradeProcessor::setCurrentProgram (int index)
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

void SampleGradeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}
void SampleGradeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SampleGradeProcessor::createEditor() { return new SampleGradeEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SampleGradeProcessor(); }
