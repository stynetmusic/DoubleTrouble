#pragma once

#include <juce_graphics/juce_graphics.h>

namespace dt
{
struct Palette
{
    static juce::Colour background()    { return juce::Colour (0xff0b0c10); }
    static juce::Colour panel()         { return juce::Colour (0xff14161c); }
    static juce::Colour panelAlt()      { return juce::Colour (0xff1b1e27); }
    static juce::Colour panelHover()    { return juce::Colour (0xff222633); }
    static juce::Colour border()        { return juce::Colour (0xff2c3140); }
    static juce::Colour text()          { return juce::Colour (0xffe8eaed); }
    static juce::Colour textDim()       { return juce::Colour (0xff8b90a0); }
    static juce::Colour accentAlign()   { return juce::Colour (0xff3ddbd9); }
    static juce::Colour accentChoir()   { return juce::Colour (0xffe08b4f); }
    static juce::Colour danger()        { return juce::Colour (0xffe05c5c); }
    static juce::Colour track()         { return juce::Colour (0xff2a2e3c); }
    static juce::Colour waveformRef()   { return juce::Colour (0xff3ddbd9); }
    static juce::Colour waveformTake()  { return juce::Colour (0xffe08b4f); }
    static juce::Colour playhead()      { return juce::Colour (0xfff2f4f8); }
};
} // namespace dt
