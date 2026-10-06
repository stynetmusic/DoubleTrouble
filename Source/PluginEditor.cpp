#include "PluginEditor.h"

DoubleTroubleEditor::DoubleTroubleEditor (DoubleTroubleProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      alignmentPanel (p),
      choirPanel (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setLookAndFeel (&lnf);

    title.setFont (juce::FontOptions (18.0f));
    title.setColour (juce::Label::textColourId, dt::Palette::text());
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    tagline.setFont (juce::FontOptions (12.0f));
    tagline.setColour (juce::Label::textColourId, dt::Palette::textDim());
    tagline.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (tagline);

    addAndMakeVisible (alignmentPanel);
    addAndMakeVisible (choirPanel);

    setResizable (true, true);
    setResizeLimits (980, 640, 1800, 1200);
    setSize (1180, 780);
    setWantsKeyboardFocus (true);
}

DoubleTroubleEditor::~DoubleTroubleEditor()
{
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void DoubleTroubleEditor::paint (juce::Graphics& g)
{
    g.fillAll (dt::Palette::background());

    auto header = getLocalBounds().removeFromTop (52).toFloat();
    juce::ColourGradient grad (dt::Palette::background().brighter (0.04f), header.getTopLeft(),
                               dt::Palette::background(), header.getBottomLeft(), false);
    g.setGradientFill (grad);
    g.fillRect (header);

    g.setColour (dt::Palette::accentAlign());
    g.fillRect (14, 18, 4, 18);
}

void DoubleTroubleEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (36);
    title.setBounds (header.removeFromLeft (280));
    tagline.setBounds (header.removeFromRight (220));
    r.removeFromTop (8);

    auto choir = r.removeFromBottom (juce::jmax (200, r.getHeight() * 32 / 100));
    r.removeFromBottom (12);
    alignmentPanel.setBounds (r);
    choirPanel.setBounds (choir);
}

bool DoubleTroubleEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        if (proc.getAlignment().isBusy())
            return true;

        if (proc.getPreview().isPlaying())
            proc.getPreview().stop();
        else
        {
            proc.refreshPreviewClips();
            proc.getPreview().play();
        }

        return true;
    }

    return false;
}
