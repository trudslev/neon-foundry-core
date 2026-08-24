#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "nf/HeaderPart.h"   // §2a: the wordmark hit box IS HeaderGeometry::nameplate()

/*  ============================================================================================
    THE ABOUT PART — shared geometry, shared behaviour, six materials.

    `design/ABOUT-PART.md` revision 2. §1 states what is shared and it is nearly everything: the
    affordance and where it sits, the veil, the box's size and position law, every internal
    coordinate in §4, every type size and tracking, the row order, the dismissal set, the link
    treatment, the character budgets, and the content of every line except five strings.

    **So the box lives here rather than six times.** That is a different split from
    `HeaderPart.h`, which carries geometry and leaves painting to the casting — correctly, because
    each header genuinely draws differently. This part does not: only its MATERIALS differ, and a
    material is a parameter. Holding it once is what makes the divergence table's seven entries
    unrepresentable for this part rather than merely unlikely.

    What a casting supplies: `AboutMaterials` (its glass, three inks, ring, tab well, three
    typefaces) and `AboutContent` (five strings plus the suite release). Nothing else.
    ============================================================================================ */

namespace nf
{

/** §4, §2 and §5 — every figure the part states, and nothing derived by eye.

    Coordinates inside the box are box-local with the origin at its top-left. */
struct AboutGeometry
{
    // --- the box, §4 ------------------------------------------------------------------------
    static constexpr int boxW = 880;
    static constexpr int boxH = 540;
    static constexpr float boxRadius = 3.0f;

    /** **§4's law is FRAME-local, not canvas-local**, and the two coincide in five castings.

        The frame is 1340 wide everywhere — it is `HeaderGeometry::canvasWidth` and the shared
        header block sits in it — but Fifth Member's WINDOW is 1444, because it draws two 52 px
        rack ears outside the frame. Its box is at **282**, which is this 230 plus that 52.

        §4's own table row reads *"Canvas width is 1340 in every casting"*, which is the sentence
        this parameter exists because of: it is true of the frame and false of one window, and it
        would have put the box 52 px left of the panel on the one casting whose §11 states the
        exception explicitly. The part's §0 has it right — *"Fifth Member is also the one x
        exception … §4's law is frame-local"* — so this is the specification being implemented
        rather than an accommodation.

        **`frameX` is a required argument everywhere it is needed, never a default.** Five castings
        write 0. A default would make the common case silent and leave the exception as the only
        one that has to remember, which is how a figure goes missing in exactly one of six. */
    static constexpr int frameW = 1340;
    static constexpr int boxX = (frameW - boxW) / 2;                        // 230, frame-local

    /** §4's law. 54 at Reflect-84's 648, 136 at Chorus-60's 812. */
    static constexpr int boxYFor (int canvasH) { return (canvasH - boxH) / 2; }

    /** 540 + 40 top and bottom. The shortest casting is 648, so every casting clears it — this
        is asserted rather than assumed, because a future canvas could not. */
    static constexpr int minCanvasH = 620;

    // --- the two columns, §4 ----------------------------------------------------------------
    static constexpr int labelX = 40;
    static constexpr int labelW = 150;
    static constexpr int valueX = 200;                                       // 40 + 150 + 10 gap
    static constexpr int valueW = 640;                                       // 200 + 640 = 840
    static constexpr int ruleX0 = 40;
    static constexpr int ruleX1 = 840;
    static_assert (valueX + valueW == boxW - labelX, "the value column must close the box's own "
                                                     "40 px right margin");

    // --- the rows, §4, in the order the table gives them -------------------------------------
    static constexpr int rowCastingName   = 40;
    static constexpr int rowModelVersion  = 84;
    static constexpr int rule1Y           = 120;
    static constexpr int rowFoundry       = 144;
    static constexpr int rowSuiteRelease  = 174;
    static constexpr int rule2Y           = 210;
    static constexpr int rowLicence       = 234;
    static constexpr int rowRepository    = 270;
    static constexpr int rowFoundryLink   = 306;
    static constexpr int rule3Y           = 342;
    static constexpr int rowTypefaces     = 366;
    static constexpr int rowResources     = 426;
    static constexpr int rowClose         = 470;

