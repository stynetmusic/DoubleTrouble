#pragma once

#include "DropZone.h"
#include "RotaryKnob.h"
#include "WaveformView.h"
#include "PluginProcessor.h"

namespace dt
{
class AlignmentPanel final : public juce::Component,
                             public juce::ChangeListener,
                             public juce::Timer,
                             public juce::ListBoxModel
{
public:
    explicit AlignmentPanel (DoubleTroubleProcessor&);
    ~AlignmentPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;

private:
    void refreshFromEngine();
    void browseFor (bool isReference);
    void handleDroppedFiles (const juce::StringArray& files, bool isReference);
    void exportSelected();

    DoubleTroubleProcessor& processor;
    juce::Label heading { {}, "ALIGNMENT" };
    juce::Label statusLabel;

    DropZone referenceDrop { "REFERENCE", Palette::accentAlign() };
    DropZone takeDrop { "TAKES", Palette::waveformTake() };
    juce::ListBox takeList { "Takes", this };

    juce::TextButton alignButton { "Align" };
    juce::TextButton playButton { "Play" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton beforeButton { "Before" };
    juce::TextButton afterButton { "After" };
    juce::TextButton gridButton { "Align to Grid" };
    juce::TextButton removeTakeButton { "Remove" };

    juce::ComboBox gridDivision;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridDivisionAttach;
    juce::AudioProcessorValueTreeState::ButtonAttachment gridAttach;
    juce::AudioProcessorValueTreeState::ButtonAttachment afterAttach;

    RotaryKnob amountKnob;
    RotaryKnob gridKnob;

    WaveformView waveform;
    juce::ProgressBar progressBar;
    double progressValue = 0.0;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignmentPanel)
};
} // namespace dt
