#include "ChoirEngine.h"

namespace dt
{
namespace
{
float dbToGain (float db) noexcept
{
    return juce::Decibels::decibelsToGain (db);
}

void equalPowerPan (float pan01, float& left, float& right) noexcept
{
    const auto angle = juce::jlimit (0.0f, 1.0f, pan01) * juce::MathConstants<float>::halfPi;
    left  = std::cos (angle);
    right = std::sin (angle);
}
} // namespace

float ChoirEngine::semitoneForVoice (int preset, int voiceIndex, int numVoices) noexcept
{
    juce::ignoreUnused (numVoices);

    static constexpr int choir[maxVoices]  = { 0, 0, 7, 12, -12, 4, -5, 3, 7, 12, -12, 0 };
    static constexpr int custom[maxVoices] = { 0, 3, 7, 12, -5, 4, -12, 7, 0, 12, 3, -7 };
    static constexpr int octaves[maxVoices] = { -12, 0, 12, -12, 0, 12, -12, 0, 12, 0, 12, -12 };

    const auto i = juce::jlimit (0, maxVoices - 1, voiceIndex);

    switch (preset)
    {
        case 0:  return 0.0f;
        case 1:  return (float) choir[i];
        case 2:  return (float) octaves[i];
        case 3:  return (float) custom[i];
        default: return 0.0f;
    }
}

float ChoirEngine::detuneSign (int voiceIndex) noexcept
{
    return (voiceIndex % 2 == 0) ? 1.0f : -1.0f;
}

void ChoirEngine::prepare (double sampleRate, int samplesPerBlock, int numChannels)
{
    currentSampleRate = sampleRate;
    juce::ignoreUnused (samplesPerBlock, numChannels);
    lastFormant = 1.0e9f;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) std::max (1, samplesPerBlock);
    spec.numChannels = 1;

    for (int v = 0; v < maxVoices; ++v)
    {
        auto& voice = voices[(size_t) v];
        voice.shifter.prepare (sampleRate, 70.0f);
        voice.delay.reset();
        voice.delay.prepare (spec);
        voice.delay.setMaximumDelayInSamples ((int) std::lround (sampleRate * 0.08));
        voice.delayMs.reset (sampleRate, 0.08);
        voice.extraCents.reset (sampleRate, 0.12);
        voice.pan.reset (sampleRate, 0.05);
        voice.rng = juce::Random (0xA5C3u + (juce::uint32) v * 97u);
        voice.samplesUntilWander = (int) (sampleRate * (0.25 + 0.4 * voice.rng.nextDouble()));
        voice.targetDelayMs = 6.0f + (float) v;
        voice.targetCents = 0.0f;
        voice.delayMs.setCurrentAndTargetValue (voice.targetDelayMs);
        voice.extraCents.setCurrentAndTargetValue (0.0f);
    }

    for (auto& f : formantL) { f.reset(); f.prepare (spec); }
    for (auto& f : formantR) { f.reset(); f.prepare (spec); }

    dryWetSmooth.reset (sampleRate, 0.03);
    gainSmooth.reset (sampleRate, 0.03);
    widthSmooth.reset (sampleRate, 0.05);
}

void ChoirEngine::reset() noexcept
{
    for (auto& voice : voices)
    {
        voice.shifter.reset();
        voice.delay.reset();
    }

    for (auto& f : formantL) f.reset();
    for (auto& f : formantR) f.reset();
}

void ChoirEngine::updateFormantFilters (double sampleRate, float formantSemitones)
{
    const auto ratio = std::pow (2.0f, formantSemitones / 12.0f);
    const auto q = 1.8f;
    const auto gain = 3.5f; // dB of each formant peak

    const float bases[3] = { 500.0f, 1500.0f, 2500.0f };

    for (int i = 0; i < 3; ++i)
    {
        const auto hz = juce::jlimit (80.0f, (float) (sampleRate * 0.45), bases[i] * ratio);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, hz, q, dbToGain (gain));
        *formantL[(size_t) i].coefficients = *coeffs;
        *formantR[(size_t) i].coefficients = *coeffs;
    }

    lastFormant = formantSemitones;
}

