#include <nf/ParameterSnapshot.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <thread>

namespace
{
    /** The smallest processor that can hold parameters. Deliberately not a casting's: the point is
        that the snapshot works against any AudioProcessor, and a test coupled to one plugin's
        parameter list would break for reasons that say nothing about the snapshot. */
    class StubProcessor final : public juce::AudioProcessor
    {
    public:
        StubProcessor()
        {
            for (auto id : { "drive", "mix", "engine1", "engine2" })
                addParameter (new juce::AudioParameterFloat ({ id, 1 }, id, 0.0f, 1.0f, 0.5f));
        }

        juce::AudioProcessorParameter* byID (const juce::String& id) const
        {
            for (auto* p : getParameters())
                if (auto* w = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                    if (w->paramID == id)
                        return w;
            return nullptr;
        }

        const juce::String getName() const override { return "Stub"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };
}

class ParameterSnapshotTests final : public juce::UnitTest
{
public:
    ParameterSnapshotTests() : juce::UnitTest ("nf::ParameterSnapshot", "core") {}

    void runTest() override
    {
        beginTest ("With no baseline captured, nothing is modified");
        {
            // Not merely defensive: a panel paints before the first Program is applied, and
            // reporting "modified" there would light SAVE on a plugin nobody has touched.
            StubProcessor proc;
            const nf::ParameterSnapshot snapshot;

            expect (snapshot.isEmpty());
            expect (! snapshot.differsFrom (proc));
        }

        beginTest ("A fresh capture reports clean");
        {
            StubProcessor proc;
            nf::ParameterSnapshot snapshot;
            snapshot.capture (proc);

            expect (! snapshot.differsFrom (proc));
        }

        beginTest ("A real move is detected");
        {
            StubProcessor proc;
            nf::ParameterSnapshot snapshot;
            snapshot.capture (proc);

            proc.byID ("drive")->setValue (0.75f);
            expect (snapshot.differsFrom (proc));
        }

        beginTest ("A movement below the epsilon is not");
        {
            // The XML round-trip case: a Program loaded from disk comes back a hair off the value
            // that was saved, and must not read as edited the moment it appears.
            StubProcessor proc;
            nf::ParameterSnapshot snapshot;
            snapshot.capture (proc);

            proc.byID ("drive")->setValue (0.5f + nf::ParameterSnapshot::epsilon * 0.5f);
            expect (! snapshot.differsFrom (proc));
        }

        beginTest ("Re-capturing after an edit makes it clean again");
        {
            // The save path: storing a Program re-takes the baseline, so the panel stops claiming
            // unsaved work the instant the work is saved.
            StubProcessor proc;
            nf::ParameterSnapshot snapshot;
            snapshot.capture (proc);

            proc.byID ("mix")->setValue (0.9f);
            expect (snapshot.differsFrom (proc));

            snapshot.capture (proc);
            expect (! snapshot.differsFrom (proc));
        }

        beginTest ("Excluded parameters do not count as edits");
        {
            StubProcessor proc;
            nf::ParameterSnapshot snapshot;
            snapshot.capture (proc);

            const auto isLatch = [] (const juce::String& id)
            {
                return id == "engine1" || id == "engine2";
            };

            proc.byID ("engine1")->setValue (1.0f);
            expect (! snapshot.differsFrom (proc, isLatch), "a pager press claimed unsaved work");

            // ...but the exclusion is narrow: everything else still counts.
            proc.byID ("drive")->setValue (0.1f);
            expect (snapshot.differsFrom (proc, isLatch));
        }

        beginTest ("Restricted to a set of IDs, only those are compared");
        {
            // The conditional-storage case: a Program stores only what is on its active path, so
            // moving a control that is not on it must not light SAVE.
            StubProcessor proc;
            nf::ParameterSnapshot snapshot;
            snapshot.capture (proc);

            proc.byID ("mix")->setValue (0.9f);

            expect (! snapshot.differsFrom (proc, juce::StringArray { "drive" }));
            expect (snapshot.differsFrom (proc, juce::StringArray { "drive", "mix" }));
        }

        beginTest ("An ID with no baseline is skipped, not treated as modified");
        {
            // A parameter that has just joined the compared set - because the selector governing
            // it moved - has no baseline. The selector itself is compared and registers the
            // change, so counting this too would light SAVE on a mode switch that changed nothing.
            StubProcessor proc;
            nf::ParameterSnapshot snapshot;
            snapshot.capture (proc);

            expect (! snapshot.differsFrom (proc, juce::StringArray { "not-a-parameter" }));
        }

        beginTest ("Capturing on one thread while comparing on another does not corrupt the baseline");
        {
            // **setStateInformation carries no thread guarantee from JUCE**, and it writes the
            // baseline while the panel polls the dirty flag on the message thread to decide whether
            // SAVE is lit. TapeRot and Elmer both guarded their own copies; Chorus-60 and
            // Gatecrasher read and wrote a plain std::vector<float> across threads.
            //
            // Unguarded, this case reliably crashes rather than merely reading a stale value -
            // differsFrom walks the map while capture is rehashing it. That is what makes it worth
            // writing as a test at all: without a sanitiser a race usually proves nothing, and this
            // one fails loudly.
            StubProcessor proc;
            nf::ParameterSnapshot snapshot;
            snapshot.capture (proc);

            std::atomic<bool> stop { false };
            std::atomic<int> captures { 0 };

            std::thread writer ([&]
            {
                while (! stop.load())
                {
                    snapshot.capture (proc);
                    captures.fetch_add (1);
                }
            });

            int reads = 0;

            for (int i = 0; i < 20000; ++i)
            {
                // The value moves under the reader too, so the answer is legitimately either - the
                // assertion is that asking is safe, not what it says.
                proc.byID ("drive")->setValue (i % 2 == 0 ? 0.1f : 0.9f);
                snapshot.differsFrom (proc);
                ++reads;
            }

            stop.store (true);
            writer.join();

            expectEquals (reads, 20000);
            expectGreaterThan (captures.load(), 0);

            // And the snapshot is still usable afterwards, rather than left in a torn state.
            snapshot.capture (proc);
            expect (! snapshot.differsFrom (proc));
        }
    }
};

static ParameterSnapshotTests parameterSnapshotTests;
