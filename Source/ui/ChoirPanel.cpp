#include "ChoirPanel.h"

namespace dt
{
ChoirPanel::ChoirPanel (DoubleTroubleProcessor& p)
    : enableAttach (p.getAPVTS(), IDs::choirOn, enableButton),
      voicesAttach (p.getAPVTS(), IDs::choirVoices, voicesSlider),
      detuneKnob ("Detune", p.getAPVTS(), IDs::choirDetune, Palette::accentChoir()),
      humanizeKnob ("Humanize", p.getAPVTS(), IDs::choirHumanize, Palette::accentChoir()),
      formantKnob ("Formant", p.getAPVTS(), IDs::choirFormant, Palette::accentChoir()),
      widthKnob ("Width", p.getAPVTS(), IDs::choirWidth, Palette::accentChoir()),
      dryWetKnob ("Dry/Wet", p.getAPVTS(), IDs::choirDryWet, Palette::accentChoir()),
      gainKnob ("Output", p.getAPVTS(), IDs::choirGain, Palette::accentChoir())
{
    heading.setFont (juce::FontOptions (13.0f));
    heading.setColour (juce::Label::textColourId, Palette::accentChoir());
    heading.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (heading);

    subtitle.setFont (juce::FontOptions (12.0f));
    subtitle.setColour (juce::Label::textColourId, Palette::textDim());
    addAndMakeVisible (subtitle);

    enableButton.setClickingTogglesState (true);
    enableButton.setColour (juce::TextButton::buttonOnColourId, Palette::accentChoir());
    addAndMakeVisible (enableButton);

    presetBox.addItemList (juce::StringArray { "Unison", "Choir", "Octaves", "Custom" }, 1);
    addAndMakeVisible (presetBox);
    presetAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.getAPVTS(), IDs::choirPreset, presetBox);

    voicesSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    voicesSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 36, 18);
    voicesSlider.setColour (juce::Slider::trackColourId, Palette::accentChoir());
    addAndMakeVisible (voicesSlider);

    voicesLabel.setFont (juce::FontOptions (11.0f));
    voicesLabel.setColour (juce::Label::textColourId, Palette::textDim());
    addAndMakeVisible (voicesLabel);

    addAndMakeVisible (detuneKnob);
    addAndMakeVisible (humanizeKnob);
    addAndMakeVisible (formantKnob);
    addAndMakeVisible (widthKnob);
    addAndMakeVisible (dryWetKnob);
    addAndMakeVisible (gainKnob);
}

void ChoirPanel::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel());
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (Palette::border());
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
}

void ChoirPanel::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (22);
    heading.setBounds (header.removeFromLeft (80));
    enableButton.setBounds (header.removeFromRight (56).withSizeKeepingCentre (56, 24));
    header.removeFromRight (8);
    presetBox.setBounds (header.removeFromRight (130).withSizeKeepingCentre (130, 24));
    subtitle.setBounds (header);

    r.removeFromTop (10);
    auto voicesRow = r.removeFromTop (24);
    voicesLabel.setBounds (voicesRow.removeFromLeft (52));
    voicesSlider.setBounds (voicesRow);

    r.removeFromTop (12);
    const auto knobW = r.getWidth() / 6;
    detuneKnob.setBounds (r.removeFromLeft (knobW));
    humanizeKnob.setBounds (r.removeFromLeft (knobW));
    formantKnob.setBounds (r.removeFromLeft (knobW));
    widthKnob.setBounds (r.removeFromLeft (knobW));
    dryWetKnob.setBounds (r.removeFromLeft (knobW));
    gainKnob.setBounds (r);
}
} // namespace dt
