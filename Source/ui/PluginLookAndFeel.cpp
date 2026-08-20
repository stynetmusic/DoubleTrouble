#include "PluginLookAndFeel.h"

namespace dt
{
PluginLookAndFeel::PluginLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background());
    setColour (juce::Label::textColourId, Palette::text());
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::Slider::rotarySliderFillColourId, Palette::accentAlign());
    setColour (juce::Slider::rotarySliderOutlineColourId, Palette::track());
    setColour (juce::Slider::thumbColourId, Palette::text());
    setColour (juce::Slider::trackColourId, Palette::accentAlign());
    setColour (juce::Slider::backgroundColourId, Palette::track());
    setColour (juce::Slider::textBoxTextColourId, Palette::text());
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    setColour (juce::TextButton::buttonColourId, Palette::panelAlt());
    setColour (juce::TextButton::buttonOnColourId, Palette::accentAlign());
    setColour (juce::TextButton::textColourOffId, Palette::text());
    setColour (juce::TextButton::textColourOnId, Palette::background());

    setColour (juce::ComboBox::backgroundColourId, Palette::panelAlt());
    setColour (juce::ComboBox::textColourId, Palette::text());
    setColour (juce::ComboBox::outlineColourId, Palette::border());
    setColour (juce::ComboBox::arrowColourId, Palette::textDim());

    setColour (juce::PopupMenu::backgroundColourId, Palette::panel());
    setColour (juce::PopupMenu::textColourId, Palette::text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::panelHover());
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::accentAlign());

    setColour (juce::ListBox::backgroundColourId, Palette::panelAlt());
    setColour (juce::ListBox::outlineColourId, Palette::border());
    setColour (juce::ListBox::textColourId, Palette::text());

    setColour (juce::ProgressBar::backgroundColourId, Palette::track());
    setColour (juce::ProgressBar::foregroundColourId, Palette::accentAlign());
}

void PluginLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                          juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                            .reduced (6.0f);
    const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto lineW = 3.4f;
    const auto arcR = radius - lineW * 0.5f;

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (backgroundArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                            rotaryStartAngle, toAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    const auto pointerLen = arcR - 2.0f;
    const auto pointerInner = 6.0f;
    juce::Point<float> tip (centre.x + std::sin (toAngle) * pointerLen,
                            centre.y - std::cos (toAngle) * pointerLen);
    juce::Point<float> inner (centre.x + std::sin (toAngle) * pointerInner,
                              centre.y - std::cos (toAngle) * pointerInner);

    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.drawLine ({ inner, tip }, 2.0f);
    g.fillEllipse (centre.x - 3.0f, centre.y - 3.0f, 6.0f, 6.0f);
}

void PluginLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, style);

    auto track = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto isBar = slider.isHorizontal();

    if (isBar)
    {
        track = track.withSizeKeepingCentre (track.getWidth() - 4.0f, 6.0f);
        g.setColour (Palette::track());
        g.fillRoundedRectangle (track, 3.0f);

        auto filled = track.withWidth (juce::jmax (6.0f, sliderPos - (float) x));
        filled.setX (track.getX());
        filled.setHeight (track.getHeight());
        filled.setY (track.getY());
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (filled, 3.0f);
    }
    else
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                          minSliderPos, maxSliderPos, style, slider);
    }
}

void PluginLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour;

    if (button.getToggleState())
        colour = button.findColour (juce::TextButton::buttonOnColourId);
    else if (shouldDrawButtonAsDown)
        colour = colour.brighter (0.15f);
    else if (shouldDrawButtonAsHighlighted)
        colour = colour.brighter (0.08f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Palette::border());
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
}

void PluginLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                      int, int, int, int, juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown);
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    const auto arrow = bounds.removeFromRight (18.0f).reduced (5.0f, 8.0f);
    juce::Path p;
    p.addTriangle (arrow.getX(), arrow.getY(),
                   arrow.getRight(), arrow.getY(),
                   arrow.getCentreX(), arrow.getBottom());
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (p);
}

void PluginLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.setColour (findColour (juce::PopupMenu::backgroundColourId));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 6.0f);
    g.setColour (Palette::border());
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 6.0f, 1.0f);
}

juce::Font PluginLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jmin (14.0f, (float) buttonHeight * 0.45f));
}

juce::Font PluginLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::FontOptions (13.0f);
}

juce::Font PluginLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::FontOptions (12.0f);
}
} // namespace dt
