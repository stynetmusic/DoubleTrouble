#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <vector>

namespace dt
{
struct AudioClip
{
    juce::File file;
    juce::String name;
    juce::AudioBuffer<float> original;
    juce::AudioBuffer<float> aligned;
    double sampleRate = 44100.0;         // original file rate
    double alignedSampleRate = 44100.0;  // rate of `aligned` (reference rate after Align)
    bool hasAligned = false;
    int globalOffsetSamples = 0; // take is this many samples late vs reference
};

struct AlignSettings
{
    float amount = 1.0f;       // 0..1
    bool alignToGrid = false;
    float gridStrength = 0.8f; // 0..1
    double bpm = 120.0;
    double beatsPerDivision = 0.25; // 1/16 = 0.25 beat
};

/**
    Offline vocal alignment.

    Jobs (load / align / export) run on a worker thread. Buffers that the
    audio thread reads (preview) are swapped under a SpinLock; never hold
    that lock for more than a pointer swap.
*/
class AlignmentEngine final : public juce::Thread,
                              public juce::ChangeBroadcaster
{
public:
    AlignmentEngine();
    ~AlignmentEngine() override;

    void loadReference (const juce::File& file);
    void addTake (const juce::File& file);
    void removeTake (int index);
    void clearAll();
    bool containsTake (const juce::File& file) const;

    void requestAlign (AlignSettings settings);
    void requestExport (int takeIndex, const juce::File& destWav);
    void requestBounceMix (const juce::File& destWav, bool preferAligned, bool includeReference);

    juce::File getBounceFile() const;
    bool isBounceReady() const;

    bool isBusy() const noexcept { return busy.load (std::memory_order_acquire); }
    float getProgress() const noexcept { return progress.load (std::memory_order_acquire); }
    juce::String getStatus() const;

    bool hasReference() const;
    int getNumTakes() const;
    int getSelectedTake() const noexcept { return selectedTake.load (std::memory_order_acquire); }
    void setSelectedTake (int index);

    juce::String getReferenceName() const;
    juce::String getTakeName (int index) const;
    juce::StringArray getTakeDisplayNames() const;
    juce::File getReferenceFile() const;
    juce::File getTakeFile (int index) const;

    /** Copies used by the editor (waveforms) and preview player.
        Safe to call from the message thread. */
    juce::AudioBuffer<float> copyReference() const;
    juce::AudioBuffer<float> copyTakeOriginal (int index) const;
    juce::AudioBuffer<float> copyTakeAligned (int index) const;

    /** Prefers an audible aligned buffer; otherwise the original take.
        Writes the sample rate of the returned buffer into `sampleRate`. */
    juce::AudioBuffer<float> copyTakePlayback (int index, bool preferAligned,
                                               double& sampleRate) const;

    double getReferenceSampleRate() const;
    double getTakeSampleRate (int index) const;
    double getTakeAlignedSampleRate (int index) const;
    bool takeHasAligned (int index) const;

    juce::SpinLock& getBufferLock() noexcept { return bufferLock; }

    /** Direct pointers — caller MUST hold bufferLock. */
    const AudioClip* getReferenceLocked() const noexcept { return reference.get(); }
    const AudioClip* getTakeLocked (int index) const noexcept;

    juce::AudioFormatManager& getFormats() noexcept { return formats; }

    static bool isAudioFile (const juce::File& file);

private:
    enum class JobType
    {
        loadReference,
        loadTake,
        align,
        exportTake,
        bounceMix
    };

    struct Job
    {
        JobType type = JobType::align;
        juce::File file;
        int takeIndex = -1;
        AlignSettings settings;
        bool preferAligned = true;
        bool includeReference = false;
    };

    void run() override;
    void execute (const Job& job);
    bool decodeFile (const juce::File& file, AudioClip& clip, juce::String& error);
    void runAlignment (const AlignSettings& settings);
    bool alignOneTake (AudioClip& take, const AudioClip& ref, const AlignSettings& settings,
                       juce::String& error);
    bool writeWav (const AudioClip& take, const juce::File& dest, juce::String& error);
    bool writeBufferWav (const juce::AudioBuffer<float>& buffer, double sampleRate,
                         const juce::File& dest, juce::String& error);
    void runBounceMix (const Job& job);
    void bumpContentEpoch() noexcept;

    juce::AudioFormatManager formats;
    mutable juce::SpinLock bufferLock;
    juce::CriticalSection jobLock;
    mutable juce::CriticalSection statusLock;

    std::unique_ptr<AudioClip> reference;
    std::vector<std::unique_ptr<AudioClip>> takes;

    std::vector<Job> jobs;
    std::atomic<bool> busy { false };
    std::atomic<float> progress { 0.0f };
    std::atomic<int> selectedTake { 0 };
    std::atomic<int> contentEpoch { 0 };
    std::atomic<int> bounceEpoch { -1 };
    juce::File bounceFile;
    juce::String status { "Drop a reference vocal to begin." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignmentEngine)
};
} // namespace dt
