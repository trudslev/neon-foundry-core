/*  §2b's pointer-size detection, macOS half.

    Objective-C++ only because `CFPreferences` sits behind CoreFoundation and this keeps core's
    whole platform surface in one small file a reader can see the whole of.

    **WHICH DETECTOR, settled by watching a live slider drag rather than by reasoning.** Both
    candidates were polled at 4 Hz for 90 seconds while the Accessibility pointer size was dragged
    from Normal to Large and back:

    | | Through a 1.0 -> 4.0 -> 1.0 drag |
    |---|---|
    | `mouseDriverCursorSize` | **1.000 -> 2.026 -> 3.433 -> 4.000 -> 3.620 -> ... -> 1.000.** Tracks continuously, live, with no synchronize call and no notification |
    | `NSCursor.currentSystemCursor` | **28 x 40 throughout. It never moved.** Across a fourfold change |

    **So `currentSystemCursor` is not a pointer-size detector at all**, and an earlier version of
    this file was built on it. It reports the cursor currently being SHOWN, so its size is a
    property of the SHAPE — which the same run caught directly: a 0.3 s blip to 18 x 28 before the
    slider was touched, as the pointer crossed something with a different cursor.

    The two readings that looked like evidence for it — 23 x 22 then 28 x 40 — were two shapes, not
    two sizes, both taken at Normal. And the four-position control built to catch exactly that could
    not: menu bar, desktop and two corners all show the arrow, so it varied nothing and confirmed
    what it was already reading.

    **The sandbox caveat is real and currently theoretical.** `CFPreferencesCopyAppValue` cannot
    read another application's domain under App Sandbox, so an AUv3 would always read default and
    keep the custom cursor. No casting builds an AUv3 today — the format lists are AU, VST3 and
    Standalone — so nothing shipping is affected, and this is recorded rather than guarded against.
*/
#import <Foundation/Foundation.h>

#include "nf/AboutPart.h"

namespace nf
{

bool systemPointerIsEnlarged()
{
    @autoreleasepool
    {
        CFPropertyListRef value = CFPreferencesCopyAppValue (CFSTR ("mouseDriverCursorSize"),
                                                             CFSTR ("com.apple.universalaccess"));

        /*  **ABSENT means never changed from default**, which is what a machine that has never
            touched the slider reads. It is not an error and not an unknown. */
        if (value == nullptr)
            return false;

        double multiplier = 1.0;
        const bool read = CFGetTypeID (value) == CFNumberGetTypeID()
                          && CFNumberGetValue ((CFNumberRef) value, kCFNumberDoubleType, &multiplier);
        CFRelease (value);

        if (! read)
            return false;

        return multiplier > enlargedPointerMultiplier;
    }
}

} // namespace nf
