#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <functional>
#include <vector>

/**
    Measurement infrastructure for the suite-wide bug sweep.

    ## This is a widening of the extraction's §I, and it is declared as one

    §I lists what must not be extracted, and the extraction's own rule is to **stop and report rather
    than widen** when a stage starts pulling something across the line. Core was scoped as *shipping
    behaviour*; a measurement harness is not that, so putting one here crosses the line and the
    crossing is deliberate rather than accidental.

    It is admitted by the same three-part test every other extraction passed:

      - **It is behaviour, not appearance.** Drivers, detectors and scanners; nothing here draws.
      - **It is identical across all six castings.** "Does this allocate on the audio thread" is one
        question asked six times, which is precisely the shape that produced six diverging copies of
        everything else this library now owns.
      - **It has no per-casting meaning in it.** See the boundary below.

    Stated here, at the exemption, on the model of `NO_REDIRECT_NEEDED` in `tools/run_tests.py`:
    named, with the reason beside it, so nobody later has to reconstruct why test-support code lives
    in a library that ships.

    ## The boundary: mechanism here, meaning in the casting

    | Core owns | The casting owns |
    |---|---|
    | Arming a detector around `processBlock` and reporting what it saw | Which of its parameters are momentary, and what its Programs mean |
    | Rendering at several block sizes and comparing them | What its decay *should* measure, and at what tolerance |
    | Scanning a buffer for subnormals, NaN and Inf | Which of its paths decay, and which deliberately generate |
    | Driving a lifecycle sequence and reporting what changed | Whether a thing surviving `reset()` is a defect or correct |

    **Nothing here may know what a Program contains, what a parameter means, or what a casting's
    correct answer is.** A driver returns measurements; the casting's own `Tests/` asserts on them.
    That is the same split core already draws everywhere else, and it is what keeps this from
    becoming six castings' expectations in a shared file.

    **Linked by castings' test targets only, never by the plugin.** No test-only code enters a
    shipping binary.
*/
namespace nf::testing
{

//==============================================================================
/** Arms a global allocation detector for its lifetime, for use around `processBlock`.

    **Why a detector rather than a code read.** Category 1's whole point is that a heap allocation on
    the audio thread is a dropout no unit test sees — it does not fail, it does not assert, it makes a
    click under load on someone else's machine. Reading `processBlock` finds the obvious cases and
    misses everything reached through a call it did not follow.

    The three leads this was built for are all **conditional**: `dryBuffer.setSize` in Chorus-60 and
    Gatecrasher, and Gatecrasher's two scratch `resize` calls, each of which allocates only when a
    host delivers more samples than it declared. A code read cannot settle those, because the
    condition is what is in question. Preparing for one block size and driving a larger one can.

    **Report a clean result as loudly as a dirty one.** "Prepared for 512, driven at 2048, no
    allocation observed" is a result, and the plan requires it at the same weight as a defect —
    otherwise the next audit re-derives the same suspicion from the same lines.

    Not thread-safe by design: arm it on the thread you are about to measure. It counts allocations
    process-wide while armed, so nothing else may be running.
*/
class AllocationSentinel
{
public:
    AllocationSentinel();
    ~AllocationSentinel();

    /** Allocations observed since construction. */
    int count() const noexcept;

    /** Bytes requested since construction, for reporting a size alongside a count. */
    size_t bytes() const noexcept;

    /** Frees observed since construction.

        **A `free()` on the audio thread is the same defect class as a `malloc()`** — it can take the
        allocator's lock and it can block. Counting only allocations misses a whole family: releasing
        the last reference to a refcounted object, a container shrinking, a `juce::String` assignment
        dropping its old buffer. That last one is not hypothetical here; see the ProgramId note in
        the suite's bug-sweep plan.
    */
    int frees() const noexcept;

    /** True if anything allocated OR freed. */
    bool sawAllocation() const noexcept { return count() > 0; }
    bool sawAnyHeapActivity() const noexcept { return count() > 0 || frees() > 0; }

private:
    AllocationSentinel (const AllocationSentinel&) = delete;
    AllocationSentinel& operator= (const AllocationSentinel&) = delete;
};

/** What an allocation probe saw. */
struct AllocationReport
{
    int preparedBlockSize = 0;
    int drivenBlockSize = 0;
    int allocations = 0;
    size_t bytes = 0;
    int frees = 0;

