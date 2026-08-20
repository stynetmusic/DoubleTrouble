#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace dt
{
/**
    WSOLA time-scale / warp applicator.

    `sourcePositionAtOutput[k]` is the (possibly fractional) source sample
    that should land at output sample `k * hop`. The output length is
    `(mapSize - 1) * hop`. Stereo channels share the same warp so the image
    stays intact.
*/
juce::AudioBuffer<float> applyWarpWsola (const juce::AudioBuffer<float>& source,
                                         const std::vector<double>& sourcePositionAtOutput,
                                         int hop = 256,
                                         int windowSize = 1024,
                                         int searchRadius = 128);

/** Linear resample of `source` onto a destination of `numOutputSamples` using a warp map
    defined at every hop. Faster / lower quality fallback used for tiny files. */
juce::AudioBuffer<float> applyWarpLinear (const juce::AudioBuffer<float>& source,
                                          const std::vector<double>& sourcePositionAtOutput,
                                          int hop,
                                          int numOutputSamples);
} // namespace dt
