#include "TimeStretch.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace dt
{
namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

int clampIndex (int i, int n) noexcept
{
    return juce::jlimit (0, std::max (0, n - 1), i);
}

float hermiteSample (const float* data, int numSamples, double position) noexcept
{
    if (numSamples <= 0)
        return 0.0f;

    if (position <= 0.0)
        return data[0];

    if (position >= (double) (numSamples - 1))
        return data[numSamples - 1];

    const auto i = (int) position;
    const auto t = (float) (position - (double) i);
    const auto ym1 = data[clampIndex (i - 1, numSamples)];
    const auto y0  = data[i];
    const auto y1  = data[clampIndex (i + 1, numSamples)];
    const auto y2  = data[clampIndex (i + 2, numSamples)];

    const auto c1 = 0.5f * (y1 - ym1);
    const auto c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const auto c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + y0;
}

double interpolateWarp (const std::vector<double>& warp, int hop, double outSample) noexcept
{
    if (warp.size() < 2 || hop <= 0)
        return 0.0;

    const auto last = (int) warp.size() - 1;
    const auto idx = outSample / (double) hop;
    const auto i0 = juce::jlimit (0, last - 1, (int) idx);
    const auto frac = idx - (double) i0;
    return warp[(size_t) i0] + frac * (warp[(size_t) i0 + 1] - warp[(size_t) i0]);
}

void smoothAndLimitWarp (std::vector<double>& warp, int hop, int numSourceSamples)
{
    if (warp.size() < 3 || hop <= 0)
        return;

    std::vector<double> tmp (warp.size());
    const auto lastSrc = (double) std::max (0, numSourceSamples - 1);

    for (int pass = 0; pass < 6; ++pass)
    {
        tmp.front() = warp.front();
        tmp.back() = warp.back();

        for (size_t i = 1; i + 1 < warp.size(); ++i)
            tmp[i] = 0.12 * warp[i - 1] + 0.76 * warp[i] + 0.12 * warp[i + 1];

        warp.swap (tmp);
    }

    // Keep time moving forward, but allow moderate compress/expand.
    const auto minStep = 0.45 * (double) hop; // max ~2.2× compress
    const auto maxStep = 2.20 * (double) hop; // max ~2.2× expand

    warp.front() = juce::jlimit (0.0, lastSrc, warp.front());

    for (size_t i = 1; i < warp.size(); ++i)
    {
        const auto lo = warp[i - 1] + minStep;
        const auto hi = warp[i - 1] + maxStep;
        warp[i] = juce::jlimit (0.0, lastSrc, juce::jlimit (lo, hi, warp[i]));
    }
}

bool isNearlyConstantDelay (const std::vector<double>& warp, int hop, double& offsetOut) noexcept
{
    if (warp.size() < 2 || hop <= 0)
        return false;

    double sum = 0.0;

    for (int k = 0; k < (int) warp.size(); ++k)
        sum += warp[(size_t) k] - (double) k * (double) hop;

    const auto offset = sum / (double) warp.size();
    offsetOut = offset;

    // 0.4 ms of elastic deviation is inaudible as stretch — treat as delay.
    const auto maxDev = 0.0004 * 48000.0; // ~19 samples; scale below with hop-free constant
    juce::ignoreUnused (maxDev);

    auto worst = 0.0;

    for (int k = 0; k < (int) warp.size(); ++k)
    {
        const auto expected = (double) k * (double) hop + offset;
        worst = std::max (worst, std::abs (warp[(size_t) k] - expected));
    }

    return worst < 0.08 * (double) hop;
}

void fadeEdges (juce::AudioBuffer<float>& buffer, int fadeSamples) noexcept
{
    const auto n = buffer.getNumSamples();
    fadeSamples = juce::jlimit (0, n / 4, fadeSamples);

    if (fadeSamples <= 1)
        return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);

        for (int i = 0; i < fadeSamples; ++i)
        {
            const auto g = 0.5f - 0.5f * std::cos (kPi * (float) i / (float) fadeSamples);
            d[i] *= g;
            d[n - 1 - i] *= g;
        }
    }
}

void protectPeaks (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto peak = buffer.getMagnitude (0, buffer.getNumSamples());

    if (peak > 0.98f && peak > 1.0e-8f)
        buffer.applyGain (0.98f / peak);
}

