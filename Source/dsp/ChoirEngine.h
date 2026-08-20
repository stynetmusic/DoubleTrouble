#pragma once

#include "PitchShifter.h"
#include <array>

namespace dt
{
struct ChoirSettings
{
    bool enabled = true;
    int numVoices = 8;
    int preset = 1;          // 0 Unison, 1 Choir, 2 Octaves, 3 Custom
    float detuneCents = 12.0f;
    float humanize = 0.35f;  // 0..1
    float formantSemitones = 0.0f;
    float width = 0.85f;     // 0..1
    float dryWet = 0.45f;    // 0..1
    float outputGainDb = 0.0f;
};

/**
    Realtime choir: mono-sum the input, spawn N pitch-shifted voices,
    pan them, mix with the dry stereo signal.
*/
class ChoirEngine
{
public:
    static constexpr int maxVoices = 12;

    void prepare (double sampleRate, int samplesPerBlock, int numChannels);
    void reset() noexcept;
    void process (juce::AudioBuffer<float>& buffer, const ChoirSettings& settings);

private:
    struct Voice
    {
        PitchShifter shifter;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 1 << 14 };
        juce::SmoothedValue<float> delayMs;
        juce::SmoothedValue<float> extraCents;
        juce::SmoothedValue<float> pan;
        float targetDelayMs = 8.0f;
        float targetCents = 0.0f;
        int samplesUntilWander = 0;
        juce::Random rng { 1 };
    };

    void updateFormantFilters (double sampleRate, float formantSemitones);
    static float semitoneForVoice (int preset, int voiceIndex, int numVoices) noexcept;
    static float detuneSign (int voiceIndex) noexcept;

    std::array<Voice, maxVoices> voices;
    std::array<juce::dsp::IIR::Filter<float>, 3> formantL;
    std::array<juce::dsp::IIR::Filter<float>, 3> formantR;
    juce::SmoothedValue<float> dryWetSmooth, gainSmooth, widthSmooth;
    double currentSampleRate = 44100.0;
    int wanderControlCounter = 0;
    float lastFormant = 1.0e9f;
};
} // namespace dt
