#include "VinylProcessor.h"
#include "VinylEditor.h"

namespace
{
using PV = std::pair<const char*, float>;
struct Preset { const char* name; std::vector<PV> values; };
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list =
    {
        { "Init",                               {} },
        { "Fresh Pressing (bright)",            { { "brilliance", 60 }, { "crackle", 5 }, { "dust", 10 }, { "pops", 0 }, { "hiss", 15 } } },
        { "Brilliance Max (x4)",                { { "brilliance", 100 }, { "brilRange", 2 }, { "sheen", 40 } } },
        { "Extreme Sizzle (x8)",                { { "brilliance", 100 }, { "brilRange", 3 }, { "sheen", 100 }, { "sheenRange", 2 } } },
        { "Air Resonance (low load)",           { { "loadPf", 100 }, { "brilliance", 70 } } },
        { "Classic MM 70s",                     { { "cart", 0 }, { "stylus", 0 }, { "brilliance", 30 }, { "tipRes", 60 } } },
        { "Bright MM",                          { { "cart", 1 }, { "brilliance", 50 }, { "tipRes", 70 } } },
        { "DJ Turntable",                       { { "cart", 3 }, { "stylus", 0 }, { "brilliance", 40 }, { "position", 50 }, { "crackle", 30 } } },
        { "Audiophile Fine Line",               { { "cart", 4 }, { "stylus", 2 }, { "brilliance", 25 }, { "crackle", 0 }, { "dust", 5 }, { "hiss", 10 }, { "cutLimit", 10 } } },
        { "Maxi 45 Open Top",                   { { "format", 1 }, { "brilliance", 45 }, { "position", 10 }, { "grooveDb", 3 } } },
        { "Hot Cut (sheen)",                    { { "grooveDb", 9 }, { "cutLimit", 70 }, { "cutRange", 1 }, { "sheen", 30 } } },
        { "End of Side (inner groove)",         { { "position", 100 }, { "stylus", 0 } } },
        { "Worn 45 Single",                     { { "format", 2 }, { "stylus", 3 }, { "wear", 50 }, { "crackle", 50 }, { "dust", 50 }, { "pops", 30 }, { "position", 70 } } },
        { "Flea Market Find",                   { { "stylus", 3 }, { "wear", 40 }, { "crackle", 70 }, { "dust", 60 }, { "pops", 50 }, { "hiss", 50 }, { "rumble", 40 }, { "warp", 40 } } },
        { "Shellac 78",                         { { "format", 3 }, { "cart", 5 }, { "stylus", 0 }, { "hiss", 70 }, { "crackle", 60 }, { "wear", 30 }, { "separation", 6 } } },
        { "Dark Loading (600 pF)",              { { "loadPf", 600 }, { "brilliance", 0 } } },
        { "Warped Record",                      { { "warp", 100 }, { "warpRange", 2 }, { "flutter", 50 } } },
        { "Crackle Bed",                        { { "brilliance", 20 }, { "crackle", 70 }, { "dust", 60 }, { "pops", 25 }, { "hiss", 30 }, { "surfRange", 1 } } },
    };
    return list;
}
const juce::StringArray kRanges { "x1", "x2", "x4", "x8" };
}

