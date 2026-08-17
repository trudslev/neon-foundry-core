#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

/**
    The header part's GEOMETRY and its LCD budget — `design/HEADER-PART.md` in one place.

    ## This is the extraction's §I crossed a THIRD time, and it is named here rather than assumed

    §I scopes core as *shipping behaviour*. The measurement harness crossed that line and said so at
    its own exemption; `nf::processInChunks` crossed it a second time and said so in the plan. **A
    third crossing allowed silently is how a boundary stops meaning anything**, so it gets the same
    treatment as the first two — named, with the three-part test written out:

      - **It is behaviour, not appearance.** Every figure here is a coordinate, a size or a derived
        budget. Nothing here has a colour, a gradient, a font file or a paint call in it. The
        material is the casting's and stays there, which is `HEADER-PART.md` §1's own split.
      - **It is identical across all six castings.** §1 lists what is shared and it is exactly this:
        the canvas width, the block's box, the nameplate zone, the caption row, the descriptor
        anchor, the five-part band with its heights, widths, gaps and baseline, and the LCD's
        budget and cap. A figure that is per casting is not in this file — see `descriptorLeading`,
        which is the one thing §4 says a seventh casting could not derive, and is therefore an
        argument rather than a constant.
      - **It has no per-casting meaning in it.** Core does not know what a Program is called, what
        material a block is, which metaphor a nameplate uses, or how a list opens.

    ## Why it exists at all, which is a measured argument rather than a tidy one

    Every figure below was held **seven times** — once in the spec, once in each of six panels, and
    again in the parts strip. `HEADER-PART.md` §10 records three propagation failures in a single
    round, in three different asset classes: the chevron glyph reached one casting and missed nine
    sites; the model-line ink landed in the strip for four castings and the bodies for two; and the
    1340 canvas reached the panels but neither exported plate.

    That is the same mechanism as this suite's divergence table, which reached **seven** entries on
    2026-08-16 — a `dryBuffer` sized in `prepareToPlay` in five castings and never in the sixth.
    The answer to six copies has never been a rule about which copy wins.

    ## The seam, and the two places it is NOT where the proposal put it

    §10 proposes one component taking "material, ink, strings and phosphor as props". Checked against
    the code it would replace — which is this suite's own rule, and what the rejected
    `ValueTree::Listener` design cost — two parts of that do not survive:

      - **The nameplate is not a prop.** Six metaphors are six paint routines, not six values of one:
        a spray stencil and an engraved relief share no parameters. The decisive case is TapeRot,
        whose wordmark is **not drawn at all** — it ships as artwork with its font binary
        deliberately absent for licensing. No struct of colours expresses that. §8 of the spec
        already says the nameplate block is the exception and is per casting.
      - **The list mechanism is not shared.** Reflect-84's Program list is a `juce::Component`; the
        other five are `PopupMenu` with the whole `menuHost` machinery — the 1 px anchor strip, the
        8 px lead, the parent area. Reflect-84's own header records deleting each of those and why.
        A part owning "open the list" would either reintroduce that machinery there or force five
        castings onto a rewrite.

    So what lives here is what the audit actually showed drifting: **figures, and the one path drawn
    from them.** Material, the nameplate painter and the list mechanism stay with the casting.
*/
namespace nf
{

//==============================================================================
/** `HEADER-PART.md` §2 — the coordinate table, canvas-local, origin top-left.

    **Every figure is the spec's, transcribed once.** A casting reads them; it does not restate them.
    Where a value below is *derived* it is computed here rather than written down, so that changing
    the term it depends on cannot leave a stale literal behind — which is the failure mode this whole
    file exists to remove.
*/
struct HeaderGeometry
{
    //== The canvas ============================================================
    /** §2. Canvas width in every casting. Canvas *height* is per casting and is not here. */
    static constexpr int canvasWidth = 1340;

    //== The block =============================================================
    static constexpr int blockX = 16;
    static constexpr int blockY = 16;
    static constexpr int blockW = 1308;
    static constexpr int blockH = 104;

    /** §2. Flush to the block's bottom edge — **derived, not transcribed**, because the whole point
        of §3's inset-ring instruction is that these two must not drift apart. A border added to the
        outside of the block would put the body one pixel low and this would still read 120. */
    static constexpr int bodyOriginY = blockY + blockH;      // 120

    //== The nameplate zone ====================================================
    static constexpr int nameplateX = 38;
    static constexpr int nameplateY = 30;
    static constexpr int nameplateW = 303;
    static constexpr int nameplateH = 84;

