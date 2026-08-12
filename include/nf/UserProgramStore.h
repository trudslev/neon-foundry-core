#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>

namespace nf
{

/** The User bank on disk: scanning it, naming into it, saving to it, deleting from it.

    Everything here is about **files and names**, not about parameters — what a Program *contains*
    is the casting's own business, so the caller hands over finished XML and gets back the file it
    landed in. That split is what lets Fifth Member's active-path serialisation and the other five
    castings' whole-state serialisation share one store.

    **SAVE always creates a new Program and never overwrites.** There is no overwrite verb and no
    "New Program" action anywhere in the suite: starting fresh is loading any Program, moving
    something, and saving. Two castings used to write straight to the composed path, which meant
    reusing a name silently replaced that Program's contents — the one way their own "never
    overwrites" guarantee could actually be broken. `getNonexistentSibling` closes it.
*/
class UserProgramStore
{
public:
    /** @param directory  where User Programs live — from `nf::userProgramDirectory`
        @param extension  the casting's own file extension, including the dot
        @param nameCap    the casting's own name cap, derived from its LCD's character budget */
    UserProgramStore (juce::File directory, juce::String extension, int nameCap)
        : dir (std::move (directory)), ext (std::move (extension)), cap (nameCap)
    {
        jassert (ext.startsWithChar ('.'));
        jassert (cap > 0);
    }

    /** Rescans the directory. Cheap enough to call after every save and delete, which is what keeps
        the list and the filesystem from disagreeing. */
    void refresh()
    {
        files.clear();

        if (dir.isDirectory())
            for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*" + ext))
                files.add (entry.getFile());

        // **Alphabetical by filename, deliberately not by modification time**: the menu's order has
        // to be the same on every launch, or a player's muscle memory for "third from the bottom"
        // is worthless.
        std::sort (files.begin(), files.end(),
                   [] (const juce::File& a, const juce::File& b)
                   {
                       // The STEM, not getFileName(): with the extension attached "AB C" sorts
                       // before "AB", because a space (0x20) precedes the dot (0x2E).
                       return a.getFileNameWithoutExtension()
                                .compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
                   });
    }

    const juce::Array<juce::File>& getFiles() const noexcept { return files; }
    juce::File getDirectory() const { return dir; }

    /** The file for a User Program's identifier, or `juce::File()` if the bank has no such stem. */
    juce::File fileFor (const juce::String& stem) const
    {
        for (const auto& f : files)
            if (f.getFileNameWithoutExtension() == stem)
                return f;

        return {};
    }

    /** Applies the suite's naming rules to whatever the user typed.

        Three of them, and all three were skipped by at least one casting before this moved:

        - **Trimmed and upper-cased.** TapeRot and Elmer applied case only at the keystroke filter,
          so any programmatic save bypassed it entirely.
        - **Capped.** Same two castings capped only at the keystroke, so the same hole applied — and
          a name longer than the LCD's budget is one the panel cannot show.
        - **`TAKE n` when empty**, where n counts from the current bank size. The suite had six
          different fallbacks — `USER PROGRAM`, `NEW PROGRAM` ×3, `UNTITLED`, `TAKE n` — and this is
          the one that is actually better rather than merely different: consecutive empty saves give
          `TAKE 3`, `TAKE 4` instead of leaning on getNonexistentSibling for `NEW PROGRAM (2)`. A
          player meeting `UNTITLED` on one casting and `TAKE 3` on another is meeting drift, not
          character.
    */
    juce::String resolveName (const juce::String& requested) const
    {
        auto name = requested.trim().toUpperCase();

        if (name.isEmpty())
            name = "TAKE " + juce::String (files.size() + 1);

        if (name.length() > cap)
            name = name.substring (0, cap);

        return name;
    }

    /** Writes `xml` under `requestedName`, creating the directory if needed, and returns the file
        it actually landed in — which is **not** necessarily the name asked for, because a collision
        takes the next free sibling. The caller reads the stem back off the returned file to set the
        current Program; taking it from the requested name instead is how a save silently points the
        panel at the wrong file.

        Returns `juce::File()` if the write failed, so a caller can report rather than assume.
    */
    juce::File save (const juce::String& requestedName, const juce::XmlElement& xml)
    {
        if (! dir.isDirectory())
            dir.createDirectory();

        auto file = dir.getChildFile (juce::File::createLegalFileName (resolveName (requestedName)) + ext);

        if (file.existsAsFile())
            file = file.getNonexistentSibling();

        if (! xml.writeTo (file))
            return {};

        refresh();
        return file;
    }

    /** Deletes by stem. Returns true if a file was removed.

        The caller gates on the BANK before calling — an id from any other bank cannot address a
        file — which is stronger than the index-range check the suite used to use.
    */
    bool remove (const juce::String& stem)
    {
        const auto file = fileFor (stem);

        if (file == juce::File() || ! file.deleteFile())
            return false;

        refresh();
        return true;
    }

    int getNameCap() const noexcept { return cap; }
    juce::String getExtension() const { return ext; }

private:
    juce::File dir;
    juce::String ext;
    int cap;
    juce::Array<juce::File> files;
};

} // namespace nf
