#include <nf/ProgramId.h>

#include <juce_core/juce_core.h>

class ProgramIdTests final : public juce::UnitTest
{
public:
    ProgramIdTests() : juce::UnitTest ("nf::ProgramId", "core") {}

    void runTest() override
    {
        beginTest ("Identity is bank + slug; the display name is not part of it");
        {
            const nf::ProgramId a { nf::ProgramBank::factory, "rain-all-day", "RAIN ALL DAY" };
            const nf::ProgramId renamed { nf::ProgramBank::factory, "rain-all-day", "RAIN ALL NIGHT" };

            // The whole reason the slug exists: a Factory name is revisable presentation, so a
            // Program has to keep equalling itself when someone corrects a typo in the bank.
            expect (a == renamed);

            const nf::ProgramId otherSlug { nf::ProgramBank::factory, "under-pressure", "RAIN ALL DAY" };
            expect (a != otherSlug);
        }

        beginTest ("The same slug in two banks is two Programs");
        {
            // A User Program is addressed by filename and a Factory one by slug, so the two
            // namespaces can legitimately collide. The bank is what keeps them apart.
            const nf::ProgramId factory { nf::ProgramBank::factory, "shared", "SHARED" };
            const nf::ProgramId user    { nf::ProgramBank::user,    "shared", "SHARED" };

            expect (factory != user);
        }

        beginTest ("Factory Programs are numbered from 01, zero-padded");
        {
            const nf::ProgramId id { nf::ProgramBank::factory, "first", "WARM CASSETTE" };

            expectEquals (nf::programDisplayLabel (id, 0),  juce::String ("01 WARM CASSETTE"));
            expectEquals (nf::programDisplayLabel (id, 8),  juce::String ("09 WARM CASSETTE"));
            expectEquals (nf::programDisplayLabel (id, 9),  juce::String ("10 WARM CASSETTE"));
            expectEquals (nf::programDisplayLabel (id, 99), juce::String ("100 WARM CASSETTE"));
        }

        beginTest ("User Programs, INIT and unresolved carry no number");
        {
            // User Programs sort alphabetically, so a number would change whenever another was
            // saved. INIT is outside both banks. An unresolved id has no position to number.
            for (auto bank : { nf::ProgramBank::user, nf::ProgramBank::init, nf::ProgramBank::unresolved })
            {
                const nf::ProgramId id { bank, "x", "TAKE 3" };
                expectEquals (nf::programDisplayLabel (id, 4), juce::String ("TAKE 3"));
            }
        }

        beginTest ("A Factory Program with no resolved position falls back to the bare name");
        {
            // Defensive rather than expected: a negative position means the bank did not contain
            // the slug, and printing "00 " there would invent a place in the running order.
            const nf::ProgramId id { nf::ProgramBank::factory, "missing", "GONE" };
            expectEquals (nf::programDisplayLabel (id, -1), juce::String ("GONE"));
        }

        beginTest ("The bank tag reads NAME while typing, whatever the Program is");
        {
            for (auto bank : { nf::ProgramBank::init, nf::ProgramBank::factory,
                               nf::ProgramBank::user, nf::ProgramBank::unresolved })
                expectEquals (nf::programBankTag ({ bank, "x", "X" }, true), juce::String ("NAME"));
        }

        beginTest ("Otherwise FACT, USER, or an em-dash for neither bank");
        {
            const auto emDash = juce::String::charToString ((juce::juce_wchar) 0x2014);

            expectEquals (nf::programBankTag ({ nf::ProgramBank::factory, "x", "X" }, false),
                          juce::String ("FACT"));
            expectEquals (nf::programBankTag ({ nf::ProgramBank::user, "x", "X" }, false),
                          juce::String ("USER"));
            expectEquals (nf::programBankTag ({ nf::ProgramBank::init, "x", "X" }, false), emDash);
            expectEquals (nf::programBankTag ({ nf::ProgramBank::unresolved, "x", "X" }, false), emDash);

            // From its codepoint, not a literal: juce::String's const char* constructor decodes as
            // Latin-1, so a UTF-8 em-dash literal renders as three stray glyphs on the panel.
            expectEquals (emDash.length(), 1);
        }

        beginTest ("A default-constructed ProgramId is a Factory Program with no slug");
        {
            const nf::ProgramId fresh;
            expect (fresh.bank == nf::ProgramBank::factory);
            expect (fresh.id.isEmpty());
        }
    }
};

static ProgramIdTests programIdTests;
