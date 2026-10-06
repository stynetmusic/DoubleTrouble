#include "AlignmentEngine.h"
#include "TimeStretch.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace dt
{
namespace
{
constexpr double kHopSeconds = 0.02;          // 20 ms analysis hop
constexpr double kMaxOffsetSeconds = 2.0;     // global lag search
constexpr int    kDtwBandFrames = 60;         // ±1.2 s Sakoe-Chiba band
constexpr int    kWarpHop = 256; // warp-map resolution (not the stretcher grain)

juce::AudioBuffer<float> resampleToRate (const juce::AudioBuffer<float>& src,
                                         double srcRate, double destRate)
{
    if (src.getNumSamples() <= 0 || src.getNumChannels() <= 0)
        return src;

    if (srcRate <= 0.0 || destRate <= 0.0 || std::abs (srcRate - destRate) < 0.05)
        return src;

    const auto numOut = std::max (1, (int) std::llround ((double) src.getNumSamples() * destRate / srcRate));
    const auto speedRatio = srcRate / destRate; // input samples per output sample
    juce::AudioBuffer<float> dest (src.getNumChannels(), numOut);
    dest.clear();

    for (int ch = 0; ch < src.getNumChannels(); ++ch)
    {
        juce::LagrangeInterpolator interpolator;
        interpolator.process (speedRatio,
                              src.getReadPointer (ch),
                              dest.getWritePointer (ch),
                              numOut,
                              src.getNumSamples(),
                              0);
    }

    return dest;
}

bool hasUsableAudio (const juce::AudioBuffer<float>& buffer) noexcept
{
    return buffer.getNumChannels() > 0 && buffer.getNumSamples() > 1;
}

float rmsOf (const juce::AudioBuffer<float>& buffer) noexcept
{
    if (! hasUsableAudio (buffer))
        return 0.0f;

    auto peakRms = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peakRms = std::max (peakRms, buffer.getRMSLevel (ch, 0, buffer.getNumSamples()));

    return peakRms;
}

/** True if `candidate` is not a silent stand-in for `source`. */
bool isAudibleTake (const juce::AudioBuffer<float>& candidate,
                    const juce::AudioBuffer<float>& source) noexcept
{
    if (! hasUsableAudio (candidate))
        return false;

    const auto cand = rmsOf (candidate);
    const auto src = rmsOf (source);

    if (cand < 1.0e-4f)
        return false;

    if (src < 1.0e-6f)
        return cand > 1.0e-4f;

    // Stretcher/offset bugs produce a full-length buffer at < 2% of the take's energy.
    return cand > 0.02f * src;
}

juce::AudioBuffer<float> placeOnTimeline (const juce::AudioBuffer<float>& src,
                                          int outLen, int offsetSamples)
{
    const auto channels = std::max (1, src.getNumChannels());
    juce::AudioBuffer<float> out (channels, std::max (1, outLen));
    out.clear();

    if (src.getNumSamples() <= 0)
        return out;

    const auto nSrc = src.getNumSamples();
    const auto nOut = out.getNumSamples();
    const auto nCh = std::min (channels, src.getNumChannels());

    for (int ch = 0; ch < nCh; ++ch)
    {
        const auto* s = src.getReadPointer (ch);
        auto* d = out.getWritePointer (ch);

        for (int i = 0; i < nOut; ++i)
        {
            const auto srcIdx = i + offsetSamples;

            if (srcIdx >= 0 && srcIdx < nSrc)
                d[i] = s[srcIdx];
        }
    }

    return out;
}

juce::AudioBuffer<float> mixToMono (const juce::AudioBuffer<float>& src)
{
    juce::AudioBuffer<float> mono (1, src.getNumSamples());
    mono.clear();

    if (src.getNumChannels() <= 0 || src.getNumSamples() <= 0)
        return mono;

    const auto gain = 1.0f / (float) src.getNumChannels();

    for (int ch = 0; ch < src.getNumChannels(); ++ch)
        mono.addFrom (0, 0, src, ch, 0, src.getNumSamples(), gain);

    return mono;
}

void highPassInPlace (juce::AudioBuffer<float>& buffer, double sampleRate, float cutoffHz)
{
    const auto rc = 1.0 / (2.0 * juce::MathConstants<double>::pi * (double) cutoffHz);
    const auto dt = 1.0 / sampleRate;
    const auto alpha = (float) (rc / (rc + dt));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto prevX = 0.0f;
        auto prevY = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto x = data[i];
            const auto y = alpha * (prevY + x - prevX);
            prevX = x;
            prevY = y;
            data[i] = y;
        }
    }
}

