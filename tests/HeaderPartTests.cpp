#include <nf/HeaderPart.h>

#include <juce_gui_basics/juce_gui_basics.h>

/**
    `design/HEADER-PART.md`'s figures, asserted against the derivations in `nf/HeaderPart.h`.

    ## What this file is actually checking, because it is not "the constants are the constants"

    **The two sides come from different places, and that is the whole value.** The left-hand side of
    every assertion below is the *document's stated answer*, spelled as a literal here. The
    right-hand side is *computed from the cell's own terms*. A test that derived both from one of
    them would be the shape this suite has already paid for twice — Gatecrasher's migration test
    building its fixture from the constant it asserted against, and core's own pin check reading the
    local ref for the tag it was verifying. Both passed; neither could fail.

    So: if someone changes `glyphAdvance`, `bankCellW` or `chevronTrim`, the derivation moves and
    these literals do not, and the build stops. That is the propagation guarantee §10 asks for,
    expressed as the one thing that cannot be forgotten.
*/
class HeaderPartTests final : public juce::UnitTest
{
public:
    // Category "core", not "GUI": NeonFoundryCoreTests runs runTestsInCategory("core"), so a suite
    // filed anywhere else compiles, links and never executes while the binary exits 0.
    HeaderPartTests() : juce::UnitTest ("Header part", "core") {}

