#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace dt
{
/**
    Internal transport for Alignment preview.

    Renders reference + take (dry or aligned) into the host buffer, resampling
    from the file rate to the host rate with linear interpolation.
*/
class PreviewPlayer
{
public:
    void prepare (double hostSampleRate, int samplesPerBlock);
    void setClips (juce::AudioBuffer<float> reference,
                   juce::AudioBuffer<float> take,
                   double fileSampleRate);
    void clear();

    void play();
    void stop();
    void toggle();
    bool isPlaying() const noexcept { return playing.load (std::memory_order_acquire); }

    void setUseAligned (bool shouldUse) noexcept { useAligned.store (shouldUse, std::memory_order_release); }
    void setMix (float referenceGain, float takeGain) noexcept;

    /** 0..1 playback progress, for the playhead. */
    float getProgress() const noexcept;

    /** Called from the audio thread. Overwrites `buffer` when playing. */
    void render (juce::AudioBuffer<float>& buffer);

private:
    juce::SpinLock lock;
    juce::AudioBuffer<float> ref;
    juce::AudioBuffer<float> take;
    double fileRate = 44100.0;
    double hostRate = 44100.0;

    std::atomic<bool> playing { false };
    std::atomic<bool> useAligned { true };
    std::atomic<float> refGain { 0.7f };
    std::atomic<float> takeGain { 0.9f };
    std::atomic<double> position { 0.0 }; // in file-rate samples
    int maxBlock = 512;
};
} // namespace dt
