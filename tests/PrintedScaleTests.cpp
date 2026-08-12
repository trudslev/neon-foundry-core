#include <nf/PrintedScale.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    /** Marks placed exactly where a range puts them — the shape a correct scale has. */
    std::vector<nf::PrintedMark> marksFor (const juce::NormalisableRange<float>& range,
                                           const std::vector<float>& values,
                                           float sweep = 270.0f)
    {
        std::vector<nf::PrintedMark> out;

        for (auto v : values)
            out.push_back ({ v, nf::sweepAngleDegrees (range.convertTo0to1 (v), sweep) });

        return out;
    }
}

class PrintedScaleTests final : public juce::UnitTest
{
public:
    PrintedScaleTests() : juce::UnitTest ("nf::PrintedScale", "core") {}

    void runTest() override
    {
        beginTest ("A linear control's quarter marks land evenly, and pass");
        {
            const juce::NormalisableRange<float> range { 0.0f, 100.0f, 0.1f };
            const auto marks = marksFor (range, { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f });

            expect (nf::printedScaleDefects (range, marks).isEmpty());

            // And the geometry is what the suite's panels actually use.
            expectWithinAbsoluteError (marks.front().angleDegrees, -135.0f, 0.01f);
            expectWithinAbsoluteError (marks[2].angleDegrees,         0.0f, 0.01f);
            expectWithinAbsoluteError (marks.back().angleDegrees,  +135.0f, 0.01f);
        }

        beginTest ("A SKEWED control's marks are not evenly spaced, and still pass");
        {
            // Fifth Member's Mod Rate: 0.1..5.0 Hz with the centre at 1.0, so the midpoint of the
            // TRAVEL is 1 Hz rather than 2.55. A scale checked against evenly-spaced angles would
            // fail this correct ring; checking against the range is what makes the test meaningful.
            juce::NormalisableRange<float> range { 0.1f, 5.0f, 0.01f };
            range.setSkewForCentre (1.0f);

            const auto marks = marksFor (range, { 0.1f, 0.5f, 1.0f, 2.0f, 5.0f });
            expect (nf::printedScaleDefects (range, marks).isEmpty());

            // 1 Hz is the skew centre, so it sits at 12 o'clock - and that is the whole point of
            // the skew being where it is.
            expectWithinAbsoluteError (marks[2].angleDegrees, 0.0f, 0.01f);

            // The spacing really is uneven, so the previous test's shape would not have caught a
            // taper mismatch here.
            expect (std::abs (marks[1].angleDegrees - marks[0].angleDegrees)
                        != std::abs (marks[4].angleDegrees - marks[3].angleDegrees));
        }

        beginTest ("A TAPER CHANGE is caught - the failure the whole check exists for");
        {
            // The ring was legended against a linear range and the parameter later gained a skew.
            // Nothing looks broken: the ring draws, the pointer turns, and the numerals now point
            // at values the pointer never reaches.
            const juce::NormalisableRange<float> linear { 0.1f, 5.0f, 0.01f };
            const auto ringFromLinear = marksFor (linear, { 0.1f, 0.5f, 1.0f, 2.0f, 5.0f });

            juce::NormalisableRange<float> skewed { 0.1f, 5.0f, 0.01f };
            skewed.setSkewForCentre (1.0f);

            const auto defects = nf::printedScaleDefects (skewed, ringFromLinear);

            expect (! defects.isEmpty(), "a ring legending the wrong taper was not reported");
            expect (defects[0].contains ("the ring and the taper disagree"), defects[0]);
        }

        beginTest ("A mark outside the parameter's range is reported");
        {
            const juce::NormalisableRange<float> range { 0.0f, 100.0f, 0.1f };
            auto marks = marksFor (range, { 0.0f, 50.0f, 100.0f });
            marks.push_back ({ 120.0f, 135.0f });

            const auto defects = nf::printedScaleDefects (range, marks);
            expect (! defects.isEmpty());
            expect (defects[0].contains ("outside the parameter's range"), defects[0]);
        }

        beginTest ("A mark outside the sweep is reported");
        {
            // The decorative fixed-pitch ring Fifth Member replaced printed a mark at +135 with no
            // twin at -135, and marks below the horizontal at both ends.
            const juce::NormalisableRange<float> range { 0.0f, 100.0f, 0.1f };
            std::vector<nf::PrintedMark> marks { { 0.0f, -135.0f }, { 50.0f, 0.0f },
                                                 { 100.0f, 160.0f } };

            const auto defects = nf::printedScaleDefects (range, marks);
            expect (! defects.isEmpty());
            expect (defects[0].contains ("outside the"), defects[0]);
        }

        beginTest ("Two marks sharing a tick are reported");
        {
            // What happened when both of Fifth Member's dial-1 rings were mapped through one
            // range: every percent mark went through the Hz range and 25/50/75/100 clamped onto a
            // single pile at +135.
            const juce::NormalisableRange<float> range { 0.0f, 100.0f, 0.1f };
            std::vector<nf::PrintedMark> marks { { 25.0f, 135.0f }, { 50.0f, 135.0f } };

            const auto defects = nf::printedScaleDefects (range, marks);
            expect (! defects.isEmpty());
            expect (defects.joinIntoString (" | ").contains ("share a tick"),
                    defects.joinIntoString (" | "));
        }

        beginTest ("A table out of order is reported");
        {
            const juce::NormalisableRange<float> range { 0.0f, 100.0f, 0.1f };
            std::vector<nf::PrintedMark> marks { { 50.0f, 0.0f }, { 25.0f, -67.5f } };

            const auto defects = nf::printedScaleDefects (range, marks);
            expect (! defects.isEmpty());
            expect (defects.joinIntoString (" | ").contains ("out of order"),
                    defects.joinIntoString (" | "));
        }

        beginTest ("A single mark legends nothing");
        {
            const juce::NormalisableRange<float> range { 0.0f, 100.0f, 0.1f };
            expect (! nf::printedScaleDefects (range, { { 50.0f, 0.0f } }).isEmpty());
        }

        beginTest ("The tolerance absorbs a spec's rounded figures but not a real mismatch");
        {
            const juce::NormalisableRange<float> range { 0.0f, 100.0f, 0.1f };
            auto marks = marksFor (range, { 0.0f, 50.0f, 100.0f });

            // A spec printing -67.5 as -67.5 is fine; 0.4 degrees off is within tolerance.
            marks[1].angleDegrees += 0.4f;
            expect (nf::printedScaleDefects (range, marks).isEmpty());

            // A degree out is not.
            marks[1].angleDegrees += 0.7f;
            expect (! nf::printedScaleDefects (range, marks).isEmpty());
        }
    }
};

static PrintedScaleTests printedScaleTests;