    static constexpr int closeX = 744;
    static constexpr int closeW = 96;
    static constexpr int closeH = 30;
    static_assert (closeX + closeW == ruleX1, "CLOSE's right edge sits on the rules' right edge");

    // --- type, §4. Sizes are CSS px: build them with withPointHeight, never withHeight --------
    static constexpr float nameCssPx        = 34.0f, nameLineBox     = 40.0f, nameTracking    = 0.10f;
    static constexpr float dataCssPx        = 18.0f, dataLineBox     = 24.0f, dataTracking    = 0.14f;
    static constexpr float foundryCssPx     = 20.0f, foundryLineBox  = 26.0f, foundryTracking = 0.20f;
    static constexpr float rowLabelCssPx    = 18.0f, rowLabelLineBox = 24.0f, rowLabelTracking= 0.22f;
    static constexpr float rowValueCssPx    = 18.0f, rowValueLineBox = 24.0f, rowValueTracking= 0.10f;
    static constexpr float creditsCssPx     = 18.0f, creditsLineBox  = 24.0f, creditsTracking = 0.04f;
    static constexpr float noteCssPx        = 16.0f, noteLineBox     = 21.0f, noteTracking    = 0.04f;
    static constexpr float closeCssPx       = 16.0f, closeLineBox    = 20.0f, closeTracking   = 0.22f;

    /** §5's floor, and it is RELATIVE rather than absolute: nothing in the box below 16, which is
        1.6x the panel's own ~10 px functional floor. At 0.5x the box's smallest text renders at 8
        where the panel's functional text renders at 5, so **the box degrades later than the panel
        it sits on** — if the panel is usable the box is. An absolute floor would force 20 px
        minimums and a box too tall for the shortest casting.

        CLOSE is included, at 16/20. Revision 1 set it at 14/18 while stating this rule two
        sections later; a constraint with one exemption in the same document is not a constraint. */
    static constexpr float typeFloorCssPx = 16.0f;
    static_assert (closeCssPx   >= typeFloorCssPx, "CLOSE is not exempt from the floor");
    static_assert (noteCssPx    >= typeFloorCssPx, "the resources note is not exempt");
    static_assert (rowValueCssPx>= typeFloorCssPx, "");

    /** §4. `github.com/trudslev/fifth-member-audio-plugin` is 45 and measures 486 px at 18 px mono
        with .10 em, inside the 640 px column with 154 to spare. **The row must not wrap** — a
        wrapped URL invites a reader to copy half of it — so a longer slug is a build failure. */
    static constexpr int repositoryCharBudget = 45;

    // --- the veil, §3 -------------------------------------------------------------------------
    /** The casting's DARKEST ink at 0.72. A darkening scrim, not the bypass veil's grey multiply
        at 0.50: different colour, opposite direction, so a reader can tell which is which with
        both on screen. They stack and neither suppresses the other. */
    static constexpr float veilAlpha = 0.72f;

    // --- the tab, §2 and §9.2 -----------------------------------------------------------------
    /** §2: the meter row's right edge, closing the header block's own 22 px left padding.

        **Derived, not re-typed.** `HeaderGeometry::bandRightEdge` is the same 1302 and is where the
        figure comes from; taking it from there means that if the header band ever moves, the tab
        moves with it or this file fails to compile. A literal here would be indistinguishable from
        the derivation for as long as they agree — which is the only window in which they differ. */
    static constexpr int tabRight = HeaderGeometry::bandRightEdge;   // 1302
    static constexpr int tabBottomInset = 20;                    // bottom edge = canvasH - 20
    static constexpr int tabH = 24;                              // 13 line box + 5.5 either side
    static constexpr int tabPadX = 10;                           // width is shrink-to-fit

    /** **2, not the box's 3**, and derived from the artefact rather than from the prose. §2 gives
        the tab no radius at all, so it was drawn at `boxRadius` — and all six delivered prototypes
        declare `border-radius: 2px` on their tab against `3px` on their box. One pixel, in the
        same direction, in six independent files, which is a decision rather than a rounding. */
    static constexpr float tabRadius = 2.0f;
    static constexpr float tabCssPx = 10.0f, tabLineBox = 13.0f;
    static_assert (tabH % 2 == 0, "even, so 0.5x lands on whole pixels");

