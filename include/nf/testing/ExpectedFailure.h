#pragma once

#include <juce_core/juce_core.h>

#include <vector>

/*  **An arm that is known to fail, marked rather than left red — and the reason is not tidiness.**

    TapeRot's suite fails five arms and Reflect-84's fails one, every run, by design: each is an
    `OPEN FINDING, DELIBERATELY RED` whose own text says a green run there would mean the arm had
    been relaxed. Leaving them red is honest about those six findings and dishonest about everything
    else, because **a permanently red suite cannot report a NEW failure.** Reflect-84's noise floor
    is one and TapeRot's is five: a seventh defect appearing in TapeRot moves the count from five to
    six, which is a number nobody is watching, in a job that was already failing.

    It also blocks publishing outright — `publish` needs the build jobs, and a failing test step
    fails its job — so those two castings cannot be released at all while their findings are open.

    So an expected failure is declared, and the declaration does three things a bare red cannot:

      * the suite goes green, so a NEW failure is visible again;
      * **an arm that starts PASSING fails the build**, because a finding that quietly resolved is
        a finding nobody wrote down — the same ratchet as the warning table and the scope counts;
      * each marker names **what it waits on**, so the reader of a green run can still see what is
        outstanding and where it is tracked.

    ## The vacuity guard, which is the whole difficulty

    An expected failure that never executes looks exactly like one that executed and failed as
    expected: in both cases nothing is reported. A `beginTest` block skipped by an early `return`,
    a suite dropped from `target_sources`, a condition that stopped being reached — each would
    silently satisfy the expectation.

    So the declaration is a LIST, checked at the end of the run: every declared id must have been
    reached. `auditExpectedFailures` reports the ones that were not, and the runner fails on them.
    Declared-but-not-executed is a failure, not a pass.
*/
namespace nf::testing
{

/** One declared expectation: a stable id, and what its resolution is waiting on. */
struct ExpectedFailure
{
    const char* id;        /**< stable, and referenced by the marker at the call site */
    const char* waitsOn;   /**< a design ask, a finding, an open question — named, not described */
};

/** Record an arm that is expected to fail today.

    `nowPasses` is the arm's own condition — the thing that WOULD be asserted if this were an
    ordinary check. Passing `true` therefore means the finding has resolved, and that FAILS the
    test: an expectation that has been overtaken must be removed deliberately rather than left to
    rot into a check that asserts nothing.
*/
void expectedFailure (juce::UnitTest& test,
                      bool nowPasses,
                      const char* id,
                      const juce::String& detail);

/** Ids declared in `declared` that no `expectedFailure` call reached during this run.

    Empty is the passing answer. Anything here is an expectation that was never exercised, which is
    indistinguishable from a satisfied one by any other means — see the vacuity note above.
*/
std::vector<juce::String> expectedFailuresNotExecuted (const std::vector<ExpectedFailure>& declared);

/** Every id reached this run, whether it failed as expected or was reported as resolved. */
std::vector<juce::String> expectedFailuresExecuted();

}  // namespace nf::testing