std::vector<float> rmsEnvelope (const float* data, int numSamples, int hop, int window)
{
    const auto frames = std::max (1, (numSamples - window) / hop + 1);
    std::vector<float> env ((size_t) frames, 0.0f);

    for (int f = 0; f < frames; ++f)
    {
        const auto start = f * hop;
        double acc = 0.0;
        int count = 0;

        for (int i = 0; i < window && start + i < numSamples; ++i)
        {
            const auto s = (double) data[start + i];
            acc += s * s;
            ++count;
        }

        const auto rms = count > 0 ? std::sqrt (acc / (double) count) : 0.0;
        env[(size_t) f] = (float) std::log1p (rms * 12.0);
    }

    return env;
}

std::vector<float> onsetEnvelope (const std::vector<float>& energy)
{
    std::vector<float> onset (energy.size(), 0.0f);

    for (size_t i = 1; i < energy.size(); ++i)
        onset[i] = std::max (0.0f, energy[i] - energy[i - 1]);

    return onset;
}

void zscore (std::vector<float>& v)
{
    if (v.empty())
        return;

    double mean = 0.0;

    for (auto x : v)
        mean += (double) x;

    mean /= (double) v.size();
    double var = 0.0;

    for (auto x : v)
    {
        const auto d = (double) x - mean;
        var += d * d;
    }

    var /= (double) v.size();
    const auto inv = 1.0 / (std::sqrt (var) + 1.0e-8);

    for (auto& x : v)
        x = (float) (((double) x - mean) * inv);
}

int envelopeLag (const std::vector<float>& ref,
                 const std::vector<float>& take,
                 int maxLag)
{
    if (ref.empty() || take.empty())
        return 0;

    auto bestLag = 0;
    auto best = -std::numeric_limits<double>::infinity();
    const auto nRef = (int) ref.size();
    const auto nTake = (int) take.size();

    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        int count = 0;

        for (int i = 0; i < nRef; ++i)
        {
            const auto j = i + lag;

            if (j >= 0 && j < nTake)
            {
                sum += (double) ref[(size_t) i] * (double) take[(size_t) j];
                ++count;
            }
        }

        if (count < 8)
            continue;

        const auto score = sum / (double) count;

        if (score > best)
        {
            best = score;
            bestLag = lag;
        }
    }

    return bestLag;
}

/** Constrained DTW. Returns, for every reference frame, the matching take frame. */
std::vector<int> bandDtw (const std::vector<float>& refEnergy,
                          const std::vector<float>& refOnset,
                          const std::vector<float>& takeEnergy,
                          const std::vector<float>& takeOnset,
                          int band)
{
    const auto nRef = (int) refEnergy.size();
    const auto nTake = (int) takeEnergy.size();
    std::vector<int> mapping ((size_t) std::max (1, nRef), 0);

    if (nRef < 2 || nTake < 2)
        return mapping;

    const auto stride = 2 * band + 1;
    const auto inf = 1.0e30f;
    std::vector<float> cost ((size_t) nRef * (size_t) stride, inf);
    std::vector<uint8_t> back ((size_t) nRef * (size_t) stride, 0); // 0 diag, 1 up, 2 left

    auto expectedJ = [nRef, nTake] (int i) -> int
    {
        if (nRef <= 1)
            return 0;

        return (int) std::lround ((double) i * (double) (nTake - 1) / (double) (nRef - 1));
    };

    auto cell = [stride, band, expectedJ] (int i, int j) -> int
    {
        const auto pos = j - (expectedJ (i) - band);

        if (pos < 0 || pos >= stride)
            return -1;

        return i * stride + pos;
    };

    auto dist = [&] (int i, int j) -> float
    {
        const auto de = refEnergy[(size_t) i] - takeEnergy[(size_t) j];
        const auto ds = refOnset[(size_t) i] - takeOnset[(size_t) j];
        return de * de + 1.6f * ds * ds;
    };

    for (int i = 0; i < nRef; ++i)
    {
        const auto j0 = std::max (0, expectedJ (i) - band);
        const auto j1 = std::min (nTake - 1, expectedJ (i) + band);

        for (int j = j0; j <= j1; ++j)
        {
            const auto idx = cell (i, j);
            const auto d = dist (i, j);
            auto best = inf;
            uint8_t which = 0;

            if (i == 0 && j == j0)
            {
                cost[(size_t) idx] = d;
                continue;
            }

            if (i > 0 && j > 0)
            {
                const auto p = cell (i - 1, j - 1);

                if (p >= 0 && cost[(size_t) p] + d < best)
                {
                    best = cost[(size_t) p] + d;
                    which = 0;
                }
            }

            if (i > 0)
            {
                const auto p = cell (i - 1, j);

                if (p >= 0 && cost[(size_t) p] + d < best)
                {
                    best = cost[(size_t) p] + d;
                    which = 1;
                }
            }

            if (j > 0)
            {
                const auto p = cell (i, j - 1);

                if (p >= 0 && cost[(size_t) p] + d < best)
                {
                    best = cost[(size_t) p] + d;
                    which = 2;
                }
            }

            cost[(size_t) idx] = best;
            back[(size_t) idx] = which;
        }
    }

    auto i = nRef - 1;
    auto j = std::min (nTake - 1, std::max (0, expectedJ (i)));

    // Snap endpoint to the cheapest in-band cell on the last row.
    {
        auto best = inf;
        auto bestJ = j;
        const auto j0 = std::max (0, expectedJ (i) - band);
        const auto j1 = std::min (nTake - 1, expectedJ (i) + band);

        for (int jj = j0; jj <= j1; ++jj)
        {
            const auto idx = cell (i, jj);

            if (idx >= 0 && cost[(size_t) idx] < best)
            {
                best = cost[(size_t) idx];
                bestJ = jj;
            }
        }

        j = bestJ;
    }

    while (i >= 0)
    {
        mapping[(size_t) i] = j;
        const auto idx = cell (i, j);

        if (idx < 0 || (i == 0 && j == 0))
            break;

        const auto which = back[(size_t) idx];

        if (which == 0)
        {
            --i;
            --j;
        }
        else if (which == 1)
        {
            --i;
        }
        else
        {
            --j;
        }

        i = std::max (i, 0);
        j = std::max (j, 0);

        if (i == 0 && which != 0)
        {
            mapping[0] = j;
            break;
        }
    }

    // Fill any frames the backtrack skipped by walking forward.
    auto last = mapping[0];

    for (int f = 1; f < nRef; ++f)
    {
        if (mapping[(size_t) f] == 0 && last > 0 && f + 1 < nRef && mapping[(size_t) f + 1] == 0)
            mapping[(size_t) f] = last;
        else
            last = mapping[(size_t) f];
    }

    return mapping;
}