    /** `frameX` is the left edge of the 1340-wide frame inside the window: 0 in five castings and
        52 in Fifth Member. `tabRight` is frame-local for the same reason `boxX` is — it is the
        meter row's right edge, and the meter row is in the header block. */
    static juce::Rectangle<int> tabFor (int canvasH, int textWidth, int frameX)
    {
        const int w = textWidth + 2 * tabPadX;
        return { frameX + tabRight - w, canvasH - tabBottomInset - tabH, w, tabH };
    }

    static juce::Rectangle<int> boxFor (int canvasH, int frameX)
    {
        return { frameX + boxX, boxYFor (canvasH), boxW, boxH };
    }
};


/** The six materials of §9 — everything a casting supplies that is not a string.

    §9's material decision: **the box is the casting's own display glass, not its fascia.** Glass is
    the surface these panels already use for dense small text at high contrast, every casting has
    one, and a reader already knows text on glass is meant to be read. It also settles §3 for free —
    a grey bypass multiply over a dark screen still reads as a grey wash. */
struct AboutMaterials
{
    juce::Colour glass;          ///< §9.1, the box's surface
    juce::Colour body;           ///< §9.1, >= 7:1 on glass
    juce::Colour dim;            ///< §9.1, the label column. The tight one — TapeRot's 7.12 is the
                                 ///< suite's narrowest, so a casting may not darken this to taste
    juce::Colour accent;         ///< §9.1, phosphor class: the two data lines and the two links
    juce::Colour ring;           ///< §9.1, the glass lightened to ~18 % against the fascia's hue

    juce::Colour wellTop;        ///< §9.2, the tab's recess
    juce::Colour wellBottom;
    juce::Colour wellInk;        ///< §9.2, >= 7:1 against the WELL, never against the fascia

    juce::Typeface::Ptr labelFace;   ///< Barlow Condensed 600
    juce::Typeface::Ptr proseFace;   ///< Barlow Condensed 500
    juce::Typeface::Ptr monoFace;    ///< the casting's own mono

