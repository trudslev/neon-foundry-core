#include "nf/testing/ProcessorHarness.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <chrono>
#include <thread>
#include <new>

namespace
{
    /** Process-wide allocation counters.

        **Deliberately not thread-safe beyond the atomics themselves.** The sentinel measures one
        thread at a time by construction — you arm it around the call you are measuring, with nothing
        else running — and a lock here would allocate on some platforms, which is the one thing an
        allocation detector must never do.
    */
    std::atomic<bool> armed { false };
    std::atomic<int> allocationCount { 0 };
    std::atomic<size_t> allocationBytes { 0 };
    std::atomic<int> freeCount { 0 };

    inline void note (size_t bytes) noexcept
    {
        if (armed.load (std::memory_order_relaxed))
        {
            allocationCount.fetch_add (1, std::memory_order_relaxed);
            allocationBytes.fetch_add (bytes, std::memory_order_relaxed);
        }
    }

    /** A deterministic input, so two renders of one spec differ only by what the processor did.

        A fixed-seed xorshift rather than juce::Random: the sequence must be identical across block
        sizes, and it is generated from the absolute sample index rather than per-block state so that
        re-cutting the same stream into different blocks yields the same samples. That is the whole
        premise of the block-size comparison — get it wrong and every casting "fails" invariance
        because the input differed.
    */
    inline float deterministicSample (int absoluteIndex, int channel) noexcept
    {
        uint32_t x = (uint32_t) (absoluteIndex * 2654435761u) ^ (uint32_t) (channel * 40503u) ^ 0x9e3779b9u;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return ((float) (x & 0xffffffu) / (float) 0x7fffff) - 1.0f;
    }
}

//==============================================================================
// **The global overrides live in this TU on purpose.** In a static library the linker pulls only
// object files that resolve a referenced symbol, so an operator new override sitting in an otherwise
// unreferenced translation unit is silently dropped and the detector counts zero — which reports as
// "no allocations" and is indistinguishable from a pass. AllocationSentinel's constructor is defined
// below, in this same file, so using the sentinel is what drags the overrides in.

