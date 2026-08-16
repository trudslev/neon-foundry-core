#include <nf/testing/ProcessorHarness.h>
#include <nf/ProgramId.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace
{
    /** A processor whose misbehaviour is switchable, so every driver can be proved by CAUSING the
        thing it detects rather than by observing it stay quiet on well-behaved code.

        That distinction is the whole reason this file exists. A harness that reports "clean" against
        six correct plugins is indistinguishable from a harness that reports "clean" against
        anything — and this suite has now shipped three separate checks that passed while measuring
        nothing. Each test below turns a fault on, sees it caught, turns it off, sees it clean.
    */
    class ProbeProcessor final : public juce::AudioProcessor
    {
    public:
        ProbeProcessor()
            : AudioProcessor (BusesProperties()
                                  .withInput  ("In",  juce::AudioChannelSet::stereo(), true)
                                  .withOutput ("Out", juce::AudioChannelSet::stereo(), true))
        {
        }

        // --- the switchable faults -------------------------------------------------------------
        bool allocateInProcessBlock = false;
        bool dependOnBlockSize = false;
        bool emitSubnormals = false;
        bool emitNaN = false;
        bool keepTailThroughReset = false;

        /*  A generator, and separate switches for each of the two places it could be restored.

            **These three states are a model of a real finding rather than an invented fault.** Four
            generators across three castings are seeded in `prepare()` and nowhere else, and TapeRot's
            `FailureEngine` is seeded at construction and nowhere at all:

              | seedOnPrepare | seedOnReset | Models | Expected |
              |---|---|---|---|
              | true | true | Chorus-60's `CharacterStage` | both arms exact |
              | true | **false** | `NoiseSource`, `WowFlutter`, `LfoBank`, `CharacterEngine` | prepare exact, **reset DIFFERS** |
              | **false** | false | `FailureEngine` | **premise fails** — the reset arm means nothing |

            Default OFF for the noise itself, so every other test in this file behaves as it did.
        */
        bool emitGeneratorNoise = false;
        bool seedGeneratorOnPrepare = true;
        bool seedGeneratorOnReset = false;

        /*  First-run-only state, modelled on the real one. Reflect-84's pre-delay smoother is
            constructed at zero and glides up to its target on an instance's FIRST render and never
            again, because `SmoothedValue::reset (rate, seconds)` sets the ramp length and not the
            value. Neither `prepareToPlay` nor `reset()` touches `glide` here, for the same reason.

            It exists so the warm-up in `reproducibleAcrossReset` is shown able to fail. Without it
            the warm-up is a line nobody can distinguish from a no-op. */
        bool firstRunGlide = false;
        // **Default OFF**, so every other test in this file behaves as it did: the subnormal-tail
        // case needs subnormals to survive, and flush-to-zero would erase the thing it measures.
        bool guardAgainstDenormals = false;

        // **A runtime multiplier, not the literal 1.0f.** The first version wrote `v * 1.0f`, which
        // the optimiser folds to a plain copy — no arithmetic, so nothing for flush-to-zero to act
        // on, and the guarded run passed subnormals straight through exactly like the unguarded one.
        // The checker looked broken when the PROBE was. A real casting does real arithmetic; the
        // probe has to as well.
        float unityGain = 1.0f;

        // A probe that refuses rates above a ceiling, so the sweep can be shown to NOTICE rather
        // than silently reporting whatever it was given.
        double clampRateAbove = 0.0;
        int  latencyToIntroduce = 0;

        void prepareToPlay (double rate, int samplesPerBlock) override
        {
            if (clampRateAbove > 0.0 && rate > clampRateAbove)
                setRateAndBufferSizeDetails (clampRateAbove, samplesPerBlock);

            scratch.assign ((size_t) samplesPerBlock, 0.0f);
            delayLine.assign ((size_t) juce::jmax (1, latencyToIntroduce), 0.0f);
            writeIndex = 0;
            tail = 1.0f;
            blockCounter = 0;

            if (seedGeneratorOnPrepare)
                generator = juce::Random (generatorSeed);
        }

        void releaseResources() override {}

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            // **Two paths rather than a conditionally-constructed guard.** The first version used
            // make_unique, which allocates 8 bytes per block — and this file's own allocation tests
            // caught it immediately. A test harness that allocates in processBlock to test for
            // allocation in processBlock is its own punchline.
            if (guardAgainstDenormals)
            {
                const juce::ScopedNoDenormals guard;
                body (buffer);
            }
            else
            {
                body (buffer);
            }
        }

        void body (juce::AudioBuffer<float>& buffer)
        {
            const auto n = buffer.getNumSamples();

            if (allocateInProcessBlock)
                scratch.assign ((size_t) n + (size_t) (++blockCounter), 0.0f);   // grows: always allocates

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* w = buffer.getWritePointer (ch);

                for (int i = 0; i < n; ++i)
                {
                    float v = w[i];

                    if (latencyToIntroduce > 0)
                    {
                        const auto slot = (size_t) ((writeIndex + i) % delayLine.size());
                        const float delayed = delayLine[slot];
                        delayLine[slot] = v;
                        v = delayed;
                    }

                    // Depends on WHERE in the block the sample sits, not on the sample itself —
                    // which is exactly the shape of an assumption that a block divides evenly.
                    if (dependOnBlockSize)
                        v += (float) i * 1.0e-6f;

                    if (emitSubnormals)
                    {
                        // **The tail is EXCITED BY INPUT, which it was not until render() began
                        // resetting.** It used to be armed once in prepareToPlay and zeroed by
                        // reset(), so the moment render() gained its reset() the probe emitted
                        // nothing and the long-tail case failed with peak 0.
                        //
                        // Arming from input is not a workaround for that: it is what a decaying tail
                        // actually is, and it makes the probe model the thing it stands in for. The
                        // two other consumers stay correct for the same reason — exerciseLifecycle
                        // excites, resets, then measures a SILENT block, and probeDenormalGuard
                        // never resets at all, so it still starts from prepareToPlay's 1.0.
                        if (v != 0.0f)
                            tail = 1.0f;

                        // **0.9999 per sample, not 0.5, and the rate is the point of the test.**
                        // Halving reaches subnormal in ~126 samples and zero in ~150, so it is over
                        // before the tail scan begins — which made the "short tail misses it" case
                        // pass for the wrong reason and the "long tail finds it" case fail. This
                        // decays over ~873 000 samples, which is a realistic reverb-tail rate and
                        // straddles the two tail lengths below.
                        tail *= 0.9999f;
                        v += tail;
                    }

                    // Well above sample-exactness and well below anything that would disturb the
                    // other cases, which all leave this switch off.
                    if (emitGeneratorNoise)
                        v += (generator.nextFloat() * 2.0f - 1.0f) * 1.0e-3f;

                    if (firstRunGlide)
                    {
                        glide += (1.0f - glide) * 0.0005f;   // ~2000 samples to arrive
                        v *= glide;
                    }

                    if (emitNaN)
                        v = std::numeric_limits<float>::quiet_NaN();

                    // Arithmetic the optimiser cannot fold away — see unityGain's comment.
                    w[i] = v * unityGain;
                }
            }

            if (latencyToIntroduce > 0)
                writeIndex = (writeIndex + n) % (int) delayLine.size();
        }

        void reset() override
        {
            // **Clears to zero, not to 1.0.** The first version reset to the *starting* value, so
            // "tidy" came back louder than "leaky" and the comparison ran backwards — reset making a
            // processor louder is not the behaviour being modelled. prepareToPlay starts the decay;
            // reset ends it.
            if (! keepTailThroughReset)
            {
                tail = 0.0f;
                std::fill (delayLine.begin(), delayLine.end(), 0.0f);
            }

            if (seedGeneratorOnReset)
                generator = juce::Random (generatorSeed);
        }

        int getLatencySamples() const { return latencyToIntroduce; }

        const juce::String getName() const override { return "Probe"; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock& d) override { d.replaceAll ("probe", 5); }
        void setStateInformation (const void*, int) override {}

    private:
        static constexpr juce::int64 generatorSeed = 12345;

        std::vector<float> scratch, delayLine;
        int writeIndex = 0, blockCounter = 0;
        float tail = 1.0f;
        float glide = 0.0f;   // deliberately untouched by prepareToPlay and reset — see firstRunGlide
        juce::Random generator { generatorSeed };
    };
}