    /** **No heap activity at all — allocations AND frees.**

        This deliberately counts both, and it was not always so. The first version reported only
        allocations, and every "clean" result taken with it was measured against half the question: a
        detector blind to `free()` is blind to a refcount reaching zero, a container shrinking, and a
        smart pointer going out of scope, all of which take the allocator's lock and can block —
        which is the property being tested.

        Every casting was re-measured when this changed. A measurement taken with an instrument since
        found incomplete does not carry forward on the strength of having been taken.
    */
    bool clean() const noexcept { return allocations == 0 && frees == 0; }

    /** The older, narrower question, kept so a report can say which one a row answers. */
    bool cleanOfAllocations() const noexcept { return allocations == 0; }

    juce::String describe() const;
};

/** Prepares at one block size, drives `processBlock` at another, and counts allocations.

    **This exists because `render()` cannot be measured.** `render` builds and grows vectors by
    construction, so arming the sentinel around it counts the harness rather than the processor. The
    sentinel here is armed around the `processBlock` call and nothing else — buffers allocated
    before, results read after.

    **`preparedBlockSize < drivenBlockSize` is the case the three leads need.** Chorus-60's and
    Gatecrasher's `dryBuffer.setSize`, and Gatecrasher's two scratch `resize` calls, all pass
    `avoidReallocating` and are sized in `prepareToPlay` — so they allocate only when a host delivers
    more samples than it declared, which is exactly what this reproduces. Call it with equal sizes
    too: the difference between the two runs is the finding.

    `warmUpBlocks` runs first, unmeasured, because a first block legitimately allocates in places a
    steady state does not — lazily-built lookup tables, a first-touch buffer — and counting those
    would report a defect where there is a one-off.
*/
AllocationReport probeProcessBlockAllocation (juce::AudioProcessor& processor,
                                              double sampleRate,
                                              int preparedBlockSize,
                                              int drivenBlockSize,
                                              int numChannels,
                                              int measuredBlocks = 8,
                                              int warmUpBlocks = 4);

//==============================================================================
/** How to drive a processor: the casting supplies this, because bus layout is its own business. */
struct RenderSpec
{
    double sampleRate = 48000.0;
    int blockSize = 512;
    int numChannels = 2;
    int numBlocks = 64;

    /** Filled per block before processing. Defaults to a deterministic pseudo-noise so two renders
        of the same spec are comparable; a casting can substitute an impulse or silence. */
    std::function<void (juce::AudioBuffer<float>&, int blockIndex)> fillInput;
};

/** Renders `spec` and returns the concatenated output, one vector per channel.

    Deterministic: the default input is a fixed-seed sequence, so the same spec twice gives the same
    samples and any difference is the processor's.
*/
std::vector<std::vector<float>> render (juce::AudioProcessor& processor, const RenderSpec& spec);

//==============================================================================
/** What an invariance comparison found. */
struct InvarianceResult
{
    bool sampleExact = false;
    double maxAbsDifference = 0.0;
    int firstDivergentSample = -1;
    int comparedSamples = 0;

    /** **What the processor actually prepared at, read back from it — not what the loop asked for.**

        Every driver here sweeps something, and every sweep has the same failure available: it
        collapses to one value while still reporting a full set of results. That happened once
        already — an algorithm loop passed a raw index where a normalised value belongs, so three of
        four iterations selected the same choice, and the ONLY tell was three identical figures.

        So each driver logs the varied quantity as the thing itself, read off the processor, rather
        than as the loop variable. A collapsed sweep then shows up as repeated values in the log
        instead of as a plausible result.
    */
    int actualBlockSize = 0;
    double actualSampleRate = 0.0;

