cmake_minimum_required(VERSION 3.24)

# The version a casting pins. Bump it when core changes; a casting moves when it chooses to, which
# is the whole point of the pin — nothing here can force a release on a plugin that is mid-cycle.
project(NeonFoundryCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------------------------
# **One JUCE tree per build, and the consumer owns it.**
#
# A casting has already run FetchContent_MakeAvailable(JUCE) by the time it adds core, so core must
# NOT fetch a second copy: two trees in one build is a duplicate-target error at best, and two
# different builds of juce_core linked into one binary at worst — which surfaces as duplicate
# symbols a long way from the line that caused it.
#
# Standalone, nobody has provided one, so core fetches its own to build and test itself. The pin
# matches what every casting is on; if they diverge, the casting's tree wins by construction,
# because it is made available first and this branch never runs.
if(PROJECT_IS_TOP_LEVEL)
    include(FetchContent)

    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.14
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
elseif(NOT TARGET juce::juce_core)
    message(FATAL_ERROR
        "neon-foundry-core needs juce::juce_core to already exist. Call "
        "FetchContent_MakeAvailable(JUCE) before adding core, so both come from ONE JUCE tree.")
endif()

# ---------------------------------------------------------------------------------------------
# **The pin is enforced here, at configure time, and a mismatch is FATAL.**
#
# A casting's FetchContent_Declare states GIT_TAG v1.0.0, but that is a statement of intent that
# nothing checks: FetchContent will happily satisfy a build from an ALREADY-POPULATED checkout and
# never contact the remote again. On 2026-08-13 four of the six castings were building against core
# commits between one and four behind the tag - two of them from before ValueCase was deleted and
# before the three menu draw* went pure-virtual - and every one of those builds was green. They were
# simply not building the core that ships.
#
# **Checked against the REMOTE, not the local tag ref, and that is the whole point.** v1.0.0 MOVES
# during pre-release (see the root CLAUDE.md). A stale shallow clone carries its own copy of the tag
# still pointing at the commit it was fetched at, so a purely local comparison agrees with itself and
# passes - the same shape as a test that derives its input from the constant it asserts against. Only
# the remote knows where the tag is now.
#
# **FATAL, never a warning.** A warning in a configure log is the same class of signal as the green
# suite that was running the previous binary: emitted, not read, and indistinguishable from success
# at the point where it matters.
if(NOT PROJECT_IS_TOP_LEVEL)
    # Defaults to the only tag that exists pre-release. A consumer overrides it by setting
    # NF_CORE_PIN before FetchContent_MakeAvailable, which is what to do once releases are tagged
    # properly - set it once and feed the same variable to GIT_TAG, so the pin has one spelling.
    if(NOT DEFINED NF_CORE_PIN)
        set(NF_CORE_PIN "v1.0.0")
    endif()

    find_package(Git QUIET REQUIRED)

    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE nf_actual OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE nf_actual_rc ERROR_QUIET)

    execute_process(COMMAND ${GIT_EXECUTABLE} ls-remote origin "refs/tags/${NF_CORE_PIN}^{}" "refs/tags/${NF_CORE_PIN}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE nf_remote OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE nf_remote_rc ERROR_QUIET)

    if(NOT nf_actual_rc EQUAL 0)
        message(FATAL_ERROR
            "neon-foundry-core: cannot read the fetched checkout's HEAD in\n"
            "  ${CMAKE_CURRENT_SOURCE_DIR}\n"
            "so the pin cannot be verified. Refusing rather than assuming it is current.")
    endif()

    if(NOT nf_remote_rc EQUAL 0 OR nf_remote STREQUAL "")
        if(NF_CORE_ALLOW_UNVERIFIED_PIN)
            message(WARNING "neon-foundry-core: pin NOT verified (offline, explicitly allowed).")
        else()
            message(FATAL_ERROR
                "neon-foundry-core: could not reach the remote to resolve '${NF_CORE_PIN}', so the "
                "pin is unverified.\n"
                "This check is deliberately fatal offline: an unverified pin is exactly the state "
                "that let four castings build against stale core.\n"
                "To build offline anyway, and knowing the checkout may be stale, configure with "
                "-DNF_CORE_ALLOW_UNVERIFIED_PIN=ON.")
        endif()
    else()
        # A dereferenced annotated tag (^{}) appears second when both lines match; take the LAST,
        # which is the commit either way. A lightweight tag yields only the first line.
        string(REGEX MATCHALL "[0-9a-f]+\trefs/tags/[^\n]*" nf_lines "${nf_remote}")
        list(GET nf_lines -1 nf_last)
        string(REGEX REPLACE "\t.*" "" nf_expected "${nf_last}")

        if(NOT nf_actual STREQUAL nf_expected)
            string(SUBSTRING "${nf_actual}" 0 9 nf_actual_short)
            string(SUBSTRING "${nf_expected}" 0 9 nf_expected_short)
            message(FATAL_ERROR
                "neon-foundry-core is STALE.\n"
                "  pinned '${NF_CORE_PIN}' is now  ${nf_expected_short}\n"
                "  this build would use          ${nf_actual_short}\n"
                "\n"
                "FetchContent reuses an already-populated checkout and does not re-contact the "
                "remote, so the tag moved and this tree did not follow.\n"
                "\n"
                "Fix, and note it is a RECONFIGURE and not a rebuild - deleting _deps takes the "
                "JUCE tree with it, and the generated project still names those paths:\n"
                "    rm -rf ${CMAKE_BINARY_DIR}/_deps\n"
                "    cmake -B <build-dir> -S <source-dir> -G Xcode")
        endif()

        string(SUBSTRING "${nf_actual}" 0 9 nf_actual_short)
        message(STATUS "neon-foundry-core: pin '${NF_CORE_PIN}' verified at ${nf_actual_short}")
    endif()
endif()

# `SystemPointer.mm` is Apple-only and is the whole of core's Objective-C++ surface: §2b's pointer
# size has no public route but AppKit's NSCursor. Windows needs no detection at all — JUCE already
# rescales custom cursors there — and X11 has no equivalent value to read, so both fall to the
# `#if ! JUCE_MAC` definition in AboutPart.cpp rather than to a stub file.
set(NF_CORE_SOURCES
    src/UserProgramDirectory.cpp
    src/AboutPart.cpp)

if(APPLE)
    list(APPEND NF_CORE_SOURCES src/SystemPointer.mm)
endif()

add_library(neon_foundry_core STATIC ${NF_CORE_SOURCES})

add_library(nf::core ALIAS neon_foundry_core)

# **A STATIC library linked into a SHARED one must be position-independent, and only Linux says so.**
#
# Every casting's VST3 is a shared object, and it links `libneon_foundry_core.a`. On Linux that is a
# hard link error:
#
#     relocation R_X86_64_PC32 against symbol `_ZTVN2nf8AboutBoxE' ... final link failed: bad value
#
# Mach-O is always position-independent, so macOS never had an opinion; Windows DLLs resolve this
# differently again. **The requirement is real on all three and enforced on one** — which is why it
# survived until the first Linux build in the suite's history.
set_target_properties(neon_foundry_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(neon_foundry_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC, not PRIVATE: the header takes and returns juce::File, so anything including it needs
# juce_core's include paths too.
# juce_audio_processors arrived with ParameterSnapshot, which reads live parameter values off an
# AudioProcessor. That is a real step up in weight from juce_core alone, and it is the right one:
# the Program model this is being extracted towards manipulates an APVTS throughout, so a core that
# could not see a parameter would push every interesting piece back to the callers it exists to
# de-duplicate.
target_link_libraries(neon_foundry_core PUBLIC
    juce::juce_core
    juce::juce_audio_processors
    # juce_gui_basics is for nf/MenuMetrics.h, which derives from LookAndFeel_V4. Every casting
    # already links it - a plugin with an editor cannot not - so this adds no dependency any
    # consumer did not already have. Core still draws nothing: MenuMetrics owns sizes and leaves
    # every draw* alone.
    juce::juce_gui_basics)

# **The two feature flags every casting sets, set here too — and the omission was invisible on two
# platforms of three.**
#
# `juce_audio_processors` pulls in `juce_gui_extra`, which core therefore compiles. That module's
# Linux branch is guarded `#elif (JUCE_LINUX || JUCE_BSD) && JUCE_WEB_BROWSER` and includes
# `<gtk/gtk.h>`. `JUCE_WEB_BROWSER` defaults to **1**, so core asked for GTK on Linux while all six
# castings had already declared they do not want a web browser — on their own targets, which does
# nothing for this one.
#
# On macOS and Windows it compiles regardless, because WebKit and WebView2 are simply there. It fails
# only on Linux, which is why it survived: three castings had never run CI at all and the other three
# are macOS-first, so the Linux branch of this dependency had never been configured by anything.
#
# **The fix removes the dependency rather than installing it.** Adding `libgtk-3-dev` to the runners
# would also have gone green, and would have left the suite building a web browser it states six
# times over that it does not use.
target_compile_definitions(neon_foundry_core PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

# Core carries NO company or product identity. Those are the casting's, passed in as arguments —
# see UserProgramDirectory.h. A shared default here would be exactly the hand-synced constant that
# drifted to a dead company name in CHORUS-60 and quietly pointed saved Programs at a directory
# nothing reads.

# ---------------------------------------------------------------------------------------------
# **nf::testing — a SECOND target, deliberately not part of nf::core.**
#
# The suite-wide bug sweep's measurement harness. It is a stated widening of the extraction's §I and
# the reasoning lives at the top of include/nf/testing/ProcessorHarness.h, at the exemption, on the
# model of NO_REDIRECT_NEEDED in tools/run_tests.py.
#
# **Separate target, and that is the whole guard.** A casting's PLUGIN target links nf::core; only
# its TEST target links nf::testing. Folding these into one library would put an operator new
# override into every shipping binary in the suite, which is exactly the sort of thing that is
# invisible until it is catastrophic.
add_library(neon_foundry_core_testing STATIC
    src/testing/ProcessorHarness.cpp
    src/testing/CpuBaseline.cpp
    src/testing/ComponentRender.cpp)

add_library(nf::testing ALIAS neon_foundry_core_testing)

target_link_libraries(neon_foundry_core_testing PUBLIC neon_foundry_core)

# ---------------------------------------------------------------------------------------------
# Tests build only when core is the top-level project, so a casting consuming it via FetchContent
# does not inherit a test target it never asked for and cannot run.
if(PROJECT_IS_TOP_LEVEL AND NOT DEFINED NF_CORE_SKIP_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
