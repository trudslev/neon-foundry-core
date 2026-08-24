#include "nf/AboutPart.h"

namespace nf
{
namespace
{

/** CSS `letter-spacing` puts a gap after EVERY character, so a shrink-to-fit box is n gaps wide
    where a hand-rolled loop advances n-1 times. The suite measured this: the build's n-1 rule is
    equivalent to CSS *with* `text-indent` to within 0.375 px, and differs from CSS without it by
    half a tracking. Every casting draws tracked text this way, so the About part does too rather
    than introducing a seventh convention. */
float trackedWidth (const juce::String& text, const juce::Font& font, float tracking)
{
    if (text.isEmpty())
        return 0.0f;

    return juce::GlyphArrangement::getStringWidth (font, text)
         + tracking * (float) (text.length() - 1);
}

/** Each glyph advances by the difference of two CUMULATIVE measurements, never by its own isolated
    width: for a proportional face those are different numbers, and summing isolated widths walks
    the characters left into one another. Ported from the castings, which found it the hard way on
    a "SYNC ON" that rendered as "SYNC OON". */
void drawTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                  float tracking, juce::Rectangle<float> area,
                  juce::Justification justification, juce::Colour colour)
{
    g.setFont (font);
    g.setColour (colour);

    const float total = trackedWidth (text, font, tracking);
    float x = area.getX();

    if (justification.testFlags (juce::Justification::horizontallyCentred))
        x = area.getCentreX() - total * 0.5f;
    else if (justification.testFlags (juce::Justification::right))
        x = area.getRight() - total;

    const float baseline = area.getCentreY() + font.getAscent() * 0.5f - font.getDescent() * 0.5f;
    float measured = 0.0f;

    for (int i = 0; i < text.length(); ++i)
    {
        const juce::String upTo = text.substring (0, i + 1);
        const float advance = juce::GlyphArrangement::getStringWidth (font, upTo) - measured;
        g.drawSingleLineText (text.substring (i, i + 1), juce::roundToInt (x), juce::roundToInt (baseline));
        x += advance + tracking;
        measured += advance;
    }
}

/** Word-wrapped tracked text.

    `drawFittedText` is the obvious call and it is the wrong one here: it applies no tracking, and
    §4 states .04 em on both wrapping rows. Dropping a stated figure because the convenient API has
    no parameter for it is how a spec quietly becomes an approximation — so the wrap is done here
    and each line goes through the same `drawTracked` every other row uses. */
void drawWrappedTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                         float tracking, juce::Rectangle<float> area, float lineBox,
                         juce::Colour colour);

/** A spec quotes an em size; `juce::Font::withHeight` sets ascent+descent, a face-specific multiple
    of it. `withPointHeight` is the one that means what a spec means. */
juce::Font faceAt (juce::Typeface::Ptr t, float cssPx)
{
    return juce::Font (juce::FontOptions (t).withPointHeight (cssPx));
}

juce::Colour brighter (juce::Colour c) { return c.brighter (0.25f); }

void drawWrappedTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                         float tracking, juce::Rectangle<float> area, float lineBox,
                         juce::Colour colour)
{
    juce::StringArray words;
    words.addTokens (text, " ", "");

    juce::String line;
    float y = area.getY();

    const auto flush = [&]
    {
        if (line.isNotEmpty())
            drawTracked (g, line, font, tracking, { area.getX(), y, area.getWidth(), lineBox },
                         juce::Justification::centredLeft, colour);
        y += lineBox;
        line.clear();
    };

    for (const auto& w : words)
    {
        const juce::String candidate = line.isEmpty() ? w : line + " " + w;
        if (trackedWidth (candidate, font, tracking) > area.getWidth() && line.isNotEmpty())
            flush();
        line = line.isEmpty() ? w : line + " " + w;
    }
    flush();
}

} // namespace


