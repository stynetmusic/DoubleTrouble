#pragma once

#include "Palette.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace dt
{
class DropZone final : public juce::Component,
                       public juce::FileDragAndDropTarget
{
public:
    DropZone (juce::String title, juce::Colour accentColour);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    void setFileName (const juce::String& name);
    void setBrowseCallback (std::function<void()> cb) { browseCallback = std::move (cb); }
    void setFilesDroppedCallback (std::function<void (const juce::StringArray&)> cb)
    {
        droppedCallback = std::move (cb);
    }

private:
    juce::String heading;
    juce::String fileName { "Drop audio, or click to browse" };
    juce::Colour accent;
    bool draggingOver = false;
    std::function<void()> browseCallback;
    std::function<void (const juce::StringArray&)> droppedCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropZone)
};
} // namespace dt
