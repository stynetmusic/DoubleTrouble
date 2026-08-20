#pragma once

#include "Palette.h"
#include "RotaryKnob.h"
#include "PluginProcessor.h"

namespace dt
{
class ChoirPanel final : public juce::Component
{
public:
    explicit ChoirPanel (DoubleTroubleProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label heading { {}, "CHOIR" };
    juce::Label subtitle { {}, "Generate 4–12 voices from one input" };

    juce::TextButton enableButton { "On" };
    juce::ComboBox presetBox;
    juce::Slider voicesSlider;
    juce::Label voicesLabel { {}, "Voices" };

    juce::AudioProcessorValueTreeState::ButtonAttachment enableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttach;
    juce::AudioProcessorValueTreeState::SliderAttachment voicesAttach;

    RotaryKnob detuneKnob;
    RotaryKnob humanizeKnob;
    RotaryKnob formantKnob;
    RotaryKnob widthKnob;
    RotaryKnob dryWetKnob;
    RotaryKnob gainKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirPanel)
};
} // namespace dt