double frameRms (const float* data, int n, double centre, int radius) noexcept
{
    if (n <= 0)
        return 0.0;

    const auto i0 = clampIndex ((int) std::lround (centre) - radius, n);
    const auto i1 = clampIndex ((int) std::lround (centre) + radius, n);
    double acc = 0.0;
    int count = 0;

    for (int i = i0; i <= i1; ++i)
    {
        const auto s = (double) data[i];
        acc += s * s;
        ++count;
    }

    return count > 0 ? std::sqrt (acc / (double) count) : 0.0;
}

int estimatePeriod (const float* data, int n, double centre, int srInt) noexcept
{
    const auto minP = std::max (16, srInt / 800);  // 800 Hz
    const auto maxP = std::min (n / 4, srInt / 70); // 70 Hz
    const auto radius = maxP * 2;
    const auto i0 = clampIndex ((int) std::lround (centre) - radius, n);
    const auto i1 = clampIndex ((int) std::lround (centre) + radius, n);
    const auto len = i1 - i0;

    if (len < maxP * 2)
        return juce::jlimit (minP, maxP, srInt / 180);

    auto bestTau = minP;
    auto best = -1.0e9;

    for (int tau = minP; tau <= maxP; ++tau)
    {
        double acc = 0.0;
        const auto lim = len - tau;

        if (lim <= 16)
            break;

        for (int i = 0; i < lim; ++i)
            acc += (double) data[i0 + i] * (double) data[i0 + i + tau];

        acc /= (double) lim;

        if (acc > best)
        {
            best = acc;
            bestTau = tau;
        }
    }

    return bestTau;
}

double normalisedCorr (const float* a, const float* b, int n) noexcept
{
    if (n <= 8)
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

    return dot / (std::sqrt (a2 * b2) + 1.0e-12);
}

int snapZeroCrossing (const float* data, int n, int pos, int radius) noexcept
{
    pos = clampIndex (pos, n);
    auto best = pos;
    auto bestAbs = std::abs (data[pos]);

    for (int d = 1; d <= radius; ++d)
    {
        const auto l = pos - d;
        const auto r = pos + d;

        if (l > 0 && l < n)
        {
            const auto crossed = (data[l] >= 0.0f) != (data[l - 1] >= 0.0f);
            const auto a = std::abs (data[l]);

            if (crossed && a <= bestAbs)
            {
                bestAbs = a;
                best = l;
            }
        }

        if (r > 0 && r < n - 1)
        {
            const auto crossed = (data[r] >= 0.0f) != (data[r - 1] >= 0.0f);
            const auto a = std::abs (data[r]);

            if (crossed && a < bestAbs)
            {
                bestAbs = a;
                best = r;
            }
        }
    }

    return best;
}

juce::AudioBuffer<float> applyConstantDelay (const juce::AudioBuffer<float>& source,
                                             double offsetSamples,
                                             int numOutputSamples)
{
    const auto channels = source.getNumChannels();
    juce::AudioBuffer<float> out (std::max (1, channels), std::max (1, numOutputSamples));
    out.clear();
    const auto nSrc = source.getNumSamples();

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* src = source.getReadPointer (ch);
        auto* dst = out.getWritePointer (ch);

        for (int i = 0; i < numOutputSamples; ++i)
            dst[i] = hermiteSample (src, nSrc, (double) i + offsetSamples);
    }

    return out;
}