    /*  **RULED by Sune 2026-08-24: the custom help cursor is RESTORED, superseding §2b revision 4.**

        Revision 4 struck it on an accessibility argument this build supplied — and that argument was
        given to the designers with **an error and an omission in it**, so the ruling rested on a
        partly wrong input:

        - **Wrong**: it said JUCE's `fromHIServices` path *"ends in `setSize:` and hands a fixed
          NSImage"*, implying Apple's own help artwork was no better than a bitmap. It is not fixed —
          it loads Apple's `cursor.pdf` and rasterises it at 1x, 2x, 3x and 4x, adding all four as
          representations. The `setSize` is per-representation and is what MAKES it multi-resolution.
        - **Omitted**: it presented a binary, when a third option existed — the custom cursor by
          default, swapping to `PointingHandCursor` only when the accessibility pointer size is
          enlarged.

        **THE DETECTION EXISTS. Established 2026-08-24 by probing, because "about twenty lines" was
        an estimate and this suite has a name for those.** What is actually true:

        | | |
        |---|---|
        | **Windows** | **JUCE ALREADY DOES IT.** `juce_Windowing_windows.cpp` rescales a custom cursor to `GetSystemMetricsForDpi (SM_CXCURSOR)`, scales the hotspot with it and caches per size. Nothing to write |
        | **macOS** | `createCursor` passes the image straight through — the gap. Two detection routes, both readable: `NSCursor.currentSystemCursor`, whose image measures **23 x 22 points with four representations** on this machine, and is **public API**; and `CFPreferencesCopyAppValue ("mouseDriverCursorSize", "com.apple.universalaccess")`, which reads ABSENT at default |
        | **Linux** | `createCustomMouseCursorInfo` uses the image at its own size, hotspot unscaled — the same gap, and X11 sizing is per-theme (`Xcursor.size`, `XCURSOR_SIZE`) rather than one setting |

        **So it is a macOS-and-Linux question, not a suite-wide one**, and on the platform that
        ships first there is a public API for it.

        **And the freshness objection was wrong too.** The concern was that no notification fires
        for the setting, so a value read at construction goes stale. `mouseEnter` already exists on
        both affordances and fires whenever the pointer crosses the zone — reading there is cheap
        and current by construction.

        **What is NOT established**, and cannot be from here: that the reported size actually
        *changes* when the setting does. Confirming that means changing a system-wide accessibility
        setting on someone's machine, so it needs a machine with it already enabled. Everything
        above is a reading of what the APIs return today at the default setting.

        **This is the one place the suite's own position argues against accepting the cost.** The
        reader who pays it is the reader with an enlarged pointer — which is the reader BRAND.md's
        7:1 floor and its 4x scaling range exist for. Recorded as a decision with its facts rather
        than a note with an estimate.

        **The accessibility cost is real and is accepted rather than mitigated.** An app-supplied
        cursor never gets the window server's larger renditions, so a reader running an enlarged
        pointer sees a 20 x 24 bitmap. That is a stated cost of the ruling, not something this
        comment should pretend away.

        The field is a MATERIAL again, and the argument for deleting it does not survive the change:
        it was *"six copies of one decision"* while every casting passed the same standard cursor.
        An embedded per-casting asset is not that. */
    /** §2b: **`help`, not `pointer`, on both affordances.** `pointer` says *this acts*; `help` says
        *this explains something*, which is what an About box is — the one signal that costs no ink,
        changes nothing at rest, and fires on an ordinary sweep. JUCE has no help cursor in
        `StandardCursorType`, so the casting builds one from the delivered
        `shared/assets/about-cursor-*.png` and hands it over. If it is ever empty the fallback is
        `PointingHandCursor`, which loses the distinction rather than the affordance. */
    juce::MouseCursor helpCursor;
};


/** §2b: **the multiplier above which the system pointer counts as ENLARGED.**

    macOS's Accessibility pointer size is a **continuous multiplier**, not a step, and it is written
    to `com.apple.universalaccess`'s `mouseDriverCursorSize`. Watched at 4 Hz through a live drag
    from Normal to Large and back, it reads:

        1.000 -> 2.026 -> 3.433 -> 4.000 -> 3.620 -> 2.694 -> 1.671 -> 1.000

    So default is exactly **1.000** — or the key is absent, on a machine that has never touched the
    slider — and the smallest non-default value observed anywhere in the drag was **1.361**.

    **1.01 therefore sits clear of both ends**: above floating-point noise around an exact 1.000,
    and far below any value a real drag produces. It is not a guess about where a step lands,
    because there are no steps.

    **And it errs in the safe direction.** Too low, and a reader at the default size sees
    `PointingHandCursor` — the signal is lost, nothing is harmed. Too high, and a reader with an
    enlarged pointer sees a 20 x 24 bitmap, which is the failure this mechanism exists to prevent. */
constexpr double enlargedPointerMultiplier = 1.01;

/** Whether the reader has enlarged their system pointer.

    **macOS only.** Returns false everywhere else, and that is correct rather than a stub:

    | | |
    |---|---|
    | **Windows** | **JUCE already rescales custom cursors**, in `juce_Windowing_windows.cpp`: it queries `GetSystemMetricsForDpi (SM_CXCURSOR)`, rescales the image, scales the hotspot with it, and caches per size. There is nothing to detect because there is nothing to fix |
    | **Linux** | **a gap, and one with nothing to read.** `createCustomMouseCursorInfo` uses the image at its own size with the hotspot unscaled, and X11 cursor sizing is per-theme — `Xcursor.size`, `XCURSOR_SIZE` — rather than one system setting. There is no equivalent value to test |

    On macOS it reads `mouseDriverCursorSize` from `com.apple.universalaccess`, **chosen by watching
    a live slider drag rather than by reasoning about the APIs.** Polled at 4 Hz for 90 seconds
    while the pointer size went Normal -> Large -> Normal:

    | | Through that drag |
    |---|---|
    | `mouseDriverCursorSize` | **1.000 -> 4.000 -> 1.000**, tracking continuously, with no synchronize call and no notification |
    | `NSCursor.currentSystemCursor` | **28 x 40 throughout — it never moved**, across a fourfold change |

    **An earlier version of this part was built on `currentSystemCursor` and was wrong.** It reports
    the cursor being SHOWN, so its size is a property of the shape: the same run caught a 0.3 s blip
    to 18 x 28 as the pointer crossed something else, before the slider was touched. The two
    readings that had looked like evidence — 23 x 22 then 28 x 40 — were two shapes at one size.

    **The sandbox caveat is real and currently theoretical.** `CFPreferencesCopyAppValue` cannot
    read another application's domain under App Sandbox, so an AUv3 would read default and keep the
    custom cursor. No casting builds an AUv3 — the format lists are AU, VST3 and Standalone — so
    nothing shipping is affected. Recorded rather than guarded against. */
bool systemPointerIsEnlarged();

/** The cursor an About affordance should show **right now**.

    Call it from `mouseEnter`, not from a constructor: macOS posts no notification when the pointer
    size changes, so a value read once goes stale. `mouseEnter` fires whenever the pointer crosses
    the zone, which makes the reading current by construction and costs one query per crossing. */
juce::MouseCursor aboutCursor (const juce::MouseCursor& custom);


/** The five strings §1 names, plus the suite release.

    §1: **suite release and plugin version are separate version strings on separate lines, both
    semver.** The suite's minor moves when any one plugin's minor moves, so a casting at 1.0.0
    inside suite 1.2.1 is the normal case rather than a mismatch — which is why they are never
    printed on one line, where a reader could take the second as a build tag of the first. */
/** §1's suite release, held ONCE for all six.

    It is a property of the suite rather than of any casting, so a per-casting `CMakeLists` figure
    would be six copies of one number — the shape this repository's divergence table has seven
    entries of. Core is the only thing all six consume, so it is the only place that can hold it
    once.

    **It is a version, not a counter**, and it is semver: a patch-only round bumps patch, any one
    plugin's minor bumps the suite's minor and resets patch to 0, a breaking change anywhere bumps
    major. Revision 1 printed `SUITE RELEASE 3` and treated the field as an integer.

    **It does not derive from any plugin's version and no plugin's derives from it.** A casting at
    1.0.0 inside suite 1.2.1 is the normal case. Mirrors `manifest/suite.toml`'s `version`, which is
    the suite's own record; when that moves, this moves with it. */
inline const juce::String suiteRelease { "1.0.0" };


struct AboutContent
{
    juce::String castingName;      ///< "GATECRASHER"
    juce::String modelCode;        ///< "GR-85"
    juce::String pluginVersion;    ///< "1.0.0" — semver, from PROJECT_VERSION, never a literal
    juce::String suiteRelease;     ///< "1.0.0" — semver, and NOT derived from the plugin's
    juce::String repositorySlug;   ///< "github.com/trudslev/gatecrasher-audio-plugin"
    juce::String typefaceCredits;  ///< §8: the faces the casting EMBEDS, not the ones it draws with
};


/** The box and its veil. One component, added over the whole canvas, visible only while open.

    §6: dismisses on **Escape, a click anywhere on the veil, and the CLOSE shoe** — all three, not a
    choice between them, because each covers a reader the others miss. The veil takes keyboard focus
    on open, which is what makes Escape work; without it the key goes to the host.

    §6 also: opening and closing **touch no parameter and persist nothing.** The box is not a state
    of the plugin — it does not enter the parameter tree, does not serialise, and is closed on every
    load. */
/** §2's affordance: **the version stamp, promoted to a recessed tab.** No new element is added to
    any panel, and the tab is here rather than six times because §1 makes the affordance and its
    position shared — only the well's material and the stamp's own string differ.

    Why not the wordmark: it is artwork on two castings, so an affordance there is a hit region over
    a bitmap on two panels and over live text on four — six answers to one question. Why not a
    screw: a screw reads inert, and making an inert-looking element load-bearing is worse than
    decorating one. The stamp is the only string on any panel whose entire job is identifying this
    build, so clicking it is not a control action.

    **The ink is measured against the WELL, not the fascia** (§9.2). Against the fascia the 7:1 floor
    is unreachable on two castings — gatecrasher tops out at 6.45 and fifth-member at 3.16 — because
    a ceiling is set by the ground, not the ink. Against the well it runs 7.15 to 11.46. */
class AboutTab final : public juce::Component
{
public:
    /** §2, revision 3: the tab takes **the face and size the casting's stamp already uses**, not
        a mono 10 / 13 the part imposes. Revision 2 stated mono and was wrong for two castings —
        Fifth Member's stamp is Barlow Condensed 600 at 11 / 13 / .26 em by its own §8 foot-strip
        row and Reflect-84's is Barlow Condensed 600 at 10 / 13 / .1 em, so forcing mono would have
        re-typed a string the casting had already specified. The part adds the recess, the ink, the
        cursor and the handlers; it does not restate the type. */
    AboutTab (AboutMaterials materials, juce::Typeface::Ptr stampFace,
              juce::String stampText, float cssPx, float trackingEm);

