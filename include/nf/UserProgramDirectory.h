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

} // namespace nf
