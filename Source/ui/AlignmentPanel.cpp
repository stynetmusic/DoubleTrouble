#include "AlignmentPanel.h"

namespace dt
{
void DropHandle::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    auto fill = Palette::accentDrop();

    if (busy)
        fill = fill.withMultipliedAlpha (0.45f);
    else if (ready)
        fill = fill.brighter (0.08f);
    else
        fill = fill.darker (0.15f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Palette::background().withAlpha (0.35f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    g.setColour (Palette::background());
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (busy ? "…" : (ready ? "DROP" : "DROP"),
                getLocalBounds(), juce::Justification::centred);
}

void DropHandle::mouseDown (const juce::MouseEvent&)
{
    dragStarted = false;
}

void DropHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || busy)
        return;

    if (e.getDistanceFromDragStart() < 6)
        return;

    if (! ready)
    {
        if (onPrepare)
            onPrepare();

        return;
    }

    if (getFile == nullptr)
        return;

    const auto file = getFile();

    if (! file.existsAsFile())
        return;

    dragStarted = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        juce::StringArray { file.getFullPathName() }, false, this);
}

void DropHandle::mouseUp (const juce::MouseEvent& e)
{
    if (dragStarted || busy)
        return;

    if (e.mouseWasClicked() && onPrepare)
        onPrepare();
}

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

    takesHeader.setFont (juce::FontOptions (11.0f));
    takesHeader.setColour (juce::Label::textColourId, Palette::waveformTake());
    takesHeader.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (takesHeader);

    listenAllButton.setClickingTogglesState (true);
    listenSoloButton.setClickingTogglesState (true);
    listenAllButton.setRadioGroupId (1001);
    listenSoloButton.setRadioGroupId (1001);
    listenAllButton.setColour (juce::TextButton::buttonOnColourId, Palette::accentAlign());
    listenSoloButton.setColour (juce::TextButton::buttonOnColourId, Palette::waveformTake());
    listenAllButton.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (listenAllButton);
    addAndMakeVisible (listenSoloButton);

    listenAllButton.onClick = [this]
    {
        processor.setPreviewSoloTake (false);
        processor.refreshPreviewClips (true);
        refreshFromEngine();
    };

    listenSoloButton.onClick = [this]
    {
        processor.setPreviewSoloTake (true);
        processor.refreshPreviewClips (true);
        refreshFromEngine();
    };

    takeList.setModel (this);
    takeList.setRowHeight (26);
    takeList.setOutlineThickness (1);
    takeList.setMultipleSelectionEnabled (false);
    takeList.setColour (juce::ListBox::backgroundColourId, Palette::panelAlt());
    takeList.setColour (juce::ListBox::outlineColourId, Palette::border());
    takeList.setColour (juce::ListBox::textColourId, Palette::text());

    if (auto* viewport = takeList.getViewport())
        viewport->setScrollBarsShown (true, false);

    addAndMakeVisible (takeList);

    alignButton.setColour (juce::TextButton::buttonColourId, Palette::accentAlign());
    alignButton.setColour (juce::TextButton::textColourOffId, Palette::background());
    addAndMakeVisible (alignButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (exportButton);
    dropHandle.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    dropHandle.onPrepare = [this] { requestDropBounce(); };
    dropHandle.getFile = [this] { return processor.getAlignment().getBounceFile(); };
    addAndMakeVisible (dropHandle);
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
        processor.getPreview().stop();
        processor.getAlignment().requestAlign (processor.makeAlignSettings());
    };

    playButton.onClick = [this]
    {
        if (processor.getAlignment().isBusy())
            return;

        if (processor.getPreview().isPlaying())
        {
            processor.getPreview().stop();
        }
        else
        {
            processor.refreshPreviewClips();
            processor.getPreview().play();
        }

        playButton.setButtonText (processor.getPreview().isPlaying() ? "Stop" : "Play");
    };

    exportButton.onClick = [this] { exportSelected(); };

    removeTakeButton.onClick = [this]
    {
        auto row = takeList.getSelectedRow();

        if (row < 0)
            row = processor.getAlignment().getSelectedTake();

        if (row >= 0 && row < processor.getAlignment().getNumTakes())
            processor.getAlignment().removeTake (row);
    };

    beforeButton.onClick = [this]
    {
        if (auto* param = processor.getAPVTS().getParameter (IDs::previewAfter))
            param->setValueNotifyingHost (0.0f);

        processor.refreshPreviewClips (true);
    };

    afterButton.onClick = [this]
    {
        if (auto* param = processor.getAPVTS().getParameter (IDs::previewAfter))
            param->setValueNotifyingHost (1.0f);

        processor.refreshPreviewClips (true);
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
    auto left = r.removeFromLeft (240);
    referenceDrop.setBounds (left.removeFromTop (58));
    left.removeFromTop (8);
    takeDrop.setBounds (left.removeFromTop (48));
    left.removeFromTop (8);
    auto listHeader = left.removeFromTop (22);
    takesHeader.setBounds (listHeader.removeFromLeft (listHeader.getWidth() - 108));
    listenSoloButton.setBounds (listHeader.removeFromRight (50).reduced (0, 1));
    listHeader.removeFromRight (4);
    listenAllButton.setBounds (listHeader.removeFromRight (50).reduced (0, 1));
    left.removeFromTop (4);
    removeTakeButton.setBounds (left.removeFromBottom (28));
    left.removeFromBottom (8);
    takeList.setBounds (left);

    r.removeFromLeft (14);
    auto transport = r.removeFromBottom (118);
    r.removeFromBottom (10);
    waveform.setBounds (r);

    auto knobs = transport.removeFromRight (220);
    amountKnob.setBounds (knobs.removeFromLeft (110));
    gridKnob.setBounds (knobs);

    auto row1 = transport.removeFromTop (32);
    alignButton.setBounds (row1.removeFromLeft (72));
    row1.removeFromLeft (6);
    playButton.setBounds (row1.removeFromLeft (58));
    row1.removeFromLeft (6);
    exportButton.setBounds (row1.removeFromLeft (66));
    row1.removeFromLeft (6);
    dropHandle.setBounds (row1.removeFromLeft (78));
    row1.removeFromLeft (6);
    beforeButton.setBounds (row1.removeFromLeft (58));
    row1.removeFromLeft (4);
    afterButton.setBounds (row1.removeFromLeft (58));

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

    const auto busy = processor.getAlignment().isBusy();
    alignButton.setEnabled (! busy);
    playButton.setEnabled (! busy);
    exportButton.setEnabled (! busy);
    dropHandle.busy = busy;
    dropHandle.ready = processor.getAlignment().isBounceReady();
    dropHandle.repaint();

    // ChangeBroadcaster callbacks can coalesce; catch the busy→idle edge here
    // so the take waveform / preview is always restored after Align.
    if (wasBusy && ! busy)
        refreshFromEngine();

    wasBusy = busy;

    if (processor.getAlignment().getNumTakes() != cachedTakeNames.size())
        refreshFromEngine();
}

