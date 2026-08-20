#pragma once

#include "Parameters.h"
#include "dsp/AlignmentEngine.h"
#include "dsp/ChoirEngine.h"
#include "dsp/PreviewPlayer.h"

class DoubleTroubleProcessor final : public juce::AudioProcessor
{
public:
    DoubleTroubleProcessor();
    ~DoubleTroubleProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.4; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    dt::AlignmentEngine& getAlignment() noexcept { return alignment; }
    dt::PreviewPlayer& getPreview() noexcept { return preview; }

    double getHostBpm() const noexcept { return hostBpm.load (std::memory_order_acquire); }
    dt::AlignSettings makeAlignSettings() const;
    dt::ChoirSettings makeChoirSettings() const;

    /** Reload preview buffers from the selected take (message thread). */
    void refreshPreviewClips();

private:
    juce::AudioProcessorValueTreeState apvts;
    dt::AlignmentEngine alignment;
    dt::ChoirEngine choir;
    dt::PreviewPlayer preview;

    std::atomic<float>* pAlignAmount   = nullptr;
    std::atomic<float>* pAlignToGrid   = nullptr;
    std::atomic<float>* pGridStrength  = nullptr;
    std::atomic<float>* pGridDivision  = nullptr;
    std::atomic<float>* pPreviewAfter  = nullptr;
    std::atomic<float>* pChoirOn       = nullptr;
    std::atomic<float>* pChoirVoices   = nullptr;
    std::atomic<float>* pChoirPreset   = nullptr;
    std::atomic<float>* pChoirDetune   = nullptr;
    std::atomic<float>* pChoirHumanize = nullptr;
    std::atomic<float>* pChoirFormant  = nullptr;
    std::atomic<float>* pChoirWidth    = nullptr;
    std::atomic<float>* pChoirDryWet   = nullptr;
    std::atomic<float>* pChoirGain     = nullptr;

    std::atomic<double> hostBpm { 120.0 };
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoubleTroubleProcessor)
};
