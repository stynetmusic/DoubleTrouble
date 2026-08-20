cmake_minimum_required(VERSION 3.22)

project(DoubleTrouble VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE lives outside this repo by default (same layout as other local plugins).
# Override with -DJUCE_DIR=/path/to/JUCE if needed.
if(NOT DEFINED JUCE_DIR)
    if(DEFINED ENV{JUCE_DIR} AND NOT "$ENV{JUCE_DIR}" STREQUAL "")
        set(JUCE_DIR "$ENV{JUCE_DIR}")
    else()
        set(JUCE_DIR "$ENV{HOME}/JUCE")
    endif()
endif()

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${JUCE_DIR}. Pass -DJUCE_DIR=/path/to/JUCE or set the JUCE_DIR env var.")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

# VST3 is the primary format. AU is enabled on Apple so a later AU-only
# product does not require a CMake rewrite. Standalone is for development.
if(APPLE)
    set(DT_FORMATS VST3 AU Standalone)
else()
    set(DT_FORMATS VST3 Standalone)
endif()

juce_add_plugin(DoubleTrouble
    COMPANY_NAME "Stynetmusic"
    BUNDLE_ID com.stynetmusic.DoubleTrouble
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Styn
    PLUGIN_CODE DblT
    FORMATS ${DT_FORMATS}
    PRODUCT_NAME "Double Trouble"
    DESCRIPTION "Vocal alignment and choir generator"
    VST3_CATEGORIES Fx Pitch Shift
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
)

target_sources(DoubleTrouble
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/Parameters.h
        Source/dsp/AlignmentEngine.cpp
        Source/dsp/AlignmentEngine.h
        Source/dsp/ChoirEngine.cpp
        Source/dsp/ChoirEngine.h
        Source/dsp/PitchShifter.cpp
        Source/dsp/PitchShifter.h
        Source/dsp/PreviewPlayer.cpp
        Source/dsp/PreviewPlayer.h
        Source/dsp/TimeStretch.cpp
        Source/dsp/TimeStretch.h
        Source/ui/AlignmentPanel.cpp
        Source/ui/AlignmentPanel.h
        Source/ui/ChoirPanel.cpp
        Source/ui/ChoirPanel.h
        Source/ui/DropZone.cpp
        Source/ui/DropZone.h
        Source/ui/Palette.h
        Source/ui/PluginLookAndFeel.cpp
        Source/ui/PluginLookAndFeel.h
        Source/ui/RotaryKnob.cpp
        Source/ui/RotaryKnob.h
        Source/ui/WaveformView.cpp
        Source/ui/WaveformView.h
)

target_include_directories(DoubleTrouble PRIVATE Source)

target_compile_definitions(DoubleTrouble
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(DoubleTrouble
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source" FILES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/dsp/AlignmentEngine.cpp
    Source/dsp/AlignmentEngine.h
    Source/dsp/ChoirEngine.cpp
    Source/dsp/ChoirEngine.h
    Source/dsp/PitchShifter.cpp
    Source/dsp/PitchShifter.h
    Source/dsp/PreviewPlayer.cpp
    Source/dsp/PreviewPlayer.h
    Source/dsp/TimeStretch.cpp
    Source/dsp/TimeStretch.h
    Source/ui/AlignmentPanel.cpp
    Source/ui/AlignmentPanel.h
    Source/ui/ChoirPanel.cpp
    Source/ui/ChoirPanel.h
    Source/ui/DropZone.cpp
    Source/ui/DropZone.h
    Source/ui/Palette.h
    Source/ui/PluginLookAndFeel.cpp
    Source/ui/PluginLookAndFeel.h
    Source/ui/RotaryKnob.cpp
    Source/ui/RotaryKnob.h
    Source/ui/WaveformView.cpp
    Source/ui/WaveformView.h
)