/** Warp map at `outputHop` resolution from a DTW path at `analysisHop` resolution.

    Using the analysis hop as the WSOLA hop was the playback-speed bug: a 20 ms
    analysis hop (~960 samples at 48 kHz) fed into WSOLA as if it were 256
    samples compressed the take by ~3.75×.
*/
std::vector<double> mappingToWarp (const std::vector<int>& frameMap,
                                   int analysisHop,
                                   int outputHop,
                                   int numOutputSamples,
                                   int numSourceSamples,
                                   float amount)
{
    const auto hops = std::max (2, (numOutputSamples + outputHop - 1) / outputHop + 1);
    std::vector<double> warp ((size_t) hops, 0.0);
    const auto lastFrame = std::max (1, (int) frameMap.size() - 1);
    const auto analysisHopD = (double) std::max (1, analysisHop);
    const auto lastSrc = (double) std::max (0, numSourceSamples - 1);

    for (int k = 0; k < hops; ++k)
    {
        const auto outSample = (double) (k * outputHop);
        const auto framePos = outSample / analysisHopD;
        const auto f0 = juce::jlimit (0, lastFrame - 1, (int) framePos);
        const auto frac = framePos - (double) f0;
        const auto takeFrame = (double) frameMap[(size_t) f0]
                             + frac * ((double) frameMap[(size_t) f0 + 1] - (double) frameMap[(size_t) f0]);
        const auto aligned = takeFrame * analysisHopD;
        const auto identity = outSample;
        const auto blended = identity + ((double) amount) * (aligned - identity);
        warp[(size_t) k] = juce::jlimit (0.0, lastSrc, blended);
    }

    return warp;
}

std::vector<int> detectOnsets (const std::vector<float>& onset, int minDistanceFrames)
{
    std::vector<int> peaks;

    if (onset.size() < 3)
        return peaks;

    auto maxVal = 0.0f;

    for (auto x : onset)
        maxVal = std::max (maxVal, x);

    const auto thresh = maxVal * 0.35f;
    const auto minDist = std::max (1, minDistanceFrames);

    for (int i = 1; i < (int) onset.size() - 1; ++i)
    {
        const auto v = onset[(size_t) i];

        if (v >= thresh
            && v >= onset[(size_t) i - 1]
            && v >= onset[(size_t) i + 1])
        {
            if (peaks.empty() || i - peaks.back() >= minDist)
                peaks.push_back (i);
        }
    }

    return peaks;
}