VinylSimProcessor::VinylSimProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VINYLSIM", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout VinylSimProcessor::createParameterLayout()
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

    juce::StringArray carts, styli, formats;
    for (auto& c : vsim::kCarts) carts.add (c.name);
    for (auto& s : vsim::kStyli) styli.add (s.name);
    for (auto& f : vsim::kFormats) formats.add (f.name);

    // brilliance & playback chain
    F ("brilliance", "Brilliance", 0.0f, 100.0f, 0.1f, 1.0f, 35.0f, "%");
    C ("brilRange",  "Brilliance Range", kRanges, 0);
    C ("cart",       "Cartridge", carts, 0);
    F ("loadPf",     "Load Capacitance", 50.0f, 1000.0f, 1.0f, 0.5f, 200.0f, "pF");
    F ("loadKohm",   "Load Resistance", 10.0f, 100.0f, 0.1f, 1.0f, 47.0f, "kOhm");
    F ("tipRes",     "Tip Resonance", 0.0f, 100.0f, 0.1f, 1.0f, 50.0f, "%");
    F ("sheen",      "Sheen (tracing H2)", 0.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    C ("sheenRange", "Sheen Range", kRanges, 0);
    C ("stylus",     "Stylus", styli, 1);
    C ("format",     "Record", formats, 0);
    F ("position",   "Groove Position", 0.0f, 100.0f, 0.1f, 1.0f, 30.0f, "%");
    F ("grooveDb",   "Cutting Level", -12.0f, 18.0f, 0.1f, 1.0f, 0.0f, "dB");
    F ("wear",       "Groove Wear", 0.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
    // cutting / playback
    F ("cutLimit",   "Lathe HF Limiter", 0.0f, 100.0f, 0.1f, 1.0f, 30.0f, "%");
    C ("cutRange",   "Limiter Range", kRanges, 0);
    F ("elliptical", "Elliptical EQ (0 = off)", 0.0f, 300.0f, 1.0f, 1.0f, 150.0f, "Hz");
    F ("separation", "Channel Separation", 6.0f, 40.0f, 0.1f, 1.0f, 25.0f, "dB");
    F ("lowCut",     "Subsonic Filter", 5.0f, 40.0f, 0.1f, 1.0f, 20.0f, "Hz");
    F ("warp",       "Warp", 0.0f, 100.0f, 0.1f, 1.0f, 15.0f, "%");
    C ("warpRange",  "Warp Range", kRanges, 0);
    F ("flutter",    "Flutter", 0.0f, 100.0f, 0.1f, 1.0f, 20.0f, "%");
    // surface
    F ("crackle",    "Crackle", 0.0f, 100.0f, 0.1f, 1.0f, 25.0f, "%");
    F ("dust",       "Dust", 0.0f, 100.0f, 0.1f, 1.0f, 25.0f, "%");
    F ("pops",       "Pops", 0.0f, 100.0f, 0.1f, 1.0f, 10.0f, "%");
    F ("hiss",       "Hiss", 0.0f, 100.0f, 0.1f, 1.0f, 25.0f, "%");
    F ("rumble",     "Rumble", 0.0f, 100.0f, 0.1f, 1.0f, 20.0f, "%");
    C ("surfRange",  "Surface Range", kRanges, 0);
    p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "seed", 1 }, "Surface Seed", 0, 999, 1));
    // output
    F ("inGain",     "Input Gain", -24.0f, 12.0f, 0.1f, 1.0f, 0.0f, "dB");
    F ("outGain",    "Output Gain", -24.0f, 12.0f, 0.1f, 1.0f, 0.0f, "dB");
    F ("mix",        "Dry/Wet", 0.0f, 100.0f, 0.1f, 1.0f, 100.0f, "%");
    B ("autoGain",   "Auto Gain", true);
    B ("bypass",     "Master Off", false);
    return { p.begin(), p.end() };
}

bool VinylSimProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == layouts.getMainInputChannelSet();
}

vsim::Params VinylSimProcessor::buildParams() const
{
    auto ci = [this] (const char* id) { return (int) std::lround (get (id)); };
    vsim::Params q;
    q.brilliance = get ("brilliance") * 0.01; q.brilRange = ci ("brilRange");
    q.cart = ci ("cart"); q.loadPf = get ("loadPf"); q.loadKohm = get ("loadKohm");
    q.tipRes = get ("tipRes") * 0.01; q.sheen = get ("sheen") * 0.01; q.sheenRange = ci ("sheenRange");
    q.stylus = ci ("stylus"); q.format = ci ("format");
    q.position = get ("position") * 0.01; q.grooveDb = get ("grooveDb"); q.wear = get ("wear") * 0.01;
    q.cutLimit = get ("cutLimit") * 0.01; q.cutRange = ci ("cutRange");
    q.ellipticalHz = get ("elliptical"); q.separationDb = get ("separation"); q.lowCutHz = get ("lowCut");
    q.warp = get ("warp") * 0.01; q.warpRange = ci ("warpRange"); q.flutter = get ("flutter") * 0.01;
    q.crackle = get ("crackle") * 0.01; q.dust = get ("dust") * 0.01; q.pops = get ("pops") * 0.01;
    q.hiss = get ("hiss") * 0.01; q.rumble = get ("rumble") * 0.01; q.surfRange = ci ("surfRange");
    q.seed = (unsigned) ci ("seed");
    q.inGainDb = get ("inGain"); q.outGainDb = get ("outGain"); q.mix = get ("mix") * 0.01;
    q.autoGain = get ("autoGain") > 0.5f; q.bypass = get ("bypass") > 0.5f;
    return q;
}

void VinylSimProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.setParams (buildParams());
    engine.prepare (sampleRate);
    setLatencySamples (engine.latencySamples());
    latencyMs.store (1000.0 * engine.latencySamples() / sampleRate);
}

void VinylSimProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    engine.setParams (buildParams());
    cartHz.store (engine.cartResonanceHz()); grooveSpeed.store (engine.grooveSpeedMs());
    sheenMult.store (engine.sheenMultiplier()); brilDb.store (engine.brilliancePeakDb());
    if (++curveCounter >= 8)
    {
        curveCounter = 0;
        for (int i = 0; i < kCurvePoints; ++i) curve[(size_t) i].store ((float) engine.magnitudeDb (curveFreq (i)));
    }

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        double a = l[i], b = r ? r[i] : l[i];
        engine.process (a, b);
        l[i] = (float) (r ? a : 0.5 * (a + b));
        if (r) r[i] = (float) b;
    }
}

int VinylSimProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String VinylSimProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (presets()[(size_t) i].name) : juce::String();
}
void VinylSimProcessor::setCurrentProgram (int index)
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

void VinylSimProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}
void VinylSimProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VinylSimProcessor::createEditor() { return new VinylSimEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VinylSimProcessor(); }