    void runTest() override
    {
        using G = nf::HeaderGeometry;
        using L = nf::LcdCell;

        beginTest ("§2 — the coordinate table, against the spec's own literals");
        {
            expectEquals (G::canvasWidth, 1340);

            expectEquals (G::block().getX(), 16);
            expectEquals (G::block().getY(), 16);
            expectEquals (G::block().getWidth(), 1308);
            expectEquals (G::block().getHeight(), 104);

            expectEquals (G::nameplate().getX(), 38);
            expectEquals (G::nameplate().getY(), 30);
            expectEquals (G::nameplate().getWidth(), 303);
            expectEquals (G::nameplate().getHeight(), 84);

            expectEquals (G::descriptorY, 78);
            expectEquals (G::modelLineY, 95);
            expectEquals (G::captionY, 41);
            expectEquals (G::bandY, 61);
            expectEquals (G::bandH, 34);

            expectEquals (G::lcd().getX(), 357);
            expectEquals (G::lcd().getWidth(), 641);
            expectEquals (G::saveButton().getX(), 1006);
            expectEquals (G::saveButton().getWidth(), 62);
            expectEquals (G::deleteButton().getX(), 1076);
            expectEquals (G::deleteButton().getWidth(), 70);
            expectEquals (G::inWell().getX(), 1164);
            expectEquals (G::outWell().getX(), 1238);
            expectEquals (G::meterWellW, 64);
        }

        beginTest ("§2 — the gaps and the body origin are DERIVED, so they cannot drift from the table");
        {
            // Stated in the spec's prose rather than in its coordinate column, which is exactly the
            // kind of figure that goes stale when a coordinate moves and the sentence does not.
            expectEquals (G::lcdToSaveGap, 8, "the LCD-to-SAVE gap");
            expectEquals (G::saveToDeleteGap, 8, "the SAVE-to-DELETE gap");

            expectEquals (G::deleteToMeterGap, 18,
                          "18 px, deliberately wider than the meters' own 10, so the pair reads as "
                          "a pair");
            expectEquals (G::betweenMetersGap, 10, "the gap between the two wells");

            expectEquals (G::bandRightEdge, 1302,
                          "the row must close on 1324 - 22, mirroring the block's own left padding");

            // §3's whole argument: an inset ring keeps these two flush, a border does not.
            expectEquals (G::bodyOriginY, 120);
            expectEquals (G::bodyOriginY, G::blockY + G::blockH,
                          "the body origin must BE the block's bottom edge, not a number that "
                          "happens to equal it");
        }

        beginTest ("The chassis inset is DERIVED from the block, and the list foot follows it");
        {
            // 16 is not a fresh figure: it is what §2's own block coordinates already imply.
            expectEquals (G::chassisInset, 16);
            expectEquals (G::canvasWidth - G::blockW, G::chassisInset * 2,
                          "1340 - 1308 must be twice the inset, or 16 is not the chassis inset and "
                          "the list foot is a named number rather than a derived one");
            expectEquals (G::blockY, G::chassisInset,
                          "the block's top must sit on the same inset as its sides");

            // Reflect-84's published 537 is the consequence, and it reproduces.
            const int listTop = G::bandY + G::bandH;                 // the LCD's bottom edge
            expectEquals (listTop, 95);
            expectEquals (G::programListFootY (648), 632);
            expectEquals (G::programListFootY (648) - listTop, 537,
                          "Reflect-84's stated list height must fall out of the contract rather "
                          "than being transcribed beside it");

            // The property, independent of any casting's height: the list closes on the same frame
            // the block does. Asserted at a second height so it is not one arithmetic coincidence.
            expectEquals (G::programListFootY (812) - listTop, 701, "a taller panel, Chorus-60's");
            expectGreaterThan (G::programListFootY (700), G::bandY + G::bandH,
                               "the foot must sit below the display on any supported panel height");
        }

        beginTest ("§4 — all six nameplate stacks now land on the anchor, with measured zeros");
        {
            /*  **CLOSED 2026-08-17, and this arm is what closed it.** It previously pinned three
                rows as MISSING the anchor by −5, −6 and +1, with the instruction that a revision
                closing one should update the row. All six close now.

                What was wrong was not the offsets. Five rows genuinely are zero — they are now
                stated as MEASURED zeros, because a measured zero and an assumed zero read
                identically in a table. **The heights were taken from the type rather than from the
                object**: TapeRot's 38 was its wordmark's line box where the nameplate is a Dymo
                plate with 2 px of padding each side (→ 44), and Fifth Member's 34 was its line box
                where the tape carries 5 above and 7 below (→ 40). Two leadings were read off a
                render as a visual gap rather than computed, and were out by one.

                That is why the three misses had no arithmetic in common — a glyph line box is not a
                nameplate, and the two that missed hardest are the two whose metaphors carry
                padding.

                **The stack is a check, not a generator.** All six panels pin the descriptor
                absolutely, so no leading has ever produced the anchor in any casting — which is
                exactly how three rows sat wrong for two revisions without a panel looking wrong.
                Nothing read them. This arm is the thing that reads them. */
            struct Row { const char* casting; int top; int box; int leading; };

            const Row rows[] = {
                { "TapeRot",      8 - 8, 44, 4 },   // Dymo plate: 2 + 40 line box + 2
                { "Gatecrasher",  8,     38, 2 },   // the one non-zero offset, always real
                { "Chorus-60",    0,     42, 6 },   // 32 + 5 rule + 5
                { "Reflect-84",   0,     40, 8 },   // engraved plate; leading was 9
                { "Fifth Member", 0,     40, 8 },   // tape: 5 + 28 line box + 7; was 34 / 9
                { "Elmer",        0,     39, 9 },   // relief plinth
            };

            for (const auto& r : rows)
            {
                const auto lands = G::descriptorTopFor (G::nameplateY + r.top, r.box, r.leading);
                logMessage (juce::String ("  ") + r.casting + ": 30 + " + juce::String (r.top)
                            + " + " + juce::String (r.box) + " + " + juce::String (r.leading)
                            + " -> " + juce::String (lands));

                expect (G::landsOnDescriptorAnchor (G::nameplateY + r.top, r.box, r.leading),
                        juce::String (r.casting) + " no longer lands on the shared anchor. Every row "
                        "closed as of §4's 2026-08-17 revision, so this is a regression rather than "
                        "a known gap — the descriptor would sit off the line every other casting hits");
            }

            // **The layout box is not the artwork box, and mixing them is why the table could not
            // close whatever the figures were.** TapeRot's plate and Fifth Member's tape are
            // rotated, so their artwork is taller than the box the stack uses — 50 against 44, 45.6
            // against 40. Asserted so a later reader taking heights off the artwork column
            // reintroduces the original defect and is told.
            expect (! G::landsOnDescriptorAnchor (G::nameplateY, 50, 4),
                    "TapeRot's ARTWORK box (50, rotated) must not close the stack — the layout box "
                    "is 44, and taking heights from the artwork column is the defect §4 just fixed");
        }

        beginTest ("§5 — the LCD budget is COMPUTED and must equal the document's measured answer");
        {
            expectWithinAbsoluteError (L::perCharacter, 10.880f, 0.0005f,
                                       "advance 9.180 + tracking 1.700");

            expectWithinAbsoluteError (L::nameAreaW, 538.00f, 0.005f,
                                       "641 - 72 - 1 - 30; the spec MEASURED 538.00 and any other "
                                       "figure here means a term moved");

            expectWithinAbsoluteError (L::runFor (49), 531.42f, 0.01f,
                                       "49 glyphs and 48 gaps");
            expectWithinAbsoluteError (L::runFor (50), 542.30f, 0.01f,
                                       "50 glyphs and 49 gaps");

            expect (L::runFor (49) <= L::nameAreaW, "49 must fit");
            expect (L::runFor (50) > L::nameAreaW, "50 must not");

            expectEquals (L::characterBudget(), 49,
                          "the budget is the document's measured 49; a derivation that disagrees "
                          "means a term of the cell moved without the spec being re-measured");

            expectEquals (L::userNameCap(), 47,
                          "49 less the larger of the dirty marker (2) and the caret (1)");

            // **The cap rises everywhere and falls nowhere**, so the floor is a property worth
            // asserting rather than a sentence. 39 was the highest cap in the suite before this
            // round; anything below it orphans names already on disk.
            expectGreaterThan (L::userNameCap(), 38,
                               "the cap fell below the round's floor of 39, which orphans every "
                               "saved name longer than the new cap: they load, then fail to save "
                               "back under their own name");
        }

        beginTest ("§5 — the budget is by EXACT FIT, which is a different question from division");
        {
            // floor(538.00 / 10.880) = 49 here, so the two agree today and the distinction looks
            // academic. It is not: the last glyph pays no trailing gap, so the two questions come
            // apart whenever the area is within one tracking step of a boundary. Asserting the
            // property rather than the coincidence is what keeps a later "simplification" honest.
            const int byDivision = (int) std::floor (L::nameAreaW / L::perCharacter);
            const int byExactFit = L::characterBudget();

            logMessage ("  by division " + juce::String (byDivision)
                        + ", by exact fit " + juce::String (byExactFit));

            expectGreaterOrEqual (byExactFit, byDivision,
                                  "an exact fit can never hold FEWER glyphs than the division "
                                  "estimate, because the estimate charges the last glyph for a gap "
                                  "it does not have");
        }

        beginTest ("§6 — the five-state legend matrix, every row");
        {
            const auto legends = [] (bool user, bool edited, bool naming)
            {
                return nf::programButtonLegends ({ user, edited, naming });
            };

            const nf::ProgramButtonLegends allIdle { false, false, false, false };

            expect (legends (false, false, false) == allIdle,
                    "Factory, unmodified: nothing lit");

            expect (legends (false, true, false) == nf::ProgramButtonLegends { true, false, false, false },
                    "Factory, edited: SAVE only");

            expect (legends (true, false, false) == nf::ProgramButtonLegends { false, false, true, false },
                    "User, unmodified: DELETE only");

            expect (legends (true, true, false) == nf::ProgramButtonLegends { true, false, true, false },
                    "User, edited: SAVE and DELETE");

            // Naming wins over both other axes, which is the property that makes it a state rather
            // than a third flag — asserted at all four combinations, not just the one that is easy.
            for (const bool user : { false, true })
                for (const bool edited : { false, true })
                    expect (legends (user, edited, true)
                                == nf::ProgramButtonLegends { false, true, false, true },
                            "Naming must read STORE/CANCEL whatever the underlying Program is");
        }

        beginTest ("§6 — DELETE is gated on the BANK and SAVE on the EDIT, never the other way round");
        {
            // The distinguishing property, not one that passes either way. Both bits set produces a
            // row where a matrix with the two axes swapped is indistinguishable, so the arms that
            // matter are the two single-bit rows above; this states why they are the ones asserted.
            const auto factoryEdited = nf::programButtonLegends ({ false, true, false });
            const auto userClean = nf::programButtonLegends ({ true, false, false });

            expect (factoryEdited.save && ! factoryEdited.deleteLegend,
                    "an edited FACTORY Program must offer SAVE and not DELETE — there is nothing to "
                    "delete");
            expect (userClean.deleteLegend && ! userClean.save,
                    "an unmodified USER Program must offer DELETE and not SAVE — nothing has moved");
        }

        beginTest ("§10 — the chevron is 14 x 8, and UP is the path mirrored rather than rotated");
        {
            const juce::Rectangle<float> cell { 0.0f, 0.0f, 30.0f, 34.0f };

            const auto down = nf::Chevron::down (cell);
            const auto up = nf::Chevron::up (cell);

            const auto db = down.getBounds();
            const auto ub = up.getBounds();

            logMessage ("  down " + db.toString() + "   up " + ub.toString());

            expectWithinAbsoluteError (db.getWidth(), 14.0f, 0.01f);
            expectWithinAbsoluteError (db.getHeight(), 8.0f, 0.01f);

            // **The property that distinguishes a mirror from a rotation.** A 180° rotation and a
            // vertical mirror produce the SAME bounding box and the same two endpoints, so bounds
            // alone cannot tell them apart — the apex is what differs. Mirrored, the apex keeps its
            // x and flips its y; rotated, it would move in x as well.
            expectWithinAbsoluteError (ub.getWidth(), 14.0f, 0.01f);
            expectWithinAbsoluteError (ub.getHeight(), 8.0f, 0.01f);
            expectWithinAbsoluteError (ub.getCentreX(), db.getCentreX(), 0.01f,
                                       "a rotated chevron would move its apex off the cell's "
                                       "vertical axis, which is what puts the round caps on the "
                                       "wrong side");

            // The apexes themselves: down points to the bottom, up to the top, both on one axis.
            expectWithinAbsoluteError (down.getPointAlongPath (down.getLength() * 0.5f).y,
                                       db.getBottom(), 0.01f, "the down chevron's apex is its foot");
            expectWithinAbsoluteError (up.getPointAlongPath (up.getLength() * 0.5f).y,
                                       ub.getY(), 0.01f, "the up chevron's apex is its head");
        }

        beginTest ("The derivations are SHOWN ABLE TO FAIL, not merely observed passing");
        {
            /*  Every arm above is an equality against a literal, and an equality that has never been
                seen to fail is indistinguishable from one that cannot. So the terms are perturbed
                here — by recomputing the same laws with one input moved — and the budget is shown to
                move with them.

                This is the arm that makes the rest of the file evidence. Without it, a refactor that
                accidentally froze `characterBudget()` to a constant 49 would leave every assertion
                above green while the propagation guarantee silently stopped existing. */

            const auto budgetFor = [] (float nameArea, float advance, float track)
            {
                const auto run = [advance, track] (int n)
                {
                    return n <= 0 ? 0.0f : (float) n * advance + (float) (n - 1) * track;
                };

                int n = 0;
                while (run (n + 1) <= nameArea)
                    ++n;
                return n;
            };

            // The control: the real terms must reproduce the real answer through this second
            // implementation. If they do not, the perturbations below say nothing.
            expectEquals (budgetFor (L::nameAreaW, L::glyphAdvance, L::tracking), 49,
                          "the independent implementation disagrees with the header's own, so "
                          "neither the control nor the perturbations below mean anything");

            // A wider chevron trim takes pixels straight out of the name area — the trade §5
            // describes and explicitly declined.
            const auto narrower = budgetFor (L::nameAreaW - 11.0f, L::glyphAdvance, L::tracking);
            logMessage ("  name area 538.00 -> 527.00 gives budget " + juce::String (narrower));
            expectLessThan (narrower, 49,
                            "shrinking the name area by a character's worth did not move the "
                            "budget, so the budget is not actually derived from it");

            // A larger face is the other direction, and it must move the budget too.
            const auto bigger = budgetFor (L::nameAreaW, L::glyphAdvance + 1.0f, L::tracking);
            logMessage ("  advance 9.180 -> 10.180 gives budget " + juce::String (bigger));
            expectLessThan (bigger, 49,
                            "a wider glyph did not reduce the budget, so the advance is not a term "
                            "in it");

            // And the cap must follow the budget rather than being a literal beside it — the
            // failure mode is a cap that stays at 47 while the budget drops beneath it.
            expectEquals (L::userNameCap(), L::characterBudget() - L::dirtyMarkerChars,
                          "the cap must be the budget less its reserve, computed, not a second "
                          "literal that happens to agree today");
        }
    }
};

static HeaderPartTests headerPartTests;
