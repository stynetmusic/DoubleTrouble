#include "PreviewPlayer.h"

namespace dt
{
void PreviewPlayer::prepare (double hostSampleRate, int samplesPerBlock)
{
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 44100.0;
    maxBlock = samplesPerBlock;
}

void PreviewPlayer::setClips (juce::AudioBuffer<float> reference,
                              juce::AudioBuffer<float> takeBuffer,
                              double fileSampleRate)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    ref = std::move (reference);
    take = std::move (takeBuffer);
    fileRate = fileSampleRate > 0.0 ? fileSampleRate : 44100.0;
    position.store (0.0, std::memory_order_release);
}

void PreviewPlayer::clear()
{
    stop();
    const juce::SpinLock::ScopedLockType sl (lock);
    ref.setSize (0, 0);
    take.setSize (0, 0);
    position.store (0.0, std::memory_order_release);
}

void PreviewPlayer::play()
{
    position.store (0.0, std::memory_order_release);
    playing.store (true, std::memory_order_release);
}

void PreviewPlayer::stop()
{
    playing.store (false, std::memory_order_release);
    position.store (0.0, std::memory_order_release);
}

void PreviewPlayer::toggle()
{
    if (isPlaying())
        stop();
    else
        play();
}

void PreviewPlayer::setMix (float referenceGain, float takeG) noexcept
{
    refGain.store (referenceGain, std::memory_order_release);
    takeGain.store (takeG, std::memory_order_release);
}

float PreviewPlayer::getProgress() const noexcept
{
    const juce::SpinLock::ScopedTryLockType sl (lock);

    if (! sl.isLocked())
        return 0.0f;

    const auto len = (double) std::max (ref.getNumSamples(), take.getNumSamples());

    if (len <= 0.0)
        return 0.0f;

    return (float) juce::jlimit (0.0, 1.0, position.load (std::memory_order_acquire) / len);
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
    const auto len = (double) std::max (ref.getNumSamples(), take.getNumSamples());

    if (len <= 1.0 || hostRate <= 0.0 || fileRate <= 0.0)
    {
        playing.store (false, std::memory_order_release);
        return;
    }

    const auto step = fileRate / hostRate;
    auto pos = position.load (std::memory_order_acquire);
    const auto gRef = refGain.load (std::memory_order_acquire);
    const auto gTake = takeGain.load (std::memory_order_acquire);

    auto read = [] (const juce::AudioBuffer<float>& src, int ch, double p) -> float
    {
        if (src.getNumSamples() <= 1 || src.getNumChannels() <= 0)
            return 0.0f;

        const auto c = juce::jmin (ch, src.getNumChannels() - 1);
        const auto n = src.getNumSamples();

        if (p >= (double) (n - 1) || p < 0.0)
            return 0.0f;

        const auto i0 = (int) p;
        const auto i1 = std::min (n - 1, i0 + 1);
        const auto frac = (float) (p - (double) i0);
        const auto* d = src.getReadPointer (c);
        return d[i0] + frac * (d[i1] - d[i0]);
    };

    for (int i = 0; i < outN; ++i)
    {
        if (pos >= len)
        {
            playing.store (false, std::memory_order_release);
            break;
        }

        for (int ch = 0; ch < outCh; ++ch)
        {
            const auto s = read (ref, ch, pos) * gRef + read (take, ch, pos) * gTake;
            buffer.setSample (ch, i, s);
        }

        pos += step;
    }

    position.store (pos, std::memory_order_release);
}
} // namespace dt