    /** §4. **The anchor line.** All six function descriptors sit on this y; the wordmark above it
        does not align across the six and must not be made to. See `descriptorLeading`. */
    static constexpr int descriptorY = 78;
    static constexpr int descriptorH = 17;

    static constexpr int modelLineY = 95;
    static constexpr int modelLineH = 14;

    //== The band ==============================================================
    /** §2. **A fixed figure, not a proportion of the panel.** The castings are differently-sized
        units rather than scales of one design, so a manufacturer uses the same physical part across
        a product line. 34 is set by the Program buttons: two 11 px legends with leading and padding
        need about 27, and a height that only just fits does not read. */
    static constexpr int bandH = 34;

    /** §2. The caption row — PROGRAM, IN, OUT all sit here. */
    static constexpr int captionY = 41;
    static constexpr int captionH = 13;

    static constexpr int bandY = 61;

    static constexpr int lcdX = 357;
    static constexpr int lcdW = 641;

    static constexpr int saveX = 1006;
    static constexpr int saveW = 62;

    static constexpr int deleteX = 1076;
    static constexpr int deleteW = 70;

    /** §2. Wells are 64 wide, and the 18 px gap from DELETE is **wider than the meters' own 10** so
        the pair reads as a pair. */
    static constexpr int meterWellW = 64;
    static constexpr int inWellX = 1164;
    static constexpr int outWellX = 1238;

    //== The gaps, derived rather than stated ==================================
    /** 8 px from the LCD to SAVE, and 8 again from SAVE to DELETE. Derived so a coordinate edit
        cannot silently change a gap the spec states in words. */
    static constexpr int lcdToSaveGap = saveX - (lcdX + lcdW);          // 8
    static constexpr int saveToDeleteGap = deleteX - (saveX + saveW);   // 8
    static constexpr int deleteToMeterGap = inWellX - (deleteX + deleteW); // 18
    static constexpr int betweenMetersGap = outWellX - (inWellX + meterWellW); // 10

    /** §2. **The row's right edge, and the check that the row closes on itself.** 1302 = 1324 − 22,
        closing the block's own left padding. An earlier revision had 76-wide wells ending flush at
        1324, which gave the block 22 px of left padding and none on the right. */
    static constexpr int bandRightEdge = outWellX + meterWellW;         // 1302

    /*  **WITHDRAWN 2026-08-17: there was a `chassisInset` and a `programListFootY` here, and the
        contract they encoded is retracted by §12.**

        They said the Program list's foot lands 16 px above the panel's bottom rather than flush, and
        the 16 was presented as derived: the block sits at x 16 / y 16 with w 1308 = 1340 − 2 × 16, so
        16 is the only inset the part uses. The arithmetic was right and the conclusion was wrong,
        for a reason worth keeping here rather than only in a changelog.

        **The 16 was found by looking for something Reflect-84's published 537 could be derived
        FROM.** It appears in no spec, no panel and no changelog as a list margin. That a figure
        *can* be derived is not evidence that it *was* — a derivation constructed after the fact to
        explain an unexplained number is a reconstruction, not a base, and this suite has already
        spent three candidates that fitted their gap and were wrong.

        **And the direction of fit was backwards.** Five castings hang a flush list; changing the
        shared contract so that one casting's transcription becomes correct is exactly the drift the
        shared part exists to prevent. §12 keeps the rule — the list runs from the display's bottom
        edge to the panel's bottom edge — and corrects the figure instead: Reflect-84 is
        **553 = 648 − 95**.

        Nothing replaces them. A casting's list height is its own canvas less its LCD's bottom edge,
        which needs no shared helper. */

    static juce::Rectangle<int> block()      { return { blockX, blockY, blockW, blockH }; }
    static juce::Rectangle<int> nameplate()  { return { nameplateX, nameplateY, nameplateW, nameplateH }; }
    static juce::Rectangle<int> lcd()        { return { lcdX, bandY, lcdW, bandH }; }
    static juce::Rectangle<int> saveButton() { return { saveX, bandY, saveW, bandH }; }
    static juce::Rectangle<int> deleteButton() { return { deleteX, bandY, deleteW, bandH }; }
    static juce::Rectangle<int> inWell()     { return { inWellX, bandY, meterWellW, bandH }; }
    static juce::Rectangle<int> outWell()    { return { outWellX, bandY, meterWellW, bandH }; }