AboutTab::AboutTab (AboutMaterials materials, juce::Typeface::Ptr stampFace,
                    juce::String stampText, float stampCssPx, float trackingEmValue)
    : mats (std::move (materials)), face (std::move (stampFace)), stamp (std::move (stampText)),
      cssPx (stampCssPx), trackingEm (trackingEmValue)
{
    // §2b. An empty MouseCursor would silently be the arrow, so the fallback is explicit.
    setMouseCursor (mats.helpCursor == juce::MouseCursor() ? juce::MouseCursor (juce::MouseCursor::PointingHandCursor)
                                                           : mats.helpCursor);

    /*  **Buffered, because promoting the stamp moved it from a baked layer into a live child.**

        Four castings drew their version stamp inside a panel background that bakes once to an
        image, so the string cost nothing per frame. As a component it is repainted with everything
        else — 20 Hz in the CPU harness — and a gradient fill, a rounded-rect stroke and a
        per-glyph tracked run are not free at that rate.

        **Elmer's baseline caught it: five editor cells at ratios 1.115 to 1.239.** Only Elmer's
        fired, and that is the ratio bar working rather than a fault in the other five — the same
        absolute cost is the largest ratio on the smallest cell, and Elmer's editor is the lowest of
        the six. The other castings were paying it silently.

        The tab is static except on hover, and `mouseEnter`/`mouseExit` already `repaint()`, so
        buffering costs one image per tab and re-renders exactly when something changes. */
    setBufferedToImage (true);
}


AboutWordmarkHit::AboutWordmarkHit (juce::MouseCursor helpCursor)
{
    setMouseCursor (helpCursor == juce::MouseCursor() ? juce::MouseCursor (juce::MouseCursor::PointingHandCursor)
                                                      : helpCursor);
}

juce::Rectangle<int> AboutWordmarkHit::zone (int frameX)
{
    return HeaderGeometry::nameplate().translated (frameX, 0);
}

void AboutWordmarkHit::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()) && onClick != nullptr)
        onClick();
}

void AboutTab::layoutFor (int canvasH, int frameX)
{
    const auto font = faceAt (face, cssPx);
    const int run = juce::roundToInt (trackedWidth (stamp, font, trackingEm * cssPx));
    setBounds (AboutGeometry::tabFor (canvasH, run, frameX));
}

void AboutTab::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();

    /*  §2: an inset ring, not a border - HEADER-PART §3 applies unchanged.

        **Revision 3 STRUCK the argument that "a shallow etched plate reads pressable at rest".**
        On hardware raised reads pressable and recessed reads engraved, so that reasoning ran
        backwards and is why §2a added the wordmark as the PRIMARY affordance. The recess stays
        because it is the right material for a stamp promoted out of the fascia - not because it
        advertises anything. What advertises is the wordmark, and the hover treatment below. */
    g.setGradientFill ({ hot ? mats.wellTop.brighter (0.18f) : mats.wellTop, 0.0f, r.getY(),
                         hot ? mats.wellBottom.brighter (0.18f) : mats.wellBottom, 0.0f, r.getBottom(),
                         false });
    g.fillRoundedRectangle (r, AboutGeometry::tabRadius);
    g.setColour (mats.ring);
    g.drawRoundedRectangle (r.reduced (0.5f), AboutGeometry::tabRadius, 1.0f);

    // §2: hover takes the ink to the casting's accent and lightens the well one step.
    drawTracked (g, stamp, faceAt (face, cssPx), trackingEm * cssPx, r,
                 juce::Justification::centred, hot ? mats.accent : mats.wellInk);
}

void AboutTab::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()) && onClick != nullptr)
        onClick();
}

void AboutTab::mouseEnter (const juce::MouseEvent&) { hot = true;  repaint(); }
void AboutTab::mouseExit  (const juce::MouseEvent&) { hot = false; repaint(); }


AboutBox::AboutBox (AboutMaterials materials, AboutContent content, int frameX)
    : mats (std::move (materials)), text (std::move (content)), frameOriginX (frameX)
{
    setVisible (false);
    setWantsKeyboardFocus (true);

    /*  **No `setAlwaysOnTop`.** It is a top-level-window call: on a child it reaches for the peer
        and JUCE gives the component one, which resized the standalone's window the moment the box
        opened — a 1340 canvas settling at 1284, caught by `capture_panel.py` refusing to measure a
        resampled GUI rather than by anything looking wrong. Z-order is the editor's job and it has
        it: the box is the last child added, so it is already above everything. */

    // §4: the row must not wrap, so a slug over budget is a build failure rather than a layout
    // that quietly degrades. A wrapped URL invites a reader to copy half of it.
    jassert (repositoryFits (text.repositorySlug));
}

