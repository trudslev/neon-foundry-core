#include <nf/UserProgramDirectory.h>

#include <juce_core/juce_core.h>

/**
    The path is asserted by SHAPE and by PLATFORM, not against a literal string.

    A test that hard-codes `/Users/x/Library/Application Support/...` passes on one machine and is
    noise everywhere else. What actually needs guarding is the thing that has gone wrong twice: the
    macOS segment, and the two segments' order.
*/
class UserProgramDirectoryTests final : public juce::UnitTest
{
public:
    UserProgramDirectoryTests() : juce::UnitTest ("nf::userProgramDirectory", "core") {}

    void runTest() override
    {
        const juce::String company ("Neon Foundry");
        const juce::String product ("TestCasting");

        beginTest ("The last three segments are company / product / Programs");
        {
            const auto dir = nf::userProgramDirectory (company, product);

            expectEquals (dir.getFileName(), juce::String ("Programs"));
            expectEquals (dir.getParentDirectory().getFileName(), product);
            expectEquals (dir.getParentDirectory().getParentDirectory().getFileName(), company);
        }

        beginTest ("It sits under the platform's application-data directory");
        {
            const auto dir = nf::userProgramDirectory (company, product);
            const auto appData = juce::File::getSpecialLocation (
                                     juce::File::userApplicationDataDirectory);

            expect (dir.isAChildOf (appData),
                    "resolved to " + dir.getFullPathName() + ", which is not under "
                        + appData.getFullPathName());
        }

       #if JUCE_MAC
        beginTest ("macOS adds Application Support, and adds it exactly once");
        {
            // The regression this exists for: the segment was omitted for a day and every casting
            // wrote to ~/Library/<Company>/ instead. The "exactly once" half guards the opposite
            // mistake - a caller that appends it again on top of core's.
            const auto dir = nf::userProgramDirectory (company, product);
            const auto path = dir.getFullPathName();

            expect (path.contains ("/Application Support/"),
                    "no Application Support segment in " + path);

            expectEquals (juce::StringArray::fromTokens (path, "/", "")
                              .strings.size()
                              - juce::StringArray::fromTokens (path.replace ("Application Support", ""),
                                                               "/", "").strings.size(),
                          0, "structure changed unexpectedly");

            int occurrences = 0;
            for (int i = 0; (i = path.indexOf (i, "Application Support")) >= 0; ++i)
                ++occurrences;

            expectEquals (occurrences, 1, "Application Support appears " + juce::String (occurrences)
                                              + " times in " + path);
        }
       #else
        beginTest ("Only macOS adds Application Support");
        {
            expect (! nf::userProgramDirectory (company, product)
                          .getFullPathName().contains ("Application Support"));
        }
       #endif

        beginTest ("An override wins outright");
        {
            // Deliberately a directory that does not exist: a test must be able to name a location
            // without creating it, and comparing against File() rather than calling exists() is
            // what allows that.
            const auto fake = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("nf-core-test-does-not-exist");

            expect (! fake.exists());
            expectEquals (nf::userProgramDirectory (company, product, fake).getFullPathName(),
                          fake.getFullPathName());
        }

        beginTest ("A default-constructed override means no override");
        {
            expectEquals (nf::userProgramDirectory (company, product, juce::File()).getFullPathName(),
                          nf::userProgramDirectory (company, product).getFullPathName());
        }

        beginTest ("Company and product are the caller's, not core's");
        {
            // The guard against core ever growing a default identity: two different callers must
            // land in two different places.
            const auto a = nf::userProgramDirectory ("CompanyA", "ProductA");
            const auto b = nf::userProgramDirectory ("CompanyB", "ProductB");

            expect (a.getFullPathName() != b.getFullPathName());
        }
    }
};

static UserProgramDirectoryTests userProgramDirectoryTests;

int main (int, char**)
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);

    // **This category, not runAllTests().** JUCE_UNIT_TESTS=1 registers JUCE's own several-hundred
    // internal tests into the same global list, and running them here proves nothing about core
    // while burying its six results in the scroll-back. The category string is the one in each
    // UnitTest's constructor.
    runner.runTestsInCategory ("core");

    jassert (runner.getNumResults() > 0);   // a typo'd category would otherwise "pass" silently

    for (int i = 0; i < runner.getNumResults(); ++i)
        if (runner.getResult (i)->failures > 0)
            return 1;

    return 0;
}
