#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace dt
{
class RotaryKnob final : public juce::Component
{
public:
    RotaryKnob (const juce::String& title,
                juce::AudioProcessorValueTreeState& state,
                const juce::String& paramId,
                juce::Colour accent);

    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    juce::Label titleLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotaryKnob)
};
} // namespace dt
