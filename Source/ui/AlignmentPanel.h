#pragma once

#include "DropZone.h"
#include "RotaryKnob.h"
#include "WaveformView.h"
#include "PluginProcessor.h"

namespace dt
{
/** Mixes takes to a temp WAV, then drags that file into the host DAW. */
class DropHandle final : public juce::Component
{
public:
    std::function<void()> onPrepare;
    std::function<juce::File()> getFile;
    bool ready = false;
    bool busy = false;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool dragStarted = false;
};

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
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

private:
    void refreshFromEngine();
    void browseFor (bool isReference);
    void handleDroppedFiles (const juce::StringArray& files, bool isReference);
    void exportSelected();
    void requestDropBounce();

    DoubleTroubleProcessor& processor;
    juce::Label heading { {}, "ALIGNMENT" };
    juce::Label statusLabel;

    DropZone referenceDrop { "REFERENCE", Palette::accentAlign() };
    DropZone takeDrop { "ADD TAKES", Palette::waveformTake() };
    juce::Label takesHeader { {}, "TAKES" };
    juce::TextButton listenAllButton { "All" };
    juce::TextButton listenSoloButton { "Solo" };
    juce::ListBox takeList;

    juce::TextButton alignButton { "Align" };
    juce::TextButton playButton { "Play" };
    juce::TextButton exportButton { "Export" };
    DropHandle dropHandle;
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
    bool wasBusy = false;
    juce::StringArray cachedTakeNames;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignmentPanel)
};
} // namespace dt
