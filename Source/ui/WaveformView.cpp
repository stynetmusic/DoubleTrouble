#include "WaveformView.h"

namespace dt
{
void WaveformView::Peaks::build (const juce::AudioBuffer<float>& buffer, int numBins)
{
    minv.clear();
    maxv.clear();

    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0 || numBins <= 4)
        return;

    minv.resize ((size_t) numBins, 0.0f);
    maxv.resize ((size_t) numBins, 0.0f);

    const auto n = buffer.getNumSamples();
    const auto ch = buffer.getNumChannels();

    for (int b = 0; b < numBins; ++b)
    {
        const auto start = (int) ((juce::int64) b * (juce::int64) n / (juce::int64) numBins);
        const auto end = (int) ((juce::int64) (b + 1) * (juce::int64) n / (juce::int64) numBins);
        auto lo = 0.0f, hi = 0.0f;

        for (int i = start; i < end; ++i)
        {
            auto sample = 0.0f;

            for (int c = 0; c < ch; ++c)
                sample += buffer.getSample (c, i);

            sample /= (float) ch;
            lo = std::min (lo, sample);
            hi = std::max (hi, sample);
        }

        minv[(size_t) b] = lo;
        maxv[(size_t) b] = hi;
    }
}

void WaveformView::setReference (const juce::AudioBuffer<float>& buffer)
{
    refSource = buffer;
    cachedWidth = 0;
    rebuildIfNeeded();
    repaint();
}

void WaveformView::setTake (const juce::AudioBuffer<float>& buffer)
{
    takeSource = buffer;
    cachedWidth = 0;
    rebuildIfNeeded();
    repaint();
}

void WaveformView::setPlayhead (float progress01)
{
    playhead = juce::jlimit (0.0f, 1.0f, progress01);
    repaint();
}

void WaveformView::clear()
{
    refSource.setSize (0, 0);
    takeSource.setSize (0, 0);
    referencePeaks = {};
    takePeaks = {};
    playhead = 0.0f;
    repaint();
}

void WaveformView::rebuildIfNeeded()
{
    const auto w = std::max (8, getWidth());

    if (w == cachedWidth)
        return;

    cachedWidth = w;
    referencePeaks.build (refSource, w);
    takePeaks.build (takeSource, w);
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::panelAlt());
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Palette::border());
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    rebuildIfNeeded();

    auto inner = bounds.reduced (8.0f, 10.0f);

    if (referencePeaks.empty() && takePeaks.empty())
    {
        g.setColour (Palette::textDim());
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Waveforms appear here after you load audio",
                    inner.toNearestInt(), juce::Justification::centred);
        return;
    }

    auto refArea = inner.removeFromTop (inner.getHeight() * 0.46f);
    inner.removeFromTop (8.0f);
    auto takeArea = inner;

    auto drawLane = [&g] (const Peaks& peaks, juce::Rectangle<float> area,
                          juce::Colour colour, const juce::String& emptyLabel)
    {
        g.setColour (colour.withAlpha (0.12f));
        g.fillRoundedRectangle (area, 4.0f);

        if (peaks.empty())
        {
            g.setColour (Palette::textDim());
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (emptyLabel, area.toNearestInt(), juce::Justification::centred);
            return;
        }

        auto peak = 0.0f;

        for (size_t i = 0; i < peaks.maxv.size(); ++i)
            peak = std::max (peak, std::max (std::abs (peaks.maxv[i]), std::abs (peaks.minv[i])));

        if (peak < 1.0e-5f)
        {
            g.setColour (Palette::textDim());
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (emptyLabel, area.toNearestInt(), juce::Justification::centred);
            return;
        }

        const auto mid = area.getCentreY();
        const auto amp = area.getHeight() * 0.46f / peak;
        const auto n = (int) peaks.maxv.size();

        juce::Path path;
        path.preallocateSpace (n * 2 + 8);
        path.startNewSubPath (area.getX(), mid);

        for (int i = 0; i < n; ++i)
        {
            const auto x = area.getX() + (float) i / (float) std::max (1, n - 1) * area.getWidth();
            path.lineTo (x, mid - peaks.maxv[(size_t) i] * amp);
        }

        for (int i = n - 1; i >= 0; --i)
        {
            const auto x = area.getX() + (float) i / (float) std::max (1, n - 1) * area.getWidth();
            path.lineTo (x, mid - peaks.minv[(size_t) i] * amp);
        }

        path.closeSubPath();
        g.setColour (colour.withAlpha (0.9f));
        g.fillPath (path);
    };

    drawLane (referencePeaks, refArea, Palette::waveformRef(), "Reference");
    drawLane (takePeaks, takeArea, Palette::waveformTake(), "Take");

    if (playhead > 0.0f)
    {
        const auto x = bounds.getX() + 8.0f + playhead * (bounds.getWidth() - 16.0f);
        g.setColour (Palette::playhead().withAlpha (0.85f));
        g.drawLine (x, bounds.getY() + 8.0f, x, bounds.getBottom() - 8.0f, 1.2f);
    }
}
} // namespace dt
