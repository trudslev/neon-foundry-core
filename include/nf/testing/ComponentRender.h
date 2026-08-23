#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*  ============================================================================================
    RENDERING A COMPONENT OFF SCREEN, so a panel can be read without a window.

    **Why this exists rather than another `capture_panel.py` run.** Driving the standalone is the
    right instrument for *interaction* and the wrong one for *drawing*, and on this suite it fails
    three ways that have nothing to do with the code under test:

      * **TCC re-prompts on every rebuild.** A locally-built bundle is ad-hoc signed against the
        binary's own hash, so each rebuild is a new app to the permission system and the microphone
        dialog returns. Until it is answered there is no window, which reads as a launch failure.
      * **A panel can be taller than the screen.** Fifth Member needs 1099 rows against a 1080
        display and its foot is exactly what gets clipped — the part of the panel the About tab
        sits in.
      * **The capture's resolution is the display's**, so the same command returns 2x on one
        machine and 1x on another and every sub-pixel figure changes instrument underneath you.

    This renders the real component through the real paint path into an `Image`, at a scale the
    caller states. It cannot see anything the window server does — focus, z-order against other
    windows, the cursor — so it does not replace a click test. It replaces a *photograph*.
    ============================================================================================ */

namespace nf::testing
{

/** Renders `c` at its current size, times `scale`, and returns the image. */
juce::Image renderComponent (juce::Component& c, float scale = 1.0f);

/** Renders and writes a PNG. Returns false if the file could not be written, which a test should
    treat as a failure rather than a skip — a probe that silently writes nothing looks clean. */
bool writeComponentPng (juce::Component& c, const juce::File& destination, float scale = 1.0f);

} // namespace nf::testing
