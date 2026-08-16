#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cmath>
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

    /** For `offlineAgainstRealtime` only: whether the processor actually reported itself
        non-realtime during the offline render.

        **Calling `setNonRealtime(true)` is not evidence that it took effect.** A processor that
        ignores it, or a JUCE version that changes the semantics, produces two identical renders that
        compare equal for the wrong reason — and "offline matches real-time" is exactly what that
        looks like. Read back, not assumed. */
    bool nonRealtimeWasHonoured = false;

    /** True when the two sides came from renders that were actually different in the way intended.
        Set by the drivers; a false here means the comparison proved nothing regardless of its
        verdict. */
    bool comparisonWasMeaningful = true;

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

/** Renders `spec`'s blocks WITHOUT preparing first — the caller owns the lifecycle.

    **This exists because `render` prepares by construction, and that made one sequence unreachable
    rather than merely untested.** `render` calls `setRateAndBufferSizeDetails`, `prepareToPlay` and
    `reset` on every invocation, so *prepare once → render → `reset()` → render* could not be
    expressed through this harness at all. Every premise check in the suite was therefore a **prepare**
    check, by construction, and nothing said so until `reproducibleAcrossReset` needed it.

    Use `render` unless you are deliberately controlling the lifecycle. This entry point makes no
    guarantee that the processor is in any particular state — that is the caller's business, which is
    the whole point of it.
*/
std::vector<std::vector<float>> renderBlocks (juce::AudioProcessor& processor, const RenderSpec& spec);

/** What `reproducibleAcrossReset` found: the reset arm, and the prepare arm it is read against. */
struct ResetReproducibility
{
    /** Two renders of one instance, each preceded by `reset()` and by nothing else. */
    InvarianceResult acrossReset;

    /** The same instance through the ordinary `render` path twice — `prepareToPlay` and `reset`
        before each. **Reported beside the result rather than checked once**, because a processor
        that cannot reproduce across prepare cannot be asked anything about reset, and the difference
        between the two arms is the whole finding. */
    InvarianceResult acrossPrepare;

    /** True when the prepare arm held, so the reset arm means what it claims. */
    bool premiseHeld() const noexcept { return acrossPrepare.sampleExact; }

    juce::String describe() const;
};

/** Does `reset()` return the processor to the same state, whatever that state is?

    Prepare once. Then `reset()`, render, `reset()`, render — and compare. **No `prepareToPlay`
    between the two renders**, which is the entire point and the reason `renderBlocks` exists.

    ## The invariant is deliberately narrow

    It asserts that `reset()` reaches a fixed point, **not** that `reset()` is equivalent to
    `prepareToPlay`. The wider claim is tempting and wrong for this suite: TapeRot re-arms a model
    switch on every *prepare* — 26.75 % of peak in FADE, 97.55 % in CLUNK — so a driver comparing
    reset against prepare would report that stage-2 defect here, in a driver written to ask about
    generators. Two renders both preceded by `reset()` cancel it out.

    ## What it is actually for

    Four generators across three castings are seeded in `prepare()` and nowhere else, so a host
    `reset()` — a transport locate, a buffer clear — leaves their streams running. Whether that is a
    defect or the correct contract was, until this driver, a question nobody could measure: the
    argument *"what a plugin owes a reset is a cleared tail, not a rewound hiss"* decides what
    `reset()` should do and establishes nothing about what it does.

    ## Two ways a green result would prove nothing

    **The arm must DRIVE the generator.** A generator inaudible at the casting's defaults reports
    reset-clean whatever `reset()` does — which is exactly how Fifth Member's and Elmer's
    energy-after-reset rows came back 0.000 twice for a coincidence. Set the parameters that engage
    it, the way a feedback arm engages feedback.

    **And read `premiseHeld()` first.** A configuration that is irreproducible across prepare —
    TapeRot with FAILURE up, whose engine is seeded nowhere and whose self-comparison is 0.914 — will
    fail the reset arm for a reason this driver is not asking about.
*/
ResetReproducibility reproducibleAcrossReset (juce::AudioProcessor& processor, const RenderSpec& spec);

/** Timing of a repeated call, in nanoseconds. Median and p95 rather than mean: a lock stall is a
    tail event, and a mean hides one block in fifty behind forty-nine quick ones. */
struct TimingReport
{
    double medianNs = 0.0;
    double p95Ns = 0.0;
    double maxNs = 0.0;
    int samples = 0;

    juce::String describe() const;
};

