#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace dt
{
/**
    Dual-grain delay-line pitch shifter.

    Two overlapping Hann grains read a ring buffer at `ratio` samples per
    output sample. Ratio 1 keeps pitch; 2^ (semitones/12) transposes.
    Cheap enough to run one instance per choir voice on the audio thread.
*/
class PitchShifter
{
public:
    void prepare (double sampleRate, float grainMs = 70.0f);
    void reset() noexcept;
    float processSample (float input, float ratio) noexcept;

private:
    std::vector<float> buffer;
    int bufferSize = 0;
    int writePos = 0;
    int grainSize = 1;
    std::array<float, 2> readPos { 0.0f, 0.0f };
    std::array<float, 2> grainPhase { 0.0f, 0.5f };
};
} // namespace dt
