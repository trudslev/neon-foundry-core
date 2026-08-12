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

add_library(neon_foundry_core STATIC
    src/UserProgramDirectory.cpp)

add_library(nf::core ALIAS neon_foundry_core)

target_include_directories(neon_foundry_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC, not PRIVATE: the header takes and returns juce::File, so anything including it needs
# juce_core's include paths too.
target_link_libraries(neon_foundry_core PUBLIC juce::juce_core)

# Core carries NO company or product identity. Those are the casting's, passed in as arguments —
# see UserProgramDirectory.h. A shared default here would be exactly the hand-synced constant that
# drifted to a dead company name in CHORUS-60 and quietly pointed saved Programs at a directory
# nothing reads.

# ---------------------------------------------------------------------------------------------
# Tests build only when core is the top-level project, so a casting consuming it via FetchContent
# does not inherit a test target it never asked for and cannot run.
if(PROJECT_IS_TOP_LEVEL AND NOT DEFINED NF_CORE_SKIP_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
