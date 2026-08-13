# JUCE is already available: the parent CMakeLists fetched it under PROJECT_IS_TOP_LEVEL, which is
# the same condition that adds this directory. Nothing here fetches anything.
juce_add_console_app(NeonFoundryCoreTests PRODUCT_NAME "NeonFoundryCoreTests")

target_sources(NeonFoundryCoreTests PRIVATE
    UserProgramDirectoryTests.cpp
    ProgramIdTests.cpp
    ParameterSnapshotTests.cpp
    UserProgramStoreTests.cpp
    ParameterReadoutTests.cpp
    MenuMetricsTests.cpp
    PrintedScaleTests.cpp
    UserEditGateTests.cpp)

target_compile_definitions(NeonFoundryCoreTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_UNIT_TESTS=1)

target_link_libraries(NeonFoundryCoreTests PRIVATE
    neon_foundry_core
    juce::juce_core
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

add_test(NAME core COMMAND NeonFoundryCoreTests)
