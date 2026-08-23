#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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

    /** Canvas width is 1340 in every casting, so x is a constant rather than a law. */
    static constexpr int canvasW = 1340;
    static constexpr int boxX = (canvasW - boxW) / 2;                       // 230

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
    /** §2: the meter row's right edge, closing the header block's own 22 px left padding. */
    static constexpr int tabRight = 1302;
    static constexpr int tabBottomInset = 20;                    // bottom edge = canvasH - 20
    static constexpr int tabH = 24;                              // 13 line box + 5.5 either side
    static constexpr int tabPadX = 10;                           // width is shrink-to-fit
    static constexpr float tabCssPx = 10.0f, tabLineBox = 13.0f;
    static_assert (tabH % 2 == 0, "even, so 0.5x lands on whole pixels");

    static juce::Rectangle<int> tabFor (int canvasH, int textWidth)
    {
        const int w = textWidth + 2 * tabPadX;
        return { tabRight - w, canvasH - tabBottomInset - tabH, w, tabH };
    }

    static juce::Rectangle<int> boxFor (int canvasH)
    {
        return { boxX, boxYFor (canvasH), boxW, boxH };
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
};


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
class AboutBox final : public juce::Component
{
public:
    AboutBox (AboutMaterials materials, AboutContent content);
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
    bool closeHot = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutBox)
};

} // namespace nf