/** Times `processBlock` over many blocks, optionally while something else runs concurrently.

    **Two questions, and only one of them is answered by timing the quiet path.**

      1. *Does `processBlock`'s normal path take a lock?* Timing it alone answers this only in the
         sense that nothing contends — a lock taken with no contender is nearly free, so a quiet
         timing cannot distinguish "takes no lock" from "takes an uncontended one".
      2. *Does the audio thread reach a lock via `setCurrentProgram`?* This is the one with a finding
         already attached: `requestProgramChange` takes `pendingLock` and performs two `free()`s
         inside it, and VST3 can deliver a program change **on the audio thread**.

    So `contend` is run repeatedly on another thread for the duration. Passing the casting's own
    `setCurrentProgram` there makes the difference between the two timings the answer: if
    `processBlock` is lock-free with respect to that lock, hammering the other entry point cannot
    stall it.

    **"Lock-free in steady state" and "lock-free" are different claims and only one is true here.**
    A probe that timed the quiet path alone would report the stronger one.
*/
TimingReport timeProcessBlock (juce::AudioProcessor& processor,
                               double sampleRate,
                               int blockSize,
                               int numChannels,
                               int blocks,
                               const std::function<void()>& contend = {});

/** Times a callable directly — for the entry point VST3 may deliver on the audio thread. */
TimingReport timeCallable (const std::function<void()>& call, int iterations);

/** Deliberately corrupts one sample of a rendered result, for proving a comparison can fail.

    **A driver that cannot demonstrate its own failure has not demonstrated anything**, and
    `blockSizeInvariance` makes the strongest claim in this harness: sample-exact across four block
    sizes. The ways that can be falsely true are all quiet — a render that never varied, a comparison
    against itself, a tolerance wide enough to swallow the difference, a driver that prepared once and
    reported four times.

    Sample-exactness means the tolerance is zero, so a one-LSB perturbation must register. If it does
    not, the comparison is not what it says it is.

    @param channel  which channel to disturb
    @param index    which sample
*/
void perturbByOneLsb (std::vector<std::vector<float>>& rendered, size_t channel, size_t index);

/** Compares two rendered results. Exposed so a test can prove the comparison detects a difference. */
InvarianceResult compareRenders (const std::vector<std::vector<float>>& a,
                                 const std::vector<std::vector<float>>& b);

/** One rate in a sample-rate sweep: what was asked for, what the processor adopted, and what a
    duration-bearing quantity measured there.

    **A rate that was requested but not adopted is a FINDING, not a row to drop.** A casting that
    clamps 192 kHz is making a statement about what it supports, and silently skipping it would
    present as a clean sweep. `adopted` is read off the processor, never echoed from the request.
*/
struct SampleRateRow
{
    double requested = 0.0;
    double adopted = 0.0;
    int adoptedBlockSize = 0;
    double measuredSeconds = -1.0;   ///< the duration-bearing quantity, in ITS OWN units

    bool rateWasAdopted() const noexcept { return std::abs (adopted - requested) < 1.0; }
    juce::String describe() const;
};

/** Sweeps sample rates and measures a decay in SECONDS at each, reading the adopted rate back.

    **Sample-exact is the wrong bar across rates and would fail every correct casting** — the same
    seconds of audio at 44.1 and 96 kHz cannot be identical sample-for-sample. What must hold is that
    a quantity with a duration keeps its value in its own units: an RT60 of 4.8 s stays 4.8 s.

    The caller decides what "excited" means and what threshold counts as decayed, because which of
    its parameters carry a duration is the casting's knowledge, not core's.
*/
std::vector<SampleRateRow> sampleRateSweep (juce::AudioProcessor& processor,
                                            RenderSpec spec,
                                            const std::vector<double>& rates,
                                            int maxTailBlocks = 8000,
                                            float threshold = 1.0e-5f);

/** One probe frequency and the gain the filter actually applied there. */
struct MagnitudeRow
{
    double frequencyHz = 0.0;
    double gainDb = 0.0;
};

/** Measures a filter's real magnitude response, by playing sine tones through it and reading the
    output level — not by reading its coefficients back.

    **Coefficient readback would answer the wrong question.** A filter that computes its coefficients
    from a normalised frequency rather than from `fs` still *reports* the cutoff it was asked for; it
    is the response that moves. "A filter that reports its intended cutoff while computing a
    different one" is the whole shape this category is about, so the measurement has to be of
    behaviour.

    **Why this matters most at the extremes.** A normalised-frequency bug is exactly right at the rate
    it was written for and wrong in proportion to the rate ratio. 44.1 -> 48 kHz moves it by 9 %,
    which hides inside any sane tolerance; 44.1 -> 192 kHz moves it by a factor of four and a bit,
    which cannot be missed. Sweep the extremes, not the neighbours.

    `processOneSample` is the casting's own filter, wrapped — core cannot know what a casting's
    filter is, and several of them (Elmer's sidechain HP, Gatecrasher's trigger HP/LP, Reflect-84's
    damping) never reach the plugin's output at all, so a render-based measurement could not see
    them.

    @param processOneSample  one sample in, one sample out, already prepared at `sampleRate`
    @param reset             called before each tone so the filter starts from a known state
*/
std::vector<MagnitudeRow> measureMagnitudeResponse (const std::function<float (float)>& processOneSample,
                                                   const std::function<void()>& reset,
                                                   double sampleRate,
                                                   const std::vector<double>& frequenciesHz,
                                                   int settleCycles = 40,
                                                   int measureCycles = 40);

