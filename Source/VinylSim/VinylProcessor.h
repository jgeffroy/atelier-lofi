#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "VinylEngine.h"

/**
    VinylSim - vinyl record simulator focused on the vinyl top end (cartridge resonance, stylus tip
    resonance, tracing "sheen"). Written from scratch; not derived from any third-party product.
*/
class VinylSimProcessor : public juce::AudioProcessor
{
public:
    VinylSimProcessor();
    ~VinylSimProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // read by the editor
    static constexpr int kCurvePoints = 96;
    std::array<std::atomic<float>, kCurvePoints> curve {};
    static double curveFreq (int i) { return 20.0 * std::pow (1000.0, (double) i / (kCurvePoints - 1)); }   // 20 Hz .. 20 kHz
    std::atomic<double> cartHz { 16000.0 }, grooveSpeed { 0.4 }, sheenMult { 1.0 }, brilDb { 0.0 }, latencyMs { 0.0 };

private:
    vsim::Params buildParams() const;
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    vsim::Engine engine;
    int currentProgram = 0, curveCounter = 0;
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VinylSimProcessor)
};
