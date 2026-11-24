#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GradeProcessor.h"
#include "../UserPresets.h"

class SampleGradeEditor : public juce::AudioProcessorEditor
{
public:
    explicit SampleGradeEditor (SampleGradeProcessor&);
    ~SampleGradeEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Item
    {
        juce::String id; int kind = 0; // 0 knob, 1 toggle, 2 combo
        std::unique_ptr<juce::Slider> slider; std::unique_ptr<juce::ToggleButton> toggle; std::unique_ptr<juce::ComboBox> combo;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    };
    void add (const char* id, const char* text, int kind);

    SampleGradeProcessor& processorRef;
    std::vector<std::unique_ptr<Item>> items;
    std::array<juce::TextButton, 4> gradeButtons;
    juce::Label title, info;
    juce::ComboBox factoryBox;
    atelier::PresetBar presetBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleGradeEditor)
};
