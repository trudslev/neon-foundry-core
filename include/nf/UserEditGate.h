#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <functional>
#include <utility>

namespace nf
{

/** The stale-replay guard, and the one event that disarms it.

    **What it guards.** A host restoring a session calls `setStateInformation` and then, separately,
    replays the program index it remembered. That replay arrives after the state is already correct,
    so honouring it re-applies a Program on top of the user's restored edits and silently discards
    them. The guard is armed by the restore and consumed by the next `setCurrentProgram`: a replay
    carrying the position we last reported is ignored exactly once.

    **Why it is a gate and not a `ValueTree::Listener`.** The extraction plan proposed that core
    subscribe to the APVTS itself, so that a casting could not fail to wire it up - which was the
    right diagnosis of the wrong mechanism, and it is recorded here so nobody implements it later.

    A `ValueTree::Listener` on the APVTS fires for **every** parameter write: a Program apply, a host
    automation lane, and a person turning a knob are indistinguishable at that layer. The guard's
    entire job is to tell those apart. A host may write automation on session load *before* replaying
    its remembered program index, so a listener would disarm the guard precisely when it is needed -
    reintroducing, in all six castings at once, the defect the guard exists to prevent.

    The only place that knows a change came from a person is the control's own drag state, which is
    why `connectUserEdit` below is where the disarm lives.

    **The asymmetry is deliberate, and it is the reason this is a helper rather than a method.**
    Reflect-84 once shipped this guard with **zero** call sites for its disarm: not wrong logic, but
    a line somebody had to remember to write and did not. Core cannot remove that line entirely -
    something must observe the drag - but it can make the disarm impossible to omit by putting it in
    the same call as the LCD hand-off, which is the thing a casting *does* remember. Reflect-84's
    defect was writing the hand-off and forgetting the disarm; through `connectUserEdit` that is not
    expressible.

    So: core tests that the mechanism works. Only a per-casting test proves that casting connected
    it. That gap is the argument for owning the call, not for testing around it.
*/
class UserEditGate
{
public:
    /** Arm on restore - and call it **after** `replaceState`, never before. The restore's own
        parameter writes run through the editor's callbacks, so arming first lets them disarm it
        immediately and the guard never fires. */
    void armRestore() noexcept { restorePending.store (true, std::memory_order_relaxed); }

    /** True if a restore is still pending, and clears it either way.

        **Consumed whether or not the caller honours it.** The guard suppresses exactly one replay;
        leaving it armed after a program change that did not match would swallow a later, genuine
        one. */
    bool consumeRestore() noexcept { return restorePending.exchange (false, std::memory_order_relaxed); }

    /** Disarm, because a person moved something. Automation must never reach this - see the class
        comment for what goes wrong when it does. */
    void noteUserEdit() noexcept { restorePending.store (false, std::memory_order_relaxed); }

    /** Reads the flag without consuming it. For tests and assertions only: a caller that branches on
        this in production has written `consumeRestore` with a race in it. */
    bool isRestorePending() const noexcept { return restorePending.load (std::memory_order_relaxed); }

private:
    // Relaxed is sufficient and correct: this flag orders nothing else. `setCurrentProgram` can
    // arrive on the audio thread, because VST3 delivers a program change as an automatable
    // parameter, while `setStateInformation` and the editor's callbacks are message-thread.
    std::atomic<bool> restorePending { false };
};

/** Wires a control so that a **user** move - and only a user move - disarms the gate and then
    announces itself to the LCD.

    One call, because they are one event. Splitting them is what let Reflect-84 ship the announcement
    without the disarm.

    `ControlType` needs `onValueChange` and `isMouseButtonDown()`; every knob and detented switch in
    the suite is a `juce::Slider` subclass, so this is a template only to avoid forcing a downcast at
    each call site.

    **Guarded on the control's own drag state, not on the attachment.** A `SliderAttachment` fires
    `onValueChange` when a Program is applied and on every host automation step. Without the guard
    the LCD latches onto whichever parameter was written last and flickers for the length of a song -
    and the gate would be disarmed by automation, which is the failure the class comment describes.

    `announce` is the casting's own LCD hand-off, and it is deliberately opaque here: three castings
    pass a `RangedAudioParameter&` to their header and three pass a parameter ID string. A control
    with no drag to end - a detented switch that settles immediately - announces and releases in the
    same lambda rather than needing a second entry point.
*/
template <typename ControlType>
void connectUserEdit (ControlType& control, UserEditGate& gate, std::function<void()> announce)
{
    control.onValueChange = [&control, &gate, tell = std::move (announce)]
    {
        if (! control.isMouseButtonDown())
            return;

        gate.noteUserEdit();

        if (tell != nullptr)
            tell();
    };
}

} // namespace nf
