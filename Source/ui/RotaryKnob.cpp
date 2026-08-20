#include "RotaryKnob.h"
#include "Palette.h"

namespace dt
{
RotaryKnob::RotaryKnob (const juce::String& title,
                        juce::AudioProcessorValueTreeState& state,
                        const juce::String& paramId,
                        juce::Colour accent)
    : attachment (state, paramId, slider)
{
    titleLabel.setText (title, juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, Palette::textDim());
    titleLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (titleLabel);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::trackColourId, accent);
    addAndMakeVisible (slider);
}

void RotaryKnob::resized()
{
    auto r = getLocalBounds();
    titleLabel.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}
} // namespace dt
