#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace nf::testing
{

/** Category 7's mechanism: what a plugin costs, measured the same way for six of them, and compared
    against a figure that is a FILE rather than a sentence.

    **Why the baseline is machine-readable and not prose.** This suite has found a stated figure
    drifting from what its own assets produce four times — theme comments claiming contrast the
    plate no longer gave, two caption comments stating 19 against a measured 18, and two
    `ProgramManager` comments stating a cap of 22 against real caps of 31 and 25, one sentence copied
    between two castings and correct in neither. The answer every time was to make the figure
    machine-readable so a tool fails when it stops matching. **A prose bar is a comparison somebody
    has to remember to perform, and this project's whole evidential basis is that those do not get
    performed.**

    **A missing baseline is a REFUSAL, not a skip** — same rule as a suite that did not run. A check
    that silently does nothing when its input is absent is indistinguishable from a check that
    passed, and this file's own suite has three recorded cases of exactly that.

    **The split is the usual one.** Core owns the mechanism — how a cost is measured, how a file is
    read, which cell failed and by how much. Each casting owns the meaning: its own numbers, on its
    own machine, in its own committed `Tests/cpu-baseline.json`.
*/

/** One matrix cell: a configuration, and what it cost. */
struct CpuCell
{
    int blockSize = 0;
    double sampleRate = 0.0;
    /** **A full-panel repaint at 20 Hz with timers fired, NOT "what the editor costs in a host".**
        Headless there is no peer, so the dirty-region path a host uses is unreachable; what this
        measures is every pixel on every timer tick. Stated as the worst case it is, because a
        figure named for something it does not measure is how a precise number becomes a wrong one —
        and it is still the right shape for the bar, which is *catch a casting that is 10x its
        siblings*: a worst case applied identically to six panels compares them correctly even
        though none of them pays it. */
    bool editorOpen = false;

    /** **Fraction of ONE core, not of wall clock.** CPU time consumed divided by the audio duration
        rendered, so 0.02 means the plugin spends 2 % of a core keeping up with real time. This is
        deliberately independent of how fast the measurement itself ran: a driver that renders as
        fast as it can and one paced to real time produce the same figure, which is what makes it
        repeatable on a machine doing other things. */
    double coreFraction = 0.0;

    juce::String key() const;
    juce::String describe() const;
};

/** What produced a set of figures. Recorded IN the file, because "one sitting, all six, one machine
    state" is only enforceable after the fact if the file says which machine and which build. */
struct CpuProvenance
{
    juce::String machine;      ///< e.g. "Apple M3, 8 cores, 24 GB"
    juce::String os;           ///< e.g. "macOS 26.5"
    juce::String config;       ///< "Release"
    juce::String coreCommit;   ///< the neon-foundry-core commit every casting was pinned at
    juce::String takenOn;      ///< ISO date

    bool isComplete() const;
};

/** A casting's committed baseline. */
struct CpuBaseline
{
    CpuProvenance provenance;
    std::vector<CpuCell> cells;

    /** The session statement: instances with editors closed, plus open, inside a fraction of a core.
        Quoted in prose everywhere, so it lives here where it can be checked. */
    int sessionClosedInstances = 0;
    int sessionOpenInstances = 0;
    double sessionCoreFraction = 0.0;

    const CpuCell* find (int blockSize, double sampleRate, bool editorOpen) const;

    juce::String toJson() const;

    /** Reads a baseline. **Returns an EMPTY optional for a missing or malformed file rather than a
        default-constructed one**, so a caller cannot mistake "nothing recorded" for "all zeroes",
        which would make every measurement a regression of infinite percent and read as a tool bug
        rather than as the missing input it is. */
    static std::optional<CpuBaseline> read (const juce::File&, juce::String& whyNot);
};

/** Renders `processor` and reports what it cost.

    **The editor, when asked for, is CONSTRUCTED AND PUMPED rather than merely constructed.** Several
    castings run 20 Hz timers and repaint scopes continuously, so an editor that exists but never
    receives a message costs nothing and would report a GUI price of zero. The message loop is run
    for the same audio duration the render covers.

    **Not the standalone, and that is a correction rather than a convenience.** A standalone cannot
    close its own editor, and the app-hidden proxy is unsound — Chorus-60's CPU *rose* 19 points when
    hidden, 67.6 % to 86.8 %, which no painting model explains. Headless also avoids the microphone
    consent dialog, which is a modal that blocks the window and reads as a launch failure.

    @param seconds  audio duration to render. Longer is steadier; 2 s is enough to put the median
                    well clear of scheduler noise on an idle machine, and machine idleness is part
                    of the method rather than a detail — the same knob measured 2022 µs on an idle
                    run against 3469 µs on a busy one, with the busy run's mean 60 % above its median.
*/
CpuCell measureCpu (juce::AudioProcessor& processor, int blockSize, double sampleRate,
                    bool editorOpen, double seconds = 2.0);

/** One measured cell against its recorded one. */
struct CpuComparison
{
    CpuCell measured;
    double baselineFraction = 0.0;
    double ratio = 0.0;        ///< measured / baseline
    bool withinTolerance = true;
    /** True when the cell is below the noise floor and the ratio bar was NOT applied to it. Reported
        rather than silent, because a cell that is not being checked must not look like one that
        passed — which is the same rule that makes a missing baseline a refusal. */
    bool belowNoiseFloor = false;

    juce::String describe() const;
};

/** Compares a full matrix against a baseline.

    @param tolerance  1.10 for the suite's stated bar: **no casting may get more than 10 % slower
                      than its recorded baseline.** Absolute figures drift with compiler and OS
                      versions and quietly stop meaning anything; a delta against a stored figure
                      stays meaningful, and it is the shape that catches "this got slower and nobody
                      noticed".
*/
/** @param noiseFloor  the absolute core fraction below which the RATIO BAR DOES NOT APPLY.

    **A ratio discards magnitude, so a bar stated as one needs a magnitude to be meaningful.** At
    0.2 % of a core a 10 % regression is 0.02 % — neither detectable nor worth detecting, and the
    bar exists to catch a casting becoming an order heavier rather than to shave points. Making the
    measurement more precise does not help: measured perfectly, 0.02 % would still mean nothing.

    **0.01 is DERIVED, not chosen round.** Four to five repeated runs of all six castings, 70 cells:

    | Magnitude | Cells | Worst spread | Median spread |
    |---|---|---|---|
    | below 1 % of a core | 32 | **71.0 %** | 3.0 % |
    | 1 % to 10 % | 11 | 7.9 % | 1.7 % |
    | above 10 % | 27 | 5.7 % | 1.9 % |

    The discriminator is not gradual: below 1 % there are cells spreading 37 %, 54 % and 71 % run to
    run, and at 1 % and above nothing exceeds 8 %. The worst offenders are every casting's
    `64@48000/closed` — the shortest per-block work, where the clock's own quantisation is the same
    size as the quantity.

    **And the 10 % bar's margin over that is thin and is stated rather than implied**: 7.9 % observed
    against a 10 % bar leaves about two points. A cell failing at 11–12 % should be re-run before it
    is believed; one failing at 30 % is a regression. That is a property of this instrument on this
    machine, which is why the figure lives beside the measurement that produced it and not in prose.
*/
/** The machine, in the exact form a baseline's provenance records it.

    One function so the writer and the reader cannot describe the same machine two ways — the
    string is built here and written from here, rather than composed at each of six call sites. */
juce::String currentMachine();

/** Whether this baseline was taken on the machine now running.

    **A CPU baseline is a property of the machine that recorded it, and asserting one somewhere else
    is asserting a figure about different hardware.** Elmer's first CI run failed five editor cells
    at 1.26x to 1.39x against figures taken on `Mac15,13, 8 cores`; the runner is not that machine.
    The closed cells were over by the same proportion — the free control saying the whole column was
    the machine rather than the code.

    So a caller asserts when this is true and **reports** when it is false. That keeps the bar
    meaningful where it means something and keeps the measurement visible everywhere else, instead
    of the two other options: a bar that fails on every runner, or a suite that skips its own
    performance test wherever it is inconvenient.

    **THE GAP THIS LEAVES IS WIDER THAN THE REASON REQUIRES, and naming it is the point.** The
    argument above is sound about asserting THIS baseline elsewhere. It says nothing against a
    second baseline recorded on the runner and keyed by `currentMachine()`, which already exists
    and is already what distinguishes them.

    What the gap costs, stated rather than left to be discovered: **a performance regression can
    merge through a fully green pipeline.** Nothing in CI will catch it, because nothing in CI
    asserts. The catch is a local run on the baseline machine, which means the bar has to be run
    deliberately before a release rather than relied on to have run — the same standing as
    `check_contrast`, which is correct on the day somebody asks for it and simply is not asked.

    This is recorded as an open design question, not a defect. It is here rather than only in a
    plan because a reason that licenses a wider conclusion than it supports is invisible to the
    question *is this still true* — nothing changed, and nothing will. */
bool baselineTakenOnThisMachine (const CpuProvenance&);

std::vector<CpuComparison> compareToBaseline (const std::vector<CpuCell>& measured,
                                              const CpuBaseline&, double tolerance = 1.10,
                                              double noiseFloor = 0.01);

/** The matrix this suite measures, so six castings cannot each pick their own and then be compared.

    48 kHz / 128 is the reference the bar is stated at; the others exist to catch a cost that scales
    with the wrong thing — a per-block fixed overhead shows up at 64, and a per-sample one does not. */
std::vector<std::pair<int, double>> standardMatrix();

} // namespace nf::testing