std::vector<double> applyGridToWarp (const std::vector<double>& warp,
                                     const std::vector<int>& onsetFrames,
                                     int warpHop,
                                     int analysisHop,
                                     double sampleRate,
                                     double bpm,
                                     double beatsPerDivision,
                                     float strength,
                                     int numSourceSamples)
{
    auto result = warp;

    if (onsetFrames.size() < 2 || bpm <= 1.0 || beatsPerDivision <= 0.0
        || warpHop <= 0 || analysisHop <= 0 || sampleRate <= 0.0)
        return result;

    const auto gridPeriod = sampleRate * (60.0 / bpm) * beatsPerDivision;

    if (gridPeriod < 32.0)
        return result;

    struct Anchor { double outSample; double srcSample; };
    std::vector<Anchor> anchors;
    anchors.push_back ({ 0.0, warp.front() });

    for (auto frame : onsetFrames)
    {
        const auto src = (double) (frame * analysisHop);
        const auto quantized = std::round (src / gridPeriod) * gridPeriod;
        const auto dst = src + (double) strength * (quantized - src);
        anchors.push_back ({ dst, src });
    }

    const auto lastOut = (double) (((int) warp.size() - 1) * warpHop);
    anchors.push_back ({ lastOut, warp.back() });
    std::sort (anchors.begin(), anchors.end(),
               [] (const Anchor& a, const Anchor& b) { return a.outSample < b.outSample; });

    auto a = 0;
    const auto lastA = (int) anchors.size() - 1;

    for (int k = 0; k < (int) warp.size(); ++k)
    {
        const auto outSample = (double) (k * warpHop);

        while (a + 1 < lastA && anchors[(size_t) a + 1].outSample < outSample)
            ++a;

        const auto& a0 = anchors[(size_t) a];
        const auto& a1 = anchors[(size_t) std::min (a + 1, lastA)];
        const auto span = std::max (1.0e-6, a1.outSample - a0.outSample);
        const auto t = juce::jlimit (0.0, 1.0, (outSample - a0.outSample) / span);
        const auto gridSrc = a0.srcSample + t * (a1.srcSample - a0.srcSample);
        const auto mixed = warp[(size_t) k] + (double) strength * (gridSrc - warp[(size_t) k]);
        result[(size_t) k] = juce::jlimit (0.0, (double) std::max (0, numSourceSamples - 1), mixed);
    }

    return result;
}
} // namespace

AlignmentEngine::AlignmentEngine()
    : juce::Thread ("DT Align")
{
    formats.registerBasicFormats();
    startThread();
}

AlignmentEngine::~AlignmentEngine()
{
    signalThreadShouldExit();
    notify();
    stopThread (4000);
}

bool AlignmentEngine::isAudioFile (const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff"
        || ext == ".flac" || ext == ".ogg" || ext == ".mp3" || ext == ".m4a";
}

void AlignmentEngine::loadReference (const juce::File& file)
{
    const juce::ScopedLock sl (jobLock);
    jobs.push_back ({ JobType::loadReference, file, -1, {} });
    notify();
}

bool AlignmentEngine::containsTake (const juce::File& file) const
{
    const auto path = file.getFullPathName();
    const juce::SpinLock::ScopedLockType sl (bufferLock);

    for (const auto& take : takes)
        if (take->file.getFullPathName() == path)
            return true;

    return false;
}

void AlignmentEngine::addTake (const juce::File& file)
{
    if (containsTake (file))
        return;

    {
        const juce::ScopedLock sl (jobLock);

        for (const auto& job : jobs)
            if (job.type == JobType::loadTake && job.file == file)
                return;

        jobs.push_back ({ JobType::loadTake, file, -1, {} });
    }

    notify();
}

void AlignmentEngine::removeTake (int index)
{
    {
        const juce::SpinLock::ScopedLockType sl (bufferLock);

        if (juce::isPositiveAndBelow (index, (int) takes.size()))
            takes.erase (takes.begin() + index);

        selectedTake.store (juce::jlimit (0, std::max (0, (int) takes.size() - 1),
                                          selectedTake.load()),
                            std::memory_order_release);
    }

    bumpContentEpoch();
    sendChangeMessage();
}

void AlignmentEngine::clearAll()
{
    {
        const juce::SpinLock::ScopedLockType sl (bufferLock);
        reference.reset();
        takes.clear();
        selectedTake.store (0, std::memory_order_release);
    }

    {
        const juce::ScopedLock sl (statusLock);
        status = "Drop a reference vocal to begin.";
    }

    bumpContentEpoch();
    sendChangeMessage();
}

void AlignmentEngine::requestAlign (AlignSettings settings)
{
    const juce::ScopedLock sl (jobLock);
    jobs.push_back ({ JobType::align, {}, -1, settings });
    notify();
}

void AlignmentEngine::requestExport (int takeIndex, const juce::File& destWav)
{
    const juce::ScopedLock sl (jobLock);
    jobs.push_back ({ JobType::exportTake, destWav, takeIndex, {} });
    notify();
}

void AlignmentEngine::requestBounceMix (const juce::File& destWav, bool preferAligned, bool includeReference)
{
    const juce::ScopedLock sl (jobLock);
    Job job;
    job.type = JobType::bounceMix;
    job.file = destWav;
    job.preferAligned = preferAligned;
    job.includeReference = includeReference;
    jobs.push_back (job);
    notify();
}

void AlignmentEngine::bumpContentEpoch() noexcept
{
    contentEpoch.fetch_add (1, std::memory_order_acq_rel);
}

juce::File AlignmentEngine::getBounceFile() const
{
    const juce::ScopedLock sl (statusLock);
    return bounceFile;
}

bool AlignmentEngine::isBounceReady() const
{
    const auto file = getBounceFile();
    return bounceEpoch.load (std::memory_order_acquire)
               == contentEpoch.load (std::memory_order_acquire)
           && file.existsAsFile();
}

