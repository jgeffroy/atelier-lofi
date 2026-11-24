cmake_minimum_required(VERSION 3.22)

project(AtelierLoFiPlugins VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link the Microsoft C++ runtime statically on Windows, so users do not need the Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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

# ---------------------------------------------------------------------------
# WowLab: wow & flutter / tape modulation
# ---------------------------------------------------------------------------
juce_add_plugin(WowLab
    COMPANY_NAME "AtelierDSP"
    PLUGIN_MANUFACTURER_CODE Atdp
    PLUGIN_CODE Wwlb
    FORMATS VST3
    PRODUCT_NAME "WowLab"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES "Fx" "Modulation"
)

target_sources(WowLab PRIVATE Source/WowLab/WowProcessor.cpp Source/WowLab/WowEditor.cpp)
target_compile_definitions(WowLab PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(WowLab
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# RetroDeck 2: second version (physical models from the literature, wider ranges), installable
# side by side with RetroDeck for comparison
# ---------------------------------------------------------------------------
juce_add_plugin(RetroDeck2
    COMPANY_NAME "AtelierDSP"
    PLUGIN_MANUFACTURER_CODE Atdp
    PLUGIN_CODE Rtd2
    FORMATS VST3
    PRODUCT_NAME "RetroDeck 2"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES "Fx" "Distortion"
)

target_sources(RetroDeck2 PRIVATE Source/RetroDeck2/RetroProcessor2.cpp Source/RetroDeck2/RetroEditor2.cpp)
target_compile_definitions(RetroDeck2 PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(RetroDeck2
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
