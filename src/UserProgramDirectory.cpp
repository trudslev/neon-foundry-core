#include "nf/UserProgramDirectory.h"

namespace nf
{

namespace
{
    /** The process-wide redirect. Function-local so there is no static initialisation order to
        reason about, and `SpinLock`-guarded because `userProgramDirectory` is reachable from a
        ProgramManager on the message thread while a test sets this up on the main one.

        The lock is not really about contention — the value is written once before any test runs and
        once after they all finish — it is about not writing a data race into the one file whose job
        is to stop tests touching the user's disk. Core already carries the same reasoning in
        ParameterSnapshot, where an unguarded shared value segfaulted three runs out of three. */
    struct OverrideState
    {
        juce::SpinLock lock;
        juce::File root;
    };

    OverrideState& overrideState()
    {
        static OverrideState state;
        return state;
    }
}

ScopedUserProgramDirectoryOverride::ScopedUserProgramDirectoryOverride (const juce::File& root)
{
    // Refusing an empty root rather than treating it as "no override": a caller passing one has a
    // path that failed to resolve, and quietly reverting to the real user directory is the exact
    // outcome this class exists to make impossible.
    jassert (root != juce::File());

    const juce::SpinLock::ScopedLockType guard (overrideState().lock);
    previous = overrideState().root;
    overrideState().root = root;
}

ScopedUserProgramDirectoryOverride::~ScopedUserProgramDirectoryOverride()
{
    const juce::SpinLock::ScopedLockType guard (overrideState().lock);
    overrideState().root = previous;
}

juce::File userProgramDirectoryOverrideRoot()
{
    const juce::SpinLock::ScopedLockType guard (overrideState().lock);
    return overrideState().root;
}

juce::File userProgramDirectory (juce::StringRef company, juce::StringRef product)
{
    // **Both segments are required.** An empty one would silently collapse the path by a level and
    // point Programs at the company folder, or at the application-data root itself — which is
    // exactly the class of failure the caller's own #error guard exists to prevent, so failing
    // here rather than returning a plausible wrong directory keeps the two consistent.
    jassert (company.isNotEmpty() && product.isNotEmpty());

    // **The process-wide redirect is checked BEFORE the real location is ever computed**, so a test
    // process cannot reach the user's Programs by any path through this function. See the header for
    // why this is a guard rather than a comment telling people not to.
    //
    // Company/product/Programs are still appended below, so a redirected directory has the same
    // shape as the shipping one — a test that asserts on the layout is asserting on the real layout.
    auto dir = userProgramDirectoryOverrideRoot();

    if (dir == juce::File())
    {
        dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

       #if JUCE_MAC
        // See the header: JUCE gives ~/Library here, not ~/Library/Application Support. This one
        // segment is the whole macOS special case, and it is not a shared literal path.
        dir = dir.getChildFile ("Application Support");
       #endif
    }

    return dir.getChildFile (company)
              .getChildFile (product)
              .getChildFile ("Programs");
}

juce::File userProgramDirectory (juce::StringRef company,
                                 juce::StringRef product,
                                 const juce::File& overrideDirectory)
{
    // A default-constructed File means "no override", matching how the castings' ProgramManagers
    // already spell it. Compared against File() rather than tested with exists(), so a test can
    // name a directory that has not been created yet.
    return overrideDirectory == juce::File() ? userProgramDirectory (company, product)
                                             : overrideDirectory;
}

} // namespace nf
