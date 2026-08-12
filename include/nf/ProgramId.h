#pragma once

#include <juce_core/juce_core.h>

namespace nf
{

/** Which bank a Program belongs to.

    **INIT is its own bank, not a magic index.** It is a single unnumbered entry above the Factory
    group, giving the effect present and audible in its plainest form — not a reset to coded
    defaults and not a bypass. It sits outside both banks because numbering it would push the first
    authored Program to 02 and imply a running order it is not part of.

    `unresolved` is a session naming a Program the bank no longer has. The VALUES are correct and
    untouched in that case; only the name is unknown, so a panel says so rather than pretending.
*/
enum class ProgramBank
{
    init,
    factory,
    user,
    unresolved
};

/** What a Program is addressed by, everywhere except the four JUCE overrides that require indices.

    **Identity is a permanent slug for Factory Programs and the filename for User Programs.** A
    Factory slug is fixed at creation and does not change when the display name is revised — which
    is the point: the name is presentation, the slug is identity.

    `displayName` is carried for presentation only and is deliberately **not** part of `operator==`.
    Comparing on it would make a Program stop equalling itself the moment someone corrected a
    typo in the bank.

    Indices survive in exactly four places per casting — `getNumPrograms`, `getCurrentProgram`,
    `setCurrentProgram`, `getProgramName` — because JUCE's AudioProcessor requires them there. They
    sit together under one comment in each casting, because that comment is the only place the
    "ordering must stay stable" constraint lives.
*/
struct ProgramId
{
    ProgramBank bank = ProgramBank::factory;
    juce::String id;
    juce::String displayName;

    bool operator== (const ProgramId& other) const noexcept
    {
        return bank == other.bank && id == other.id;   // displayName is not identity
    }

    bool operator!= (const ProgramId& other) const noexcept { return ! operator== (other); }
};

/** The label a panel prints: `NN ` and the name for a Factory Program, the bare name otherwise.

    **The number is presentation and is computed here, never stored.** It is derived from the
    Factory position at the moment of painting, so nothing is ever looked up by it. User Programs
    carry none at all — they sort alphabetically, so any number would change whenever another was
    saved — and INIT carries none because it is in neither bank.

    @param id               the Program to label
    @param factoryPosition  its 0-based position in the Factory bank, or a negative value if it is
                            not a Factory Program. The caller resolves this because the Factory
                            bank is the casting's own; core never holds one.
*/
inline juce::String programDisplayLabel (const ProgramId& id, int factoryPosition)
{
    if (id.bank == ProgramBank::factory && factoryPosition >= 0)
        return juce::String (factoryPosition + 1).paddedLeft ('0', 2) + " " + id.displayName;

    return id.displayName;
}

/** What the LCD's bank cell reads.

    `NAME` while a name is being typed — **not** `USER`. The Program is not in the user bank until
    the name is committed, and if the user cancels it never will be, so `USER` there names a thing
    that does not exist yet. Elmer had this right first and it is the suite standard.

    An em-dash for INIT and for an unresolved identifier: both are in neither bank, and either word
    would be a lie. Returned as U+2014 from its codepoint rather than as a literal, because
    `juce::String`'s `const char*` constructor decodes as Latin-1 and a UTF-8 literal renders as
    three stray glyphs on the panel.
*/
inline juce::String programBankTag (const ProgramId& id, bool isNaming)
{
    if (isNaming)
        return "NAME";

    switch (id.bank)
    {
        case ProgramBank::user:    return "USER";
        case ProgramBank::factory: return "FACT";
        case ProgramBank::init:
        case ProgramBank::unresolved:
        default:                   return juce::String::charToString ((juce::juce_wchar) 0x2014);
    }
}

} // namespace nf