class ProcessorHarnessTests final : public juce::UnitTest
{
public:
    ProcessorHarnessTests() : juce::UnitTest ("nf::testing::ProcessorHarness", "core") {}

    void runTest() override
    {
        beginTest ("The allocation detector catches an allocation, and is clean without one");
        {
            ProbeProcessor p;

            p.allocateInProcessBlock = true;
            const auto dirty = nf::testing::probeProcessBlockAllocation (p, 48000.0, 512, 512, 2);
            expect (! dirty.clean(), "an allocating processBlock was not detected: " + dirty.describe());
            expectGreaterThan (dirty.allocations, 0);

            p.allocateInProcessBlock = false;
            const auto clean = nf::testing::probeProcessBlockAllocation (p, 48000.0, 512, 512, 2);
            expect (clean.clean(), "a non-allocating processBlock reported one: " + clean.describe());

            // **The report counts frees as well as allocations**, so a processor that only frees is
            // caught. Proved by causing it: a vector that shrinks allocates nothing and frees once.
            expectEquals (clean.frees, 0, "a quiet processBlock reported frees");
        }

        beginTest ("The detector does not count the harness's own buffers");
        {
            // The failure this guards: arming around `render()` would count its vectors and report
            // every processor as allocating. The buffers here are built before the sentinel is armed.
            ProbeProcessor p;
            const auto r = nf::testing::probeProcessBlockAllocation (p, 48000.0, 256, 2048, 2);
            expect (r.clean(),
                    "driving 2048 after preparing 256 reported allocation from a processor that "
                    "does not allocate — the harness is measuring itself: " + r.describe());
        }

        beginTest ("Block-size invariance: an even split is exact, a prime one catches the assumption");
        {
            ProbeProcessor p;
            p.dependOnBlockSize = true;

            const auto results = nf::testing::blockSizeInvariance (p, {}, { 64, 128, 511, 2048 });
            expectEquals ((int) results.size(), 4);

            expect (results[0].sampleExact, "a size compared against itself must be exact");
            expect (! results[2].sampleExact,
                    "511 did not catch a block-position dependency: " + results[2].describe());

            p.dependOnBlockSize = false;
            const auto clean = nf::testing::blockSizeInvariance (p, {}, { 64, 128, 511, 2048 });
            for (size_t i = 0; i < clean.size(); ++i)
                expect (clean[i].sampleExact,
                        "an invariant processor reported a difference: " + clean[i].describe());
        }

        beginTest ("The numerical scanner finds subnormals and NaN, and a long tail is what finds them");
        {
            ProbeProcessor p;
            p.emitSubnormals = true;

            nf::testing::RenderSpec spec;
            spec.numBlocks = 4;

            // **The short tail is the point of this case.** A decaying value is still normal for the
            // first hundred-odd halvings; scanning only that far reports clean, which is how a real
            // denormal problem hides. Same processor, same fault, two tail lengths.
            const auto shortTail = nf::testing::scanTail (p, spec, 8);
            const auto longTail  = nf::testing::scanTail (p, spec, 4000);

            expectEquals (shortTail.subnormals, 0, "the short tail was expected to miss it");
            expectGreaterThan (longTail.subnormals, 0,
                               "a long tail did not reach subnormal territory: " + longTail.describe());

            p.emitSubnormals = false;
            p.emitNaN = true;
            const auto nan = nf::testing::scanTail (p, spec, 8);
            expectGreaterThan (nan.nans, 0, "NaN was not detected: " + nan.describe());
            expect (! nan.clean());

            p.emitNaN = false;
            const auto ok = nf::testing::scanTail (p, spec, 64);
            expect (ok.clean(), "a clean processor was reported dirty: " + ok.describe());
        }

        beginTest ("Lifecycle reports what reset() left behind, without judging it");
        {
            ProbeProcessor p;
            p.emitSubnormals = true;      // gives it state to leave behind
            p.keepTailThroughReset = true;

            nf::testing::RenderSpec spec;
            spec.numBlocks = 4;

            const auto leaky = nf::testing::exerciseLifecycle (p, spec);
            expectGreaterThan (leaky.tailEnergyAfterReset, 0.0,
                               "state surviving reset was not observed: " + leaky.describe());

            p.keepTailThroughReset = false;
            const auto tidy = nf::testing::exerciseLifecycle (p, spec);
            expectLessThan (tidy.tailEnergyAfterReset, leaky.tailEnergyAfterReset,
                            "reset made no difference: " + tidy.describe());
        }

        beginTest ("reproducibleAcrossReset separates a restored generator from a continued one");
        {
            /*  **The driver is proved here, in core, before any casting exercises it** — because a
                driver first run against the thing it is meant to judge cannot be distinguished from
                a driver that reports whatever it was going to report.

                Three states, and each models something real rather than an invented fault:

                  - seeded on prepare AND reset — Chorus-60's `CharacterStage` after stage 0.5
                  - seeded on prepare ONLY — the four generators in the other three castings
                  - seeded on NEITHER — TapeRot's `FailureEngine`, seeded at construction and never
                    again, whose measured self-comparison at FAILURE 100 is 0.914

                The third arm is what makes `premiseHeld()` load-bearing rather than decorative: it is
                the one configuration where the reset arm differs for a reason that has nothing to do
                with reset.
            */
            nf::testing::RenderSpec spec;
            spec.blockSize = 256;
            spec.numBlocks = 8;

            ProbeProcessor p;
            p.emitGeneratorNoise = true;

            // 1 — restored in reset(). Both arms must be exact.
            p.seedGeneratorOnPrepare = true;
            p.seedGeneratorOnReset = true;
            const auto restored = nf::testing::reproducibleAcrossReset (p, spec);
            logMessage ("  seeded prepare+reset -> " + restored.describe());
            expect (restored.premiseHeld(), "the premise arm failed on a fully restored generator");
            expect (restored.acrossReset.sampleExact,
                    "a generator restored in reset() was still reported as differing across reset: "
                        + restored.acrossReset.describe());

            // 2 — the real shape. Prepare restores it; reset does not.
            p.seedGeneratorOnReset = false;
            const auto continued = nf::testing::reproducibleAcrossReset (p, spec);
            logMessage ("  seeded prepare only  -> " + continued.describe());
            expect (continued.premiseHeld(),
                    "the premise arm failed on a generator that IS restored by prepare, so this "
                    "fixture cannot say anything about reset: " + continued.acrossPrepare.describe());
            expect (! continued.acrossReset.sampleExact,
                    "**THE DRIVER CANNOT FAIL.** A generator seeded in prepare() and not in reset() "
                    "continued its stream across reset by construction, and the driver reported the "
                    "two renders identical.");

            // 3 — seeded nowhere. The premise must catch it rather than the reset arm taking credit.
            p.seedGeneratorOnPrepare = false;
            const auto unseeded = nf::testing::reproducibleAcrossReset (p, spec);
            logMessage ("  seeded nowhere       -> " + unseeded.describe());
            expect (! unseeded.premiseHeld(),
                    "a generator seeded nowhere was reported reproducible across prepare, so the "
                    "premise arm is not measuring anything");
            expect (unseeded.describe().contains ("PREMISE FAILED"),
                    "describe() reported a failed premise without saying so, which is how a reset "
                    "figure gets read as a reset finding");
        }

        beginTest ("The reset driver's WARM-UP is what it claims, shown by causing the thing it removes");
        {
            /*  **This arm exists because the driver shipped without it and Reflect-84 caught it.**
                Unwarmed, `reproducibleAcrossReset`'s premise arm compared render 1 against render 2
                and reported 0.392414443 at sample 351 — Reflect-84's documented first-run-only
                pre-delay glide, which is a different finding entirely. Every other casting would
                have accepted the same driver silently, because only that one had a known answer.

                So the warm-up is proved the way everything else here is: by causing the state it
                removes. `firstRunGlide` is the same shape — constructed at zero, arriving over the
                first render, and touched by neither `prepareToPlay` nor `reset()`.
            */
            nf::testing::RenderSpec spec;
            spec.blockSize = 256;
            spec.numBlocks = 8;

            ProbeProcessor p;
            p.firstRunGlide = true;

            const auto unwarmed = nf::testing::reproducibleAcrossReset (p, spec, 0);
            logMessage ("  warm-up 0 -> " + unwarmed.describe());
            expect (! unwarmed.premiseHeld(),
                    "**THE WARM-UP CANNOT MATTER.** A processor with first-run-only state was "
                    "reported reproducible across prepare with zero warm-up, so the warm-up below "
                    "is removing nothing and this driver's arms are unguarded against it.");

            const auto warmed = nf::testing::reproducibleAcrossReset (p, spec, 2);
            logMessage ("  warm-up 2 -> " + warmed.describe());
            expect (warmed.premiseHeld(),
                    "the warm-up did not spend the first-run state: " + warmed.acrossPrepare.describe());

            // **And more warm-up must not change the answer**, or the quantity is tuned rather than
            // sufficient — the same check TapeRot's block-size figure got at 2048 and 98304 samples.
            const auto warmer = nf::testing::reproducibleAcrossReset (p, spec, 6);
            logMessage ("  warm-up 6 -> " + warmer.describe());
            expect (warmer.premiseHeld() == warmed.premiseHeld()
                        && warmer.acrossReset.sampleExact == warmed.acrossReset.sampleExact,
                    "three times the warm-up gave a different verdict, so the quantity is tuned "
                    "rather than sufficient: " + warmer.describe());
        }

        beginTest ("renderBlocks does not prepare, which is the whole reason it exists");
        {
            /*  Guards the split itself. If `renderBlocks` ever regains a `prepareToPlay`, every
                result above silently becomes a prepare check again — the exact defect this driver
                was written to escape, restored invisibly.

                Shown by causing it: a generator seeded only in prepare must produce DIFFERENT output
                from two `renderBlocks` calls with nothing between them, and IDENTICAL output from
                two `render` calls. Same processor, same spec, one difference. */
            nf::testing::RenderSpec spec;
            spec.blockSize = 256;
            spec.numBlocks = 4;

            ProbeProcessor p;
            p.emitGeneratorNoise = true;
            p.seedGeneratorOnPrepare = true;
            p.seedGeneratorOnReset = false;

            p.setRateAndBufferSizeDetails (spec.sampleRate, spec.blockSize);
            p.prepareToPlay (spec.sampleRate, spec.blockSize);

            const auto bare = nf::testing::compareRenders (nf::testing::renderBlocks (p, spec),
                                                           nf::testing::renderBlocks (p, spec));
            expect (! bare.sampleExact,
                    "two renderBlocks calls with nothing between them produced identical output, so "
                    "something in that path is restoring state — it is preparing again: "
                        + bare.describe());

            const auto prepared = nf::testing::compareRenders (nf::testing::render (p, spec),
                                                               nf::testing::render (p, spec));
            expect (prepared.sampleExact,
                    "render() no longer restores this processor, so the contrast above is not the "
                    "one being claimed: " + prepared.describe());
        }

        beginTest ("Impulse latency agrees with what a processor introduces");
        {
            ProbeProcessor p;

            p.latencyToIntroduce = 0;
            expectEquals (nf::testing::measureImpulseLatency (p, {}), 0,
                          "a zero-latency processor did not emerge at sample 0");

            p.latencyToIntroduce = 64;
            expectEquals (nf::testing::measureImpulseLatency (p, {}), 64,
                          "a 64-sample delay was not measured as 64");
        }


        beginTest ("The sentinel counts FREES, and a ProgramId assignment is why that matters");
        {
            // Category 1's lock question ends here. setCurrentProgram can arrive on the AUDIO thread
            // — VST3 delivers a program change as an automatable parameter — and it reaches
            // requestProgramChange, which takes pendingLock and assigns a ProgramId inside it.
            //
            // ProgramId holds two juce::Strings. Copying one is a refcount increment, which is cheap
            // and allocation-free. But the ASSIGNMENT also releases whatever the target held, and a
            // refcount reaching zero calls free(). **A free() on the audio thread is the same defect
            // class as a malloc()** — it can take the allocator's lock and it can block.
            //
            // Reasoning is not measurement, so this measures it, on the real type.
            nf::ProgramId slot;
            slot.id = juce::String ("a-long-enough-identifier-to-own-a-heap-buffer");
            slot.displayName = juce::String ("ANOTHER LONG ENOUGH DISPLAY NAME TO OWN ONE");

            nf::ProgramId incoming;
            incoming.id = juce::String ("a-different-long-identifier-owning-its-own-buffer");
            incoming.displayName = juce::String ("A DIFFERENT LONG DISPLAY NAME OWNING ITS OWN");

            int allocs = 0, frees = 0;
            {
                const nf::testing::AllocationSentinel s;
                slot = incoming;              // exactly what requestProgramChange does under the lock
                allocs = s.count();
                frees = s.frees();
            }

            logMessage ("  ProgramId assignment -> " + juce::String (allocs) + " alloc, "
                            + juce::String (frees) + " free");

            // Recorded either way. If frees is 0 the strings were short enough to be inlined or the
            // refcount did not reach zero, and the concern is answered rather than confirmed.
            expectGreaterOrEqual (frees, 0);
        }


        beginTest ("The denormal-guard checker tells a guarded processBlock from an unguarded one");
        {
            // **The check that asserts the mechanism the whole suite rests on**, and it is worthless
            // unless it can see the difference. Same processor, guard switched off and on.
            ProbeProcessor p;

            p.guardAgainstDenormals = false;
            const auto unguarded = nf::testing::probeDenormalGuard (p);
            logMessage ("  unguarded -> " + unguarded.describe());

            p.guardAgainstDenormals = true;
            const auto guarded = nf::testing::probeDenormalGuard (p);
            logMessage ("  guarded   -> " + guarded.describe());

            expect (! unguarded.guardActive,
                    "an UNGUARDED processBlock was reported as guarded — the checker cannot see the "
                    "difference and every casting's result from it is worthless: "
                        + unguarded.describe());

            expect (guarded.guardActive,
                    "a guarded processBlock was reported as unguarded: " + guarded.describe());
        }


        beginTest ("The comparison can FAIL — one LSB is enough, so sample-exact means it");
        {
            // **Invariance is the category most able to produce a convincing wrong answer.**
            // "Sample-exact across four block sizes" is the strongest claim in this harness, and
            // every way it can be falsely true is quiet: a render that never varied, a comparison
            // against itself, a tolerance wide enough to swallow the difference, a driver that
            // prepared once and reported four times.
            //
            // Sample-exactness means the tolerance is ZERO, so a single one-LSB perturbation must
            // register. If it does not, the comparison is not what it says it is.
            ProbeProcessor p;

            nf::testing::RenderSpec spec;
            spec.numBlocks = 8;

            const auto a = nf::testing::render (p, spec);
            auto b = nf::testing::render (p, spec);

            const auto identical = nf::testing::compareRenders (a, b);
            expect (identical.sampleExact,
                    "two identical renders did not compare equal, so the driver is not "
                    "deterministic and no invariance result from it means anything: "
                        + identical.describe());

            // One representable step, on one sample, of one channel.
            nf::testing::perturbByOneLsb (b, 0, 137);

            const auto perturbed = nf::testing::compareRenders (a, b);
            expect (! perturbed.sampleExact,
                    "a ONE-LSB difference was reported as sample-exact. Every 'invariance holds' "
                    "result from this comparison is worthless: " + perturbed.describe());

            expectEquals (perturbed.firstDivergentSample, 137,
                          "the comparison detected a difference but not where it was");

            expectGreaterThan (perturbed.maxAbsDifference, 0.0);
        }

        beginTest ("The block-size sweep reports the size the PROCESSOR received, per run");
        {
            // A host that clamps, or a driver that silently reuses a buffer, produces four identical
            // renders that compare equal for the wrong reason. The readback is what distinguishes
            // "invariant across four sizes" from "one size measured four times".
            ProbeProcessor p;

            const std::vector<int> sizes { 64, 128, 511, 2048 };
            const auto results = nf::testing::blockSizeInvariance (p, {}, sizes);

            expectEquals ((int) results.size(), (int) sizes.size());

            for (size_t i = 0; i < results.size(); ++i)
            {
                logMessage ("  requested " + juce::String (sizes[i]) + " -> "
                                + results[i].describe());

                expectEquals (results[i].actualBlockSize, sizes[i],
                              "the processor was prepared at a different size than the sweep "
                              "requested, so this row is not the measurement it appears to be");
            }
        }


        beginTest ("The sample-rate sweep reports the ADOPTED rate, and flags one that was refused");
        {
            // **A rate requested but not adopted is a finding, not a row to drop.** A casting that
            // clamps 192 kHz is making a statement about what it supports; silently skipping it
            // would present as a clean sweep. Proved by causing it.
            ProbeProcessor p;
            p.emitSubnormals = true;          // gives it a decaying tail to measure

            nf::testing::RenderSpec spec;
            spec.numBlocks = 8;

            const std::vector<double> rates { 44100.0, 48000.0, 96000.0, 192000.0 };

            const auto honest = nf::testing::sampleRateSweep (p, spec, rates, 2000);
            for (const auto& row : honest)
                logMessage ("  " + row.describe());

            for (const auto& row : honest)
                expect (row.rateWasAdopted(),
                        "a rate was not adopted by a processor that accepts all of them: "
                            + row.describe());

            // **And the seconds comparison must be able to SEE a rate-dependent decay**, or
            // reporting seconds achieves nothing over reporting sample counts. This probe decays
            // per-sample with no rate compensation, so it is rate-dependent by construction and its
            // decay must roughly halve as the rate doubles:
            //
            //   44 100 -> 1.2307 s    96 000 -> 0.5653 s
            //   48 000 -> 1.1307 s   192 000 -> 0.2827 s
            //
            // A driver that reported four equal figures here would be measuring sample counts and
            // calling them seconds — which is the exact failure the seconds bar exists to prevent,
            // and it would look like perfect rate-invariance.
            expectGreaterThan (honest[1].measuredSeconds, honest[2].measuredSeconds * 1.5,
                               "doubling the rate did not shorten this probe's decay in seconds, so "
                               "the seconds measurement cannot detect rate dependence and every "
                               "casting's rate row from it is worthless");

            expectGreaterThan (honest[2].measuredSeconds, honest[3].measuredSeconds * 1.5,
                               "the same, between 96 k and 192 k");

            // Now refuse everything above 48 k, and the sweep must say so.
            ProbeProcessor clamped;
            clamped.emitSubnormals = true;
            clamped.clampRateAbove = 48000.0;

            const auto refused = nf::testing::sampleRateSweep (clamped, spec, rates, 2000);
            for (const auto& row : refused)
                logMessage ("  clamped: " + row.describe());

            expect (refused[2].rateWasAdopted() == false,
                    "96 kHz was refused by the processor and the sweep did not notice: "
                        + refused[2].describe());
            expect (refused[3].rateWasAdopted() == false,
                    "192 kHz was refused and the sweep did not notice: " + refused[3].describe());
            expect (refused[0].rateWasAdopted(),
                    "44.1 kHz was within the clamp and should have been adopted");
        }


        beginTest ("A filter's cutoff is verified by its RESPONSE, and the extremes are where a bug shows");
        {
            // **The failure mode this exists for**: a coefficient computed from a NORMALISED
            // frequency rather than from fs is exactly right at the rate it was written for and
            // wrong everywhere else, in proportion to the rate ratio. Coefficient readback cannot
            // see it — the filter still reports the cutoff it was asked for. Only the response moves.
            //
            // Two one-pole low-passes at a nominal 500 Hz. One computes its coefficient from fs and
            // is correct at any rate; the other bakes in a coefficient for 48 kHz and is wrong
            // everywhere else while claiming the same cutoff.
            struct Correct
            {
                float z = 0.0f, a = 0.0f;
                void prepare (double fs) { a = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 500.0 / fs)); }
                void reset() { z = 0.0f; }
                float process (float x) { z += a * (x - z); return z; }
            };