void* operator new (size_t size)
{
    note (size);

    if (auto* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

void* operator new[] (size_t size)
{
    note (size);

    if (auto* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

namespace
{
    inline void noteFree (void* p) noexcept
    {
        if (p != nullptr && armed.load (std::memory_order_relaxed))
            freeCount.fetch_add (1, std::memory_order_relaxed);
    }
}

void operator delete (void* p) noexcept { noteFree (p); std::free (p); }
void operator delete[] (void* p) noexcept { noteFree (p); std::free (p); }
void operator delete (void* p, size_t) noexcept { noteFree (p); std::free (p); }
void operator delete[] (void* p, size_t) noexcept { noteFree (p); std::free (p); }

namespace nf::testing
{

AllocationSentinel::AllocationSentinel()
{
    allocationCount.store (0, std::memory_order_relaxed);
    allocationBytes.store (0, std::memory_order_relaxed);
    freeCount.store (0, std::memory_order_relaxed);
    armed.store (true, std::memory_order_relaxed);
}

AllocationSentinel::~AllocationSentinel()
{
    armed.store (false, std::memory_order_relaxed);
}

int AllocationSentinel::count() const noexcept
{
    return allocationCount.load (std::memory_order_relaxed);
}

size_t AllocationSentinel::bytes() const noexcept
{
    return allocationBytes.load (std::memory_order_relaxed);
}

int AllocationSentinel::frees() const noexcept
{
    return freeCount.load (std::memory_order_relaxed);
}

//==============================================================================
juce::String AllocationReport::describe() const
{
    juce::String s;
    s << "prepared " << preparedBlockSize << ", driven " << drivenBlockSize << ": ";

    if (clean())
    {
        s << "no heap activity (0 alloc, 0 free)";
    }
    else
    {
        s << allocations << " alloc (" << (int) bytes << " bytes), " << frees << " free";

        // Named explicitly, because "clean of allocations but not of frees" is the row the older
        // instrument would have called clean.
        if (allocations == 0)
            s << "  <- FREES ONLY: invisible to an allocation-only detector";
    }

    return s;
}

AllocationReport probeProcessBlockAllocation (juce::AudioProcessor& processor,
                                              double sampleRate,
                                              int preparedBlockSize,
                                              int drivenBlockSize,
                                              int numChannels,
                                              int measuredBlocks,
                                              int warmUpBlocks)
{
    AllocationReport report;
    report.preparedBlockSize = preparedBlockSize;
    report.drivenBlockSize = drivenBlockSize;

    processor.setRateAndBufferSizeDetails (sampleRate, preparedBlockSize);
    processor.prepareToPlay (sampleRate, preparedBlockSize);

    // Everything the measurement needs is built BEFORE the sentinel is armed. A buffer allocated
    // inside the armed region would be counted as the processor's.
    juce::AudioBuffer<float> buffer (numChannels, drivenBlockSize);
    juce::MidiBuffer midi;

    int absolute = 0;

    auto fill = [&]
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < drivenBlockSize; ++i)
                buffer.setSample (ch, i, deterministicSample (absolute + i, ch));

        absolute += drivenBlockSize;
    };

    // Unmeasured warm-up: a first block may legitimately allocate where a steady state does not.
    for (int i = 0; i < warmUpBlocks; ++i)
    {
        fill();
        midi.clear();
        processor.processBlock (buffer, midi);
    }

    for (int i = 0; i < measuredBlocks; ++i)
    {
        fill();
        midi.clear();

        const AllocationSentinel sentinel;
        processor.processBlock (buffer, midi);

        report.allocations += sentinel.count();
        report.bytes += sentinel.bytes();
        report.frees += sentinel.frees();
    }

    return report;
}

//==============================================================================
std::vector<std::vector<float>> render (juce::AudioProcessor& processor, const RenderSpec& spec)
{
    processor.setRateAndBufferSizeDetails (spec.sampleRate, spec.blockSize);
    processor.prepareToPlay (spec.sampleRate, spec.blockSize);

    juce::AudioBuffer<float> buffer (spec.numChannels, spec.blockSize);
    juce::MidiBuffer midi;

    std::vector<std::vector<float>> out ((size_t) spec.numChannels);
    for (auto& channel : out)
        channel.reserve ((size_t) (spec.blockSize * spec.numBlocks));

    int absolute = 0;

    for (int block = 0; block < spec.numBlocks; ++block)
    {
        buffer.clear();

        if (spec.fillInput != nullptr)
        {
            spec.fillInput (buffer, block);
        }
        else
        {
            for (int ch = 0; ch < spec.numChannels; ++ch)
                for (int i = 0; i < spec.blockSize; ++i)
                    buffer.setSample (ch, i, deterministicSample (absolute + i, ch));
        }

        absolute += spec.blockSize;
        midi.clear();
        processor.processBlock (buffer, midi);

        for (int ch = 0; ch < spec.numChannels; ++ch)
        {
            const auto* read = buffer.getReadPointer (ch);
            out[(size_t) ch].insert (out[(size_t) ch].end(), read, read + spec.blockSize);
        }
    }

    return out;
}

//==============================================================================
namespace
{
    InvarianceResult compare (const std::vector<std::vector<float>>& a,
                              const std::vector<std::vector<float>>& b)
    {
        InvarianceResult r;

        const auto channels = juce::jmin (a.size(), b.size());

        for (size_t ch = 0; ch < channels; ++ch)
        {
            const auto n = juce::jmin (a[ch].size(), b[ch].size());
            r.comparedSamples = juce::jmax (r.comparedSamples, (int) n);

            for (size_t i = 0; i < n; ++i)
            {
                const double diff = std::abs ((double) a[ch][i] - (double) b[ch][i]);

                if (diff > r.maxAbsDifference)
                    r.maxAbsDifference = diff;

                if (diff != 0.0 && r.firstDivergentSample < 0)
                    r.firstDivergentSample = (int) i;
            }
        }

        r.sampleExact = (r.maxAbsDifference == 0.0);
        return r;
    }
}

juce::String InvarianceResult::describe() const
{
    // The readback leads, so a collapsed sweep is visible before the verdict is read.
    juce::String prefix;
    prefix << "[prepared " << actualBlockSize << " @ " << juce::String (actualSampleRate, 1) << " Hz] ";

    if (! comparisonWasMeaningful)
        prefix << "**COMPARISON PROVED NOTHING** ";

    if (sampleExact)
        return prefix + "sample-exact over " + juce::String (comparedSamples) + " samples";

    return prefix

         + "DIFFERS: max |delta| " + juce::String (maxAbsDifference, 9)
         + ", first at sample " + juce::String (firstDivergentSample)
         + " of " + juce::String (comparedSamples);
}

std::vector<InvarianceResult> blockSizeInvariance (juce::AudioProcessor& processor,
                                                   RenderSpec spec,
                                                   const std::vector<int>& blockSizes)
{
    jassert (! blockSizes.empty());

    // Every size renders the SAME number of samples, not the same number of blocks — otherwise a
    // 2048 run is 32× longer than a 64 run and the comparison is against a different signal.
    const int totalSamples = spec.blockSize * spec.numBlocks;

    auto renderAt = [&] (int blockSize)
    {
        auto s = spec;
        s.blockSize = blockSize;
        s.numBlocks = juce::jmax (1, totalSamples / blockSize);
        return render (processor, s);
    };

    const auto reference = renderAt (blockSizes.front());

    std::vector<InvarianceResult> results;
    results.reserve (blockSizes.size());

    for (auto size : blockSizes)
    {
        auto r = compare (reference, renderAt (size));

        // Read back from the processor rather than echoing `size`: if it clamped or ignored the
        // request, the log shows repeats instead of the sweep the loop believed it ran.
        r.actualBlockSize = processor.getBlockSize();
        r.actualSampleRate = processor.getSampleRate();
        results.push_back (r);
    }

    return results;
}

InvarianceResult offlineAgainstRealtime (juce::AudioProcessor& processor, RenderSpec spec)
{
    processor.setNonRealtime (false);
    const auto realtime = render (processor, spec);

    processor.setNonRealtime (true);

    // **Read back rather than assume the call took effect.** Two identical renders compare equal
    // whether the processor switched modes or ignored the request, and the second is not a pass.
    const bool honoured = processor.isNonRealtime();

    const auto offline = render (processor, spec);

    processor.setNonRealtime (false);

    auto r = compare (realtime, offline);
    r.actualBlockSize = processor.getBlockSize();
    r.actualSampleRate = processor.getSampleRate();
    r.nonRealtimeWasHonoured = honoured;
    r.comparisonWasMeaningful = honoured;
    return r;
}

//==============================================================================
juce::String NumericalReport::describe() const
{
    juce::String s;
    s << "subnormals " << subnormals << ", NaN " << nans << ", Inf " << infinities
      << ", peak " << juce::String (peakAbs, 9);

    s << (blocksUntilSilent >= 0 ? ", silent after " + juce::String (blocksUntilSilent) + " tail blocks"
                                 : ", never fell silent");
    s << "  [prepared " << actualBlockSize << " @ " << juce::String (actualSampleRate, 1) << " Hz]";
    return s;
}

NumericalReport scanTail (juce::AudioProcessor& processor,
                          RenderSpec spec,
                          int tailBlocks,
                          float silenceThreshold)
{
    NumericalReport report;

    // Excite first, with the caller's input.
    render (processor, spec);

    report.actualSampleRate = processor.getSampleRate();
    report.actualBlockSize = processor.getBlockSize();

    juce::AudioBuffer<float> buffer (spec.numChannels, spec.blockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < tailBlocks; ++block)
    {
        buffer.clear();          // silence in
        midi.clear();
        processor.processBlock (buffer, midi);

        double blockPeak = 0.0;

        for (int ch = 0; ch < spec.numChannels; ++ch)
        {
            const auto* read = buffer.getReadPointer (ch);

            for (int i = 0; i < spec.blockSize; ++i)
            {
                const float v = read[i];

                switch (std::fpclassify (v))
                {
                    case FP_SUBNORMAL: ++report.subnormals; break;
                    case FP_NAN:       ++report.nans;       break;
                    case FP_INFINITE:  ++report.infinities; break;
                    default: break;
                }

                blockPeak = juce::jmax (blockPeak, (double) std::abs (v));
            }
        }

        report.peakAbs = juce::jmax (report.peakAbs, blockPeak);

        if (report.blocksUntilSilent < 0 && blockPeak < (double) silenceThreshold)
            report.blocksUntilSilent = block;
    }

    return report;
}

//==============================================================================
juce::String LifecycleReport::describe() const
{
    juce::String s;
    s << "double prepare " << (survivedDoublePrepare ? "stable" : "CHANGED OUTPUT")
      << ", rate change " << (sampleRateChangeHandled ? "handled" : "FAILED")
      << ", energy after reset " << juce::String (tailEnergyAfterReset, 9);

    if (stateRoundTripMismatch.isNotEmpty())
        s << ", state round trip: " << stateRoundTripMismatch;

    return s;
}

LifecycleReport exerciseLifecycle (juce::AudioProcessor& processor, RenderSpec spec)
{
    LifecycleReport report;

    const auto once = render (processor, spec);

    processor.prepareToPlay (spec.sampleRate, spec.blockSize);
    processor.prepareToPlay (spec.sampleRate, spec.blockSize);
    const auto twice = render (processor, spec);
    report.survivedDoublePrepare = compare (once, twice).sampleExact;

    // A rate change mid-session: the driver reports that it did not throw or produce non-finite
    // output. Whether the SOUND should be identical is a per-casting question and is not asked here.
    auto changed = spec;
    changed.sampleRate = spec.sampleRate * 2.0;
    const auto afterChange = render (processor, changed);

    report.sampleRateChangeHandled = true;
    for (const auto& channel : afterChange)
        for (auto v : channel)
            if (! std::isfinite (v))
                report.sampleRateChangeHandled = false;

    // Excite, reset, then measure what one silent block still contains.
    render (processor, spec);
    processor.reset();

    juce::AudioBuffer<float> buffer (spec.numChannels, spec.blockSize);
    juce::MidiBuffer midi;
    buffer.clear();
    processor.processBlock (buffer, midi);

    for (int ch = 0; ch < spec.numChannels; ++ch)
        report.tailEnergyAfterReset = juce::jmax (report.tailEnergyAfterReset,
                                                  (double) buffer.getMagnitude (ch, 0, spec.blockSize));

    // State round trip, by bytes. A mismatch is reported rather than asserted, because a processor
    // may legitimately re-serialise equivalent state differently.
    juce::MemoryBlock a, b;
    processor.getStateInformation (a);
    processor.setStateInformation (a.getData(), (int) a.getSize());
    processor.getStateInformation (b);

    if (a != b)
        report.stateRoundTripMismatch = "re-serialised to " + juce::String ((int) b.getSize())
                                      + " bytes from " + juce::String ((int) a.getSize());

    return report;
}

//==============================================================================
int measureImpulseLatency (juce::AudioProcessor& processor, RenderSpec spec, float threshold)
{
    auto impulse = spec;
    impulse.fillInput = [] (juce::AudioBuffer<float>& b, int blockIndex)
    {
        b.clear();

        if (blockIndex == 0)
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                b.setSample (ch, 0, 1.0f);
    };

    const auto out = render (processor, impulse);

    for (size_t i = 0; i < (out.empty() ? 0u : out[0].size()); ++i)
        for (const auto& channel : out)
            if (std::abs (channel[i]) > threshold)
                return (int) i;

    return -1;
}

} // namespace nf::testing

namespace nf::testing
{

juce::String DecayResult::describe() const
{
    juce::String s;
    s << "[requested " << juce::String (requestedSampleRate, 1)
      << " Hz, PREPARED AT " << juce::String (actualSampleRate, 1)
      << " Hz, block " << actualBlockSize << "] ";

    if (secondsToThreshold < 0.0)
        s << "never fell below threshold";
    else
        s << juce::String (secondsToThreshold, 4) << " s to threshold";

    s << ", peak " << juce::String (peakAbs, 6);
    return s;
}

DecayResult measureDecaySeconds (juce::AudioProcessor& processor,
                                 RenderSpec spec,
                                 int maxTailBlocks,
                                 float threshold)
{
    DecayResult r;
    r.requestedSampleRate = spec.sampleRate;

    render (processor, spec);

    // **Read back, always.** If the processor clamped the rate — or the harness failed to set it —
    // the seconds figure below would be computed against a rate that was never used, and a
    // rate-invariance sweep would report four identical values as a pass.
    r.actualSampleRate = processor.getSampleRate();
    r.actualBlockSize = processor.getBlockSize();

    juce::AudioBuffer<float> buffer (spec.numChannels, spec.blockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < maxTailBlocks; ++block)
    {
        buffer.clear();
        midi.clear();
        processor.processBlock (buffer, midi);

        double peak = 0.0;
        for (int ch = 0; ch < spec.numChannels; ++ch)
            peak = juce::jmax (peak, (double) buffer.getMagnitude (ch, 0, spec.blockSize));

        r.peakAbs = juce::jmax (r.peakAbs, peak);

        if (r.secondsToThreshold < 0.0 && peak < (double) threshold)
        {
            const auto samples = (double) ((block + 1) * spec.blockSize);
            r.secondsToThreshold = r.actualSampleRate > 0.0 ? samples / r.actualSampleRate : -1.0;
            break;
        }
    }

    return r;
}

}

namespace nf::testing
{

juce::String DenormalGuardReport::describe() const
{
    juce::String s;
    s << subnormalsIn << " subnormal samples in -> " << subnormalsOut << " out: ";
    s << (guardActive ? "guard ACTIVE" : "GUARD NOT COVERING THIS PATH");
    return s;
}

DenormalGuardReport probeDenormalGuard (juce::AudioProcessor& processor,
                                        double sampleRate,
                                        int blockSize,
                                        int numChannels)
{
    DenormalGuardReport report;

    processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer midi;

    // A value that IS subnormal: below FLT_MIN (1.175e-38) and above zero.
    const float subnormal = 1.0e-40f;

    // Warm up first, so a first-block transient is not what is measured.
    for (int i = 0; i < 4; ++i)
    {
        buffer.clear();
        midi.clear();
        processor.processBlock (buffer, midi);
    }

    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample (ch, i, subnormal);

    report.subnormalsIn = numChannels * blockSize;

    midi.clear();
    processor.processBlock (buffer, midi);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* read = buffer.getReadPointer (ch);

        for (int i = 0; i < blockSize; ++i)
            if (std::fpclassify (read[i]) == FP_SUBNORMAL)
                ++report.subnormalsOut;
    }

    report.guardActive = (report.subnormalsOut == 0);
    return report;
}

}

namespace nf::testing
{

void perturbByOneLsb (std::vector<std::vector<float>>& rendered, size_t channel, size_t index)
{
    if (channel >= rendered.size() || index >= rendered[channel].size())
        return;

    // std::nextafter moves by exactly one representable step, which is the smallest difference a
    // sample-exact comparison must still catch. A larger nudge would prove less.
    auto& v = rendered[channel][index];
    v = std::nextafter (v, std::numeric_limits<float>::infinity());
}

InvarianceResult compareRenders (const std::vector<std::vector<float>>& a,
                                 const std::vector<std::vector<float>>& b)
{
    return compare (a, b);
}

}

namespace nf::testing
{

juce::String SampleRateRow::describe() const
{
    juce::String s;
    s << "requested " << juce::String (requested, 1) << " Hz -> ADOPTED "
      << juce::String (adopted, 1) << " Hz, block " << adoptedBlockSize;

    if (! rateWasAdopted())
        s << "  <- **RATE NOT ADOPTED** — this row is a finding, not a measurement";

    s << ": ";
    s << (measuredSeconds >= 0.0 ? juce::String (measuredSeconds, 4) + " s"
                                 : juce::String ("never reached threshold"));
    return s;
}

std::vector<SampleRateRow> sampleRateSweep (juce::AudioProcessor& processor,
                                            RenderSpec spec,
                                            const std::vector<double>& rates,
                                            int maxTailBlocks,
                                            float threshold)
{
    std::vector<SampleRateRow> rows;
    rows.reserve (rates.size());

    for (auto rate : rates)
    {
        auto s = spec;
        s.sampleRate = rate;

        const auto decay = measureDecaySeconds (processor, s, maxTailBlocks, threshold);

        SampleRateRow row;
        row.requested = rate;
        row.adopted = decay.actualSampleRate;
        row.adoptedBlockSize = decay.actualBlockSize;
        row.measuredSeconds = decay.secondsToThreshold;
        rows.push_back (row);
    }

    return rows;
}

}

namespace nf::testing
{

std::vector<MagnitudeRow> measureMagnitudeResponse (const std::function<float (float)>& processOneSample,
                                                    const std::function<void()>& reset,
                                                    double sampleRate,
                                                    const std::vector<double>& frequenciesHz,
                                                    int settleCycles,
                                                    int measureCycles)
{
    std::vector<MagnitudeRow> rows;
    rows.reserve (frequenciesHz.size());

    for (auto hz : frequenciesHz)
    {
        if (reset != nullptr)
            reset();

        const double omega = juce::MathConstants<double>::twoPi * hz / sampleRate;
        const auto samplesPerCycle = sampleRate / hz;

        // Settle first: a filter's transient is not its steady-state response, and measuring
        // through it reports the transient.
        const auto settle = (int) (samplesPerCycle * settleCycles);
        for (int i = 0; i < settle; ++i)
            processOneSample ((float) std::sin (omega * i));

        // RMS over a whole number of cycles, so the window does not bias the result.
        const auto measure = (int) (samplesPerCycle * measureCycles);
        double sum = 0.0;

        for (int i = 0; i < measure; ++i)
        {
            const double y = processOneSample ((float) std::sin (omega * (settle + i)));
            sum += y * y;
        }

        const double rms = measure > 0 ? std::sqrt (sum / (double) measure) : 0.0;

        // A unit sine has RMS 1/sqrt(2); express the result relative to that so 0 dB is unity gain.
        const double gain = rms * juce::MathConstants<double>::sqrt2;
        rows.push_back ({ hz, gain > 0.0 ? 20.0 * std::log10 (gain) : -200.0 });
    }

    return rows;
}

double largestResponseDifferenceDb (const std::vector<MagnitudeRow>& a,
                                    const std::vector<MagnitudeRow>& b)
{
    double worst = 0.0;
    const auto n = juce::jmin (a.size(), b.size());

    for (size_t i = 0; i < n; ++i)
        worst = juce::jmax (worst, std::abs (a[i].gainDb - b[i].gainDb));

    return worst;
}

}

namespace nf::testing
{

std::vector<MagnitudeRow> measureProcessorMagnitudeResponse (juce::AudioProcessor& processor,
                                                             double sampleRate,
                                                             int blockSize,
                                                             const std::vector<double>& frequenciesHz,
                                                             int settleBlocks,
                                                             int measureBlocks)
{
    std::vector<MagnitudeRow> rows;
    rows.reserve (frequenciesHz.size());

    processor.setRateAndBufferSizeDetails (sampleRate, blockSize);

    for (auto hz : frequenciesHz)
    {
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        const double omega = juce::MathConstants<double>::twoPi * hz / sampleRate;
        juce::int64 n = 0;

        const auto fill = [&]
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (ch, i, (float) std::sin (omega * (double) (n + i)));
        };

        for (int b = 0; b < settleBlocks; ++b)
        {
            fill();
            n += blockSize;
            midi.clear();
            processor.processBlock (buffer, midi);
        }

        double sum = 0.0;
        juce::int64 count = 0;

        for (int b = 0; b < measureBlocks; ++b)
        {
            fill();
            n += blockSize;
            midi.clear();
            processor.processBlock (buffer, midi);

            const auto* read = buffer.getReadPointer (0);

            for (int i = 0; i < blockSize; ++i)
            {
                sum += (double) read[i] * (double) read[i];
                ++count;
            }
        }

        const double rms = count > 0 ? std::sqrt (sum / (double) count) : 0.0;
        const double gain = rms * juce::MathConstants<double>::sqrt2;
        rows.push_back ({ hz, gain > 0.0 ? 20.0 * std::log10 (gain) : -200.0 });
    }

    return rows;
}

}

