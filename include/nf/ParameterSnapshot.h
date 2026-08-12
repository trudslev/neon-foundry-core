#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <map>

namespace nf
{

/** The baseline a Program is compared against to decide whether it has been edited.

    **One definition, and every consumer reads it rather than deciding for itself.** A panel's
    dirty marker and its SAVE lamp must agree always, and the cheapest way to guarantee that is to
    give them nothing to disagree about.

    **Keyed by parameter ID, not by index.** Four castings held a `std::vector<float>` in parameter
    order and guarded it with `if (snapshot.size() != params.size()) return false` — which is a
    guard against exactly the fragility the index introduces, and which silently reports "not
    modified" when it fires. An ID-keyed map cannot mismatch that way, and it is also what lets a
    casting with a conditional data model compare only the parameters currently in play.

    **Values are normalised 0..1**, so one epsilon is meaningful for every parameter regardless of
    its real-world range.
*/
class ParameterSnapshot
{
public:
    /** Parameters excluded from the comparison, by ID.

        **This is the lighter of two different tools and they get confused.** Excluding a parameter
        here means it is still stored and still recalled — touching it simply does not claim unsaved
        work. Excluding it from *storage* is the heavier tool and needs a different test: a
        parameter may be left out of a Program only when the Program cannot recall a state in which
        that parameter is audible.

        Chorus-60 uses this for `engine1`/`engine2`, its pager and bypass, which are hit
        mid-performance — treating a press as an edit means merely bypassing the plugin lights SAVE
        and claims you have unsaved work.
    */
    using ExclusionPredicate = std::function<bool (const juce::String& parameterID)>;

    ParameterSnapshot() = default;

    /** Captures every parameter the processor exposes that has an ID.

        Call this on **apply, on save, and on session restore** — the three events after which the
        panel is showing a Program nobody has edited yet. Missing one leaves SAVE lit on a clean
        Program or dark on a dirty one; five to six call sites per casting is what that used to cost
        before the events moved into one place.
    */
    void capture (const juce::AudioProcessor& processor)
    {
        values.clear();

        for (const auto* p : processor.getParameters())
            if (const auto* withID = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p))
                values[withID->paramID] = withID->getValue();
    }

    /** True when any parameter differs from the baseline by more than `epsilon`.

        @param processor  the live parameters to compare against the baseline
        @param isExcluded optional; return true for a parameter that should not count as an edit
    */
    bool differsFrom (const juce::AudioProcessor& processor,
                      const ExclusionPredicate& isExcluded = {}) const
    {
        if (values.empty())
            return false;   // nothing captured yet: no baseline to differ from

        for (const auto* p : processor.getParameters())
        {
            const auto* withID = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p);

            if (withID == nullptr)
                continue;

            if (isExcluded && isExcluded (withID->paramID))
                continue;

            const auto it = values.find (withID->paramID);

            // **A parameter with no baseline is not a modification.** It has just joined the set
            // being compared — on castings with a conditional data model that happens when the
            // selector governing it moves — and the selector itself is compared, so the change is
            // already registered. Treating this as "modified" would light SAVE on a mode switch
            // that changed nothing else.
            if (it == values.end())
                continue;

            if (std::abs (withID->getValue() - it->second) > epsilon)
                return true;
        }

        return false;
    }

    /** The same, restricted to a set of parameter IDs — for a casting whose Program stores only the
        parameters on its currently active path. IDs absent from the baseline are skipped, per the
        rule above.
    */
    bool differsFrom (const juce::AudioProcessor& processor,
                      const juce::StringArray& parameterIDs) const
    {
        if (values.empty())
            return false;

        for (const auto& id : parameterIDs)
        {
            const auto it = values.find (id);

            if (it == values.end())
                continue;

            if (const auto* p = findParameter (processor, id))
                if (std::abs (p->getValue() - it->second) > epsilon)
                    return true;
        }

        return false;
    }

    bool isEmpty() const noexcept { return values.empty(); }

    /** **1e-4 in normalised space**, and the figure is load-bearing in both directions.

        Loose enough to absorb the float round-trip through a user Program's XML, so a Program that
        has just been loaded never reads as dirty. Far tighter than the smallest movement any
        control can actually produce, so a real edit is never missed.
    */
    static constexpr float epsilon = 1.0e-4f;

private:
    static const juce::AudioProcessorParameter* findParameter (const juce::AudioProcessor& processor,
                                                               const juce::String& id)
    {
        for (const auto* p : processor.getParameters())
            if (const auto* withID = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p))
                if (withID->paramID == id)
                    return withID;

        return nullptr;
    }

    std::map<juce::String, float> values;
};

} // namespace nf
