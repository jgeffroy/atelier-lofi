#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "GradeEngine.h"

/**
    SampleGrade - original emulation of the sampling grades of a late-90s phrase sampler.
    Written from scratch; not derived from any third-party product.
*/
class SampleGradeProcessor : public juce::AudioProcessor
{
public:
    SampleGradeProcessor();
    ~SampleGradeProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    grade::Params buildParams() const;
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    grade::Engine engine;
    juce::dsp::Oversampling<float> oversampling { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 4096 };
    juce::AudioBuffer<float> dryBuffer;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleGradeProcessor)
};
