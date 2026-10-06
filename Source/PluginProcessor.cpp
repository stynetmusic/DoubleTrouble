#include "PluginProcessor.h"
#include "PluginEditor.h"

DoubleTroubleProcessor::DoubleTroubleProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", dt::createParameterLayout())
{
    auto grab = [this] (const char* id) -> std::atomic<float>*
    {
        return apvts.getRawParameterValue (id);
    };

    pAlignAmount   = grab (dt::IDs::alignAmount);
    pAlignToGrid   = grab (dt::IDs::alignToGrid);
    pGridStrength  = grab (dt::IDs::gridStrength);
    pGridDivision  = grab (dt::IDs::gridDivision);
    pPreviewAfter  = grab (dt::IDs::previewAfter);
    pChoirOn       = grab (dt::IDs::choirOn);
    pChoirVoices   = grab (dt::IDs::choirVoices);
    pChoirPreset   = grab (dt::IDs::choirPreset);
    pChoirDetune   = grab (dt::IDs::choirDetune);
    pChoirHumanize = grab (dt::IDs::choirHumanize);
    pChoirFormant  = grab (dt::IDs::choirFormant);
    pChoirWidth    = grab (dt::IDs::choirWidth);
    pChoirDryWet   = grab (dt::IDs::choirDryWet);
    pChoirGain     = grab (dt::IDs::choirGain);
}

DoubleTroubleProcessor::~DoubleTroubleProcessor() = default;

void DoubleTroubleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    choir.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    preview.prepare (sampleRate, samplesPerBlock);
}

void DoubleTroubleProcessor::releaseResources()
{
    choir.reset();
    preview.stop();
}

bool DoubleTroubleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

dt::AlignSettings DoubleTroubleProcessor::makeAlignSettings() const
{
    dt::AlignSettings s;
    s.amount = (pAlignAmount != nullptr ? pAlignAmount->load() : 100.0f) * 0.01f;
    s.alignToGrid = pAlignToGrid != nullptr && pAlignToGrid->load() > 0.5f;
    s.gridStrength = (pGridStrength != nullptr ? pGridStrength->load() : 80.0f) * 0.01f;
    const auto divIndex = pGridDivision != nullptr ? (int) pGridDivision->load() : 3;
    s.beatsPerDivision = dt::beatsForGridDivision (divIndex);
    s.bpm = hostBpm.load (std::memory_order_acquire);
    return s;
}

dt::ChoirSettings DoubleTroubleProcessor::makeChoirSettings() const
{
    dt::ChoirSettings s;
    s.enabled = pChoirOn == nullptr || pChoirOn->load() > 0.5f;
    s.numVoices = pChoirVoices != nullptr ? (int) pChoirVoices->load() : 8;
    s.preset = pChoirPreset != nullptr ? (int) pChoirPreset->load() : 1;
    s.detuneCents = pChoirDetune != nullptr ? pChoirDetune->load() : 12.0f;
    s.humanize = (pChoirHumanize != nullptr ? pChoirHumanize->load() : 35.0f) * 0.01f;
    s.formantSemitones = pChoirFormant != nullptr ? pChoirFormant->load() : 0.0f;
    s.width = (pChoirWidth != nullptr ? pChoirWidth->load() : 85.0f) * 0.01f;
    s.dryWet = (pChoirDryWet != nullptr ? pChoirDryWet->load() : 45.0f) * 0.01f;
    s.outputGainDb = pChoirGain != nullptr ? pChoirGain->load() : 0.0f;
    return s;
}

void DoubleTroubleProcessor::refreshPreviewClips (bool keepPosition)
{
    if (alignment.isBusy())
        return;

    auto refBuf = alignment.copyReference();
    const auto refSR = alignment.getReferenceSampleRate();
    const auto useAfter = pPreviewAfter != nullptr && pPreviewAfter->load() > 0.5f;
    const auto numTakes = alignment.getNumTakes();
    std::vector<dt::PreviewClip> clips;

    if (numTakes > 0)
    {
        const auto solo = previewSolo.load (std::memory_order_acquire);

        if (solo)
        {
            double sr = 44100.0;
            auto buf = alignment.copyTakePlayback (alignment.getSelectedTake(), useAfter, sr);

            if (buf.getNumSamples() > 1)
                clips.push_back ({ std::move (buf), sr });
        }
        else
        {
            clips.reserve ((size_t) numTakes);

            for (int i = 0; i < numTakes; ++i)
            {
                double sr = 44100.0;
                auto buf = alignment.copyTakePlayback (i, useAfter, sr);

                if (buf.getNumSamples() > 1)
                    clips.push_back ({ std::move (buf), sr });
            }
        }
    }

    preview.setSession (std::move (refBuf), refSR, std::move (clips), keepPosition);
}

void DoubleTroubleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();

    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm.store (*bpm, std::memory_order_release);
    }

    if (preview.isPlaying())
    {
        preview.render (buffer);
        return; // alignment preview is dry — choir must not pitch-shift the result
    }

    choir.process (buffer, makeChoirSettings());
}

void DoubleTroubleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("refFile", alignment.getReferenceFile().getFullPathName(), nullptr);

    juce::ValueTree takeList ("TAKES");

    for (int i = 0; i < alignment.getNumTakes(); ++i)
        takeList.appendChild (juce::ValueTree ("TAKE",
                                               { { "path", alignment.getTakeFile (i).getFullPathName() } }),
                              nullptr);

    state.appendChild (takeList, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DoubleTroubleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid())
        return;

    auto takeList = state.getChildWithName ("TAKES");
    state.removeChild (takeList, nullptr);
    apvts.replaceState (state);

    alignment.clearAll();
    const auto refPath = state.getProperty ("refFile").toString();

    if (refPath.isNotEmpty())
        alignment.loadReference (juce::File (refPath));

    if (takeList.isValid())
        for (int i = 0; i < takeList.getNumChildren(); ++i)
        {
            const auto path = takeList.getChild (i).getProperty ("path").toString();

            if (path.isNotEmpty())
                alignment.addTake (juce::File (path));
        }
}

juce::AudioProcessorEditor* DoubleTroubleProcessor::createEditor()
{
    return new DoubleTroubleEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DoubleTroubleProcessor();
}
