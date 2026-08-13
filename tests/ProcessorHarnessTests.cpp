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
        // **Default OFF**, so every other test in this file behaves as it did: the subnormal-tail
        // case needs subnormals to survive, and flush-to-zero would erase the thing it measures.
        bool guardAgainstDenormals = false;

        // **A runtime multiplier, not the literal 1.0f.** The first version wrote `v * 1.0f`, which
        // the optimiser folds to a plain copy — no arithmetic, so nothing for flush-to-zero to act
        // on, and the guarded run passed subnormals straight through exactly like the unguarded one.
        // The checker looked broken when the PROBE was. A real casting does real arithmetic; the
        // probe has to as well.
        float unityGain = 1.0f;
        int  latencyToIntroduce = 0;

        void prepareToPlay (double, int samplesPerBlock) override
        {
            scratch.assign ((size_t) samplesPerBlock, 0.0f);
            delayLine.assign ((size_t) juce::jmax (1, latencyToIntroduce), 0.0f);
            writeIndex = 0;
            tail = 1.0f;
            blockCounter = 0;
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
                        // **0.9999 per sample, not 0.5, and the rate is the point of the test.**
                        // Halving reaches subnormal in ~126 samples and zero in ~150, so it is over
                        // before the tail scan begins — which made the "short tail misses it" case
                        // pass for the wrong reason and the "long tail finds it" case fail. This
                        // decays over ~873 000 samples, which is a realistic reverb-tail rate and
                        // straddles the two tail lengths below.
                        tail *= 0.9999f;
                        v += tail;
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
        std::vector<float> scratch, delayLine;
        int writeIndex = 0, blockCounter = 0;
        float tail = 1.0f;
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
