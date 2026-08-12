#include <nf/MenuMetrics.h>

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
    /** A concrete subclass whose text measurement is a fixed width per character.

        Deliberately not a real font: the thing under test is what the base class does with a
        measurement, and a font would make the expected numbers depend on which face a machine
        happens to resolve.
    */
    class StubMenu final : public nf::MenuMetricsLookAndFeel
    {
    public:
        explicit StubMenu (nf::MenuMetrics m) : nf::MenuMetricsLookAndFeel (m) {}

        float perCharacter = 10.0f;
        float headerPerCharacter = 0.0f;   // 0 = fall through to the row measurement

    protected:
        float measureMenuItemText (const juce::String& text) override
        {
            return perCharacter * (float) text.length();
        }

        float measureSectionHeaderText (const juce::String& text) override
        {
            return headerPerCharacter > 0.0f ? headerPerCharacter * (float) text.length()
                                             : nf::MenuMetricsLookAndFeel::measureSectionHeaderText (text);
        }
    };
}

class MenuMetricsTests final : public juce::UnitTest
{
public:
    MenuMetricsTests() : juce::UnitTest ("nf::MenuMetrics", "core") {}

    void runTest() override
    {
        beginTest ("A row is the stated height and never the platform's standard");
        {
            // **The rule with a documented failure behind it.** Four castings wrote
            // jmax(rowHeight, standardMenuItemHeight), which grows the row the moment a caller sets
            // a standard height - and a bank then stops fitting the panel. None sets one today, so
            // both spellings agree in the suite as it stands; this asserts the pinned behaviour so
            // that stays true if anyone ever does.
            nf::MenuMetrics m;
            m.rowHeight = 24;
            StubMenu lf { m };

            int w = 0, h = 0;

            lf.getIdealPopupMenuItemSize ("ROOM", false, 0, w, h);
            expectEquals (h, 24);

            lf.getIdealPopupMenuItemSize ("ROOM", false, 40, w, h);
            expectEquals (h, 24, "the row grew to the platform's standard item height");

            lf.getIdealPopupMenuItemSize ("ROOM", false, 1, w, h);
            expectEquals (h, 24);
        }

        beginTest ("A row is wide enough for its own text, the tick column and the padding");
        {
            nf::MenuMetrics m;
            m.leadingColumn = 22;
            m.horizontalPadding = 26;
            StubMenu lf { m };
            lf.perCharacter = 10.0f;

            int w = 0, h = 0;
            lf.getIdealPopupMenuItemSize ("ABCD", false, 0, w, h);   // 4 chars -> 40
            expectEquals (w, 40 + 22 + 26);
        }

        beginTest ("A separator gets its own height and a width that cannot dominate the list");
        {
            nf::MenuMetrics m;
            m.separatorHeight = 9;
            m.minimumSeparatorWidth = 50;
            StubMenu lf { m };
            lf.perCharacter = 1000.0f;   // absurd, to prove the text is not measured at all

            int w = 0, h = 0;
            lf.getIdealPopupMenuItemSize ("a very long separator label", true, 0, w, h);

            expectEquals (h, 9);
            expectEquals (w, 50);
        }

        beginTest ("A section caption is the STATED height, not JUCE's row-and-a-half");
        {
            // JUCE's LookAndFeel_V2 ends getIdealPopupMenuSectionHeaderSizeWithOptions with
            // `idealHeight += idealHeight / 2`, so a 24px row gives a 36px caption. Four castings
            // took that by omission; Elmer overrode it with a measured 19 against 22px rows and
            // recorded that the default "pushed everything below FACTORY 14px down the list".
            nf::MenuMetrics m;
            m.rowHeight = 22;
            m.sectionHeaderHeight = 19;
            StubMenu lf { m };

            int w = 0, h = 0;
            const juce::PopupMenu::Options options;

            lf.getIdealPopupMenuSectionHeaderSizeWithOptions ("FACTORY", 0, w, h, options);
            expectEquals (h, 19, "the caption fell back to JUCE's row-and-a-half");

            // And it is independent of the standard item height too.
            lf.getIdealPopupMenuSectionHeaderSizeWithOptions ("FACTORY", 40, w, h, options);
            expectEquals (h, 19);
        }

        beginTest ("A caption may be measured in its own face");
        {
            nf::MenuMetrics m;
            m.leadingColumn = 0;
            m.horizontalPadding = 24;
            StubMenu lf { m };
            lf.perCharacter = 10.0f;
            lf.headerPerCharacter = 6.0f;    // a smaller caption face

            int w = 0, h = 0;
            const juce::PopupMenu::Options options;

            lf.getIdealPopupMenuSectionHeaderSizeWithOptions ("USER", 0, w, h, options);
            expectEquals (w, 24 + 24);       // 4 chars at 6 = 24, plus padding
        }

        beginTest ("A caption defaults to the row's measurement when it shares the row's face");
        {
            nf::MenuMetrics m;
            m.leadingColumn = 0;
            m.horizontalPadding = 0;
            StubMenu lf { m };
            lf.perCharacter = 10.0f;         // headerPerCharacter left at 0

            int w = 0, h = 0;
            const juce::PopupMenu::Options options;

            lf.getIdealPopupMenuSectionHeaderSizeWithOptions ("USER", 0, w, h, options);
            expectEquals (w, 40);
        }

        beginTest ("The border size is the stated one");
        {
            nf::MenuMetrics m;
            m.borderSize = 4;
            StubMenu lf { m };

            expectEquals (lf.getPopupMenuBorderSize(), 4);
            expectEquals (lf.getMenuMetrics().borderSize, 4);
        }
    }
};

static MenuMetricsTests menuMetricsTests;