    std::function<void()> onClick;

    /** Places itself from §2's law against the canvas it sits on. `frameX` is the frame's left
        edge in the window — 0 in five castings, 52 in Fifth Member. */
    void layoutFor (int canvasH, int frameX);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    AboutMaterials mats;
    juce::Typeface::Ptr face;
    juce::String stamp;
    float cssPx, trackingEm;
    bool hot = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutTab)
};


/** §2a's PRIMARY affordance: **the wordmark opens the box too.**

    Revision 2 ruled the wordmark out and revision 3 reverses it, striking the argument that a
    recessed plate reads pressable — on hardware **raised** reads pressable and **recessed reads
    engraved**, so the original reasoning had picked the least discoverable spot on the panel and
    justified it with a hardware idiom running backwards. Two further problems with the stamp
    alone: a 10 px dim string in the bottom-right corner is where nothing is, so a hover-only
    reveal never fires; and hardware has no About box to borrow a convention from, so any
    affordance here is a software convention in hardware clothes.

    **The objection that ruled the wordmark out was a BUILD objection answering a DISCOVERABILITY
    question.** *"A hit region over a bitmap"* is about drawing, and a hit region needs only a
    rectangle — `HeaderGeometry::nameplate()`, **303 x 84**, shared and identical in all six. A hit
    region over a bitmap and one over live text are the same rectangle, which is exactly why the
    zone is used rather than the letterforms.

    It draws nothing. The wordmark is already on the panel; this only claims its box. */
