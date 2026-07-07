cmake_minimum_required(VERSION 3.22)

project(Noisemaker VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(NoisemakerAssets
    SOURCES
        Resources/noisemaker_icon.svg
)

juce_add_plugin(Noisemaker
    COMPANY_NAME "Graywave"
    BUNDLE_ID "com.graywave.noisemaker"
    PLUGIN_MANUFACTURER_CODE GryW
    PLUGIN_CODE NoMk
    FORMATS VST3 Standalone
    PRODUCT_NAME "Noisemaker"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx Instrument Tools
)

target_sources(Noisemaker
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PitchDetector.cpp
        Source/SampleEngine.cpp
)

target_include_directories(Noisemaker
    PRIVATE
        Source
)

target_compile_definitions(Noisemaker
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Noisemaker
    PRIVATE
        NoisemakerAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
