cmake_minimum_required(VERSION 3.22)

project(JucePitch VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../JUCE ${CMAKE_BINARY_DIR}/JUCE)

juce_set_vst3_sdk_path(${CMAKE_CURRENT_SOURCE_DIR}/../vst3sdk)

juce_add_plugin(JucePitch
    COMPANY_NAME "Tokr"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Tokr
    PLUGIN_CODE Jpit
    FORMATS VST3
    VST3_AUTO_MANIFEST FALSE
    PRODUCT_NAME "Juce Pitch")

target_include_directories(JucePitch
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui)

target_sources(JucePitch
    PRIVATE
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        dsp/PitchShifterDSP.cpp
        gui/LookAndFeel.cpp
        gui/RotarySlider.cpp
        gui/MainEditor.cpp)

target_compile_features(JucePitch PRIVATE cxx_std_20)

target_compile_definitions(JucePitch
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(JucePitch
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