AboutBox::~AboutBox() = default;

void AboutBox::open()
{
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();     // §3: without this Escape goes to the host
    repaint();
}

void AboutBox::close()
{
    setVisible (false);
    if (onDismiss != nullptr)
        onDismiss();
}

juce::Rectangle<int> AboutBox::boxBounds() const
{
    return AboutGeometry::boxFor (getHeight(), frameOriginX);
}

juce::Rectangle<int> AboutBox::closeBounds() const
{
    const auto b = boxBounds();
    return { b.getX() + AboutGeometry::closeX, b.getY() + AboutGeometry::rowClose,
             AboutGeometry::closeW, AboutGeometry::closeH };
}

void AboutBox::resized()
{
    // §4's minimum. Asserted rather than assumed: every casting clears 620 today and a future
    // canvas could not, in which case the box would sit off its own panel.
    jassert (getHeight() >= AboutGeometry::minCanvasH);
}

void AboutBox::mouseMove (const juce::MouseEvent& e)
{
    const bool hot = closeBounds().contains (e.getPosition());
    if (hot != closeHot)
    {
        closeHot = hot;
        repaint (closeBounds().expanded (2));
    }
    setMouseCursor (hot ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void AboutBox::mouseUp (const juce::MouseEvent& e)
{
    // §6: the veil is a dismissal target, and so is CLOSE. A click inside the box that is not on
    // CLOSE does nothing - the box is not a control surface.
    if (! boxBounds().contains (e.getPosition()) || closeBounds().contains (e.getPosition()))
        close();
}

bool AboutBox::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

void AboutBox::paint (juce::Graphics& g)
{
    using G = AboutGeometry;

    /*  §3: a DARKENING scrim at 0.72 of the casting's darkest ink - not the bypass veil's grey
        multiply at 0.50. Different colour, opposite direction, so a reader can tell which is which
        with both on screen. They stack and neither suppresses the other: bypass is host-driven and
        About is user-driven, and suppressing either from the other's state would put panel logic in
        charge of a host decision. */
    g.fillAll (mats.glass.withAlpha (G::veilAlpha));

    const auto box = boxBounds().toFloat();

    // §9: the box is the casting's own display GLASS, not its fascia.
    g.setColour (mats.glass);
    g.fillRoundedRectangle (box, G::boxRadius);

    // §4 and HEADER-PART §3: an inset ring, never a border.
    g.setColour (mats.ring);
    g.drawRoundedRectangle (box.reduced (0.5f), G::boxRadius, 1.0f);

    const float bx = box.getX(), by = box.getY();

    const auto label = [&] (int y, const juce::String& s)
    {
        drawTracked (g, s, faceAt (mats.labelFace, G::rowLabelCssPx), G::rowLabelTracking * G::rowLabelCssPx,
                     { bx + G::labelX, by + (float) y, (float) G::labelW, G::rowLabelLineBox },
                     juce::Justification::centredLeft, mats.dim);
    };

    const auto value = [&] (int y, const juce::String& s, juce::Colour ink, bool underscored)
    {
        const auto font = faceAt (mats.monoFace, G::rowValueCssPx);
        const float tracking = G::rowValueTracking * G::rowValueCssPx;
        const juce::Rectangle<float> area { bx + G::valueX, by + (float) y,
                                            (float) G::valueW, G::rowValueLineBox };
        drawTracked (g, s, font, tracking, area, juce::Justification::centredLeft, ink);

        /*  §7: **the underscore marks the link, the accent marks the phosphor class.** Two marks
            doing two jobs. The plugin and suite lines are accent because they ARE phosphor-class
            data - numbers read off the build, the same class as a figure in a meter well - so
            underscoring them would say they lead somewhere. Accent alone is not a link here;
            accent PLUS an underscore is. */
        if (underscored)
        {
            const float w = trackedWidth (s, font, tracking);
            g.setColour (ink.withAlpha (0.5f));
            g.fillRect (area.getX(), area.getBottom() - 2.0f, w, 1.0f);
        }
    };

    const auto rule = [&] (int y)
    {
        // §5: the three rules are STRUCTURE, not functional text, and are exempt from the floor.
        g.setColour (mats.body.withAlpha (0.18f));
        g.fillRect (bx + G::ruleX0, by + (float) y, (float) (G::ruleX1 - G::ruleX0), 1.0f);
    };

    drawTracked (g, text.castingName, faceAt (mats.labelFace, G::nameCssPx),
                 G::nameTracking * G::nameCssPx,
                 { bx + G::labelX, by + G::rowCastingName, (float) (G::ruleX1 - G::ruleX0), G::nameLineBox },
                 juce::Justification::centredLeft, mats.body);

    value (G::rowModelVersion,
           "MODEL " + text.modelCode + " " + juce::String::charToString (0x00b7)
               + " VERSION " + text.pluginVersion,
           mats.accent, false);

    rule (G::rule1Y);

    drawTracked (g, "NEON FOUNDRY", faceAt (mats.labelFace, G::foundryCssPx),
                 G::foundryTracking * G::foundryCssPx,
                 { bx + G::labelX, by + G::rowFoundry, (float) (G::ruleX1 - G::ruleX0), G::foundryLineBox },
                 juce::Justification::centredLeft, mats.body);

    value (G::rowSuiteRelease, "SUITE RELEASE " + text.suiteRelease, mats.accent, false);

    rule (G::rule2Y);

    label (G::rowLicence,    "LICENCE");
    value (G::rowLicence,    "AGPLv3 " + juce::String::charToString (0x00b7) + " SOURCE AVAILABLE",
           mats.body, false);

    label (G::rowRepository, "REPOSITORY");
    value (G::rowRepository, text.repositorySlug, mats.accent, true);

    label (G::rowFoundryLink, "FOUNDRY");
    value (G::rowFoundryLink, "neonfoundry.io", mats.accent, true);

    rule (G::rule3Y);

    label (G::rowTypefaces, "TYPEFACES");
    {
        // §8: an acknowledgement, not a legal document - it names faces and licence families and
        // points at the file. It must not paraphrase a licence.
        drawWrappedTracked (g, text.typefaceCredits, faceAt (mats.proseFace, G::creditsCssPx),
                            G::creditsTracking * G::creditsCssPx,
                            { bx + G::valueX, by + G::rowTypefaces, (float) G::valueW, G::creditsLineBox },
                            G::creditsLineBox, mats.body);
    }

    {
        drawWrappedTracked (g, "Full licence text ships as THIRD-PARTY-LICENCES.txt in this "
                               "bundle's Resources.",
                            faceAt (mats.proseFace, G::noteCssPx), G::noteTracking * G::noteCssPx,
                            { bx + G::labelX, by + G::rowResources,
                              (float) (G::ruleX1 - G::ruleX0), G::noteLineBox },
                            G::noteLineBox, mats.dim);
    }

    /*  §6: CLOSE is a LEGEND ON A SHOE, not a new part - PARTS-CATALOGUE §4B's two-position shoe at
        96 x 30, drawn in its idle state with a permanent legend. Nothing else in the box is a
        control.

        §9 states no shoe face per casting, so the idle face is the casting's own well pair from
        §9.2 - the one recessed-control material the part does state. Flagged in the ask rather
        than invented quietly. */
    {
        const auto shoe = closeBounds().toFloat();
        g.setGradientFill ({ closeHot ? brighter (mats.wellTop) : mats.wellTop, 0.0f, shoe.getY(),
                             closeHot ? brighter (mats.wellBottom) : mats.wellBottom, 0.0f, shoe.getBottom(),
                             false });
        g.fillRoundedRectangle (shoe, G::boxRadius);
        g.setColour (mats.ring);
        g.drawRoundedRectangle (shoe.reduced (0.5f), G::boxRadius, 1.0f);

        drawTracked (g, "CLOSE", faceAt (mats.labelFace, G::closeCssPx),
                     G::closeTracking * G::closeCssPx, shoe,
                     juce::Justification::centred,
                     closeHot ? mats.accent : mats.wellInk);
    }
}

} // namespace nf
