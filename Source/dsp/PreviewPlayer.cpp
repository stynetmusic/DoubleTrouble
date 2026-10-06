#include "PreviewPlayer.h"

#include <cmath>

namespace dt
{
void PreviewPlayer::prepare (double hostSampleRate, int samplesPerBlock)
{
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 44100.0;
    maxBlock = samplesPerBlock;
}

void PreviewPlayer::setSession (juce::AudioBuffer<float> reference, double referenceSampleRate,
                                std::vector<PreviewClip> newTakes,
                                bool keepPosition)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    ref = std::move (reference);
    takes = std::move (newTakes);
    refRate = referenceSampleRate > 0.0 ? referenceSampleRate : 44100.0;

    if (! keepPosition)
        positionSeconds.store (0.0, std::memory_order_release);
    else
        positionSeconds.store (juce::jlimit (0.0, durationSeconds(),
                                             positionSeconds.load (std::memory_order_acquire)),
                               std::memory_order_release);
}

void PreviewPlayer::clear()
{
    stop();
    const juce::SpinLock::ScopedLockType sl (lock);
    ref.setSize (0, 0);
    takes.clear();
    positionSeconds.store (0.0, std::memory_order_release);
}

void PreviewPlayer::play()
{
    if (! hasContent())
        return;

    positionSeconds.store (0.0, std::memory_order_release);
    playing.store (true, std::memory_order_release);
}

void PreviewPlayer::stop()
{
    playing.store (false, std::memory_order_release);
    positionSeconds.store (0.0, std::memory_order_release);
}

void PreviewPlayer::toggle()
{
    if (isPlaying())
        stop();
    else
        play();
}

bool PreviewPlayer::hasContent() const noexcept
{
    const juce::SpinLock::ScopedTryLockType sl (lock);

    if (! sl.isLocked())
        return true;

    if (ref.getNumSamples() > 1)
        return true;

    for (const auto& take : takes)
        if (take.buffer.getNumSamples() > 1)
            return true;

    return false;
}

void PreviewPlayer::setMix (float referenceGain, float takeG) noexcept
{
    refGain.store (referenceGain, std::memory_order_release);
    takeGain.store (takeG, std::memory_order_release);
}

double PreviewPlayer::durationSeconds() const noexcept
{
    auto len = refRate > 0.0 ? (double) ref.getNumSamples() / refRate : 0.0;

    for (const auto& take : takes)
        if (take.sampleRate > 0.0)
            len = std::max (len, (double) take.buffer.getNumSamples() / take.sampleRate);

    return len;
}

float PreviewPlayer::getProgress() const noexcept
{
    const juce::SpinLock::ScopedTryLockType sl (lock);

    if (! sl.isLocked())
        return 0.0f;

    const auto len = durationSeconds();

    if (len <= 0.0)
        return 0.0f;

    return (float) juce::jlimit (0.0, 1.0, positionSeconds.load (std::memory_order_acquire) / len);
}

float PreviewPlayer::readClip (const juce::AudioBuffer<float>& src, int ch, double samplePos) noexcept
{
    if (src.getNumSamples() <= 1 || src.getNumChannels() <= 0)
        return 0.0f;

    const auto c = juce::jmin (ch, src.getNumChannels() - 1);
    const auto n = src.getNumSamples();

    if (samplePos >= (double) (n - 1) || samplePos < 0.0)
        return 0.0f;

    const auto i0 = (int) samplePos;
    const auto i1 = std::min (n - 1, i0 + 1);
    const auto frac = (float) (samplePos - (double) i0);
    const auto* d = src.getReadPointer (c);
    return d[i0] + frac * (d[i1] - d[i0]);
}

juce::AudioBuffer<float> PreviewPlayer::copyTakeMixForDisplay() const
{
    const juce::SpinLock::ScopedLockType sl (lock);

    if (takes.empty())
        return {};

    if (takes.size() == 1)
        return takes.front().buffer;

    const auto destSR = refRate > 0.0 ? refRate : takes.front().sampleRate;
    const auto destLen = juce::jlimit (256, 8192, (int) std::llround (durationSeconds() * destSR));
    auto channels = ref.getNumChannels();

    for (const auto& take : takes)
        channels = std::max (channels, take.buffer.getNumChannels());

    channels = std::max (1, channels);
    juce::AudioBuffer<float> mix (channels, destLen);
    mix.clear();
    const auto gain = 0.85f / std::sqrt ((float) takes.size());

    for (const auto& take : takes)
    {
        const auto sr = take.sampleRate > 0.0 ? take.sampleRate : destSR;

        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < destLen; ++i)
            {
                const auto t = (double) i / destSR;
                mix.addSample (ch, i, readClip (take.buffer, ch, t * sr) * gain);
            }
    }

    return mix;
}

void PreviewPlayer::render (juce::AudioBuffer<float>& buffer)
{
    buffer.clear();

    if (! playing.load (std::memory_order_acquire))
        return;

    const juce::SpinLock::ScopedTryLockType sl (lock);

    if (! sl.isLocked())
        return;

    const auto outN = buffer.getNumSamples();
    const auto outCh = buffer.getNumChannels();
    const auto len = durationSeconds();

    if (len <= 0.0 || hostRate <= 0.0)
    {
        playing.store (false, std::memory_order_release);
        return;
    }

    const auto dt = 1.0 / hostRate;
    auto t = positionSeconds.load (std::memory_order_acquire);
    const auto gRef = refGain.load (std::memory_order_acquire);
    const auto gTake = takeGain.load (std::memory_order_acquire);
    const auto takeNorm = takes.empty() ? 0.0f
                                        : 0.85f / std::sqrt ((float) takes.size());

    for (int i = 0; i < outN; ++i)
    {
        if (t >= len)
        {
            playing.store (false, std::memory_order_release);
            break;
        }

        const auto refPos = t * refRate;

        for (int ch = 0; ch < outCh; ++ch)
        {
            auto s = readClip (ref, ch, refPos) * gRef;

            for (const auto& take : takes)
            {
                const auto sr = take.sampleRate > 0.0 ? take.sampleRate : refRate;
                s += readClip (take.buffer, ch, t * sr) * gTake * takeNorm;
            }

            buffer.setSample (ch, i, s);
        }

        t += dt;
    }

    positionSeconds.store (t, std::memory_order_release);
}
} // namespace dt