    /** For a report line. Never a verdict — whether a difference matters is the casting's call. */
    juce::String describe() const;
};

/** Renders the same input at `blockSizes` and compares each against the first.

    **511 belongs in that list and is not arbitrary**: it is prime, so it catches any assumption that
    a block divides evenly into an internal chunk — the failure a 64/128/2048 sweep walks straight
    past because all three share factors.

    The processor is re-prepared for each size, and the input is regenerated from the same seed, so
    the sample stream is identical regardless of how it is cut into blocks.
*/
std::vector<InvarianceResult> blockSizeInvariance (juce::AudioProcessor& processor,
                                                   RenderSpec spec,
                                                   const std::vector<int>& blockSizes);

/** Renders the same input offline and real-time, and compares.

    `setNonRealtime(true)` is a hint a processor may legitimately act on — a higher-quality path, a
    different oversampling factor — so a difference here is **not automatically a defect**. The
    driver reports; the casting decides.
*/
InvarianceResult offlineAgainstRealtime (juce::AudioProcessor& processor, RenderSpec spec);

//==============================================================================
/** A decay measured in SECONDS, with the rate it was measured at read back from the processor. */
struct DecayResult
{
    double requestedSampleRate = 0.0;
    double actualSampleRate = 0.0;    ///< read back — a sweep that collapsed shows as repeats
    int actualBlockSize = 0;
    double secondsToThreshold = -1.0; ///< -1 if it never fell below
    double peakAbs = 0.0;

    juce::String describe() const;
};

/** Excites the processor, then measures how long the tail takes to fall below `threshold`.

    **In seconds, not sample counts.** An RT60 of 4.8 s must stay 4.8 s at every rate; comparing
    sample counts would call a correct processor broken and a rate-dependent one fine.
*/
DecayResult measureDecaySeconds (juce::AudioProcessor& processor,
                                 RenderSpec spec,
                                 int maxTailBlocks = 8000,
                                 float threshold = 1.0e-5f);

/** What the numerical scanner found in a rendered tail. */
struct NumericalReport
{
    int subnormals = 0;
    int nans = 0;
    int infinities = 0;
    double peakAbs = 0.0;
    int blocksUntilSilent = -1;   ///< -1 if it never fell below the threshold
    double actualSampleRate = 0.0;
    int actualBlockSize = 0;

    bool clean() const noexcept { return subnormals == 0 && nans == 0 && infinities == 0; }
    juce::String describe() const;
};

/** Drives `spec`, then feeds silence for `tailBlocks` and scans everything that comes out.

    **`tailBlocks` must be thousands, not tens.** Denormals appear in a decaying path long after the
    input stops — that is what "decaying" means — so a short tail scans the part of the signal where
    the values are still large enough to be normal, and reports clean.

    Whether a casting *should* fall silent is not asked here: TapeRot generates hiss and hum
    deliberately, so `blocksUntilSilent == -1` is correct for it and a defect elsewhere.
*/
NumericalReport scanTail (juce::AudioProcessor& processor,
                          RenderSpec spec,
                          int tailBlocks,
                          float silenceThreshold = 1.0e-7f);

//==============================================================================
/** What changed across a lifecycle sequence. */
struct LifecycleReport
{
    bool survivedDoublePrepare = false;   ///< output after preparing twice matched preparing once
    bool sampleRateChangeHandled = false;
    double tailEnergyAfterReset = 0.0;    ///< what `reset()` left behind
    juce::String stateRoundTripMismatch;  ///< empty when the round trip was clean

    juce::String describe() const;
};

/** `prepareToPlay` twice, a sample-rate change mid-session, `reset()`, and a state round-trip.

    **`tailEnergyAfterReset` is reported, never judged.** A reverb tail surviving `reset()` is a
    defect; a Program selection surviving is correct. Core cannot tell those apart and does not try.
*/
LifecycleReport exerciseLifecycle (juce::AudioProcessor& processor, RenderSpec spec);

//==============================================================================
/** Renders an impulse and reports where it first emerges, for verifying declared latency.

    Returns the sample index of the first output above `threshold`, or -1 if nothing emerged.

    **This is the check that five castings' "no latency" claims need**, because a declaration of zero
    is a claim like any other. Compare against `processor.getLatencySamples()`: they must agree.
*/
int measureImpulseLatency (juce::AudioProcessor& processor,
                           RenderSpec spec,
                           float threshold = 1.0e-4f);

} // namespace nf::testing
