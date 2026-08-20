#include "DropZone.h"
#include "dsp/AlignmentEngine.h"

namespace dt
{
DropZone::DropZone (juce::String title, juce::Colour accentColour)
    : heading (std::move (title)),
      accent (accentColour)
{
}

void DropZone::setFileName (const juce::String& name)
{
    fileName = name.isNotEmpty() ? name : "Drop audio, or click to browse";
    repaint();
}

void DropZone::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto fill = draggingOver ? Palette::panelHover() : Palette::panelAlt();
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (draggingOver ? accent : Palette::border());
    const float dash[2] = { 4.0f, 3.0f };
    juce::Path border;
    border.addRoundedRectangle (bounds, 8.0f);
    juce::PathStrokeType stroke (1.2f);
    stroke.createDashedStroke (border, border, dash, 2);
    g.strokePath (border, juce::PathStrokeType (1.2f));

    g.setColour (accent);
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    g.drawText (heading, bounds.removeFromTop (22.0f), juce::Justification::centred);

    g.setColour (Palette::textDim());
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText (fileName, bounds.reduced (8.0f).toNearestInt(),
                      juce::Justification::centred, 2);
}

void DropZone::resized() {}

void DropZone::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && browseCallback)
        browseCallback();
}

bool DropZone::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (AlignmentEngine::isAudioFile (juce::File (path)))
            return true;

    return false;
}

void DropZone::fileDragEnter (const juce::StringArray&, int, int)
{
    draggingOver = true;
    repaint();
}

void DropZone::fileDragExit (const juce::StringArray&)
{
    draggingOver = false;
    repaint();
}

void DropZone::filesDropped (const juce::StringArray& files, int, int)
{
    draggingOver = false;
    repaint();

    if (droppedCallback)
        droppedCallback (files);
}
} // namespace dt