juce::String AlignmentEngine::getStatus() const
{
    const juce::ScopedLock sl (statusLock);
    return status;
}

bool AlignmentEngine::hasReference() const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);
    return reference != nullptr && reference->original.getNumSamples() > 0;
}

int AlignmentEngine::getNumTakes() const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);
    return (int) takes.size();
}

void AlignmentEngine::setSelectedTake (int index)
{
    const auto n = getNumTakes();
    const auto clamped = juce::jlimit (0, std::max (0, n - 1), index);

    if (clamped == selectedTake.load (std::memory_order_acquire))
        return;

    selectedTake.store (clamped, std::memory_order_release);
    sendChangeMessage();
}

juce::String AlignmentEngine::getReferenceName() const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);
    return reference != nullptr ? reference->name : juce::String();
}

juce::String AlignmentEngine::getTakeName (int index) const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);

    if (juce::isPositiveAndBelow (index, (int) takes.size()))
        return takes[(size_t) index]->name;

    return {};
}

juce::StringArray AlignmentEngine::getTakeDisplayNames() const
{
    juce::StringArray names;
    juce::StringArray parents;

    {
        const juce::SpinLock::ScopedLockType sl (bufferLock);
        names.ensureStorageAllocated ((int) takes.size());

        for (const auto& take : takes)
        {
            auto name = take->file.getFileName();

            if (name.isEmpty())
                name = take->name;

            names.add (name);
            parents.add (take->file.getParentDirectory().getFileName());
        }
    }

    for (int i = 0; i < names.size(); ++i)
    {
        auto clash = false;

        for (int j = 0; j < names.size(); ++j)
            if (i != j && names[i] == names[j])
                clash = true;

        if (clash && parents[i].isNotEmpty())
            names.set (i, parents[i] + "/" + names[i]);
    }

    return names;
}

juce::File AlignmentEngine::getReferenceFile() const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);
    return reference != nullptr ? reference->file : juce::File();
}

juce::File AlignmentEngine::getTakeFile (int index) const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);

    if (juce::isPositiveAndBelow (index, (int) takes.size()))
        return takes[(size_t) index]->file;

    return {};
}

juce::AudioBuffer<float> AlignmentEngine::copyReference() const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);

    if (reference == nullptr)
        return {};

    return reference->original;
}

juce::AudioBuffer<float> AlignmentEngine::copyTakeOriginal (int index) const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);

    if (! juce::isPositiveAndBelow (index, (int) takes.size()))
        return {};

    return takes[(size_t) index]->original;
}

juce::AudioBuffer<float> AlignmentEngine::copyTakeAligned (int index) const
{
    double ignored = 44100.0;
    return copyTakePlayback (index, true, ignored);
}

juce::AudioBuffer<float> AlignmentEngine::copyTakePlayback (int index, bool preferAligned,
                                                            double& sampleRate) const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);

    if (! juce::isPositiveAndBelow (index, (int) takes.size()))
    {
        sampleRate = 44100.0;
        return {};
    }

    const auto& take = *takes[(size_t) index];

    if (preferAligned && take.hasAligned && isAudibleTake (take.aligned, take.original))
    {
        sampleRate = take.alignedSampleRate > 0.0 ? take.alignedSampleRate : take.sampleRate;
        return take.aligned;
    }

    sampleRate = take.sampleRate;
    return take.original;
}

double AlignmentEngine::getReferenceSampleRate() const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);
    return reference != nullptr ? reference->sampleRate : 44100.0;
}

double AlignmentEngine::getTakeSampleRate (int index) const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);

    if (! juce::isPositiveAndBelow (index, (int) takes.size()))
        return 44100.0;

    return takes[(size_t) index]->sampleRate;
}

double AlignmentEngine::getTakeAlignedSampleRate (int index) const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);

    if (! juce::isPositiveAndBelow (index, (int) takes.size()))
        return 44100.0;

    return takes[(size_t) index]->alignedSampleRate;
}

bool AlignmentEngine::takeHasAligned (int index) const
{
    const juce::SpinLock::ScopedLockType sl (bufferLock);

    if (! juce::isPositiveAndBelow (index, (int) takes.size()))
        return false;

    const auto& take = *takes[(size_t) index];
    return take.hasAligned && isAudibleTake (take.aligned, take.original);
}

const AudioClip* AlignmentEngine::getTakeLocked (int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, (int) takes.size()))
        return nullptr;

    return takes[(size_t) index].get();
}

void AlignmentEngine::run()
{
    while (! threadShouldExit())
    {
        Job job;
        auto haveJob = false;

        {
            const juce::ScopedLock sl (jobLock);

            if (! jobs.empty())
            {
                job = jobs.front();
                jobs.erase (jobs.begin());
                haveJob = true;
            }
        }

        if (haveJob)
            execute (job);
        else
            wait (50);
    }
}

