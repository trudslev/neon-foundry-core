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

    /* The default above is JUCE's own rule and is deliberately the WRONG answer for this suite - it
       is what a casting gets by saying nothing, and saying nothing is the thing being fixed. Set it
       from `captionHeight()` instead. */

    int separatorHeight = 9;

    /** Space above the first row and below the last. JUCE's default is 2; Elmer's spec asks for 4. */
    int borderSize = 2;

    /** Added to the measured text width: the tick or marker column plus the row's own padding. */
    int leadingColumn = 0;
    int horizontalPadding = 26;

    /** JUCE asks a separator for a width too, and it must not be the widest thing in the list. */
    int minimumSeparatorWidth = 50;
};

/** The height a section caption takes: its padding plus the line box its own type produces.

    **The construction, not the number — and that distinction is the whole ruling.** Every casting
    that uses this currently lands on 19, and *that is a coincidence*: Share Tech Mono's line box is
    1.127 em against IBM Plex Mono's 1.300, and 11px of the former plus 9px of the latter happen to
    meet once the padding is added. Written as a literal `19` in five themes, the construction would
    be lost and the first font or type-size change would silently break the rule — which is exactly
    how the caption came to inherit JUCE's `rowHeight + rowHeight / 2` in the first place.

    So a casting states its padding and its caption font, and the number falls out. A theme that
    later changes its caption type gets a correct caption without anyone noticing they needed to.

    **`juce::Font::getHeight()` is the CSS `normal` line box** for a font built with
    `FontOptions::withPointHeight`, which is how every casting here builds one: JUCE defines height
    as ascent + descent scaled to the em size, and CSS `normal` is
    `(ascender − descender + lineGap) / unitsPerEm`. The two agree exactly while `lineGap` is zero,
    which it is for both faces in this suite — Plex Mono 1025/−275/0 and Share Tech Mono 885/−242/0.
    A face with a non-zero line gap would need that gap added here, and this is the comment that
    should be read before assuming it does not.

    @param captionFont    the font the caption is actually drawn in, not a nominal size
    @param topPadding     above the line box
    @param bottomPadding  below it
*/
inline int captionHeight (const juce::Font& captionFont, int topPadding, int bottomPadding)
{
    return juce::roundToInt ((float) topPadding + captionFont.getHeight() + (float) bottomPadding);
}

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

    //==============================================================================
    /** **The three `draw*` a casting must supply, made pure so it cannot forget one.**

        A derived class that omits one silently inherits `LookAndFeel_V4`'s — JUCE's own menu
        painting, inside an instrument. That is a mechanism in core permitting the thing the design
        rules forbid, which is the same reason `ReadoutFormat::ValueCase` was deleted rather than
        documented.

        All five castings that use this already override all three, so this costs nothing today and
        closes the hole for good.

        **`drawPopupMenuUpDownArrow` is deliberately NOT among them**, and that is a gap rather
        than an exemption. No casting overrides it, so all five currently draw JUCE's scroll arrows
        — and they are reachable: the suite contract runs a list to the panel's bottom, so a long
        enough User bank scrolls. Making it pure would force five castings to invent scroll-arrow
        artwork no designer has specified, which is the opposite failure.

        Reflect-84 is not in that five. Its list is a `Component` rather than a `PopupMenu` (three
        of its GUI-SPEC §9 requirements are things `PopupMenu` structurally cannot do), so it paints
        its own chevrons from delivered artwork and never reaches this class. **§H4's extraction is
        therefore complete at five, not half-done at six** — Reflect-84 is not a casting that failed
        to adopt the shared metrics, it is a casting whose list is a different kind of object.

        When the other five get chevron artwork of their own, the fourth becomes pure too. */
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override = 0;

    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override = 0;

    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override = 0;

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