    /** §4. **Core CHECKS the anchor; it does not compute the leading.** Read the reason before
        replacing this with the arithmetic that looks like it should be here.

        Each nameplate metaphor is a different physical object at a different height — a label-maker
        strip, a spray stencil, a silkscreen with a rule under it, an engraved plate, a length of
        gaffer tape, a moulded relief. §4 gives a table of heights and leadings and states that all
        six land the descriptor on `descriptorY`.

        **That table does not close for three of the six**, measured 2026-08-16 by computing
        `nameplateY + topOffset + height + leading` for every published row:

        | Casting | Stack | Lands | |
        |---|---|---|---|
        | Gatecrasher | 30 + **8** + 38 + 2 | 78 | closes |
        | Chorus-60 | 30 + 32+5+5 + 6 | 78 | closes |
        | Elmer | 30 + 39 + 9 | 78 | closes |
        | Reflect-84 | 30 + 40 + 9 | 79 | **+1** |
        | Fifth Member | 30 + 34 + 9 | 73 | **−5** |
        | TapeRot | 30 + 38 + 4 | 72 | **−6** |

        Gatecrasher's row is the tell: it carries an explicit `+ 8 offset`, so the spec **does** model
        a per-casting top offset inside the zone — and names it for exactly one casting. The other
        three shortfalls are almost certainly unstated offsets of the same kind, but *almost
        certainly* is not a figure, and inventing three would be fitting a control to the answer.

        So the casting supplies all three terms and core asserts the property that actually matters:
        **whatever the metaphor does inside the 303 × 84 zone, the descriptor's line box starts on
        78.** That is §4's own instruction to a seventh casting, verbatim, rather than a derivation
        the published table cannot support. `design-asks/header-nameplate-offsets.md` carries the ask.
    */
    static constexpr int descriptorTopFor (int nameplateTop, int nameplateHeightPx, int leading)
    {
        return nameplateTop + nameplateHeightPx + leading;
    }

    /** True when a casting's nameplate stack lands the descriptor on the shared anchor. */
    static constexpr bool landsOnDescriptorAnchor (int nameplateTop, int nameplateHeightPx, int leading)
    {
        return descriptorTopFor (nameplateTop, nameplateHeightPx, leading) == descriptorY;
    }
};

//==============================================================================
/** `HEADER-PART.md` §5 — the LCD cell, and the budget derived from it.

    **The budget is COMPUTED here, never transcribed.** §5 states 49 characters and a 47-character
    user-name cap, and both fall out of the cell's own terms. Writing them down as literals is how a
    figure survives the change that invalidates it: this suite has five recorded instances of a
    number that was true when written, and one — `lcdCharacterBudget` at 36 against a measured 41 —
    is this exact quantity in this exact place.

    The spec's own literals are asserted against these derivations in core's tests, which is the
    check that the law and the stated answer still agree. That comparison is only worth anything
    because its two sides come from different places: the law from the cell's measured terms, the
    answer from the document. A test deriving both from one of them would prove nothing.
*/
struct LcdCell
{
    /** §5, measured off Share Tech Mono at 17 px. */
    static constexpr float glyphAdvance = 9.180f;

    /** §5, stated: .10 em at 17 px. */
    static constexpr float tracking = 1.700f;

    /** §5, computed. */
    static constexpr float perCharacter = glyphAdvance + tracking;      // 10.880

    /** §5, measured. `FACT` / `USER` / `NAME` — four characters at a 41.82 px glyph run, so a 72 px
        cell carries 15.09 px of padding per side, in the same family as every other inset in the
        part.

        **This cell is now spent as reserve.** It has about 2 px of padding left before it reaches
        the part's 14 px text inset, so the next request for glyph room here does come out of the
        name budget rather than out of this. */
    static constexpr float bankCellW = 72.0f;

    static constexpr float dividerW = 1.0f;

    /** §5, measured. Raised 25 → 30 in revision 2 so the chevron gains a 16 px box inset matching
        the cell's own text — 14.1 px to the rotated ink, since the glyph's ink reaches past its
        box. The 5 px came out of the bank cell rather than out of the name area, which is why the
        budget did not move. */
    static constexpr float chevronTrim = 30.0f;

    /** §5, derived: 641 − 72 − 1 − 30 = 538.00. */
    static constexpr float nameAreaW =
        (float) HeaderGeometry::lcdW - bankCellW - dividerW - chevronTrim;

    /** n glyphs need n advances and n−1 gaps. */
    static constexpr float runFor (int glyphs)
    {
        return glyphs <= 0 ? 0.0f
                           : (float) glyphs * glyphAdvance + (float) (glyphs - 1) * tracking;
    }

    /** §5. **The budget, by exact fit rather than by division** — `floor(area / perCharacter)` is
        not the same question, because the last glyph pays no trailing gap. */
    static constexpr int characterBudget()
    {
        int n = 0;
        while (runFor (n + 1) <= nameAreaW)
            ++n;
        return n;                                                        // 49
    }