//==============================================================================
juce::AudioBuffer<float> applyWsolaHq (const juce::AudioBuffer<float>& source,
                                       const std::vector<double>& warp,
                                       int warpHop,
                                       int numOutputSamples,
                                       double sampleRate)
{
    const auto channels = source.getNumChannels();
    const auto nSrc = source.getNumSamples();
    juce::AudioBuffer<float> out (channels, numOutputSamples);
    juce::AudioBuffer<float> weight (1, numOutputSamples);
    out.clear();
    weight.clear();

    const auto winSamples = std::max (512, (int) std::lround (sampleRate * 0.072)); // ~72 ms
    const auto hop = std::max (64, (int) std::lround (sampleRate * 0.008));         // ~8 ms
    const auto corrLen = std::max (64, (int) std::lround (sampleRate * 0.016));     // 16 ms NCC
    auto* wAcc = weight.getWritePointer (0);

    std::vector<float> window ((size_t) winSamples);
    for (int i = 0; i < winSamples; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) (winSamples - 1));

    juce::AudioBuffer<float> mono (1, nSrc);
    mono.clear();
    const auto monoGain = 1.0f / (float) std::max (1, channels);

    for (int ch = 0; ch < channels; ++ch)
        mono.addFrom (0, 0, source, ch, 0, nSrc, monoGain);

    const auto* monoSrc = mono.getReadPointer (0);
    std::vector<float> prevTail ((size_t) corrLen, 0.0f);
    std::vector<float> candidate ((size_t) corrLen, 0.0f);
    auto havePrev = false;
    auto lastPeriod = (int) std::lround (sampleRate / 180.0);
    const auto srInt = (int) std::lround (sampleRate);
    const auto silenceThresh = 1.5e-4; // ~ -76 dBFS RMS

    const auto numHops = std::max (1, (numOutputSamples + hop - 1) / hop);

    for (int k = 0; k < numHops; ++k)
    {
        const auto outPos = k * hop;
        auto target = interpolateWarp (warp, warpHop, (double) outPos);
        target = juce::jlimit (0.0, (double) std::max (0, nSrc - 1), target);

        const auto rms = frameRms (monoSrc, nSrc, target + 0.5 * (double) winSamples,
                                   winSamples / 2);

        auto bestPos = target;

        if (rms < silenceThresh)
        {
            havePrev = false;
        }
        else
        {
            if ((k % 4) == 0)
                lastPeriod = estimatePeriod (monoSrc, nSrc, target, srInt);

            const auto search = juce::jlimit (hop, lastPeriod * 2,
                                              (int) std::lround (sampleRate * 0.018));

            if (havePrev)
            {
                auto bestCorr = -2.0;
                const auto lo = std::max (0.0, target - (double) search);
                const auto hi = std::min ((double) (nSrc - winSamples - 2), target + (double) search);

                for (double cand = lo; cand <= hi; cand += 4.0)
                {
                    for (int i = 0; i < corrLen; ++i)
                        candidate[(size_t) i] = hermiteSample (monoSrc, nSrc, cand + (double) i);

                    const auto corr = normalisedCorr (prevTail.data(), candidate.data(), corrLen);

                    if (corr > bestCorr)
                    {
                        bestCorr = corr;
                        bestPos = cand;
                    }
                }

                const auto refineLo = std::max (0.0, bestPos - 6.0);
                const auto refineHi = std::min ((double) (nSrc - winSamples - 2), bestPos + 6.0);

                for (double cand = refineLo; cand <= refineHi; cand += 1.0)
                {
                    for (int i = 0; i < corrLen; ++i)
                        candidate[(size_t) i] = hermiteSample (monoSrc, nSrc, cand + (double) i);

                    const auto corr = normalisedCorr (prevTail.data(), candidate.data(), corrLen);

                    if (corr > bestCorr)
                    {
                        bestCorr = corr;
                        bestPos = cand;
                    }
                }

                bestPos = (double) snapZeroCrossing (monoSrc, nSrc,
                                                     (int) std::lround (bestPos),
                                                     std::max (4, lastPeriod / 8));
            }

            for (int i = 0; i < corrLen; ++i)
                prevTail[(size_t) i] = hermiteSample (monoSrc, nSrc, bestPos + (double) hop + (double) i);

            havePrev = true;
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* src = source.getReadPointer (ch);
            auto* dst = out.getWritePointer (ch);

            for (int i = 0; i < winSamples; ++i)
            {
                const auto idx = outPos + i;

                if (idx >= numOutputSamples)
                    break;

                const auto s = hermiteSample (src, nSrc, bestPos + (double) i);
                const auto w = window[(size_t) i];
                dst[idx] += s * w;

                if (ch == 0)
                    wAcc[idx] += w;
            }
        }
    }

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* dst = out.getWritePointer (ch);

        for (int i = 0; i < numOutputSamples; ++i)
        {
            const auto w = wAcc[i];
            dst[i] = (w > 1.0e-4f) ? dst[i] / w : 0.0f;
        }
    }

    return out;
}

//==============================================================================
float wrapPhase (float x) noexcept
{
    const auto twoPi = kTwoPi;
    x = std::fmod (x + kPi, twoPi);

    if (x < 0.0f)
        x += twoPi;

    return x - kPi;
}