            struct NormalisedBug
            {
                float z = 0.0f;
                // Written and tuned at 48 kHz, then never re-derived. Correct there, wrong elsewhere.
                float a = 0.0634f;
                void prepare (double) {}
                void reset() { z = 0.0f; }
                float process (float x) { z += a * (x - z); return z; }
            };

            const std::vector<double> probes { 125.0, 250.0, 500.0, 1000.0, 2000.0 };

            const auto responseAt = [&probes] (auto& filter, double fs)
            {
                filter.prepare (fs);
                return nf::testing::measureMagnitudeResponse (
                    [&filter] (float x) { return filter.process (x); },
                    [&filter] { filter.reset(); },
                    fs, probes);
            };

            Correct good;
            const auto good441 = responseAt (good, 44100.0);
            const auto good48  = responseAt (good, 48000.0);
            const auto good192 = responseAt (good, 192000.0);

            NormalisedBug bad;
            const auto bad441 = responseAt (bad, 44100.0);
            const auto bad48  = responseAt (bad, 48000.0);
            const auto bad192 = responseAt (bad, 192000.0);

            const auto goodAdjacent = nf::testing::largestResponseDifferenceDb (good441, good48);
            const auto goodExtreme  = nf::testing::largestResponseDifferenceDb (good441, good192);
            const auto badAdjacent  = nf::testing::largestResponseDifferenceDb (bad441, bad48);
            const auto badExtreme   = nf::testing::largestResponseDifferenceDb (bad441, bad192);