void AlignmentEngine::execute (const Job& job)
{
    busy.store (true, std::memory_order_release);
    progress.store (0.0f, std::memory_order_release);

    juce::String error;

    switch (job.type)
    {
        case JobType::loadReference:
        {
            {
                const juce::ScopedLock sl (statusLock);
                status = "Loading reference…";
            }

            sendChangeMessage();
            auto clip = std::make_unique<AudioClip>();

            if (decodeFile (job.file, *clip, error))
            {
                const juce::SpinLock::ScopedLockType sl (bufferLock);
                reference = std::move (clip);
                bumpContentEpoch();
            }

            break;
        }

        case JobType::loadTake:
        {
            {
                const juce::ScopedLock sl (statusLock);
                status = "Loading take…";
            }

            sendChangeMessage();
            auto clip = std::make_unique<AudioClip>();

            if (decodeFile (job.file, *clip, error))
            {
                const juce::SpinLock::ScopedLockType sl (bufferLock);
                auto already = false;

                for (const auto& take : takes)
                    if (take->file == job.file)
                    {
                        already = true;
                        break;
                    }

                if (! already)
                {
                    takes.push_back (std::move (clip));
                    selectedTake.store ((int) takes.size() - 1, std::memory_order_release);
                    bumpContentEpoch();
                }
            }

            break;
        }

        case JobType::align:
            runAlignment (job.settings);
            bumpContentEpoch();
            break;

        case JobType::exportTake:
        {
            AudioClip snapshot;
            auto ok = false;

            {
                const juce::SpinLock::ScopedLockType sl (bufferLock);

                if (juce::isPositiveAndBelow (job.takeIndex, (int) takes.size()))
                {
                    snapshot = *takes[(size_t) job.takeIndex];
                    ok = true;
                }
            }

            if (! ok)
            {
                error = "No take to export.";
            }
            else if (! snapshot.hasAligned)
            {
                error = "Align the take before exporting.";
            }
            else if (! writeWav (snapshot, job.file, error))
            {
                // error already set
            }
            else
            {
                const juce::ScopedLock sl (statusLock);
                status = "Exported " + job.file.getFileName();
            }

            break;
        }

        case JobType::bounceMix:
            runBounceMix (job);
            break;
    }

    if (error.isNotEmpty())
    {
        const juce::ScopedLock sl (statusLock);
        status = error;
    }
    else if (job.type == JobType::loadReference || job.type == JobType::loadTake)
    {
        const juce::ScopedLock sl (statusLock);
        status = "Ready. Adjust Amount / Grid, then Align.";
    }

    progress.store (1.0f, std::memory_order_release);
    busy.store (false, std::memory_order_release);
    sendChangeMessage();
}

bool AlignmentEngine::decodeFile (const juce::File& file, AudioClip& clip, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "File not found: " + file.getFileName();
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        error = "Could not decode " + file.getFileName();
        return false;
    }

    const auto numSamples = (int) std::min ((juce::int64) std::numeric_limits<int>::max() - 8,
                                            reader->lengthInSamples);

    if (numSamples <= 0)
    {
        error = "Empty audio file.";
        return false;
    }

    clip.file = file;
    clip.name = file.getFileName();
    clip.sampleRate = reader->sampleRate;
    clip.alignedSampleRate = reader->sampleRate;
    clip.original.setSize ((int) reader->numChannels, numSamples, false, true, false);
    reader->read (&clip.original, 0, numSamples, 0, true, true);
    clip.aligned = clip.original;
    clip.hasAligned = false;
    progress.store (0.4f, std::memory_order_release);
    return true;
}

void AlignmentEngine::runAlignment (const AlignSettings& settings)
{
    AudioClip refCopy;
    std::vector<AudioClip> takeCopies;
    std::vector<int> takeIndices;

    {
        const juce::SpinLock::ScopedLockType sl (bufferLock);

        if (reference == nullptr || reference->original.getNumSamples() <= 0)
        {
            const juce::ScopedLock st (statusLock);
            status = "Load a reference vocal first.";
            return;
        }

        if (takes.empty())
        {
            const juce::ScopedLock st (statusLock);
            status = "Load at least one take.";
            return;
        }

        refCopy = *reference;
        takeCopies.reserve (takes.size());

        for (int i = 0; i < (int) takes.size(); ++i)
        {
            takeCopies.push_back (*takes[(size_t) i]);
            takeIndices.push_back (i);
        }
    }

    juce::String error;
    const auto n = (int) takeCopies.size();

    for (int i = 0; i < n; ++i)
    {
        {
            const juce::ScopedLock sl (statusLock);
            status = "Aligning " + takeCopies[(size_t) i].name
                   + " (" + juce::String (i + 1) + "/" + juce::String (n) + ")…";
        }

        sendChangeMessage();
        progress.store ((float) i / (float) n, std::memory_order_release);

        if (! alignOneTake (takeCopies[(size_t) i], refCopy, settings, error))
            break;

        {
            const juce::SpinLock::ScopedLockType sl (bufferLock);
            const auto idx = takeIndices[(size_t) i];

            if (juce::isPositiveAndBelow (idx, (int) takes.size()))
            {
                // Never touch `original` — only swap in an audible aligned result.
                auto& dest = *takes[(size_t) idx];
                dest.globalOffsetSamples = takeCopies[(size_t) i].globalOffsetSamples;
                dest.alignedSampleRate = takeCopies[(size_t) i].alignedSampleRate;
                dest.hasAligned = takeCopies[(size_t) i].hasAligned
                    && isAudibleTake (takeCopies[(size_t) i].aligned, dest.original);

                if (dest.hasAligned)
                    dest.aligned = std::move (takeCopies[(size_t) i].aligned);
            }
        }
    }

    if (error.isNotEmpty())
    {
        const juce::ScopedLock sl (statusLock);
        status = error;
    }
    else
    {
        const juce::ScopedLock sl (statusLock);
        status = "Aligned " + juce::String (n) + (n == 1 ? " take." : " takes.");
    }
}