class AboutWordmarkHit final : public juce::Component
{
public:
    explicit AboutWordmarkHit (juce::MouseCursor helpCursor);

    std::function<void()> onClick;

    void paint (juce::Graphics&) override {}          // the wordmark is drawn by the panel
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;

    /** The nameplate zone, in WINDOW coordinates: `HeaderGeometry`'s frame-local rectangle
        moved by the frame's own origin. */
    static juce::Rectangle<int> zone (int frameX);

private:
    /** Kept so `mouseEnter` can re-decide. The constructor's choice would be stale the moment the
        reader changed their pointer size, and macOS gives no notification to hang an update on. */
    juce::MouseCursor custom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutWordmarkHit)
};


class AboutBox final : public juce::Component
{
public:
    AboutBox (AboutMaterials materials, AboutContent content, int frameX);
    ~AboutBox() override;

    /** Opens over `canvas`, which must be the editor's untransformed content bounds. */
    void open();
    void close();

    std::function<void()> onDismiss;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** §4's budget, checked rather than trusted. */
    static bool repositoryFits (const juce::String& slug)
    {
        return slug.length() <= AboutGeometry::repositoryCharBudget;
    }

private:
    juce::Rectangle<int> boxBounds() const;
    juce::Rectangle<int> closeBounds() const;

    AboutMaterials mats;
    AboutContent text;
    int frameOriginX;
    bool closeHot = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutBox)
};

} // namespace nf