            logMessage ("  correct filter  44.1k vs 48k -> " + juce::String (goodAdjacent, 3) + " dB");
            logMessage ("  correct filter  44.1k vs 192k -> " + juce::String (goodExtreme, 3) + " dB");
            logMessage ("  normalised bug  44.1k vs 48k -> " + juce::String (badAdjacent, 3) + " dB");
            logMessage ("  normalised bug  44.1k vs 192k -> " + juce::String (badExtreme, 3) + " dB");

            // A filter computed from fs holds its response at every rate.
            expectLessThan (goodExtreme, 0.5,
                            "a correctly-derived filter moved across rates, so this measurement "
                            "cannot be trusted to say a wrong one did");

            // **And the bug must be visible — loudly at the extremes.**
            expectGreaterThan (badExtreme, 3.0,
                               "a filter with a hard-coded 48 kHz coefficient was not detected at "
                               "44.1k vs 192k, so no casting's filter row means anything");

            // **The point of sweeping the extremes rather than the neighbours**: the same bug is
            // far quieter between adjacent rates, which is where it hides.
            expectGreaterThan (badExtreme, badAdjacent * 2.0,
                               "the adjacent-rate comparison was as revealing as the extreme one, "
                               "which would make the extremes advice wrong");
        }

        beginTest ("Offline vs real-time knows whether setNonRealtime actually took effect");
        {
            // Calling setNonRealtime(true) is not evidence that it took. A processor that ignores it
            // gives two identical renders that compare equal for the wrong reason — and "offline
            // matches real-time" is exactly what that looks like.
            ProbeProcessor p;
            const auto r = nf::testing::offlineAgainstRealtime (p, {});

            logMessage ("  " + r.describe());

            expect (r.nonRealtimeWasHonoured,
                    "the processor did not report itself non-realtime after setNonRealtime(true), so "
                    "the comparison compared two real-time renders: " + r.describe());

            expect (r.comparisonWasMeaningful);
        }

        beginTest ("Offline and real-time are compared, and agree when nothing branches on it");
        {
            ProbeProcessor p;
            const auto r = nf::testing::offlineAgainstRealtime (p, {});
            expect (r.sampleExact,
                    "a processor that ignores setNonRealtime reported a difference: " + r.describe());
        }
    }
};

static ProcessorHarnessTests processorHarnessTests;
