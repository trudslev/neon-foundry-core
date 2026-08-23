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

/** A spec quotes an em size; `juce::Font::withHeight` sets ascent+descent, a face-specific multiple
    of it. `withPointHeight` is the one that means what a spec means. */
juce::Font faceAt (juce::Typeface::Ptr t, float cssPx)
{
    return juce::Font (juce::FontOptions (t).withPointHeight (cssPx));
}

juce::Colour brighter (juce::Colour c) { return c.brighter (0.25f); }

} // namespace


AboutBox::AboutBox (AboutMaterials materials, AboutContent content)
    : mats (std::move (materials)), text (std::move (content))
{
    setVisible (false);
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);

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
    return AboutGeometry::boxFor (getHeight());
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
        g.setFont (faceAt (mats.proseFace, G::creditsCssPx));
        g.setColour (mats.body);
        g.drawFittedText (text.typefaceCredits,
                          juce::Rectangle<int> ((int) bx + G::valueX, (int) by + G::rowTypefaces,
                                                G::valueW, (int) (G::creditsLineBox * 2.0f)),
                          juce::Justification::topLeft, 2);
    }

    {
        g.setFont (faceAt (mats.proseFace, G::noteCssPx));
        g.setColour (mats.dim);
        g.drawFittedText ("Full licence text ships as THIRD-PARTY-LICENCES.txt in this bundle's Resources.",
                          juce::Rectangle<int> ((int) bx + G::labelX, (int) by + G::rowResources,
                                                G::ruleX1 - G::ruleX0, (int) (G::noteLineBox * 2.0f)),
                          juce::Justification::topLeft, 2);
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
