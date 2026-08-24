#include <nf/AboutPart.h>
#include <nf/HeaderPart.h>

#include <juce_gui_basics/juce_gui_basics.h>

/**
    `ABOUT-PART.md` §4's placement law, and the one exception in it.

    **The law is FRAME-local and five castings' frames start at the window's left edge**, so five of
    six would pass every assertion here whatever `frameX` did with them. Fifth Member's 52 px rack
    ears are the only thing that separates a frame-local law from a canvas-local one — which makes
    it the known case, and it is asserted FIRST for that reason rather than left to fall wherever
    the other five put it.

    §4's own table says *"Canvas width is 1340 in every casting"*. That is true of the frame and
    false of one window, and §0 of the same document states the exception outright. A part built
    from the table row alone lands Fifth Member's box 52 px left of its panel, on top of the left
    rack ear.
*/
class AboutGeometryTests final : public juce::UnitTest
{
public:
    AboutGeometryTests() : juce::UnitTest ("About geometry", "core") {}

    void runTest() override
    {
        beginTest ("the frame origin moves the box, the tab and the wordmark zone together");

        // The known case first. §0: "Fifth Member is also the one x exception - 282, not 230".
        const int ears = 52;
        expect (nf::AboutGeometry::boxFor (1012, ears).getX() == 282,
                "a 52 px frame origin must put the box at 282, not "
                    + juce::String (nf::AboutGeometry::boxFor (1012, ears).getX()));
        expect (nf::AboutGeometry::boxFor (1012, ears).getY() == 236,
                "§11's y: (1012 - 540) / 2");

        // And the five that pass 0 land on §4's own figure, so the exception cannot have been
        // bought by moving the default.
        expect (nf::AboutGeometry::boxFor (648, 0).getX() == 230, "reflect-84's x");
        expect (nf::AboutGeometry::boxFor (812, 0).getX() == 230, "chorus-60's x");

        // §4's per-casting y, all six, from the law rather than from a table of six numbers.
        expect (nf::AboutGeometry::boxFor (648,  0).getY() ==  54, "reflect-84");
        expect (nf::AboutGeometry::boxFor (660,  0).getY() ==  60, "elmer");
        expect (nf::AboutGeometry::boxFor (700,  0).getY() ==  80, "gatecrasher");
        expect (nf::AboutGeometry::boxFor (790,  0).getY() == 125, "taperot");
        expect (nf::AboutGeometry::boxFor (812,  0).getY() == 136, "chorus-60");
        expect (nf::AboutGeometry::boxFor (1012, ears).getY() == 236, "fifth-member");

        /*  §2's tab. Its right edge is the meter row's, which is inside the header block — so it is
            frame-local for the same reason the box is, and a canvas-local reading would hang it
            52 px off Fifth Member's right edge into the rack ear. */
        expect (nf::AboutGeometry::tabFor (812, 100, 0).getRight() == 1302, "five castings");
        expect (nf::AboutGeometry::tabFor (1012, 100, ears).getRight() == 1302 + ears,
                "fifth-member's tab must follow its frame");
        expect (nf::AboutGeometry::tabFor (812, 100, 0).getWidth() == 100 + 2 * nf::AboutGeometry::tabPadX,
                "width is shrink-to-fit: the run plus §2's two 10 px pads");

        // §2a's hit box is HeaderGeometry's, moved by the frame — one rectangle for six panels.
        expect (nf::AboutWordmarkHit::zone (0) == nf::HeaderGeometry::nameplate(),
                "with no ears the zone IS the shared nameplate");
        expect (nf::AboutWordmarkHit::zone (ears).getX()
                    == nf::HeaderGeometry::nameplate().getX() + ears,
                "fifth-member's wordmark sits a frame origin to the right");
        expect (nf::AboutWordmarkHit::zone (ears).getWidth() == 303
                    && nf::AboutWordmarkHit::zone (ears).getHeight() == 84,
                "§2a states 303 x 84 whatever the frame does");

        /*  The failure this guards against, stated as an assertion rather than as a comment: a
            canvas-local law would read the WINDOW width. 1444 - 880 over 2 is 282 as well, by
            arithmetic coincidence — so a canvas-local implementation would agree on Fifth Member's
            box and disagree on its tab and its wordmark. Naming it here is what stops the
            coincidence being read as corroboration. */
        expect ((1444 - nf::AboutGeometry::boxW) / 2 == 282,
                "the coincidence this test exists to distinguish");
        expect (nf::AboutGeometry::tabFor (1012, 100, ears).getRight() != nf::AboutGeometry::tabRight,
                "the tab is where a canvas-local reading and a frame-local one differ");

        beginTest ("§4's internal figures still close the box");

        expect (nf::AboutGeometry::minCanvasH == 620, "540 + 40 top and bottom");
        expect (648 >= nf::AboutGeometry::minCanvasH, "the shortest casting clears it");
        expect (nf::AboutBox::repositoryFits ("github.com/trudslev/fifth-member-audio-plugin"),
                "§4's 45-character budget, measured on the suite's longest slug");
        expect (! nf::AboutBox::repositoryFits (juce::String::repeatedString ("x", 46)),
                "the budget must be able to fail");
    }
};

static AboutGeometryTests aboutGeometryTests;
