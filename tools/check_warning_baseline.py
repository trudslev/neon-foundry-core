#!/usr/bin/env python3
"""Every compiler warning class is baselined BY CLASS, and a class nobody named fails the build.

## Why per-class and not a count

Three findings in one week were already sitting in output nobody reads: the CPU harness's
closed-editor cells, a Windows test step printing 8 lines against macOS's 94, and **168 × C4566**
naming a silent string-corruption defect outright. Those channels are unread by construction — long,
mostly noise, and consulted only when something already looks wrong — so a defect that keeps the
build green is invisible in them.

A single warning COUNT with a growth baseline cannot close that, because its baseline is taken from
today's output: the 168 would have been absorbed **invisibly** as part of one number, and the check
could not have found the defect that prompted it.

**Per class, the same population is absorbed VISIBLY** — one line, `C4566: 168`, in a file somebody
had to write and review. Committing the table is itself the act of reading the classes. And a class
nobody named appears as a NEW ROW, which is the coverage an evidential `-Werror` list cannot have,
because that list can only name classes someone already thought of.

**Be precise about what each half buys**, or the next reader will over-read this file:

  * `/we4566` and friends **detect** — instance one, by name, no baseline. Only for classes that
    have already cost a finding.
  * this table **surfaces** — the existing population for human triage, and automatically fails on
    the next class nobody named.

The table would NOT have caught the 168 on its own. It would have made them conspicuous, which is
where they would have been noticed. That is a weaker claim than detection and it is the true one.

## Exact match, in both directions

A count above baseline is a regression. A count BELOW baseline also fails, with an instruction to
lower it — because a stale-high baseline silently permits regrowth, and a table that is allowed to
be wrong in one direction is a table nobody updates. Satisfying it is one line.

## Usage

    check_warning_baseline.py <platform> <build-log> <baseline.json>

`platform` is one of macos / linux / windows. The baseline is
`{"macos": {"-Wfloat-equal": 4, ...}, "linux": {...}, "windows": {"C4458": 3}}`.
"""
import collections
import json
import pathlib
import re
import sys

# MSVC prints `warning C4458:`; clang and gcc print the flag in brackets, `[-Wshadow]`.
MSVC = re.compile(r'\bwarning (C\d{4})\b')
GNU = re.compile(r'\[-W([a-z0-9-]+)\]')


def classes_in(text):
    found = collections.Counter()
    for m in MSVC.finditer(text):
        found[m.group(1)] += 1
    for m in GNU.finditer(text):
        found["-W" + m.group(1)] += 1
    return found


def main():
    if len(sys.argv) != 4:
        print(__doc__.strip())
        return 2

    platform, log_path, baseline_path = sys.argv[1], sys.argv[2], sys.argv[3]

    log = pathlib.Path(log_path)
    if not log.exists():
        print("no build log at %s — the build step must tee its output" % log_path)
        return 1

    observed = classes_in(log.read_text(errors="replace"))

    bp = pathlib.Path(baseline_path)
    if not bp.exists():
        print("no baseline at %s. Observed on %s:\n" % (baseline_path, platform))
        print(json.dumps({platform: dict(sorted(observed.items()))}, indent=2))
        return 1

    baseline = json.loads(bp.read_text()).get(platform)
    if baseline is None:
        print("baseline has no '%s' section. Observed:\n" % platform)
        print(json.dumps(dict(sorted(observed.items())), indent=2))
        return 1

    new = sorted(set(observed) - set(baseline))
    gone = sorted(set(baseline) - set(observed))
    grew = sorted(k for k in set(observed) & set(baseline) if observed[k] > baseline[k])
    shrank = sorted(k for k in set(observed) & set(baseline) if observed[k] < baseline[k])

    for k in sorted(set(observed) | set(baseline)):
        print("  %-34s baseline %-5s observed %s"
              % (k, baseline.get(k, "-"), observed.get(k, 0)))
    print()

    failed = False

    # The arm this table exists for. A class nobody named is the case the -Werror list cannot reach.
    if new:
        failed = True
        print("** NEW WARNING CLASS — nobody has named this one **")
        for k in new:
            print("   %s x %d" % (k, observed[k]))
        print("   Read it before adding the row: a class appearing for the first time is either a")
        print("   new defect or a compiler change, and only one of those is a row to write down.")

    if grew:
        failed = True
        print("** MORE than baseline **")
        for k in grew:
            print("   %s: %d -> %d" % (k, baseline[k], observed[k]))

    if shrank or gone:
        failed = True
        print("** FEWER than baseline — lower it, so the table stays true **")
        for k in shrank:
            print("   %s: %d -> %d" % (k, baseline[k], observed[k]))
        for k in gone:
            print("   %s: %d -> 0 (remove the row)" % (k, baseline[k]))

    if failed:
        print("\nThe table for '%s' as observed:\n" % platform)
        print(json.dumps(dict(sorted(observed.items())), indent=2))
        return 1

    print("%s: %d warning class(es), all at baseline" % (platform, len(observed)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
