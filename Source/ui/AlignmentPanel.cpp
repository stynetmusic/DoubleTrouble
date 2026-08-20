#include "AlignmentPanel.h"

namespace dt
{
AlignmentPanel::AlignmentPanel (DoubleTroubleProcessor& p)
    : processor (p),
      gridAttach (p.getAPVTS(), IDs::alignToGrid, gridButton),
      afterAttach (p.getAPVTS(), IDs::previewAfter, afterButton),
      amountKnob ("Amount", p.getAPVTS(), IDs::alignAmount, Palette::accentAlign()),
      gridKnob ("Grid Strength", p.getAPVTS(), IDs::gridStrength, Palette::accentAlign()),
      progressBar (progressValue)
{
    heading.setFont (juce::FontOptions (13.0f));
    heading.setColour (juce::Label::textColourId, Palette::accentAlign());
    heading.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (heading);

    statusLabel.setFont (juce::FontOptions (12.0f));
    statusLabel.setColour (juce::Label::textColourId, Palette::textDim());
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    addAndMakeVisible (referenceDrop);
    addAndMakeVisible (takeDrop);
    addAndMakeVisible (takeList);
    takeList.setRowHeight (22);
    takeList.setColour (juce::ListBox::backgroundColourId, Palette::panelAlt());

    alignButton.setColour (juce::TextButton::buttonColourId, Palette::accentAlign());
    alignButton.setColour (juce::TextButton::textColourOffId, Palette::background());
    addAndMakeVisible (alignButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (removeTakeButton);

    beforeButton.setClickingTogglesState (true);
    afterButton.setClickingTogglesState (true);
    gridButton.setClickingTogglesState (true);
    addAndMakeVisible (beforeButton);
    addAndMakeVisible (afterButton);
    addAndMakeVisible (gridButton);

    gridDivision.addItemList (juce::StringArray { "1/4", "1/8", "1/8T", "1/16", "1/16T" }, 1);
    addAndMakeVisible (gridDivision);
    gridDivisionAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.getAPVTS(), IDs::gridDivision, gridDivision);

    addAndMakeVisible (amountKnob);
    addAndMakeVisible (gridKnob);
    addAndMakeVisible (waveform);
    addAndMakeVisible (progressBar);

    referenceDrop.setBrowseCallback ([this] { browseFor (true); });
    takeDrop.setBrowseCallback ([this] { browseFor (false); });
    referenceDrop.setFilesDroppedCallback ([this] (const juce::StringArray& f) { handleDroppedFiles (f, true); });
    takeDrop.setFilesDroppedCallback ([this] (const juce::StringArray& f) { handleDroppedFiles (f, false); });

    alignButton.onClick = [this]
    {
        processor.getAlignment().requestAlign (processor.makeAlignSettings());
    };

    playButton.onClick = [this]
    {
        processor.refreshPreviewClips();
        processor.getPreview().toggle();
        playButton.setButtonText (processor.getPreview().isPlaying() ? "Stop" : "Play");
    };

    exportButton.onClick = [this] { exportSelected(); };

    removeTakeButton.onClick = [this]
    {
        processor.getAlignment().removeTake (processor.getAlignment().getSelectedTake());
    };

    beforeButton.onClick = [this]
    {
        if (auto* param = processor.getAPVTS().getParameter (IDs::previewAfter))
            param->setValueNotifyingHost (0.0f);

        processor.refreshPreviewClips();
    };

    afterButton.onClick = [this]
    {
        if (auto* param = processor.getAPVTS().getParameter (IDs::previewAfter))
            param->setValueNotifyingHost (1.0f);

        processor.refreshPreviewClips();
    };

    processor.getAlignment().addChangeListener (this);
    startTimerHz (24);
    refreshFromEngine();
}

AlignmentPanel::~AlignmentPanel()
{
    processor.getAlignment().removeChangeListener (this);
}

void AlignmentPanel::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel());
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (Palette::border());
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
}

