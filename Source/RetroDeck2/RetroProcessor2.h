#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RetroEngine2.h"
#include <atomic>

/**
    RetroDeck - original cassette, vinyl and VHS emulation.
    Written from scratch; not derived from any third-party product.
*/
class RetroDeck2Processor : public juce::AudioProcessor
{
public:
    RetroDeck2Processor();
    ~RetroDeck2Processor() override = default;

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
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float getInputLevelDb() const { return inputDb.load(); }
    float getOutputLevelDb() const { return outputDb.load(); }
    double getTransportSpeed() const { return speedShown.load(); }

private:
    retro2::Params buildParams (double bpm) const;
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void setPlain (const juce::String& id, float v);

    retro2::Engine engine;
    int currentProgram = 0;
    std::atomic<float> inputDb { -90.0f }, outputDb { -90.0f };
    std::atomic<double> speedShown { 1.0 };
    float inMeter = 0.0f, outMeter = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetroDeck2Processor)
};
