cmake_minimum_required(VERSION 3.22)
project(HarshVox VERSION 0.2.0 LANGUAGES C CXX)   # bump per release; see CHANGELOG.md

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(HV_BUILD_TESTS "Build unit tests" ON)
option(HV_BUILD_TOOLS "Build render / benchmark tools" ON)
option(HV_BUILD_PLUGIN "Build the VST3 plugin" ON)

add_subdirectory(external/JUCE)

# ---------------------------------------------------------------------------
# DSP core: headless, depends only on juce_audio_basics (AudioBuffer). INTERFACE so each final
# binary compiles JUCE module sources exactly once.
set(HV_DSP_SOURCES
    src/dsp/Lpc.cpp
    src/dsp/Analysis.cpp
    src/dsp/Engine.cpp
    src/dsp/ModuleFactory.cpp
    src/dsp/modules/LpcModules.cpp
    src/dsp/modules/PitchModules.cpp
    src/dsp/modules/PhysicalModules.cpp
    src/dsp/modules/SegmentModules.cpp
    src/dsp/modules/CrowdModules.cpp)

add_library(hv_dsp INTERFACE)
target_sources(hv_dsp INTERFACE ${HV_DSP_SOURCES})
target_include_directories(hv_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(hv_dsp INTERFACE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hv_dsp INTERFACE $<$<CONFIG:Release>:-O3> -Wall -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Plugin
set(HV_PLUGIN_SOURCES
    src/plugin/PluginProcessor.cpp
    src/plugin/Presets.cpp)
set(HV_GUI_SOURCES
    src/gui/PluginEditor.cpp
    src/gui/Widgets.cpp
    src/gui/SlotComponents.cpp
    src/gui/AnalysisDisplay.cpp
    src/gui/BlockFont.cpp)

if(HV_BUILD_PLUGIN)
    juce_add_plugin(HarshVox
        COMPANY_NAME "Static Ruin"
        COMPANY_WEBSITE "https://github.com/clhaydel/vocalVST"
        BUNDLE_ID com.staticruin.harshvox
        PLUGIN_MANUFACTURER_CODE StRn
        PLUGIN_CODE HVox
        FORMATS VST3
        PRODUCT_NAME "Harsh Vox"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Fx Distortion
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_add_binary_data(HarshVoxData SOURCES resources/fonts/SpaceGrotesk-Medium.ttf resources/fonts/DMMono-Regular.ttf)

    target_sources(HarshVox PRIVATE ${HV_PLUGIN_SOURCES} ${HV_GUI_SOURCES})
    target_compile_definitions(HarshVox PUBLIC
        HV_VERSION_STRING="${PROJECT_VERSION}"
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(HarshVox
        PRIVATE hv_dsp HarshVoxData juce::juce_audio_utils juce::juce_audio_processors
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

    if(HV_BUILD_TOOLS)
        # editor snapshot tool (needs a display, e.g. xvfb-run)
        juce_add_gui_app(hv_ui_snapshot PRODUCT_NAME "hv_ui_snapshot")
        target_sources(hv_ui_snapshot PRIVATE tools/ui_snapshot.cpp tests/TestHelpers.cpp ${HV_PLUGIN_SOURCES} ${HV_GUI_SOURCES})
        target_include_directories(hv_ui_snapshot PRIVATE tests)
        target_compile_definitions(hv_ui_snapshot PRIVATE HV_VERSION_STRING="${PROJECT_VERSION}" JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(hv_ui_snapshot PRIVATE hv_dsp HarshVoxData juce::juce_audio_utils juce::juce_audio_formats
            juce::juce_recommended_config_flags)
    endif()
endif()

# ---------------------------------------------------------------------------
# Headless processor library for tests/tools (no editor): define HV_HEADLESS
if(HV_BUILD_TESTS OR HV_BUILD_TOOLS)
    add_library(hv_headless INTERFACE)
    target_sources(hv_headless INTERFACE ${HV_PLUGIN_SOURCES})
    target_compile_definitions(hv_headless INTERFACE HV_HEADLESS=1 HV_VERSION_STRING="${PROJECT_VERSION}")
    target_link_libraries(hv_headless INTERFACE hv_dsp juce::juce_audio_processors juce::juce_audio_formats)
endif()

if(HV_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.7.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    juce_add_console_app(hv_tests PRODUCT_NAME "hv_tests")
    target_sources(hv_tests PRIVATE
        tests/TestMain.cpp tests/TestHelpers.cpp tests/AnalysisTests.cpp tests/ModuleTests.cpp tests/EngineTests.cpp)
    target_compile_definitions(hv_tests PRIVATE HV_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(hv_tests PRIVATE hv_headless Catch2::Catch2
        juce::juce_recommended_config_flags)
    include(CTest)
    include(Catch)
    catch_discover_tests(hv_tests)
endif()

if(HV_BUILD_TOOLS)
    juce_add_console_app(hv_cli PRODUCT_NAME "hv_cli")
    target_sources(hv_cli PRIVATE tools/hv_cli.cpp tests/TestHelpers.cpp)
    target_include_directories(hv_cli PRIVATE tests)
    target_link_libraries(hv_cli PRIVATE hv_headless juce::juce_recommended_config_flags)
endif()
