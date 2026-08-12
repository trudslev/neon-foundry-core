#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace nf
{

/** The Program dropdown's **sizes**. Not its colours, and not one pixel of its painting.

    Every casting's dropdown is drawn as an extension of its own LCD, so the painting is genuinely
    per-casting and stays there. The metrics are not: they decide how tall the list comes out, and
    the root `CLAUDE.md` contract says the list runs from the display's bottom edge to the panel's
    bottom and never outgrows it. A row height nobody chose puts that out.
*/
struct MenuMetrics
{
    /** The height of one Program row. */
    int rowHeight = 24;

    /** The height of a `FACTORY` / `USER` section caption.

        **Stated rather than inherited, and that is the point of this field existing.** JUCE's
        `LookAndFeel_V2::getIdealPopupMenuSectionHeaderSizeWithOptions` ends with
        `idealHeight += idealHeight / 2` — a caption is one and a half rows, so 24px rows give a
        36px caption. Four castings took that by omission and one, Elmer, overrode it with a
        measured 19 against its 22px rows: *"LookAndFeel_V2 sizes a section header at the item
        height plus half again — 33 against our 22 — which pushed everything below FACTORY 14px down
        the list and put the whole bank out of step with the render."*

        A caption *taller* than a row is JUCE's convention for a menu; a caption *smaller* than a
        row is what these panels' design language actually asks for, since it is a small-caps label
        rather than a heading. The suite does not currently agree, and the disagreement is visible.
        Whichever way it goes, a casting says the number here instead of inheriting one.
    */
    int sectionHeaderHeight = 36;

    int separatorHeight = 9;

    /** Space above the first row and below the last. JUCE's default is 2; Elmer's spec asks for 4. */
    int borderSize = 2;

    /** Added to the measured text width: the tick or marker column plus the row's own padding. */
    int leadingColumn = 0;
    int horizontalPadding = 26;

    /** JUCE asks a separator for a width too, and it must not be the widest thing in the list. */
    int minimumSeparatorWidth = 50;
};

/** A `LookAndFeel_V4` that owns the Program dropdown's metrics and nothing else.

    **Every `draw*` is left alone deliberately.** Each casting paints its list as an extension of its
    own LCD — Elmer's omits its top border so the join with the glass reads as one instrument, and
    that is not a shared decision. What *is* shared is how big the pieces are.

    A casting derives from this, passes its `MenuMetrics`, and implements the one hook that cannot
    be shared: measuring its own text, since a casting drawing tracked text measures it differently
    from one drawing a plain run.
*/
class MenuMetricsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit MenuMetricsLookAndFeel (MenuMetrics metricsToUse) : metrics (metricsToUse) {}

    const MenuMetrics& getMenuMetrics() const noexcept { return metrics; }

    /** **The row height never grows to the platform's standard item height.**

        JUCE passes `standardMenuItemHeight` from `PopupMenu::Options::withStandardItemHeight`, and
        the four castings that wrote `jmax (rowHeight, standardMenuItemHeight)` would have grown
        their rows the moment anyone set one. None does today, so that spelling and this one produce
        identical lists — **a latent divergence, not a live defect**, and worth saying plainly
        rather than counting as a bug fixed.

        Elmer alone pinned it, and its reason is the general one: a bank has to fit the panel
        without scrolling, and the platform standard is taller than any of these rows on macOS. So
        the pinned form is the one that moves here.
    */
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override
    {
        juce::ignoreUnused (standardMenuItemHeight);

        if (isSeparator)
        {
            idealWidth  = metrics.minimumSeparatorWidth;
            idealHeight = metrics.separatorHeight;
            return;
        }

        idealWidth  = (int) std::ceil (measureMenuItemText (text))
                    + metrics.leadingColumn + metrics.horizontalPadding;
        idealHeight = metrics.rowHeight;
    }

    /** **Overridden on the WithOptions form, because that is the one V2 actually calls.** The older
        two-argument variant delegates to it, so overriding that one instead silently does nothing —
        which is how four castings ended up with a caption height none of them chose. */
    void getIdealPopupMenuSectionHeaderSizeWithOptions (const juce::String& text,
                                                        int standardMenuItemHeight,
                                                        int& idealWidth, int& idealHeight,
                                                        const juce::PopupMenu::Options& options) override
    {
        juce::ignoreUnused (standardMenuItemHeight, options);

        idealWidth  = (int) std::ceil (measureSectionHeaderText (text))
                    + metrics.leadingColumn + metrics.horizontalPadding;
        idealHeight = metrics.sectionHeaderHeight;
    }

    int getPopupMenuBorderSize() override { return metrics.borderSize; }

protected:
    /** The width `text` will occupy as this casting actually draws it.

        Not shareable: a casting drawing tracked text measures it differently from one drawing a
        plain run, and measuring it the other way is how a row comes out too narrow for its own
        content. */
    virtual float measureMenuItemText (const juce::String& text) = 0;

    /** The same for a section caption, which may be a different face or size. Defaults to the row
        measurement, which is right whenever the caption shares the row's font. */
    virtual float measureSectionHeaderText (const juce::String& text)
    {
        return measureMenuItemText (text);
    }

private:
    MenuMetrics metrics;
};

} // namespace nf