/** The same measurement, driven through a whole `AudioProcessor` rather than a per-sample callable.

    **This exists to validate the callable, not to replace it.** Four of the suite's five cutoffs
    never reach their plugin's output — Elmer's sidechain HP feeds the detector, Gatecrasher's two
    feed the gate, Reflect-84's damping sits inside the tanks — so for those the callable is the only
    instrument that works, and an instrument that cannot be cross-checked is one that has to be
    trusted.

    **TapeRot is the single opportunity to check it.** Its `ToneFilters` sit on the audio path, so its
    cutoffs are reachable both ways. Measuring both and comparing either validates the callable
    against a path where an independent check is possible — and the four callable-only cases inherit
    that — or produces a finding about the instrument before any of its results are believed. Every
    other instrument in this sweep has had to earn its results the same way.

    Block-oriented, so it measures the processor as a host drives it.
*/
std::vector<MagnitudeRow> measureProcessorMagnitudeResponse (juce::AudioProcessor& processor,
                                                             double sampleRate,
                                                             int blockSize,
                                                             const std::vector<double>& frequenciesHz,
                                                             int settleBlocks = 24,
                                                             int measureBlocks = 24);

/** The largest difference, in dB, between two responses measured at the same frequencies.

    **A screen for "did anything change", and NOT a classifier. Do not rank a finding with it.**

    This comment used to end "if the curve moved, the cutoff moved", and that sentence is false in
    the one case the function gets reached for. It collapses a whole curve to a single number, so a
    corner that has SHIFTED and a far field whose SHAPE differs come back indistinguishable —
    and a one-pole's far field legitimately depends on normalised frequency, so two correct filters
    at two sample rates differ there by design.

    Measured, on Reflect-84's dampHF at 2 kHz: this function returned 1.861 dB and was read as a
    moved cutoff. The corner is -2.981 dB at 44.1 kHz and -3.009 dB at 192 kHz — correct at both,
    to 0.03 dB. The entire 1.861 dB was the far field at 16 kHz, which is 0.36 of Nyquist at one
    rate and 0.083 at the other. A range defect would have been filed as a rate defect.

    **To ask where a corner is, measure AT the corner**: a correct one-pole reads -3.01 dB at its
    own cutoff whatever the sample rate. Report the curves too — they are what makes the corner and
    the far field separable — but classify from the corner.

    Second instance of this shape in the suite, after gradient-per-pixel. Aggregates are where it
    keeps happening: one number reads as a finding, survives review because it is precise, and says
    nothing about the axis it collapsed.
*/
double largestResponseDifferenceDb (const std::vector<MagnitudeRow>& a,
                                    const std::vector<MagnitudeRow>& b);

/** Renders the same input offline and real-time, and compares.

    `setNonRealtime(true)` is a hint a processor may legitimately act on — a higher-quality path, a
    different oversampling factor — so a difference here is **not automatically a defect**. The
    driver reports; the casting decides.
*/
InvarianceResult offlineAgainstRealtime (juce::AudioProcessor& processor, RenderSpec spec);

//==============================================================================
/** What a denormal-guard probe saw. */
struct DenormalGuardReport
{
    int subnormalsIn = 0;      ///< how many subnormal samples were fed in
    int subnormalsOut = 0;     ///< how many survived to the output
    bool guardActive = false;

    juce::String describe() const;
};

/** Feeds SUBNORMAL input and reports whether the processor's guard flushed it.

    **This asserts the mechanism the whole suite rests on.** `ScopedNoDenormals` is one line in one
    file per casting, and no DSP stage in the suite carries its own guard — so every decaying path in
    every plugin is covered by a single statement that, until this existed, nothing asserted and
    nothing tested. A floor in one filter defends one site; this defends all six.

    **How it works, and why it is a real check rather than a restatement.** `ScopedNoDenormals` sets
    the CPU's flush-to-zero mode, which on every platform this suite targets also treats subnormal
    *inputs* as zero. So a subnormal fed into a guarded `processBlock` cannot survive to the output;
    fed into an unguarded one, an ordinary passthrough preserves it.

    It therefore fails if the guard is **removed**, **narrowed** to part of the function, or a path is
    **scoped past it** — the three ways one line stops covering what it appears to.

    Proved by causing it: core's own tests run this against a probe processor with the guard and
    without, and the two must disagree. A guard-checker that cannot tell them apart is exactly the
    class of check this project keeps finding.
*/
DenormalGuardReport probeDenormalGuard (juce::AudioProcessor& processor,
                                        double sampleRate = 48000.0,
                                        int blockSize = 512,
                                        int numChannels = 2);

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