    /** §5. The dirty marker is ` *` (2) and the caret is a block (1); the cap reserves the larger.

        **The cap rises everywhere and falls nowhere.** Previous caps were Reflect-84 39, Chorus-60
        31, Fifth Member 26, Gatecrasher 27, TapeRot 25, Elmer 22 — so the floor for this round was
        39, the highest current one, because lowering a cap orphans names already saved: they load,
        and then fail to save back under their own name. */
    static constexpr int dirtyMarkerChars = 2;
    static constexpr int caretChars = 1;

    static constexpr int userNameCap()
    {
        return characterBudget() - (dirtyMarkerChars > caretChars ? dirtyMarkerChars : caretChars);
    }
};

//==============================================================================
/** `HEADER-PART.md` §6 — the Program buttons' five-state matrix.

    **Two legends each, permanently printed; the legend itself lights.** SAVE above STORE, DELETE
    above CANCEL — the resting function on top, what the button becomes during naming beneath it.
    Neither legend moves, changes weight or changes size, and there is **no disabled face**: cap,
    ring and highlight are identical in all five states, because both legends stepped back reads as
    "nothing to do here" where a blank button reads as broken.

    This is here rather than in six panels because it is a *decision table*, and a decision table
    copied six times is the shape that produced one casting printing the plus sign at `db >= 0.0f`
    and another at `db > 0.0f` — one value, two castings, no reason.
*/
struct ProgramButtonLegends
{
    bool save = false;
    bool store = false;
    bool deleteLegend = false;
    bool cancel = false;

    bool operator== (const ProgramButtonLegends& o) const noexcept
    {
        return save == o.save && store == o.store
            && deleteLegend == o.deleteLegend && cancel == o.cancel;
    }
};

/** The panel state the matrix is indexed by. `naming` wins over the other two axes, which is why it
    is a state here rather than a third bool: while naming, what the underlying Program is and
    whether it is edited change nothing about the legends. */
struct ProgramPanelState
{
    bool isUserProgram = false;
    bool isEdited = false;
    bool isNaming = false;
};

/** §6's table, as a function rather than as six transcriptions.

    | Panel state | SAVE | STORE | DELETE | CANCEL |
    |---|---|---|---|---|
    | Factory, unmodified | idle | idle | idle | idle |
    | Factory, edited | **lit** | idle | idle | idle |
    | User, unmodified | idle | idle | **lit** | idle |
    | User, edited | **lit** | idle | **lit** | idle |
    | Naming | idle | **lit** | idle | **lit** |

    **SAVE always creates a new named Program and never overwrites**, even with a User Program
    loaded, and DELETE works only on User Programs — which is what makes the second column of this
    table depend on the bank at all. Escaping out of naming leaves the Program still edited, because
    nothing was stored.
*/
inline ProgramButtonLegends programButtonLegends (const ProgramPanelState& state) noexcept
{
    if (state.isNaming)
        return { false, true, false, true };

    return { state.isEdited, false, state.isUserProgram, false };
}

//==============================================================================
/** §10's chevron — **the glyph that reached one casting and missed nine sites.**

    It is here because it is the propagation failure the spec leads with, and because it is a *path*
    rather than a figure: nine sites drew a 9 × 9 rotated box at 84.5° against the drawn path's 77°,
    which is a difference no coordinate table would have caught.

    **The up chevron is this path MIRRORED, not rotated.** A rotated V puts its round caps on the
    wrong axis, so the two marks stop being the same object seen twice.

    Core returns the path and draws nothing: stroke width, colour and glow are the casting's, per §1.
*/
struct Chevron
{
    static constexpr float width = 14.0f;
    static constexpr float height = 8.0f;

    /** A downward chevron inside `bounds`, centred, at the part's own 14 × 8 proportion. */
    static juce::Path down (juce::Rectangle<float> bounds)
    {
        const auto box = juce::Rectangle<float> (width, height).withCentre (bounds.getCentre());

        juce::Path p;
        p.startNewSubPath (box.getX(), box.getY());
        p.lineTo (box.getCentreX(), box.getBottom());
        p.lineTo (box.getRight(), box.getY());
        return p;
    }

    /** The same path mirrored about its own horizontal centre line — never rotated. */
    static juce::Path up (juce::Rectangle<float> bounds)
    {
        const auto box = juce::Rectangle<float> (width, height).withCentre (bounds.getCentre());

        juce::Path p;
        p.startNewSubPath (box.getX(), box.getBottom());
        p.lineTo (box.getCentreX(), box.getY());
        p.lineTo (box.getRight(), box.getBottom());
        return p;
    }
};

} // namespace nf
