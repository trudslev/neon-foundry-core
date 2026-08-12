# neon-foundry-core

Shared **behaviour** for the Neon Foundry castings. Not shared appearance.

Each casting pins a tag of this repository in its own `CMakeLists.txt`, beside the JUCE pin and by
the same mechanism. A change here forces nothing on anyone: a plugin moves when it chooses to.

```cmake
FetchContent_Declare(NeonFoundryCore
    GIT_REPOSITORY https://github.com/trudslev/neon-foundry-core.git
    GIT_TAG v0.1.0)
FetchContent_MakeAvailable(NeonFoundryCore)

target_link_libraries(MyPlugin PRIVATE nf::core)
```

**Make JUCE available first.** Core links `juce::juce_core` and deliberately does not fetch its own
copy when consumed — two JUCE trees in one build link two `juce_core` builds into one binary, and
that surfaces as duplicate symbols a long way from the cause. Standalone (tests, CI) it fetches its
own, pinned to the same 8.0.14 every casting uses.

## What belongs here

Behaviour that is identical across castings and has a defect history proving it drifts:

| | |
|---|---|
| `nf::userProgramDirectory` | where a casting keeps the user's saved Programs, per platform |

## What does not

Themes, palettes, knob rendering, panel backgrounds, factory banks, menu row painting, name caps,
sweep angles — anything whose divergence between castings is deliberate. **The line is behaviour
shared, appearance not.** If a change starts pulling appearance across it, stop: a separate
repository with its own tagging discipline is justified by shared behaviour and stops being
justified the moment it becomes a shared theme.

Company and product names are never defaults here. They are arguments, because a hand-synced copy
of one drifted to a dead company name in CHORUS-60 and quietly pointed saved Programs at a
directory nothing reads.

## Build and test

```sh
cmake -B build -G Xcode          # macOS; -A x64 on Windows, -DCMAKE_BUILD_TYPE=Release on Linux
cmake --build build --config Release
./build/tests/NeonFoundryCoreTests_artefacts/Release/NeonFoundryCoreTests
```
