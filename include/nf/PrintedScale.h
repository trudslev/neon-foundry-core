#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <vector>

namespace nf
{

/** One numeral printed around a knob, and where its tick is actually drawn.

    `angleDegrees` is measured from 12 o'clock, positive clockwise — so a 270° sweep runs
    −135°…+135°, which is what every casting in this suite uses.
*/
struct PrintedMark
{
    float value;
    float angleDegrees;
};

/** The angle a pointer takes at a given rotation proportion.

    Ten lines of shared surface is not worth a module boundary on its own — this is here because the
    *check* below needs it, and because writing it twice is how a ring and its pointer come to
    disagree.
*/
inline float sweepAngleDegrees (float proportion, float sweepDegrees = 270.0f)
{
    return -sweepDegrees * 0.5f + proportion * sweepDegrees;
}

/** Everything wrong with a printed scale, or an empty array.

    **BRAND.md makes the printed scale a correctness requirement**, and the failure it guards is
    specific: a knob's ring legends a taper, so if the parameter's taper changes and the ring does
    not, the numerals point at values the pointer never reaches. Nothing looks broken — the ring
    still draws, the pointer still turns, and the two simply stop meaning the same thing.

    The check is therefore always **against the parameter's own `NormalisableRange`**, never against
    a second table of angles. A test that compares stored angles with stored angles asserts that
    somebody transcribed a spec consistently; it says nothing about whether the ring matches the
    control.

    What it looks for:

    - **A mark outside the parameter's range.** A numeral the control cannot reach.
    - **A mark whose tick is not where the range puts it**, beyond `toleranceDegrees`. This is the
      one that catches a taper change.
    - **A mark outside the sweep**, which is drawn but unreachable.
    - **Two marks that do not increase together.** Values ascending while angles do not means the
      table is out of order or the range is non-monotonic, and both put a numeral under the wrong
      tick.

    @param range              the bound parameter's own range — the authority
    @param marks              the printed numerals and the angles their ticks are drawn at
    @param sweepDegrees       the ring's total travel; 270 on every casting here
    @param toleranceDegrees   how far a printed tick may sit from the computed angle. Default 0.5°,
                              which at a 60px radius is about half a pixel — tight enough to catch a
                              taper mismatch, loose enough to absorb a spec's rounded figures
*/
inline juce::StringArray printedScaleDefects (const juce::NormalisableRange<float>& range,
                                              const std::vector<PrintedMark>& marks,
                                              float sweepDegrees = 270.0f,
                                              float toleranceDegrees = 0.5f)
{
    juce::StringArray defects;

    if (marks.size() < 2)
    {
        defects.add ("a printed scale needs at least two marks to legend anything");
        return defects;
    }

    const float limit = sweepDegrees * 0.5f;

    for (size_t i = 0; i < marks.size(); ++i)
    {
        const auto& m = marks[i];
        const auto describe = [&m] { return "mark " + juce::String (m.value); };

        if (m.value < range.start - 1.0e-4f || m.value > range.end + 1.0e-4f)
        {
            defects.add (describe() + " is outside the parameter's range "
                         + juce::String (range.start) + ".." + juce::String (range.end)
                         + " - the control cannot reach the value it prints");
            continue;
        }

        if (std::abs (m.angleDegrees) > limit + toleranceDegrees)
        {
            defects.add (describe() + " is drawn at " + juce::String (m.angleDegrees, 2)
                         + " degrees, outside the " + juce::String (sweepDegrees, 0)
                         + "-degree sweep - it is printed where the pointer never goes");
            continue;
        }

        const float expected = sweepAngleDegrees (range.convertTo0to1 (m.value), sweepDegrees);

        if (std::abs (m.angleDegrees - expected) > toleranceDegrees)
            defects.add (describe() + " is drawn at " + juce::String (m.angleDegrees, 2)
                         + " degrees but its range puts it at " + juce::String (expected, 2)
                         + " - the ring and the taper disagree, so the numeral points at a value "
                           "the pointer does not reach");

        if (i > 0)
        {
            const auto& prev = marks[i - 1];

            if (m.value <= prev.value)
                defects.add (describe() + " does not increase on " + juce::String (prev.value)
                             + " - the table is out of order");
            else if (m.angleDegrees <= prev.angleDegrees)
                defects.add (describe() + " is drawn at or before " + juce::String (prev.value)
                             + " despite being a larger value - two numerals share a tick");
        }
    }

    return defects;
}

} // namespace nf
