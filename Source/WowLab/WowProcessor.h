#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "WowEngine.h"

/**
    WowLab - original wow & flutter / tape modulation processor.
    Written from scratch; not derived from any third-party product.
*/
class WowLabProcessor : public juce::AudioProcessor
{
public:
    WowLabProcessor();
    ~WowLabProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.6; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    double getCurrentBpm() const { return lastBpm.load(); }

private:
    wow::Params buildParams (double hostBpm) const;
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    wow::Engine engine;
    int currentProgram = 0, reportedLatency = -1;
    std::atomic<double> lastBpm { 120.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WowLabProcessor)
};
