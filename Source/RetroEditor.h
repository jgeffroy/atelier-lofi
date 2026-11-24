#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RetroProcessor.h"
#include "UserPresets.h"

class RetroDeckEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit RetroDeckEditor (RetroDeckProcessor&);
    ~RetroDeckEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum Kind { KnobK, ToggleK, ComboK };
    enum Page { PMain = 0, PTone, PCassette, PVinyl, PVhs, PMotion, PNoise, PEcho, NumPages };

    struct Control
    {
        juce::String id, label;
        Kind kind;
        int page;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::ComboBox> combo;
        juce::Label labelComp;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        juce::Component* comp() const
        {
            if (slider) return slider.get();
            if (toggle) return toggle.get();
            return combo.get();
        }
    };

    void add (const char* id, const char* label, Kind kind, int page);
    void showPage (int page);
    void timerCallback() override;
    void updateEnabled();

    RetroDeckProcessor& processorRef;
    std::vector<std::unique_ptr<Control>> controls;

    juce::TextButton pageButtons[NumPages];
    juce::Label titleLabel, statusLabel;
    juce::ComboBox factoryBox;
    atelier::PresetBar userPresets;
    juce::Rectangle<int> meterArea;
    float shownIn = -90.0f, shownOut = -90.0f;
    int currentPage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetroDeckEditor)
};
