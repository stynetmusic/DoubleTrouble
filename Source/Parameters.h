#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** Parameter IDs shared by the processor, choir DSP and the editor. */
namespace dt
{
namespace IDs
{
    inline constexpr const char* alignAmount   = "alignAmount";
    inline constexpr const char* alignToGrid   = "alignToGrid";
    inline constexpr const char* gridStrength  = "gridStrength";
    inline constexpr const char* gridDivision  = "gridDivision";
    inline constexpr const char* previewAfter  = "previewAfter";

    inline constexpr const char* choirOn       = "choirOn";
    inline constexpr const char* choirVoices   = "choirVoices";
    inline constexpr const char* choirPreset   = "choirPreset";
    inline constexpr const char* choirDetune   = "choirDetune";
    inline constexpr const char* choirHumanize = "choirHumanize";
    inline constexpr const char* choirFormant  = "choirFormant";
    inline constexpr const char* choirWidth    = "choirWidth";
    inline constexpr const char* choirDryWet   = "choirDryWet";
    inline constexpr const char* choirGain     = "choirGain";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { IDs::alignAmount, 1 }, "Align Amount",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { IDs::alignToGrid, 1 }, "Align to Grid", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { IDs::gridStrength, 1 }, "Grid Strength",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 80.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { IDs::gridDivision, 1 }, "Grid",
        juce::StringArray { "1/4", "1/8", "1/8T", "1/16", "1/16T" }, 3));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { IDs::previewAfter, 1 }, "Preview After", true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { IDs::choirOn, 1 }, "Choir", true));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { IDs::choirVoices, 1 }, "Voices", 4, 12, 8));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { IDs::choirPreset, 1 }, "Preset",
        juce::StringArray { "Unison", "Choir", "Octaves", "Custom" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { IDs::choirDetune, 1 }, "Detune",
        juce::NormalisableRange<float> { 0.0f, 40.0f, 0.1f }, 12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ct")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { IDs::choirHumanize, 1 }, "Humanize",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 35.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { IDs::choirFormant, 1 }, "Formant",
        juce::NormalisableRange<float> { -12.0f, 12.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("st")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { IDs::choirWidth, 1 }, "Width",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 85.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { IDs::choirDryWet, 1 }, "Dry/Wet",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 45.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { IDs::choirGain, 1 }, "Output",
        juce::NormalisableRange<float> { -24.0f, 12.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

/** Musical grid: parameter index → length of one division, in beats. */
inline double beatsForGridDivision (int index) noexcept
{
    switch (index)
    {
        case 0:  return 1.0;        // 1/4
        case 1:  return 0.5;        // 1/8
        case 2:  return 1.0 / 3.0;  // 1/8T
        case 3:  return 0.25;       // 1/16
        case 4:  return 1.0 / 6.0;  // 1/16T
        default: return 0.25;
    }
}
} // namespace dt