namespace nf::testing
{

namespace
{
    TimingReport summarise (std::vector<double>& ns)
    {
        TimingReport r;
        if (ns.empty())
            return r;

        std::sort (ns.begin(), ns.end());
        r.samples = (int) ns.size();
        r.medianNs = ns[ns.size() / 2];
        r.p95Ns = ns[(size_t) ((double) ns.size() * 0.95)];
        r.maxNs = ns.back();
        return r;
    }
}

juce::String TimingReport::describe() const
{
    juce::String s;
    s << "median " << juce::String (medianNs / 1000.0, 2) << " us, p95 "
      << juce::String (p95Ns / 1000.0, 2) << " us, max "
      << juce::String (maxNs / 1000.0, 2) << " us over " << samples;
    return s;
}

TimingReport timeProcessBlock (juce::AudioProcessor& processor,
                               double sampleRate,
                               int blockSize,
                               int numChannels,
                               int blocks,
                               const std::function<void()>& contend)
{
    processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer midi;

    std::atomic<bool> running { true };
    std::thread contender;

    if (contend != nullptr)
        contender = std::thread ([&running, &contend]
        {
            while (running.load (std::memory_order_relaxed))
                contend();
        });

    // Warm up unmeasured, so a first-block transient is not the tail this reports.
    for (int i = 0; i < 16; ++i)
    {
        buffer.clear();
        midi.clear();
        processor.processBlock (buffer, midi);
    }

    std::vector<double> ns;
    ns.reserve ((size_t) blocks);

    for (int i = 0; i < blocks; ++i)
    {
        buffer.clear();
        midi.clear();

        const auto t0 = std::chrono::steady_clock::now();
        processor.processBlock (buffer, midi);
        const auto t1 = std::chrono::steady_clock::now();

        ns.push_back ((double) std::chrono::duration_cast<std::chrono::nanoseconds> (t1 - t0).count());
    }

    running.store (false, std::memory_order_relaxed);

    if (contender.joinable())
        contender.join();

    return summarise (ns);
}

TimingReport timeCallable (const std::function<void()>& call, int iterations)
{
    std::vector<double> ns;
    ns.reserve ((size_t) iterations);

    for (int i = 0; i < 16; ++i)
        call();

    for (int i = 0; i < iterations; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();
        call();
        const auto t1 = std::chrono::steady_clock::now();
        ns.push_back ((double) std::chrono::duration_cast<std::chrono::nanoseconds> (t1 - t0).count());
    }

    return summarise (ns);
}

}
