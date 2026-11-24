#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "WowProcessor.h"
#include "../UserPresets.h"

class WowLabEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WowLabEditor (WowLabProcessor&);
    ~WowLabEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Item
    {
        juce::String id; int kind = 0, page = 0; // kind: 0 knob, 1 toggle, 2 combo
        std::unique_ptr<juce::Slider> slider; std::unique_ptr<juce::ToggleButton> toggle; std::unique_ptr<juce::ComboBox> combo;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        juce::Component* comp() const { return slider ? (juce::Component*) slider.get() : toggle ? (juce::Component*) toggle.get() : (juce::Component*) combo.get(); }
    };
    void add (const char* id, const char* text, int kind, int page);
    void showPage (int page);
    void timerCallback() override;

    WowLabProcessor& processorRef;
    std::vector<std::unique_ptr<Item>> items;
    juce::TextButton pageButtons[2] { juce::TextButton ("Main"), juce::TextButton ("Advanced") };
    juce::Label title, info;
    juce::ComboBox factoryBox;
    atelier::PresetBar presetBar;
    juce::Rectangle<int> scope;
    int currentPage = 0;
    float scopePhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WowLabEditor)
};
