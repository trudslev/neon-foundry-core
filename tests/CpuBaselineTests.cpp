#include <nf/testing/CpuBaseline.h>
#include <juce_audio_processors/juce_audio_processors.h>

/*  Core owns the mechanism; each casting owns its numbers. So what is asserted here is the
    mechanism's behaviour on inputs a casting cannot produce — a missing file, a malformed one, a
    cell the baseline does not carry — and NOT any figure, because a figure measured here would be a
    figure about this test binary.  */
class CpuBaselineTests final : public juce::UnitTest
{
public:
    CpuBaselineTests() : juce::UnitTest ("CPU baseline", "core") {}

    void runTest() override
    {
        beginTest ("A missing baseline is a REFUSAL, not a skip");
        {
            /*  The rule this suite has three recorded violations of: a check that silently does
                nothing when its input is absent is indistinguishable from a check that passed. So
                `read` returns an empty optional and a reason, and cannot be mistaken for a
                default-constructed baseline of all zeroes — which would make every measurement an
                infinite regression and read as a tool defect rather than as the missing input. */
            juce::String why;
            const auto missing = nf::testing::CpuBaseline::read (
                juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("nf-no-such-baseline.json"), why);

            expect (! missing.has_value(), "a missing baseline was accepted");
            expect (why.contains ("refusal, not a skip"),
                    "the refusal must say why, since the caller's only sane response is to run an "
                    "explicit re-baseline: " + why);
        }

        beginTest ("A baseline with no provenance is refused");
        {
            /*  A figure whose machine and build are unrecorded cannot be compared against a later
                one, which is the only thing the file is for. This is also what makes "one sitting,
                all six, one machine state" enforceable AFTER the fact rather than a promise. */
            auto file = juce::File::createTempFile (".json");
            file.replaceWithText (R"({ "cells": [ { "blockSize": 128, "sampleRate": 48000,
                                        "editorOpen": false, "coreFraction": 0.01 } ] })");

            juce::String why;
            const auto b = nf::testing::CpuBaseline::read (file, why);
            file.deleteFile();

            expect (! b.has_value(), "a baseline with no provenance was accepted");
            expect (why.contains ("provenance"), why);
        }

        beginTest ("A round trip preserves every cell and its provenance");
        {
            nf::testing::CpuBaseline written;
            written.provenance = { "Apple M3, 8 cores, 24 GB", "macOS 26.5", "Release",
                                    "fcd8268d5", "2026-08-20" };
            written.sessionClosedInstances = 24;
            written.sessionOpenInstances = 4;
            written.sessionCoreFraction = 0.5;

            for (const auto& [block, rate] : nf::testing::standardMatrix())
                for (const bool open : { false, true })
                    written.cells.push_back ({ block, rate, open, 0.0125 });

            auto file = juce::File::createTempFile (".json");
            file.replaceWithText (written.toJson());

            juce::String why;
            const auto read = nf::testing::CpuBaseline::read (file, why);
            file.deleteFile();

            expect (read.has_value(), why);

            if (read.has_value())
            {
                expectEquals ((int) read->cells.size(), (int) written.cells.size());
                expectEquals (read->provenance.coreCommit, juce::String ("fcd8268d5"));
                expectEquals (read->sessionClosedInstances, 24);

                const auto* cell = read->find (128, 48000.0, true);
                expect (cell != nullptr, "the 128/48k editor-open cell did not survive the round trip");
                if (cell != nullptr)
                    expectWithinAbsoluteError (cell->coreFraction, 0.0125, 1.0e-6);
            }
        }

        beginTest ("The 10 % bar catches a regression, and a cell the baseline lacks is a FAILURE");
        {
            /*  **Both directions, because one only confirms what you already believed.** A
                comparison that passes everything and one that fails everything are the same
                uselessness, so the arm asserts a figure inside the bar passes, one outside it
                fails, and an unrecorded cell fails rather than being silently skipped — the matrix
                is shared precisely so six castings cannot each measure a different set and then be
                compared. */
            nf::testing::CpuBaseline baseline;
            baseline.provenance = { "m", "os", "Release", "abc", "2026-08-20" };
            baseline.cells.push_back ({ 128, 48000.0, false, 0.0100 });

            const std::vector<nf::testing::CpuCell> measured {
                { 128, 48000.0, false, 0.0109 },   // +9 %, inside
                { 128, 48000.0, true,  0.0300 },   // not in the baseline at all
            };

            const auto result = nf::testing::compareToBaseline (measured, baseline, 1.10);

            expectEquals ((int) result.size(), 2);
            expect (result[0].withinTolerance, "+9 % must pass a 10 % bar: " + result[0].describe());
            expect (! result[1].withinTolerance,
                    "a cell the baseline does not carry must fail, not pass by absence");

            const std::vector<nf::testing::CpuCell> over { { 128, 48000.0, false, 0.0111 } };
            const auto regressed = nf::testing::compareToBaseline (over, baseline, 1.10);
            expect (! regressed[0].withinTolerance,
                    "+11 % must fail a 10 % bar: " + regressed[0].describe());
        }

        beginTest ("Below the noise floor the ratio bar does not apply, and the cell SAYS so");
        {
            /*  Derived from 70 cells over four to five repeated runs of all six castings: below 1 %
                of a core the run-to-run spread reaches 71 %, and at 1 % and above nothing exceeds
                8 %. A 10 % bar under that floor is not strict or lax, it is SILENT - and a silent
                bar that occasionally fires reads as a flaky test. */
            nf::testing::CpuBaseline b;
            b.provenance = { "m", "os", "Release", "abc", "2026-08-20" };
            b.cells.push_back ({ 128, 48000.0, false, 0.0030 });   // 0.3 % - under the floor
            b.cells.push_back ({ 128, 48000.0, true,  0.2000 });   // 20 %  - well over it

            const std::vector<nf::testing::CpuCell> measured {
                { 128, 48000.0, false, 0.0060 },   // +100 %, and meaningless at 0.3 % of a core
                { 128, 48000.0, true,  0.4000 },   // +100 %, and a real regression
            };

            const auto r = nf::testing::compareToBaseline (measured, b, 1.10, 0.01);

            expect (r[0].belowNoiseFloor, "0.3 % must be under a 1 % floor");
            expect (r[0].withinTolerance, "an exempt cell must not fail: " + r[0].describe());
            expect (r[0].describe().contains ("NOT applied"),
                    "an unchecked cell must not read like a checked one that passed");

            expect (! r[1].belowNoiseFloor, "20 % is not under the floor");
            expect (! r[1].withinTolerance, "doubling at 20 % of a core must fail");

            /*  **The exemption takes BOTH figures, not just the recorded one.** A cell that was
                cheap when baselined and is expensive now is exactly the regression worth catching;
                exempting it on the old figure alone would hide it. */
            const std::vector<nf::testing::CpuCell> grew { { 128, 48000.0, false, 0.0500 } };
            const auto g = nf::testing::compareToBaseline (grew, b, 1.10, 0.01);
            expect (! g[0].belowNoiseFloor,
                    "0.3 % becoming 5 % must NOT be exempted on the baseline's figure alone");
            expect (! g[0].withinTolerance, "0.3 % to 5 % is a 16x regression and must fail");
        }

        beginTest ("The matrix is shared, so six castings cannot each measure a different set");
        {
            const auto matrix = nf::testing::standardMatrix();
            expectGreaterThan ((int) matrix.size(), 1);

            // 48 kHz / 128 is the configuration the bar is stated at and must be in the set.
            const bool hasReference =
                std::any_of (matrix.begin(), matrix.end(),
                             [] (const auto& c) { return c.first == 128 && c.second == 48000.0; });

            expect (hasReference, "the reference cell the bar is stated at is not in the matrix");
        }
    }
};

static CpuBaselineTests cpuBaselineTests;
