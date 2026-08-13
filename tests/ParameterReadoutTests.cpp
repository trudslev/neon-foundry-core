#include <nf/ParameterReadout.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    /** A float parameter with whatever range and attributes a case needs.

        Deliberately built here rather than borrowed from a casting: the point of these tests is
        what `describeParameter` does with a parameter, and coupling them to one plugin's parameter
        list would break them for reasons that say nothing about the readout.
    */
    std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id,
                                                          const juce::String& name,
                                                          juce::NormalisableRange<float> range,
                                                          float value,
                                                          juce::AudioParameterFloatAttributes attrs)
    {
        auto p = std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                              range, value, attrs);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        return p;
    }
}

class ParameterReadoutTests final : public juce::UnitTest
{
public:
    ParameterReadoutTests() : juce::UnitTest ("nf::ParameterReadout", "core") {}

    void runTest() override
    {
        beginTest ("The name is upper-cased, the value is not, and the unit comes from the label");
        {
            auto p = makeFloat ("release", "RELEASE", { 0.0f, 10.0f, 0.1f }, 2.5f,
                                juce::AudioParameterFloatAttributes().withLabel ("s"));

            expectEquals (nf::describeParameter (*p), juce::String ("RELEASE: 2.5 s"));
        }

        beginTest ("A lowercase unit survives, because a capital S is a different unit");
        {
            // Elmer's source got this right first and Reflect-84 had it wrong: upper-casing the
            // value reaches the unit, and "4.8 KHZ" is not a unit at all.
            auto p = makeFloat ("damp", "DAMPING HF", { 0.0f, 20.0f, 0.1f }, 4.8f,
                                juce::AudioParameterFloatAttributes().withLabel ("kHz"));

            expectEquals (nf::describeParameter (*p), juce::String ("DAMPING HF: 4.8 kHz"));


        }


        beginTest ("A parameter with no label gets no trailing space");
        {
            // Reflect-84's arrangement: the unit is baked into the value text and the label is
            // empty, so nothing is doubled. That works because the label is empty, not because of a
            // special case in describeParameter.
            auto p = makeFloat ("trim", "OUTPUT TRIM", { -24.0f, 24.0f, 0.1f }, 2.5f,
                                juce::AudioParameterFloatAttributes()
                                    .withStringFromValueFunction ([] (float v, int)
                                    {
                                        return (v >= 0.0f ? juce::String ("+") : juce::String())
                                             + juce::String (v, 1) + " dB";
                                    }));

            expectEquals (nf::describeParameter (*p), juce::String ("OUTPUT TRIM: +2.5 dB"));
        }

        beginTest ("Elmer's colon-free spelling is the one legitimate divergence");
        {
            auto p = makeFloat ("iron", "IRON", { 0.0f, 100.0f, 0.1f }, 40.0f,
                                juce::AudioParameterFloatAttributes().withLabel ("%"));

            nf::ReadoutFormat noColon;
            noColon.separatorColon = false;

            expectEquals (nf::describeParameter (*p, noColon), juce::String ("IRON 40.0 %"));
        }

        beginTest ("A choice parameter prints its authored string, un-recased");
        {
            juce::AudioParameterChoice knee { juce::ParameterID { "knee", 1 }, "KNEE",
                                              { "Soft", "Hard" }, 0 };

            expectEquals (nf::describeParameter (knee), juce::String ("KNEE: Soft"));
        }


        beginTest ("Nothing is re-cased - neither the value nor the name");
        {
            // **Case belongs at the source.** ValueCase is gone, and the name is not upper-cased
            // either: parameter names are authored in caps in each casting's Parameters.h, so the
            // panel and the host's automation lane print the same string. Re-casing here would make
            // this the only site that did - which is the failure the ruling generalises from
            // Program names.
            juce::AudioParameterChoice knee { juce::ParameterID { "knee", 1 }, "KNEE",
                                              { "Soft", "Hard" }, 0 };

            expectEquals (nf::describeParameter (knee), juce::String ("KNEE: Soft"));

            juce::AudioParameterChoice authored { juce::ParameterID { "k2", 1 }, "KNEE",
                                                  { "SOFT", "HARD" }, 0 };

            expectEquals (nf::describeParameter (authored), juce::String ("KNEE: SOFT"));
        }

        beginTest ("A range with NO interval renders at seven decimal places");
        {
            // **The defect this whole extraction exists to make visible**, asserted rather than
            // described. JUCE leaves numDecimalPlacesToDisplay at 7 when a NormalisableRange has a
            // zero interval, and writeDouble then applies std::ios_base::fixed - so a parameter
            // with no stringFromValueFunction prints "20.0000000" on the panel, in the host's
            // automation lane and in any generic editor.
            //
            // TapeRot shipped that on every float parameter. Gatecrasher had already hit it and
            // written the fix down. Elmer masked it with a hand-rolled formatter, so its panel
            // looked right while its automation lane did not.
            //
            // describeParameter deliberately does NOT round this away: doing so would restore the
            // panel/host disagreement it exists to prevent. The fix belongs in the casting's
            // Parameters.h, and this test is what says so out loud.
            auto p = makeFloat ("drive", "DRIVE", { 0.0f, 100.0f }, 20.0f,
                                juce::AudioParameterFloatAttributes().withLabel ("%"));

            expectEquals (nf::describeParameter (*p), juce::String ("DRIVE: 20.0000000 %"));

            // The same parameter with an interval, which is what a casting should give it.
            auto fixed = makeFloat ("drive", "DRIVE", { 0.0f, 100.0f, 0.1f }, 20.0f,
                                    juce::AudioParameterFloatAttributes().withLabel ("%"));

            expectEquals (nf::describeParameter (*fixed), juce::String ("DRIVE: 20.0 %"));
        }

        beginTest ("A long name is elided to the character budget, not overrun");
        {
            auto p = makeFloat ("x", "SIDECHAIN HIGH PASS FILTER", { 0.0f, 1.0f, 0.01f }, 0.5f,
                                juce::AudioParameterFloatAttributes());

            nf::ReadoutFormat narrow;
            narrow.nameCharacterBudget = 12;

            const auto described = nf::describeParameter (*p, narrow);
            expect (described.startsWith ("SIDECHAIN"), described);
            expect (described.upToFirstOccurrenceOf (":", false, false).length() <= 12, described);
        }

        //==============================================================================
        beginTest ("The takeover holds while the control is down and reverts 900 ms after release");
        {
            nf::ReadoutTimer timer;
            expectEquals (timer.revertMs(), 900);

            timer.show ("DRIVE: 20.0 %");

            // Held: no deadline while the control is still being moved, however long that is.
            expectEquals (timer.textAt (0),       juce::String ("DRIVE: 20.0 %"));
            expectEquals (timer.textAt (1000000), juce::String ("DRIVE: 20.0 %"));

            timer.release (10000);

            expectEquals (timer.textAt (10000),  juce::String ("DRIVE: 20.0 %"));
            expectEquals (timer.textAt (10899),  juce::String ("DRIVE: 20.0 %"));
            expectEquals (timer.textAt (10900),  juce::String());   // the boundary, exactly
            expectEquals (timer.textAt (999999), juce::String());
        }

        beginTest ("Showing again cancels a pending revert");
        {
            // The live case: release a knob, grab it again inside the revert window. Without this
            // the second takeover would inherit the first one's deadline and vanish mid-drag.
            nf::ReadoutTimer timer;
            timer.show ("MIX: 50 %");
            timer.release (1000);

            timer.show ("MIX: 60 %");

            expectEquals (timer.textAt (5000), juce::String ("MIX: 60 %"));
        }

        beginTest ("suppress clears immediately and does not come back");
        {
            // Naming mode. Elmer relied on paint order here, which hides the takeover without
            // cancelling it - so it returned the moment naming ended, if the revert had not fired.
            nf::ReadoutTimer timer;
            timer.show ("IRON: 40 %");
            timer.release (1000);

            timer.suppress();

            expectEquals (timer.textAt (1000), juce::String());
            expectEquals (timer.textAt (1500), juce::String());
        }

        beginTest ("release with nothing showing does not resurrect old text");
        {
            nf::ReadoutTimer timer;
            timer.show ("MIX: 50 %");
            timer.suppress();
            timer.release (1000);

            expectEquals (timer.textAt (1000), juce::String());
        }

        //==============================================================================
        beginTest ("readoutDefects catches the seven-place default and passes a formatted value");
        {
            auto bad = makeFloat ("drive", "DRIVE", { 0.0f, 100.0f }, 20.0f,
                                  juce::AudioParameterFloatAttributes().withLabel ("%"));

            const auto defects = nf::readoutDefects (*bad);
            expect (! defects.isEmpty(), "a 7-place value was not reported");
            expect (defects[0].contains ("decimal places"), defects[0]);

            auto good = makeFloat ("drive", "DRIVE", { 0.0f, 100.0f, 0.1f }, 20.0f,
                                   juce::AudioParameterFloatAttributes().withLabel ("%"));
            expect (nf::readoutDefects (*good).isEmpty());
        }

        beginTest ("readoutDefects catches a unit that would print twice");
        {
            // The other half of the same mistake: a formatter that bakes the unit in, on a
            // parameter that also carries a label, gives "4.8 kHz kHz".
            auto p = makeFloat ("damp", "DAMPING", { 0.0f, 20.0f, 0.1f }, 4.8f,
                                juce::AudioParameterFloatAttributes()
                                    .withLabel ("kHz")
                                    .withStringFromValueFunction ([] (float v, int)
                                    {
                                        return juce::String (v, 1) + " kHz";
                                    }));

            const auto defects = nf::readoutDefects (*p);
            expect (! defects.isEmpty(), "a doubled unit was not reported");
            expect (defects[0].contains ("twice"), defects[0]);
            expectEquals (nf::describeParameter (*p), juce::String ("DAMPING: 4.8 kHz kHz"));
        }

        beginTest ("readoutDefects passes a baked unit with NO label");
        {
            // Reflect-84's arrangement, which is fine: the unit is in the text and the label is
            // empty, so nothing doubles. The check must not flag it.
            auto p = makeFloat ("decay", "DECAY", { 0.4f, 8.0f, 0.1f }, 4.6f,
                                juce::AudioParameterFloatAttributes()
                                    .withStringFromValueFunction ([] (float v, int)
                                    {
                                        return juce::String (v, 1) + " s";
                                    }));

            expect (nf::readoutDefects (*p).isEmpty());
        }

        beginTest ("readoutDefects passes a choice parameter");
        {
            juce::AudioParameterChoice knee { juce::ParameterID { "knee", 1 }, "KNEE",
                                              { "Soft", "Hard" }, 0 };
            expect (nf::readoutDefects (knee).isEmpty());
        }

        beginTest ("A non-default revertMs is honoured");
        {
            nf::ReadoutFormat slow;
            slow.revertMs = 1200;

            nf::ReadoutTimer timer { slow };
            timer.show ("X: 1");
            timer.release (0);

            expectEquals (timer.textAt (1199), juce::String ("X: 1"));
            expectEquals (timer.textAt (1200), juce::String());
        }
    }
};

static ParameterReadoutTests parameterReadoutTests;