void AlignmentPanel::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (22);
    heading.setBounds (header.removeFromLeft (140));
    statusLabel.setBounds (header);

    r.removeFromTop (10);
    auto left = r.removeFromLeft (230);
    referenceDrop.setBounds (left.removeFromTop (72));
    left.removeFromTop (8);
    takeDrop.setBounds (left.removeFromTop (72));
    left.removeFromTop (8);
    takeList.setBounds (left.removeFromTop (juce::jmax (70, left.getHeight() - 36)));
    left.removeFromTop (8);
    removeTakeButton.setBounds (left.removeFromTop (28));

    r.removeFromLeft (14);
    auto transport = r.removeFromBottom (118);
    r.removeFromBottom (10);
    waveform.setBounds (r);

    auto knobs = transport.removeFromRight (220);
    amountKnob.setBounds (knobs.removeFromLeft (110));
    gridKnob.setBounds (knobs);

    auto row1 = transport.removeFromTop (32);
    alignButton.setBounds (row1.removeFromLeft (88));
    row1.removeFromLeft (8);
    playButton.setBounds (row1.removeFromLeft (72));
    row1.removeFromLeft (8);
    exportButton.setBounds (row1.removeFromLeft (80));
    row1.removeFromLeft (8);
    beforeButton.setBounds (row1.removeFromLeft (70));
    row1.removeFromLeft (4);
    afterButton.setBounds (row1.removeFromLeft (70));

    transport.removeFromTop (8);
    auto row2 = transport.removeFromTop (28);
    gridButton.setBounds (row2.removeFromLeft (120));
    row2.removeFromLeft (8);
    gridDivision.setBounds (row2.removeFromLeft (90));
    transport.removeFromTop (8);
    progressBar.setBounds (transport.removeFromTop (10));
}

void AlignmentPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromEngine();
}

void AlignmentPanel::timerCallback()
{
    progressValue = (double) processor.getAlignment().getProgress();
    progressBar.repaint();
    waveform.setPlayhead (processor.getPreview().getProgress());
    playButton.setButtonText (processor.getPreview().isPlaying() ? "Stop" : "Play");

    const auto after = processor.getAPVTS().getRawParameterValue (IDs::previewAfter)->load() > 0.5f;
    beforeButton.setToggleState (! after, juce::dontSendNotification);
    afterButton.setToggleState (after, juce::dontSendNotification);

    alignButton.setEnabled (! processor.getAlignment().isBusy());
}

void AlignmentPanel::refreshFromEngine()
{
    auto& engine = processor.getAlignment();
    statusLabel.setText (engine.getStatus(), juce::dontSendNotification);
    referenceDrop.setFileName (engine.getReferenceName());
    takeList.updateContent();
    takeList.selectRow (engine.getSelectedTake());

    if (engine.hasReference())
        waveform.setReference (engine.copyReference());

    const auto idx = engine.getSelectedTake();

    if (engine.getNumTakes() > 0)
    {
        const auto after = processor.getAPVTS().getRawParameterValue (IDs::previewAfter)->load() > 0.5f;
        waveform.setTake (after && engine.takeHasAligned (idx) ? engine.copyTakeAligned (idx)
                                                               : engine.copyTakeOriginal (idx));
    }

    processor.refreshPreviewClips();
}

int AlignmentPanel::getNumRows()
{
    return processor.getAlignment().getNumTakes();
}

void AlignmentPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (selected)
    {
        g.setColour (Palette::panelHover());
        g.fillRoundedRectangle (2.0f, 1.0f, (float) width - 4.0f, (float) height - 2.0f, 4.0f);
    }

    g.setColour (selected ? Palette::accentAlign() : Palette::text());
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (processor.getAlignment().getTakeName (row),
                8, 0, width - 12, height, juce::Justification::centredLeft);
}

void AlignmentPanel::selectedRowsChanged (int lastRowSelected)
{
    if (lastRowSelected >= 0)
        processor.getAlignment().setSelectedTake (lastRowSelected);
}

void AlignmentPanel::browseFor (bool isReference)
{
    chooser = std::make_unique<juce::FileChooser> (
        isReference ? "Load reference vocal" : "Load vocal take",
        juce::File{},
        "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a");

    constexpr auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::canSelectMultipleItems;

    chooser->launchAsync (flags, [this, isReference] (const juce::FileChooser& c)
    {
        juce::StringArray paths;

        for (auto& f : c.getResults())
            paths.add (f.getFullPathName());

        handleDroppedFiles (paths, isReference);
    });
}

void AlignmentPanel::handleDroppedFiles (const juce::StringArray& files, bool isReference)
{
    auto first = true;

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (! AlignmentEngine::isAudioFile (file))
            continue;

        if (isReference && first)
        {
            processor.getAlignment().loadReference (file);
            first = false;
        }
        else
        {
            processor.getAlignment().addTake (file);
        }
    }
}

void AlignmentPanel::exportSelected()
{
    const auto idx = processor.getAlignment().getSelectedTake();
    const auto name = processor.getAlignment().getTakeName (idx);

    if (name.isEmpty())
        return;

    chooser = std::make_unique<juce::FileChooser> (
        "Export aligned take",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile (name + "_aligned.wav"),
        "*.wav");

    constexpr auto flags = juce::FileBrowserComponent::saveMode
                         | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [this, idx] (const juce::FileChooser& c)
    {
        auto file = c.getResult();

        if (file == juce::File{})
            return;

        if (file.getFileExtension().isEmpty())
            file = file.withFileExtension ("wav");

        processor.getAlignment().requestExport (idx, file);
    });
}
} // namespace dt
