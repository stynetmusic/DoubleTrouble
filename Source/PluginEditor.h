#pragma once

#include "PluginProcessor.h"
#include "ui/AlignmentPanel.h"
#include "ui/ChoirPanel.h"
#include "ui/PluginLookAndFeel.h"

class DoubleTroubleEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DoubleTroubleEditor (DoubleTroubleProcessor&);
    ~DoubleTroubleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    DoubleTroubleProcessor& proc;
    dt::PluginLookAndFeel lnf;
    dt::AlignmentPanel alignmentPanel;
    dt::ChoirPanel choirPanel;
    juce::Label title { {}, "DOUBLE TROUBLE" };
    juce::Label tagline { {}, "Vocal Align  ·  Choir" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoubleTroubleEditor)
};
