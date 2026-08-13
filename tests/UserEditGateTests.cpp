#include <nf/UserEditGate.h>

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
    /** A control shaped like every knob and detented switch in the suite - a `juce::Slider` with an
        `onValueChange` and a drag state - with the drag state made settable so a test can play the
        part of a person or of an automation lane.

        Deliberately not a casting's knob: what is being tested is that the gate cannot be disarmed
        by a write the user did not make, and a test coupled to one plugin's control would break for
        reasons that say nothing about the gate. */
    class StubControl final : public juce::Slider
    {
    public:
        bool isMouseButtonDown (bool = false) const { return dragging; }

        /** Fires the callback the way a SliderAttachment does - which is the same call whether the
            write came from a person, a Program apply, or a host automation step. That is precisely
            why the drag state has to be the discriminator. */
        void fire() { if (onValueChange != nullptr) onValueChange(); }

        bool dragging = false;
    };
}

class UserEditGateTests final : public juce::UnitTest
{
public:
    UserEditGateTests() : juce::UnitTest ("nf::UserEditGate", "core") {}

    void runTest() override
    {
        beginTest ("A fresh gate has nothing pending");
        {
            nf::UserEditGate gate;
            expect (! gate.isRestorePending());
            expect (! gate.consumeRestore());
        }

        beginTest ("Arming, then consuming once — a second consume is false");
        {
            nf::UserEditGate gate;
            gate.armRestore();
            expect (gate.isRestorePending());

            expect (gate.consumeRestore(), "the first consume reports the pending restore");
            expect (! gate.consumeRestore(),
                    "the guard suppresses exactly ONE replay; staying armed would swallow a later "
                    "genuine program change");
        }

        beginTest ("consumeRestore clears whether or not the caller honours it");
        {
            // The processor's shape: `if (consumeRestore() && index == getCurrentProgram()) return;`
            // A non-matching index still consumes, because short-circuiting the OTHER way round
            // would leave the flag armed for the next, unrelated call.
            nf::UserEditGate gate;
            gate.armRestore();
            const bool pending = gate.consumeRestore();
            expect (pending);
            expect (! gate.isRestorePending());
        }

        beginTest ("noteUserEdit disarms");
        {
            nf::UserEditGate gate;
            gate.armRestore();
            gate.noteUserEdit();
            expect (! gate.isRestorePending());
            expect (! gate.consumeRestore());
        }

        beginTest ("A USER move disarms the gate and announces");
        {
            nf::UserEditGate gate;
            StubControl control;
            int announced = 0;

            nf::connectUserEdit (control, gate, [&announced] { ++announced; });

            gate.armRestore();
            control.dragging = true;
            control.fire();

            expect (! gate.isRestorePending(), "a person moved it, so the restore is no longer live");
            expectEquals (announced, 1);
        }

        beginTest ("AUTOMATION does not disarm the gate, and does not announce");
        {
            // The whole reason this is not a ValueTree::Listener. A host may write automation on
            // session load BEFORE replaying its remembered program index; a mechanism that could not
            // tell that apart would disarm the guard exactly when it is needed, and the replay would
            // land on top of the restored state.
            nf::UserEditGate gate;
            StubControl control;
            int announced = 0;

            nf::connectUserEdit (control, gate, [&announced] { ++announced; });

            gate.armRestore();
            control.dragging = false;   // a SliderAttachment write, not a drag
            control.fire();

            expect (gate.isRestorePending(),
                    "automation must never disarm the stale-replay guard");
            expectEquals (announced, 0,
                          "and it must not take the LCD over either — that is the flicker this "
                          "guard's sibling rule prevents");
        }

        beginTest ("A Program apply looks exactly like automation here, and is treated as such");
        {
            nf::UserEditGate gate;
            StubControl control;
            nf::connectUserEdit (control, gate, [] {});

            gate.armRestore();

            // Applying a Program writes every parameter through the same attachments. Six writes,
            // none of them a drag.
            for (int i = 0; i < 6; ++i)
                control.fire();

            expect (gate.isRestorePending());
        }

        beginTest ("The disarm cannot be wired without the announcement, or the announcement "
                   "without the disarm");
        {
            // This is the defect being designed out rather than tested for: Reflect-84 shipped the
            // announcement with the disarm missing. Through connectUserEdit the two are one call, so
            // the omission is not expressible. What CAN still be checked is that a caller passing no
            // announcement at all still gets the disarm — the argument is optional, the gate is not.
            nf::UserEditGate gate;
            StubControl control;
            nf::connectUserEdit (control, gate, nullptr);

            gate.armRestore();
            control.dragging = true;
            control.fire();

            expect (! gate.isRestorePending(),
                    "an empty announcement must not skip the disarm");
        }

        beginTest ("Re-arming after a user edit works — a second session restore still guards");
        {
            nf::UserEditGate gate;
            StubControl control;
            nf::connectUserEdit (control, gate, [] {});

            gate.armRestore();
            control.dragging = true;
            control.fire();
            expect (! gate.isRestorePending());

            gate.armRestore();
            expect (gate.consumeRestore(), "the gate is reusable, not one-shot for the object's life");
        }
    }
};

static UserEditGateTests userEditGateTests;