juce::AudioBuffer<float> applyPhaseVocoder (const juce::AudioBuffer<float>& source,
                                            const std::vector<double>& warp,
                                            int warpHop,
                                            int numOutputSamples,
                                            double sampleRate)
{
    const auto channels = source.getNumChannels();
    const auto nSrc = source.getNumSamples();

    auto order = 12; // 4096
    if (sampleRate >= 80000.0)
        order = 13;  // 8192 at 96 kHz so the window stays ~85 ms

    juce::dsp::FFT fft (order);
    const auto fftSize = fft.getSize();
    const auto hop = std::max (64, fftSize / 8); // 87.5 % overlap
    const auto bins = fftSize / 2;
    const auto silenceThresh = 1.5e-4;

    std::vector<float> window ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) (fftSize - 1));

    juce::AudioBuffer<float> out (channels, numOutputSamples + fftSize);
    juce::AudioBuffer<float> weight (channels, numOutputSamples + fftSize);
    out.clear();
    weight.clear();

    using Complex = juce::dsp::Complex<float>;
    std::vector<Complex> timeDomain ((size_t) fftSize);
    std::vector<Complex> freqDomain ((size_t) fftSize);

    const auto numFrames = std::max (1, (numOutputSamples + hop - 1) / hop);

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* src = source.getReadPointer (ch);
        auto* dst = out.getWritePointer (ch);
        auto* wAcc = weight.getWritePointer (ch);

        std::vector<float> prevAnaPhase ((size_t) (bins + 1), 0.0f);
        std::vector<float> synthPhase ((size_t) (bins + 1), 0.0f);
        std::vector<float> mag ((size_t) (bins + 1), 0.0f);
        std::vector<float> anaPhase ((size_t) (bins + 1), 0.0f);
        auto first = true;

        for (int f = 0; f < numFrames; ++f)
        {
            const auto tOut = (double) (f * hop);
            const auto tIn = interpolateWarp (warp, warpHop, tOut);
            const auto tInNext = interpolateWarp (warp, warpHop, tOut + (double) hop);
            const auto localRate = juce::jlimit (0.45, 2.2, (tInNext - tIn) / (double) hop);
            const auto analysisHop = (double) hop * localRate;
            const auto centre = tIn;

            const auto rms = frameRms (src, nSrc, centre, fftSize / 2);

            if (rms < silenceThresh)
            {
                first = true;
                continue;
            }

            const auto winStart = centre - 0.5 * (double) fftSize;

            for (int i = 0; i < fftSize; ++i)
            {
                const auto s = hermiteSample (src, nSrc, winStart + (double) i);
                timeDomain[(size_t) i] = Complex (s * window[(size_t) i], 0.0f);
            }

            fft.perform (timeDomain.data(), freqDomain.data(), false);

            for (int b = 0; b <= bins; ++b)
            {
                const auto re = freqDomain[(size_t) b].real();
                const auto im = freqDomain[(size_t) b].imag();
                mag[(size_t) b] = std::sqrt (re * re + im * im);
                anaPhase[(size_t) b] = std::atan2 (im, re);
            }

            if (first)
            {
                synthPhase = anaPhase;
                prevAnaPhase = anaPhase;
                first = false;
            }
            else
            {
                for (int b = 0; b <= bins; ++b)
                {
                    const auto expected = kTwoPi * (float) b * (float) analysisHop / (float) fftSize;
                    auto delta = anaPhase[(size_t) b] - prevAnaPhase[(size_t) b];
                    delta = wrapPhase (delta - expected) + expected;
                    const auto synthAdvance = (delta / (float) analysisHop) * (float) hop;
                    synthPhase[(size_t) b] = wrapPhase (synthPhase[(size_t) b] + synthAdvance);
                }
            }

            // Identity phase locking around spectral peaks (Laroche / Dolson).
            std::vector<int> peaks;
            peaks.reserve ((size_t) bins / 4);

            for (int b = 2; b < bins - 1; ++b)
            {
                const auto m = mag[(size_t) b];

                if (m > mag[(size_t) b - 1] && m >= mag[(size_t) b + 1]
                    && m > mag[(size_t) b - 2] && m >= mag[(size_t) b + 2]
                    && m > 1.0e-5f)
                {
                    peaks.push_back (b);
                }
            }

            if (! peaks.empty())
            {
                auto peakIdx = 0;

                for (int b = 0; b <= bins; ++b)
                {
                    while (peakIdx + 1 < (int) peaks.size()
                           && std::abs (b - peaks[(size_t) peakIdx + 1])
                                  < std::abs (b - peaks[(size_t) peakIdx]))
                        ++peakIdx;

                    const auto peak = peaks[(size_t) peakIdx];
                    const auto rel = wrapPhase (anaPhase[(size_t) b] - anaPhase[(size_t) peak]);
                    synthPhase[(size_t) b] = wrapPhase (synthPhase[(size_t) peak] + rel);
                }
            }

            prevAnaPhase = anaPhase;

            for (int b = 0; b <= bins; ++b)
            {
                const auto m = mag[(size_t) b];
                const auto p = synthPhase[(size_t) b];
                freqDomain[(size_t) b] = Complex (m * std::cos (p), m * std::sin (p));
            }

            for (int b = 1; b < bins; ++b)
                freqDomain[(size_t) (fftSize - b)] = std::conj (freqDomain[(size_t) b]);

            fft.perform (freqDomain.data(), timeDomain.data(), true);

            const auto invN = 1.0f / (float) fftSize;
            const auto olaPos = f * hop;

            for (int i = 0; i < fftSize; ++i)
            {
                const auto idx = olaPos + i;

                if (idx >= weight.getNumSamples())
                    break;

                const auto w = window[(size_t) i];
                dst[idx] += timeDomain[(size_t) i].real() * invN * w;
                wAcc[idx] += w;
            }
        }
    }

    juce::AudioBuffer<float> trimmed (channels, numOutputSamples);
    trimmed.clear();

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* src = out.getReadPointer (ch);
        const auto* wAcc = weight.getReadPointer (ch);
        auto* dst = trimmed.getWritePointer (ch);

        for (int i = 0; i < numOutputSamples; ++i)
        {
            const auto w = wAcc[i];
            dst[i] = (w > 1.0e-4f) ? src[i] / w : 0.0f;
        }
    }

    return trimmed;
}

