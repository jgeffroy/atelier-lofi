cmake_minimum_required(VERSION 3.22)

project(AtelierLoFiPlugins VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

# ---------------------------------------------------------------------------
# RetroDeck: cassette, vinyl and VHS emulation
# ---------------------------------------------------------------------------
juce_add_plugin(RetroDeck
    COMPANY_NAME "AtelierDSP"
    PLUGIN_MANUFACTURER_CODE Atdp
    PLUGIN_CODE Rtdk
    FORMATS VST3
    PRODUCT_NAME "RetroDeck"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES "Fx" "Distortion"
)

target_sources(RetroDeck PRIVATE Source/RetroProcessor.cpp Source/RetroEditor.cpp)
target_compile_definitions(RetroDeck PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(RetroDeck
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# SampleGrade: sampling grades of a late-90s phrase sampler
# ---------------------------------------------------------------------------
juce_add_plugin(SampleGrade
    COMPANY_NAME "AtelierDSP"
    PLUGIN_MANUFACTURER_CODE Atdp
    PLUGIN_CODE Smgr
    FORMATS VST3
    PRODUCT_NAME "SampleGrade"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES "Fx" "Distortion"
)

target_sources(SampleGrade PRIVATE Source/SampleGrade/GradeProcessor.cpp Source/SampleGrade/GradeEditor.cpp)
target_compile_definitions(SampleGrade PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SampleGrade
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