void ChoirEngine::process (juce::AudioBuffer<float>& buffer, const ChoirSettings& settings)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numCh = buffer.getNumChannels();

    if (numSamples <= 0 || numCh <= 0)
        return;

    if (! settings.enabled)
        return;

    const auto wetMix = juce::jlimit (0.0f, 1.0f, settings.dryWet);
    dryWetSmooth.setTargetValue (wetMix);
    gainSmooth.setTargetValue (dbToGain (settings.outputGainDb));
    widthSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, settings.width));

    if (wetMix <= 0.0001f && dryWetSmooth.getCurrentValue() <= 0.0001f)
    {
        const auto g = gainSmooth.getNextValue();

        if (std::abs (g - 1.0f) > 0.0001f)
            buffer.applyGain (g);

        return;
    }

    if (std::abs (settings.formantSemitones - lastFormant) > 0.01f)
        updateFormantFilters (currentSampleRate, settings.formantSemitones);

    const auto nVoices = juce::jlimit (1, maxVoices, settings.numVoices);
    const auto voiceNorm = 0.85f / std::sqrt ((float) nVoices);
    const auto human = juce::jlimit (0.0f, 1.0f, settings.humanize);

    auto* left = buffer.getWritePointer (0);
    auto* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const auto inL = left[n];
        const auto inR = right != nullptr ? right[n] : inL;
        const auto mono = 0.5f * (inL + inR);

        if (++wanderControlCounter >= 64)
        {
            wanderControlCounter = 0;

            for (int v = 0; v < nVoices; ++v)
            {
                auto& voice = voices[(size_t) v];
                voice.samplesUntilWander -= 64;

                if (voice.samplesUntilWander <= 0)
                {
                    voice.samplesUntilWander = (int) (currentSampleRate
                                                      * (0.18 + 0.7 * voice.rng.nextDouble()));
                    voice.targetDelayMs = 4.0f + (float) v * 1.4f
                                        + human * (voice.rng.nextFloat() * 28.0f);
                    voice.targetCents = (voice.rng.nextFloat() * 2.0f - 1.0f) * human * 18.0f;
                    voice.delayMs.setTargetValue (voice.targetDelayMs);
                    voice.extraCents.setTargetValue (voice.targetCents);
                }
            }
        }

        float wetL = 0.0f, wetR = 0.0f;
        const auto width = widthSmooth.getNextValue();

        for (int v = 0; v < nVoices; ++v)
        {
            auto& voice = voices[(size_t) v];
            const auto delaySamples = juce::jlimit (
                1.0f,
                (float) voice.delay.getMaximumDelayInSamples() - 4.0f,
                voice.delayMs.getNextValue() * 0.001f * (float) currentSampleRate);

            voice.delay.pushSample (0, mono);
            const auto delayed = voice.delay.popSample (0, delaySamples);

            const auto semi = semitoneForVoice (settings.preset, v, nVoices);
            const auto cents = detuneSign (v) * settings.detuneCents
                             * (0.35f + 0.65f * ((float) v / (float) nVoices))
                             + voice.extraCents.getNextValue();
            const auto ratio = std::pow (2.0f, (semi + cents / 100.0f) / 12.0f);
            auto shifted = voice.shifter.processSample (delayed, ratio);

            const auto rawPan = nVoices == 1 ? 0.5f
                                             : (float) v / (float) (nVoices - 1);
            const auto pan = 0.5f + (rawPan - 0.5f) * width;
            float gL = 0.0f, gR = 0.0f;
            equalPowerPan (pan, gL, gR);
            wetL += shifted * gL * voiceNorm;
            wetR += shifted * gR * voiceNorm;
        }

        if (std::abs (settings.formantSemitones) > 0.05f)
        {
            for (auto& f : formantL) wetL = f.processSample (wetL);
            for (auto& f : formantR) wetR = f.processSample (wetR);
        }

        const auto mix = dryWetSmooth.getNextValue();
        const auto gain = gainSmooth.getNextValue();
        const auto outL = (inL * (1.0f - mix) + wetL * mix) * gain;
        const auto outR = (inR * (1.0f - mix) + wetR * mix) * gain;

        left[n] = outL;

        if (right != nullptr)
            right[n] = outR;
    }
}
} // namespace dt
