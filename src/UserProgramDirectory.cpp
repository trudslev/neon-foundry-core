#include "nf/UserProgramDirectory.h"

namespace nf
{

juce::File userProgramDirectory (juce::StringRef company, juce::StringRef product)
{
    // **Both segments are required.** An empty one would silently collapse the path by a level and
    // point Programs at the company folder, or at the application-data root itself — which is
    // exactly the class of failure the caller's own #error guard exists to prevent, so failing
    // here rather than returning a plausible wrong directory keeps the two consistent.
    jassert (company.isNotEmpty() && product.isNotEmpty());

    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

   #if JUCE_MAC
    // See the header: JUCE gives ~/Library here, not ~/Library/Application Support. This one
    // segment is the whole macOS special case, and it is not a shared literal path.
    dir = dir.getChildFile ("Application Support");
   #endif

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
