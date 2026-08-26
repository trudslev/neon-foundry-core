#include "nf/testing/ExpectedFailure.h"

#include <algorithm>
#include <set>

namespace nf::testing
{

namespace
{
    // Process-wide, because the audit happens in TestMain after every suite has run and there is no
    // other object outliving all of them. Single-threaded by construction: JUCE's UnitTestRunner
    // runs suites in sequence.
    std::set<juce::String>& reached()
    {
        static std::set<juce::String> s;
        return s;
    }
}

void expectedFailure (juce::UnitTest& test,
                      bool nowPasses,
                      const char* id,
                      const juce::String& detail)
{
    reached().insert (juce::String (id));

    if (nowPasses)
    {
        // **The ratchet.** A finding that has resolved must be removed from the declaration
        // deliberately. Left alone it becomes a marker asserting nothing, which is the state this
        // whole mechanism exists to avoid reproducing.
        test.expect (false,
                     juce::String ("EXPECTED FAILURE '") + id + "' now PASSES. The finding it "
                     "marks has resolved — remove the marker and its declaration, and let the arm "
                     "assert normally. Leaving it makes a check that cannot fail.\n  " + detail);
        return;
    }

    test.logMessage (juce::String ("  EXPECTED FAILURE  ") + id);
    test.logMessage (juce::String ("                    ") + detail);
}

std::vector<juce::String> expectedFailuresExecuted()
{
    return { reached().begin(), reached().end() };
}

std::vector<juce::String> expectedFailuresNotExecuted (const std::vector<ExpectedFailure>& declared)
{
    std::vector<juce::String> missing;

    for (const auto& d : declared)
        if (reached().count (juce::String (d.id)) == 0)
            missing.push_back (juce::String (d.id));

    return missing;
}

}  // namespace nf::testing
