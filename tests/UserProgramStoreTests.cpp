#include <nf/UserProgramStore.h>

#include <juce_core/juce_core.h>

namespace
{
    /** A temporary directory that removes itself. Every test here writes real files — the store's
        whole job is the filesystem, so a mocked one would test nothing — and none of them may write
        into the user's own Programs folder. That is not hypothetical: a cleanup glob in this suite
        once destroyed a Program somebody had just saved. */
    struct TempDir
    {
        TempDir()
            : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("nf-store-test-" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30))))
        {
            dir.createDirectory();
        }

        ~TempDir() { dir.deleteRecursively(); }

        juce::File dir;
    };

    juce::XmlElement makeXml (const juce::String& marker)
    {
        juce::XmlElement xml ("TestProgram");
        xml.setAttribute ("marker", marker);
        return xml;
    }
}

class UserProgramStoreTests final : public juce::UnitTest
{
public:
    UserProgramStoreTests() : juce::UnitTest ("nf::UserProgramStore", "core") {}

    void runTest() override
    {
        beginTest ("An empty name becomes TAKE n, counting from the bank size");
        {
            TempDir tmp;
            nf::UserProgramStore store { tmp.dir, ".testprogram", 20 };
            store.refresh();

            expectEquals (store.resolveName (""), juce::String ("TAKE 1"));
            expectEquals (store.resolveName ("   "), juce::String ("TAKE 1"));

            store.save ("", makeXml ("a"));
            expectEquals (store.resolveName (""), juce::String ("TAKE 2"));
        }

        beginTest ("Names are trimmed, upper-cased and capped");
        {
            TempDir tmp;
            nf::UserProgramStore store { tmp.dir, ".testprogram", 8 };
            store.refresh();

            expectEquals (store.resolveName ("  quiet room  "), juce::String ("QUIET RO"));
            expectEquals (store.resolveName ("short"), juce::String ("SHORT"));
        }

        beginTest ("Save never overwrites — a collision takes the next sibling");
        {
            // The guarantee two castings broke by writing straight to the composed path: reusing a
            // name silently replaced that Program's contents.
            TempDir tmp;
            nf::UserProgramStore store { tmp.dir, ".testprogram", 20 };
            store.refresh();

            const auto first = store.save ("ROOM", makeXml ("first"));
            const auto second = store.save ("ROOM", makeXml ("second"));

            expect (first != juce::File());
            expect (second != juce::File());
            expect (first != second, "the second save overwrote the first");
            expectEquals (store.getFiles().size(), 2);

            // And the first file still holds what it held.
            const auto reloaded = juce::XmlDocument::parse (first);
            expect (reloaded != nullptr);
            expectEquals (reloaded->getStringAttribute ("marker"), juce::String ("first"));
        }

        beginTest ("The returned file is what to read the stem from, not the requested name");
        {
            TempDir tmp;
            nf::UserProgramStore store { tmp.dir, ".testprogram", 20 };
            store.refresh();

            store.save ("ROOM", makeXml ("first"));
            const auto second = store.save ("ROOM", makeXml ("second"));

            // Taking the stem from the REQUEST here would point the panel at the first file while
            // the values came from the second.
            expect (second.getFileNameWithoutExtension() != "ROOM");
            expect (store.fileFor (second.getFileNameWithoutExtension()) == second);
        }

        beginTest ("The list sorts by stem, alphabetically, ignoring case");
        {
            TempDir tmp;
            nf::UserProgramStore store { tmp.dir, ".testprogram", 20 };
            store.refresh();

            for (auto n : { "ZEBRA", "alpha", "Mid" })
                store.save (n, makeXml (n));

            const auto& files = store.getFiles();
            expectEquals (files.size(), 3);
            expectEquals (files[0].getFileNameWithoutExtension(), juce::String ("ALPHA"));
            expectEquals (files[1].getFileNameWithoutExtension(), juce::String ("MID"));
            expectEquals (files[2].getFileNameWithoutExtension(), juce::String ("ZEBRA"));
        }

        beginTest ("Sorting is by STEM, so a space does not beat the dot");
        {
            // "AB C.ext" against "AB.ext": on the full filename the space (0x20) precedes the dot
            // (0x2E), so "AB C" would sort first. On the stem, "AB" correctly precedes "AB C".
            TempDir tmp;
            nf::UserProgramStore store { tmp.dir, ".testprogram", 20 };
            store.refresh();

            store.save ("AB C", makeXml ("1"));
            store.save ("AB", makeXml ("2"));

            expectEquals (store.getFiles()[0].getFileNameWithoutExtension(), juce::String ("AB"));
            expectEquals (store.getFiles()[1].getFileNameWithoutExtension(), juce::String ("AB C"));
        }

        beginTest ("Delete removes the file and refreshes the list");
        {
            TempDir tmp;
            nf::UserProgramStore store { tmp.dir, ".testprogram", 20 };
            store.refresh();

            store.save ("GONE", makeXml ("x"));
            expectEquals (store.getFiles().size(), 1);

            expect (store.remove ("GONE"));
            expectEquals (store.getFiles().size(), 0);

            expect (! store.remove ("GONE"), "removing a stem that is not there reported success");
        }

        beginTest ("The directory is created on first save, not on construction");
        {
            // A plugin that has never saved should not leave an empty folder in the user's
            // application-data directory.
            TempDir tmp;
            const auto nested = tmp.dir.getChildFile ("not-yet");

            nf::UserProgramStore store { nested, ".testprogram", 20 };
            store.refresh();

            expect (! nested.isDirectory());
            expectEquals (store.getFiles().size(), 0);

            store.save ("FIRST", makeXml ("x"));
            expect (nested.isDirectory());
        }

        beginTest ("Only files with this casting's extension are listed");
        {
            TempDir tmp;
            nf::UserProgramStore store { tmp.dir, ".testprogram", 20 };

            tmp.dir.getChildFile ("stray.txt").replaceWithText ("not a program");
            tmp.dir.getChildFile ("other.otherprogram").replaceWithText ("another casting's");
            store.refresh();

            expectEquals (store.getFiles().size(), 0);

            store.save ("MINE", makeXml ("x"));
            expectEquals (store.getFiles().size(), 1);
        }
    }
};

static UserProgramStoreTests userProgramStoreTests;