void AlignmentPanel::refreshFromEngine()
{
    auto& engine = processor.getAlignment();
    statusLabel.setText (engine.getStatus(), juce::dontSendNotification);
    referenceDrop.setFileName (engine.getReferenceName());

    const auto numTakes = engine.getNumTakes();
    const auto idx = juce::jlimit (0, std::max (0, numTakes - 1), engine.getSelectedTake());

    cachedTakeNames = engine.getTakeDisplayNames();
    takesHeader.setText (numTakes == 0 ? "TAKES"
                                       : "TAKES  ·  " + juce::String (numTakes),
                         juce::dontSendNotification);
    takeDrop.setFileName (numTakes == 0 ? "Drop takes, or click to browse"
                                        : "Drop to add more  ·  " + juce::String (numTakes)
                                              + (numTakes == 1 ? " loaded" : " loaded"));

    takeList.updateContent();
    takeList.repaint();

    if (numTakes > 0)
    {
        if (takeList.getSelectedRow() != idx)
            takeList.selectRow (idx, false, true);
    }
    else
    {
        takeList.deselectAllRows();
    }

    if (engine.isBusy())
        return;

    if (engine.hasReference())
        waveform.setReference (engine.copyReference());

    processor.refreshPreviewClips();

    listenAllButton.setToggleState (! processor.isPreviewSoloTake(), juce::dontSendNotification);
    listenSoloButton.setToggleState (processor.isPreviewSoloTake(), juce::dontSendNotification);

    if (numTakes > 0)
    {
        if (processor.isPreviewSoloTake())
        {
            const auto after = processor.getAPVTS().getRawParameterValue (IDs::previewAfter)->load() > 0.5f;
            double takeSR = 44100.0;
            waveform.setTake (engine.copyTakePlayback (idx, after, takeSR));
        }
        else
        {
            waveform.setTake (processor.getPreview().copyTakeMixForDisplay());
        }
    }
    else
    {
        waveform.setTake ({});
    }
}

int AlignmentPanel::getNumRows()
{
    return cachedTakeNames.size();
}

void AlignmentPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, cachedTakeNames.size()))
        return;

    if (selected)
    {
        g.setColour (Palette::panelHover());
        g.fillRoundedRectangle (2.0f, 1.0f, (float) width - 4.0f, (float) height - 2.0f, 4.0f);
    }

    g.setColour (selected ? Palette::accentAlign() : Palette::textDim());
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (row + 1), 6, 0, 18, height, juce::Justification::centredLeft);

    g.setColour (selected ? Palette::accentAlign() : Palette::text());
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (cachedTakeNames[row], 26, 0, width - 32, height,
                juce::Justification::centredLeft, true);
}

void AlignmentPanel::selectedRowsChanged (int lastRowSelected)
{
    if (lastRowSelected < 0)
        return;

    if (lastRowSelected != processor.getAlignment().getSelectedTake())
        processor.getAlignment().setSelectedTake (lastRowSelected);
}

void AlignmentPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0)
        return;

    processor.getAlignment().setSelectedTake (row);
    processor.setPreviewSoloTake (true);
    listenSoloButton.setToggleState (true, juce::dontSendNotification);
    listenAllButton.setToggleState (false, juce::dontSendNotification);

    if (! processor.getAlignment().isBusy())
        processor.refreshPreviewClips (true);
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
    processor.getPreview().stop();
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
        else if (! processor.getAlignment().containsTake (file))
        {
            processor.getAlignment().addTake (file);
        }
    }
}

void AlignmentPanel::requestDropBounce()
{
    auto& engine = processor.getAlignment();

    if (engine.isBusy())
        return;

    if (engine.getNumTakes() <= 0)
    {
        statusLabel.setText ("Load takes before using Drop.", juce::dontSendNotification);
        return;
    }

    if (engine.isBounceReady())
    {
        statusLabel.setText ("DROP ready — drag the Drop button onto a DAW track",
                             juce::dontSendNotification);
        return;
    }

    processor.getPreview().stop();
    const auto after = processor.getAPVTS().getRawParameterValue (IDs::previewAfter)->load() > 0.5f;
    const auto dest = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("DoubleTrouble_mix_"
                                         + juce::String (juce::Time::getMillisecondCounter())
                                         + ".wav");
    engine.requestBounceMix (dest, after, false);
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
