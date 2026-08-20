#pragma once

#include "Palette.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace dt
{
class WaveformView final : public juce::Component
{
public:
    WaveformView() = default;

    void setReference (const juce::AudioBuffer<float>& buffer);
    void setTake (const juce::AudioBuffer<float>& buffer);
    void setPlayhead (float progress01);
    void clear();

    void paint (juce::Graphics&) override;

private:
    struct Peaks
    {
        std::vector<float> minv, maxv;
        void build (const juce::AudioBuffer<float>& buffer, int numBins);
        bool empty() const { return minv.empty(); }
    };

    Peaks referencePeaks, takePeaks;
    float playhead = 0.0f;
    int cachedWidth = 0;

    void rebuildIfNeeded();
    juce::AudioBuffer<float> refSource, takeSource;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
} // namespace dt
