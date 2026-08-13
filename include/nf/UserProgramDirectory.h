#pragma once

#include <juce_core/juce_core.h>

/**
    Where a casting keeps the Programs its user saves.

    This is the first thing extracted into core, and it was chosen for being the least interesting:
    about thirty lines, no appearance, six near-identical copies, and a defect history long enough
    to make the case on its own. The point of Stage 1 is to prove the FetchContent mechanism, not to
    move anything difficult.
*/
namespace nf
{

/** Resolves the directory a casting's user Programs live in, per platform.

    `~/Library/Application Support/<company>/<product>/Programs` on macOS,
    `%APPDATA%\<company>\<product>\Programs` on Windows,
    `~/.config/<company>/<product>/Programs` on Linux.

    **macOS needs the "Application Support" segment added by hand, and only macOS.** JUCE resolves
    `userApplicationDataDirectory` to `~/Library` there — *not* `~/Library/Application Support` —
    while it is `%APPDATA%` on Windows and `~/.config` on Linux, both already the right root.
    JUCE's own `PropertiesFile` appends the segment the same way, guarded the same way.

    That was got wrong once in exactly the plausible direction. The note in five castings claimed
    JUCE resolved the segment for us, and that hard-coding it "would be wrong on two of the three
    platforms" — the first clause false, the second true of a *shared literal path* and no argument
    at all against one platform's extra segment. Sitting next to each other, the true half made the
    false half read as checked, and every casting wrote to `~/Library/<Company>/` for a day. It was
    caught by noticing a panel listing a Program the filesystem did not have where the note said it
    would.

    **`~/Library/Audio/Presets` is not this and never was.** That directory is for the AU preset
    *format* — `.aupreset` files the AU system itself scans, reads and writes. A `.taperotprogram`
    sitting there is discovered by nothing, so the path buys no interoperability while asking
    Apple's folder to hold a format it does not understand. Five castings pointed there and were
    corrected; Elmer had it right first.

    @param company  the company segment, from CMake — never a string literal at the call site
    @param product  the product segment, likewise
*/
juce::File userProgramDirectory (juce::StringRef company, juce::StringRef product);

/** The same, with an override that wins when it is not a default-constructed `juce::File`.

    **The override is why this is testable at all**, and it is the reason Reflect-84 and Fifth
    Member were the two implementations worth extracting from. The other four resolve the real
    user directory unconditionally, so a test either writes into the tester's own Program folder or
    does not run — and writing there has already destroyed a Program someone had just saved, via a
    cleanup glob rather than an exact filename.

    Kept in the shared signature deliberately: a casting that adopts core gets the seam whether or
    not it currently has tests, so adding them later is not also a refactor.
*/
juce::File userProgramDirectory (juce::StringRef company,
                                 juce::StringRef product,
                                 const juce::File& overrideDirectory);

/** Redirects the resolved directory for the whole process, so a test cannot reach real Programs.

    **Why this exists rather than a note telling people not to.** The per-call override above only
    helps a caller that has one. A casting's shipping `AudioProcessor` does not: it constructs its
    ProgramManager with the real path, because that is its job. So the moment a test harness became
    able to construct the real processor — Reflect-84's `EditorWiringTests`, and the same port in
    the other five — every one of those suites gained the ability to reach
    `~/Library/Application Support/<Company>/<Product>/Programs`.

    What stood between that and a test writing there was a comment at the top of one file saying it
    must not. That is a convention, and this project's entire evidential basis is that **a convention
    gets broken silently and a guard does not.** It is also the convention most likely to be broken
    by someone doing exactly the right thing: verifying the Program list needs several saved
    Programs, and building that state is the obvious way to get it. Eight were created by hand for
    exactly that reason during the Reflect-84 list work.

    The stakes are not hypothetical either. A cleanup `rm *.taperotprogram` has already destroyed a
    Program a user had just saved, and there is no undo.

    **With this installed, the real per-OS location is unreachable from `userProgramDirectory`.** An
    explicit per-call override still wins — a test that names its own scratch directory means it, and
    silently redirecting that would break the tests that assert against the path they chose — but the
    *default* branch resolves under `root` instead of under the user's application data. So a suite
    reaches real Programs only by naming them explicitly, which is no longer something anyone can do
    by forgetting.

    Install it in `TestMain.cpp` beside `ScopedJuceInitialiser_GUI`, for the same reason that one
    lives there: it must be in force before the first line of the first test.

    Scoped rather than a bare setter so it cannot be left installed by a test that throws, and so
    nothing outside a test process can be redirected by accident.
*/
class ScopedUserProgramDirectoryOverride
{
public:
    /** @param root  the directory to resolve under. Company/product/Programs are still appended, so
                     the shape a test sees matches the shape the plugin ships. */
    explicit ScopedUserProgramDirectoryOverride (const juce::File& root);
    ~ScopedUserProgramDirectoryOverride();

    ScopedUserProgramDirectoryOverride (const ScopedUserProgramDirectoryOverride&) = delete;
    ScopedUserProgramDirectoryOverride& operator= (const ScopedUserProgramDirectoryOverride&) = delete;

private:
    juce::File previous;
};

/** The process-wide root, or a default-constructed `juce::File` when none is installed.

    For tests and assertions. A caller that branches on this in production has written a redirect it
    did not mean to have.
*/
juce::File userProgramDirectoryOverrideRoot();

} // namespace nf