bool AlignmentEngine::alignOneTake (AudioClip& take, const AudioClip& ref,
                                    const AlignSettings& settings, juce::String& error)
{
    if (take.original.getNumSamples() < 256 || ref.original.getNumSamples() < 256)
    {
        error = "Audio is too short to align.";
        return false;
    }

    // Work entirely on the reference clock so WSOLA output is 1:1 with the ref.
    const auto workRate = ref.sampleRate > 0.0 ? ref.sampleRate : 44100.0;
    auto takeOnRefClock = resampleToRate (take.original, take.sampleRate, workRate);

    auto refMono = mixToMono (ref.original);
    auto takeMono = mixToMono (takeOnRefClock);
    highPassInPlace (refMono, workRate, 80.0f);
    highPassInPlace (takeMono, workRate, 80.0f);

    const auto analysisHop = std::max (32, (int) std::lround (workRate * kHopSeconds));
    const auto window = analysisHop * 2;

    auto eRef = rmsEnvelope (refMono.getReadPointer (0), refMono.getNumSamples(), analysisHop, window);
    auto eTake = rmsEnvelope (takeMono.getReadPointer (0), takeMono.getNumSamples(), analysisHop, window);
    auto oRef = onsetEnvelope (eRef);
    auto oTake = onsetEnvelope (eTake);
    zscore (eRef);
    zscore (eTake);
    zscore (oRef);
    zscore (oTake);

    const auto maxLag = (int) std::lround (kMaxOffsetSeconds / kHopSeconds);
    const auto lagFrames = envelopeLag (eRef, eTake, maxLag);
    take.globalOffsetSamples = lagFrames * analysisHop;

    // Shift take envelopes so DTW sits on the diagonal.
    std::vector<float> eTakeShift = eRef;
    std::vector<float> oTakeShift = oRef;
    std::fill (eTakeShift.begin(), eTakeShift.end(), 0.0f);
    std::fill (oTakeShift.begin(), oTakeShift.end(), 0.0f);

    for (int i = 0; i < (int) eRef.size(); ++i)
    {
        const auto j = i + lagFrames;

        if (j >= 0 && j < (int) eTake.size())
        {
            eTakeShift[(size_t) i] = eTake[(size_t) j];
            oTakeShift[(size_t) i] = oTake[(size_t) j];
        }
    }

    auto frameMap = bandDtw (eRef, oRef, eTakeShift, oTakeShift, kDtwBandFrames);

    // frameMap is in the *shifted* take. Convert back to original take frames.
    for (auto& f : frameMap)
        f = juce::jlimit (0, (int) eTake.size() - 1, f + lagFrames);

    const auto amount = juce::jlimit (0.0f, 1.0f, settings.amount);
    const auto outLen = ref.original.getNumSamples();
    auto warp = mappingToWarp (frameMap, analysisHop, kWarpHop,
                               outLen, takeOnRefClock.getNumSamples(), amount);

    if (settings.alignToGrid)
    {
        const auto peaks = detectOnsets (oTake, std::max (2, (int) (0.08 / kHopSeconds)));
        warp = applyGridToWarp (warp, peaks, kWarpHop, analysisHop, workRate,
                                settings.bpm, settings.beatsPerDivision,
                                juce::jlimit (0.0f, 1.0f, settings.gridStrength),
                                takeOnRefClock.getNumSamples());
    }

    take.aligned = applyWarpHighQuality (takeOnRefClock, warp, kWarpHop, outLen, workRate);
    take.alignedSampleRate = workRate;

    // A "successful" stretch can still be a full-length silent buffer (PV skipped
    // every frame, or offset shoved the take off the timeline). That is what made
    // the Take lane go blank and left only Reference in the preview.
    if (! isAudibleTake (take.aligned, takeOnRefClock))
        take.aligned = placeOnTimeline (takeOnRefClock, outLen, take.globalOffsetSamples);

    if (! isAudibleTake (take.aligned, takeOnRefClock))
        take.aligned = placeOnTimeline (takeOnRefClock, outLen, 0);

    take.hasAligned = isAudibleTake (take.aligned, takeOnRefClock);
    return true;
}

