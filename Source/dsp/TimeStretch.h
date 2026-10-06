#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace dt
{
/**
    High-quality elastic time-alignment.

    `sourcePositionAtOutput[k]` is the source sample that should land at
    output sample `k * hop`.

    Strategy:
      - constant-offset warps are a pure delay (transparent);
      - small local rate changes use pitch-synchronous WSOLA with OLA
        normalisation (keeps formants, low grain);
      - larger stretches use an identity-locked phase vocoder.
*/
juce::AudioBuffer<float> applyWarpHighQuality (const juce::AudioBuffer<float>& source,
                                               const std::vector<double>& sourcePositionAtOutput,
                                               int hop,
                                               int numOutputSamples,
                                               double sampleRate);

juce::AudioBuffer<float> applyWarpLinear (const juce::AudioBuffer<float>& source,
                                          const std::vector<double>& sourcePositionAtOutput,
                                          int hop,
                                          int numOutputSamples);
} // namespace dt
