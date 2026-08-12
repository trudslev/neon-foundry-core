#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace nf
{

/** How a casting spells its LCD parameter readout.

    The suite convention is `NAME: VALUE UNIT` — the control's printed name, the value, and the
    unit — shown while a control is being moved and reverting shortly after release. Six castings
    implemented it five different ways.
*/
struct ReadoutFormat
{
    /** `NAME: VALUE` against `NAME VALUE`. Elmer's spec asks for no colon and that is the only
        legitimate divergence; everything else here is drift. */
    bool separatorColon = true;

    /** How the VALUE half is cased. The name half is always upper-cased - every casting
        silk-screens its control names in caps.

        **Two castings independently arrived at `wordsOnly` and one argues explicitly against it**,
        so this is a real disagreement with reasoning on both sides rather than drift, and core
        carries both rather than picking:

        - TapeRot and Gatecrasher upper-case a value that carries no unit and contains no digit,
          on the grounds that such a value is a WORD and should get the name's treatment -
          `ALGORITHM: PLATE` beside `THRESHOLD: -18.5 dB`. Their comments are near-identical.
        - Elmer's source says the opposite and the argument is sharp: *"If the display should say
          SOFT, author the choice that way in Parameters.h so the host's automation lane agrees -
          do not re-case it here, or the two disagree again by exactly this route."* That is the
          same failure this whole extraction exists to prevent, one level down.

        Whichever a casting picks, it states it here, where both arguments are written down. */
    enum class ValueCase
    {
        /** Exactly what the parameter's own `getText` returned. */
        asAuthored,

        /** Upper-cased only when the value carries no unit and contains no digit - i.e. when it is
            a choice name rather than a reading. A number has no case to change, and any letters in
            it belong to a unit. */
        wordsOnly,

        /** Upper-cased unconditionally.

            **This is the one that was a bug.** Reflect-84 used it and printed `DAMPING HF: 4.8
            KHZ`, `DECAY: 4.6 S` and `OUTPUT TRIM: +2.5 DB`, because its units are baked into the
            value text rather than carried in `getLabel()`. `KHZ` is not a unit and a capital S is a
            different unit from a lowercase one. Kept available and named plainly so that choosing
            it is a decision somebody made, not something that happened. */
        all
    };

    ValueCase valueCase = ValueCase::asAuthored;

    /** **900 ms, and the number is single-sourced here rather than being a plurality vote.**

        The suite ran 800 / 900 / 1100 / 1200 under three different constant names and two
        mechanisms, with no spec anywhere justifying any of them. 900 was what three castings had,
        and there is no argument for the others beyond nobody having compared them. */
    int revertMs = 900;

    /** Passed to `getName()`. A casting sets this to its own LCD character budget so a long
        control name is elided by JUCE rather than overrunning the cell. */
    int nameCharacterBudget = 64;
};

/** The readout string for a parameter: `NAME: VALUE UNIT`.

    **Routed through the parameter's own `getText` and `getLabel`, never re-derived.** That is the
    whole reason this is one function: the LCD and the host's automation lane read the same
    parameter, so they must print the same string, and a casting that formats values itself is
    maintaining a second implementation that will disagree.

    It disagreed twice. TapeRot printed `DRIVE: 20.0000000` because no float parameter carried a
    `stringFromValueFunction` and JUCE leaves `numDecimalPlacesToDisplay` at 7 for a zero interval.
    Gatecrasher had already hit and fixed exactly that, and wrote it down. Elmer hand-rolled an
    `if`-chain per parameter ID, which looked right on the panel and left the host showing raw
    floats — the same defect, hidden rather than absent.

    **So a value that formats badly is fixed in the casting's `Parameters.h`, not here.** This
    function cannot round or truncate: doing so would restore the disagreement it exists to prevent.

    The unit comes from `getLabel()` and is appended with a space. A parameter that bakes its unit
    into its text carries no label, so nothing is doubled — that is Reflect-84's arrangement, and it
    works because the label is empty rather than because of a special case here.
*/
inline juce::String describeParameter (const juce::AudioProcessorParameter& param,
                                       const ReadoutFormat& format = {})
{
    // The NAME is upper-cased and the VALUE is not. The name is a panel label, and every casting
    // silk-screens it in caps; the value may contain a unit or an authored choice string.
    const auto unit = param.getLabel();
    auto value = param.getText (param.getValue(), 0);

    switch (format.valueCase)
    {
        case ReadoutFormat::ValueCase::all:
            value = value.toUpperCase();
            break;

        case ReadoutFormat::ValueCase::wordsOnly:
            // A unitless, digit-free value is a choice name rather than a reading.
            if (unit.isEmpty() && ! value.containsAnyOf ("0123456789"))
                value = value.toUpperCase();
            break;

        case ReadoutFormat::ValueCase::asAuthored:
        default:
            break;
    }

    auto text = param.getName (format.nameCharacterBudget).toUpperCase()
              + (format.separatorColon ? ": " : " ")
              + value;

    if (unit.isNotEmpty())
        text += " " + unit;

    return text;
}

/** Everything wrong with how a parameter would print on an LCD, or an empty array.

    **This is the part that stops the defect class rather than today's instances.** Extracting
    `describeParameter` fixes the six castings that exist; it does nothing about the next parameter
    somebody adds without an interval, which is exactly how TapeRot got there — its formatters were
    not deleted, they were never written, one parameter at a time.

    Intended to be called from each casting's own test target over its whole parameter layout, so a
    parameter that would print badly fails a build instead of a panel. What it looks for:

    - **A run of three or more decimal digits.** The signature of JUCE's 7-place default: a
      `NormalisableRange` with a zero interval and no `stringFromValueFunction` renders 20 as
      `20.0000000`. Three is the threshold rather than four because no control on any panel in this
      suite is legible to a thousandth, so a real value never reaches it.
    - **A unit in the value text AND a label**, which prints the unit twice.
    - **An empty value**, which is a formatter returning nothing rather than a value of nothing.

    It deliberately does NOT check the name budget: eliding a long name is `getName`'s job and a
    casting may legitimately have a name longer than its cell.

    @param param   the parameter to check, at its current value
    @param format  the casting's own readout spelling
*/
inline juce::StringArray readoutDefects (const juce::AudioProcessorParameter& param,
                                         const ReadoutFormat& format = {})
{
    juce::StringArray defects;

    const auto value = param.getText (param.getValue(), 0);
    const auto label = param.getLabel();

    if (value.isEmpty())
    {
        defects.add ("value text is empty");
        return defects;   // nothing further is meaningful
    }

    // A run of >= 3 digits after a decimal point. Walked rather than matched with a regex, because
    // juce::String has no regex and pulling in <regex> for this would cost more than it saves.
    for (int i = 0; i < value.length() - 3; ++i)
    {
        if (value[i] != '.')
            continue;

        int digits = 0;

        while (i + 1 + digits < value.length()
               && juce::CharacterFunctions::isDigit (value[i + 1 + digits]))
            ++digits;

        if (digits >= 3)
        {
            defects.add ("value \"" + value + "\" has " + juce::String (digits)
                         + " decimal places - the range probably has no interval and the parameter "
                           "no stringFromValueFunction, so JUCE is printing at its 7-place default");
            break;
        }
    }

    if (label.isNotEmpty() && value.endsWith (label))
        defects.add ("value \"" + value + "\" already ends with the label \"" + label
                     + "\", so the unit would print twice");

    juce::ignoreUnused (format);
    return defects;
}

/** The takeover's lifetime: what to show, and until when.

    **It owns the deadline and nothing else.** The caller decides how to notice the deadline has
    passed — four castings poll it from a repaint timer they already run, two use a one-shot
    `juce::Timer` — and the caller does all the painting. Neither mechanism is wrong and forcing one
    would mean rewriting a paint loop for no behavioural gain.

    **Time is a parameter, never read from a clock inside.** That is what makes the revert testable
    without sleeping, and it is why the tests here can assert the boundary at exactly 900 ms rather
    than around it.
*/
class ReadoutTimer
{
public:
    explicit ReadoutTimer (ReadoutFormat formatToUse = {}) : format (formatToUse) {}

    /** Begins or refreshes the takeover, and cancels any pending revert.

        **Called on every value change while a control is moved, not only on grab.** Reflect-84
        wired only `onDragStart`, so its LCD showed the value the knob held at the instant it was
        grabbed and never updated while turning — the takeover was live and simply frozen, which
        reads as a stuck display rather than as a missing call. */
    void show (juce::String textToShow)
    {
        text = std::move (textToShow);
        deadlineMs = 0;                  // held: the control is still being moved
    }

    /** Arms the revert. The takeover stays up for `revertMs` from `nowMs`. */
    void release (juce::uint32 nowMs)
    {
        if (text.isNotEmpty())
            deadlineMs = nowMs + (juce::uint32) format.revertMs;
    }

    /** Clears the takeover immediately — **naming mode**.

        The glass belongs to the name field until it commits or cancels, so a knob moved just before
        SAVE must not reappear over a half-typed name. Five castings guarded the entry point; Elmer
        relied on paint order instead, which hides the takeover without cancelling it, so it
        returned the moment naming ended if the revert had not yet fired. */
    void suppress()
    {
        text.clear();
        deadlineMs = 0;
    }

    /** The text to paint at `nowMs`, or empty once the takeover has reverted. */
    juce::String textAt (juce::uint32 nowMs) const
    {
        if (text.isEmpty())
            return {};

        // deadlineMs 0 means "held" - the control is still down, so it never expires on its own.
        if (deadlineMs != 0 && nowMs >= deadlineMs)
            return {};

        return text;
    }

    bool isShowing (juce::uint32 nowMs) const { return textAt (nowMs).isNotEmpty(); }

    /** For a caller driving a one-shot `juce::Timer` rather than polling. */
    int revertMs() const noexcept { return format.revertMs; }

    const ReadoutFormat& getFormat() const noexcept { return format; }

private:
    ReadoutFormat format;
    juce::String text;
    juce::uint32 deadlineMs = 0;
};

} // namespace nf