bool AlignmentEngine::writeWav (const AudioClip& take, const juce::File& dest, juce::String& error)
{
    return writeBufferWav (take.aligned, take.alignedSampleRate, dest, error);
}

bool AlignmentEngine::writeBufferWav (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                      const juce::File& dest, juce::String& error)
{
    dest.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (dest.createOutputStream());

    if (stream == nullptr)
    {
        error = "Could not write " + dest.getFileName();
        return false;
    }

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (sampleRate > 0.0 ? sampleRate : 44100.0)
                             .withNumChannels (std::max (1, buffer.getNumChannels()))
                             .withBitsPerSample (24);

    auto writer = wav.createWriterFor (stream, options);

    if (writer == nullptr)
    {
        error = "WAV writer failed.";
        return false;
    }

    if (! writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()))
    {
        error = "Failed while writing samples.";
        return false;
    }

    return true;
}

void AlignmentEngine::runBounceMix (const Job& job)
{
    {
        const juce::ScopedLock sl (statusLock);
        status = "Mixing takes for Drop…";
    }

    sendChangeMessage();

    const auto n = getNumTakes();

    if (n <= 0)
    {
        const juce::ScopedLock sl (statusLock);
        status = "Load takes before using Drop.";
        return;
    }

    struct Clip
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
    };

    std::vector<Clip> clips;
    clips.reserve ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        progress.store ((float) i / (float) (n + 1), std::memory_order_release);
        double sr = 44100.0;
        auto buf = copyTakePlayback (i, job.preferAligned, sr);

        if (buf.getNumChannels() > 0 && buf.getNumSamples() > 1)
            clips.push_back ({ std::move (buf), sr > 0.0 ? sr : 44100.0 });
    }

    juce::AudioBuffer<float> refBuf;
    double refSR = 44100.0;

    if (job.includeReference && hasReference())
    {
        refBuf = copyReference();
        refSR = getReferenceSampleRate();
    }

    if (clips.empty() && refBuf.getNumSamples() <= 1)
    {
        const juce::ScopedLock sl (statusLock);
        status = "Nothing to mix.";
        return;
    }

    const auto destSR = hasReference() ? getReferenceSampleRate()
                                       : (clips.empty() ? 44100.0 : clips.front().sampleRate);

    auto duration = 0.0;

    for (const auto& clip : clips)
        duration = std::max (duration, (double) clip.buffer.getNumSamples() / clip.sampleRate);

    if (refBuf.getNumSamples() > 1 && refSR > 0.0)
        duration = std::max (duration, (double) refBuf.getNumSamples() / refSR);

    const auto outN = std::max (1, (int) std::llround (duration * destSR));
    juce::AudioBuffer<float> mix (2, outN);
    mix.clear();

    auto addClip = [destSR, outN, &mix] (const juce::AudioBuffer<float>& src, double sr, float gain)
    {
        if (src.getNumSamples() <= 1 || sr <= 0.0)
            return;

        for (int i = 0; i < outN; ++i)
        {
            const auto pos = ((double) i / destSR) * sr;

            if (pos >= (double) (src.getNumSamples() - 1))
                break;

            const auto i0 = (int) pos;
            const auto i1 = std::min (src.getNumSamples() - 1, i0 + 1);
            const auto frac = (float) (pos - (double) i0);

            for (int ch = 0; ch < 2; ++ch)
            {
                const auto c = juce::jmin (ch, src.getNumChannels() - 1);
                const auto s0 = src.getSample (c, i0);
                const auto s1 = src.getSample (c, i1);
                mix.addSample (ch, i, (s0 + frac * (s1 - s0)) * gain);
            }
        }
    };

    const auto takeGain = clips.empty() ? 0.0f : 0.85f / std::sqrt ((float) clips.size());

    for (const auto& clip : clips)
        addClip (clip.buffer, clip.sampleRate, takeGain);

    if (refBuf.getNumSamples() > 1)
        addClip (refBuf, refSR, 0.55f);

    const auto peak = mix.getMagnitude (0, outN);

    if (peak > 0.98f)
        mix.applyGain (0.98f / peak);

    juce::String error;

    if (! writeBufferWav (mix, destSR, job.file, error))
    {
        const juce::ScopedLock sl (statusLock);
        status = error;
        return;
    }

    {
        const juce::ScopedLock sl (statusLock);
        bounceFile = job.file;
        bounceEpoch.store (contentEpoch.load (std::memory_order_acquire), std::memory_order_release);
        status = "DROP ready — drag the Drop button onto a DAW track";
    }
}
} // namespace dt
