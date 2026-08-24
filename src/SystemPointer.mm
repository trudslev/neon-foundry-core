/*  §2b's pointer-size detection, macOS half.

    Objective-C++ because the only public route to this value is AppKit's `NSCursor`. It is a
    separate translation unit rather than making `AboutPart.cpp` ObjC++ on Apple, so the platform
    surface is one small file that a reader can see the whole of.

    **Why `currentSystemCursor` and not the preference key.** `CFPreferencesCopyAppValue
    ("mouseDriverCursorSize", "com.apple.universalaccess")` looks like the exact signal and is
    simply wrong: measured with the pointer visibly enlarged it returns **1.000**, the value meaning
    default. It also cannot be read under App Sandbox, where an AUv3 runs — but the sandbox was the
    lesser objection, and it is worth recording which one the measurement actually killed.

    `currentSystemCursor` is public AppKit, works sandboxed, and was checked for the failure it
    invites: it does NOT report the shape under the pointer. Sampled at the menu bar, the desktop
    centre and two corners it reads the same size at every position. It costs a threshold in
    exchange — see `enlargedPointerThresholdPoints`, which carries its two measured readings.
*/
#import <Cocoa/Cocoa.h>

#include "nf/AboutPart.h"

#include <algorithm>

namespace nf
{

bool systemPointerIsEnlarged()
{
    @autoreleasepool
    {
        NSCursor* current = [NSCursor currentSystemCursor];

        if (current == nil)
            return false;                       // no reading is not a reason to downgrade

        NSImage* image = [current image];

        if (image == nil)
            return false;

        const NSSize size = [image size];

        /*  **The LARGER dimension, and this is measured rather than tidy.** The enlarged arrow is
            not the default one scaled: 23 x 22 becomes 28 x 40, which is 1.22x wide and 1.82x tall.
            **Width alone would have missed it** — 28 is under the 32-point threshold. */
        const double largest = std::max ((double) size.width, (double) size.height);

        return largest > enlargedPointerThresholdPoints;
    }
}

} // namespace nf
