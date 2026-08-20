#include "PitchShifter.h"

#include <cmath>

namespace dt
{
void PitchShifter::prepare (double sampleRate, float grainMs)
{
    grainSize = std::max (64, (int) std::lround (sampleRate * (double) grainMs * 0.001));
    bufferSize = grainSize * 8; // headroom for ±1 octave plus humanize delay
    buffer.assign ((size_t) bufferSize, 0.0f);
    reset();
}

void PitchShifter::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
    grainPhase[0] = 0.0f;
    grainPhase[1] = 0.5f;
    readPos[0] = 0.0f;
    readPos[1] = (float) (grainSize / 2);
}

float PitchShifter::processSample (float input, float ratio) noexcept
{
    if (bufferSize <= 0)
        return input;

    buffer[(size_t) writePos] = input;

    const auto safeRatio = juce::jlimit (0.25f, 4.0f, ratio);
    float mixed = 0.0f;

    for (int g = 0; g < 2; ++g)
    {
        auto r = readPos[(size_t) g];

        while (r >= (float) bufferSize) r -= (float) bufferSize;
        while (r < 0.0f)                r += (float) bufferSize;

        const auto i0 = (int) r;
        const auto i1 = (i0 + 1) % bufferSize;
        const auto frac = r - (float) i0;
        const auto sample = buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);

        const auto phase = grainPhase[(size_t) g];
        const auto window = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * phase);
        mixed += sample * window;

        auto newPhase = phase + (1.0f / (float) grainSize);

        if (newPhase >= 1.0f)
        {
            newPhase -= 1.0f;
            // Restart this grain just behind the write head, far enough for the ratio.
            const auto lookback = (int) std::lround ((double) grainSize * (double) std::max (1.0f, safeRatio));
            readPos[(size_t) g] = (float) ((writePos - lookback + bufferSize * 4) % bufferSize);
        }
        else
        {
            readPos[(size_t) g] = r + safeRatio;
        }

        grainPhase[(size_t) g] = newPhase;
    }

    writePos = (writePos + 1) % bufferSize;
    return mixed;
}
} // namespace dt
