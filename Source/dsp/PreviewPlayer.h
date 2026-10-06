#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

namespace dt
{
struct PreviewClip
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
};

/**
    Internal transport for Alignment preview.

    Advances in *seconds* so each clip plays at its own sample rate.
    Supports mixing every loaded take, or soloing one of them.
*/
class PreviewPlayer
{
public:
    void prepare (double hostSampleRate, int samplesPerBlock);

    void setSession (juce::AudioBuffer<float> reference, double referenceSampleRate,
                     std::vector<PreviewClip> takes,
                     bool keepPosition = false);
    void clear();

    void play();
    void stop();
    void toggle();
    bool isPlaying() const noexcept { return playing.load (std::memory_order_acquire); }
    bool hasContent() const noexcept;

    void setMix (float referenceGain, float takeGain) noexcept;

    float getProgress() const noexcept;

    /** Sum of currently loaded takes, for the waveform lane. */
    juce::AudioBuffer<float> copyTakeMixForDisplay() const;

    void render (juce::AudioBuffer<float>& buffer);

private:
    double durationSeconds() const noexcept;
    static float readClip (const juce::AudioBuffer<float>& src, int ch, double samplePos) noexcept;

    mutable juce::SpinLock lock;
    juce::AudioBuffer<float> ref;
    std::vector<PreviewClip> takes;
    double refRate = 44100.0;
    double hostRate = 44100.0;

    std::atomic<bool> playing { false };
    std::atomic<float> refGain { 0.7f };
    std::atomic<float> takeGain { 0.9f };
    std::atomic<double> positionSeconds { 0.0 };
    int maxBlock = 512;
};
} // namespace dt
