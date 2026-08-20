#include "TimeStretch.h"

#include <algorithm>
#include <cmath>

namespace dt
{
namespace
{
float interpolateSample (const float* data, int numSamples, double position) noexcept
{
    if (numSamples <= 0)
        return 0.0f;

    if (position <= 0.0)
        return data[0];

    if (position >= (double) (numSamples - 1))
        return data[numSamples - 1];

    const auto i0 = (int) position;
    const auto i1 = i0 + 1;
    const auto frac = (float) (position - (double) i0);
    return data[i0] + frac * (data[i1] - data[i0]);
}

double normalisedCorrelation (const float* a, const float* b, int n) noexcept
{
    if (n <= 4)
        return 0.0;

    double dot = 0.0, a2 = 0.0, b2 = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const auto av = (double) a[i];
        const auto bv = (double) b[i];
        dot += av * bv;
        a2 += av * av;
        b2 += bv * bv;
    }

    const auto denom = std::sqrt (a2 * b2) + 1.0e-12;
    return dot / denom;
}
} // namespace

juce::AudioBuffer<float> applyWarpLinear (const juce::AudioBuffer<float>& source,
                                          const std::vector<double>& sourcePositionAtOutput,
                                          int hop,
                                          int numOutputSamples)
{
    const auto channels = source.getNumChannels();
    juce::AudioBuffer<float> out (channels, std::max (1, numOutputSamples));
    out.clear();

    if (source.getNumSamples() <= 1 || sourcePositionAtOutput.size() < 2 || hop <= 0)
        return out;

    const auto lastHop = (int) sourcePositionAtOutput.size() - 1;

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* src = source.getReadPointer (ch);
        auto* dst = out.getWritePointer (ch);
        const auto nSrc = source.getNumSamples();

        for (int i = 0; i < numOutputSamples; ++i)
        {
            const auto hopIndex = (double) i / (double) hop;
            const auto h0 = juce::jlimit (0, lastHop - 1, (int) hopIndex);
            const auto frac = (float) (hopIndex - (double) h0);
            const auto pos = sourcePositionAtOutput[(size_t) h0]
                           + ((double) frac) * (sourcePositionAtOutput[(size_t) h0 + 1]
                                                - sourcePositionAtOutput[(size_t) h0]);
            dst[i] = interpolateSample (src, nSrc, pos);
        }
    }

    return out;
}

juce::AudioBuffer<float> applyWarpWsola (const juce::AudioBuffer<float>& source,
                                         const std::vector<double>& sourcePositionAtOutput,
                                         int hop,
                                         int windowSize,
                                         int searchRadius)
{
    const auto channels = source.getNumChannels();
    const auto nSrc = source.getNumSamples();
    const auto numHops = (int) sourcePositionAtOutput.size();

    if (channels <= 0 || nSrc <= 1 || numHops < 2 || hop <= 0 || windowSize < hop * 2)
        return applyWarpLinear (source, sourcePositionAtOutput, hop,
                                std::max (1, (numHops - 1) * hop));

    const auto numOutput = (numHops - 1) * hop + windowSize;
    juce::AudioBuffer<float> out (channels, numOutput);
    out.clear();

    std::vector<float> window ((size_t) windowSize);
    for (int i = 0; i < windowSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                     * (float) i / (float) (windowSize - 1));

    const auto overlap = windowSize - hop;
    std::vector<float> lastGrain ((size_t) windowSize, 0.0f);
    std::vector<float> candidate ((size_t) windowSize, 0.0f);
    std::vector<float> bestGrain ((size_t) windowSize, 0.0f);

    // Analysis uses a mono mix so the search is coherent across channels.
    juce::AudioBuffer<float> mono (1, nSrc);
    mono.clear();

    for (int ch = 0; ch < channels; ++ch)
        mono.addFrom (0, 0, source, ch, 0, nSrc, 1.0f / (float) channels);

    const auto* monoSrc = mono.getReadPointer (0);
    auto lastCentre = sourcePositionAtOutput.front();

    for (int k = 0; k < numHops; ++k)
    {
        auto target = sourcePositionAtOutput[(size_t) k];
        target = juce::jlimit (0.0, (double) (nSrc - 1), target);

        auto bestPos = target;
        auto bestCorr = -2.0;

        if (k > 0)
        {
            const auto lo = std::max (0.0, target - (double) searchRadius);
            const auto hi = std::min ((double) (nSrc - windowSize - 1), target + (double) searchRadius);

            // 4-sample steps in the search, then refine ±3.
            for (double cand = lo; cand <= hi; cand += 4.0)
            {
                for (int i = 0; i < overlap; ++i)
                    candidate[(size_t) i] = interpolateSample (monoSrc, nSrc, cand + (double) i);

                const auto corr = normalisedCorrelation (lastGrain.data() + hop,
                                                         candidate.data(),
                                                         overlap);
                if (corr > bestCorr)
                {
                    bestCorr = corr;
                    bestPos = cand;
                }
            }

            const auto refineLo = std::max (0.0, bestPos - 3.0);
            const auto refineHi = std::min ((double) (nSrc - windowSize - 1), bestPos + 3.0);

            for (double cand = refineLo; cand <= refineHi; cand += 1.0)
            {
                for (int i = 0; i < overlap; ++i)
                    candidate[(size_t) i] = interpolateSample (monoSrc, nSrc, cand + (double) i);

                const auto corr = normalisedCorrelation (lastGrain.data() + hop,
                                                         candidate.data(),
                                                         overlap);
                if (corr > bestCorr)
                {
                    bestCorr = corr;
                    bestPos = cand;
                }
            }
        }

        lastCentre = bestPos;
        const auto outOffset = k * hop;

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* src = source.getReadPointer (ch);
            auto* dst = out.getWritePointer (ch);

            for (int i = 0; i < windowSize; ++i)
            {
                const auto s = interpolateSample (src, nSrc, bestPos + (double) i);
                const auto idx = outOffset + i;

                if (idx < numOutput)
                    dst[idx] += s * window[(size_t) i];

                if (ch == 0)
                    lastGrain[(size_t) i] = s * window[(size_t) i];
            }
        }

        juce::ignoreUnused (lastCentre);
    }

    // Trim the extra OLA tail so the buffer matches the intended output length.
    const auto intended = std::max (1, (numHops - 1) * hop);
    juce::AudioBuffer<float> trimmed (channels, intended);

    for (int ch = 0; ch < channels; ++ch)
        trimmed.copyFrom (ch, 0, out, ch, 0, intended);

    return trimmed;
}
} // namespace dt