double meanAbsRateDeviation (const std::vector<double>& warp, int hop) noexcept
{
    if (warp.size() < 2 || hop <= 0)
        return 0.0;

    double acc = 0.0;

    for (size_t i = 1; i < warp.size(); ++i)
    {
        const auto rate = (warp[i] - warp[i - 1]) / (double) hop;
        acc += std::abs (rate - 1.0);
    }

    return acc / (double) (warp.size() - 1);
}
} // namespace

juce::AudioBuffer<float> applyWarpLinear (const juce::AudioBuffer<float>& source,
                                          const std::vector<double>& sourcePositionAtOutput,
                                          int hop,
                                          int numOutputSamples)
{
    const auto channels = source.getNumChannels();
    juce::AudioBuffer<float> out (std::max (1, channels), std::max (1, numOutputSamples));
    out.clear();

    if (source.getNumSamples() <= 1 || sourcePositionAtOutput.size() < 2 || hop <= 0)
        return out;

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* src = source.getReadPointer (ch);
        auto* dst = out.getWritePointer (ch);
        const auto nSrc = source.getNumSamples();

        for (int i = 0; i < numOutputSamples; ++i)
            dst[i] = hermiteSample (src, nSrc, interpolateWarp (sourcePositionAtOutput, hop, (double) i));
    }

    return out;
}

juce::AudioBuffer<float> applyWarpHighQuality (const juce::AudioBuffer<float>& source,
                                               const std::vector<double>& sourcePositionAtOutput,
                                               int hop,
                                               int numOutputSamples,
                                               double sampleRate)
{
    const auto channels = source.getNumChannels();
    const auto intended = std::max (1, numOutputSamples);

    if (channels <= 0 || source.getNumSamples() <= 1 || sourcePositionAtOutput.size() < 2 || hop <= 0)
        return juce::AudioBuffer<float> (std::max (1, channels), intended);

    auto warp = sourcePositionAtOutput;
    smoothAndLimitWarp (warp, hop, source.getNumSamples());

    const auto fade = std::max (32, (int) std::lround ((sampleRate > 0.0 ? sampleRate : 44100.0) * 0.006));

    double delayOffset = 0.0;

    if (isNearlyConstantDelay (warp, hop, delayOffset))
    {
        auto out = applyConstantDelay (source, delayOffset, intended);
        fadeEdges (out, fade);
        protectPeaks (out);
        return out;
    }

    const auto rateDev = meanAbsRateDeviation (warp, hop);
    juce::AudioBuffer<float> out;

    // WSOLA is more natural on small vocal timing tweaks; the phase vocoder
    // holds up better once syllables are actually stretched/compressed.
    if (rateDev < 0.08)
        out = applyWsolaHq (source, warp, hop, intended, sampleRate > 0.0 ? sampleRate : 44100.0);
    else
        out = applyPhaseVocoder (source, warp, hop, intended, sampleRate > 0.0 ? sampleRate : 44100.0);

    fadeEdges (out, fade);
    protectPeaks (out);
    return out;
}
} // namespace dt
