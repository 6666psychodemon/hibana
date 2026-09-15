cmake_minimum_required(VERSION 3.22)
project(Hibana VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout if HIBANA_JUCE_DIR is given, otherwise download it.
set(HIBANA_JUCE_DIR "" CACHE PATH "Path to an existing JUCE checkout (optional)")
if(HIBANA_JUCE_DIR)
    add_subdirectory(${HIBANA_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(HIBANA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND HIBANA_FORMATS AU)
endif()

juce_add_plugin(Hibana
    PRODUCT_NAME                "Hibana"
    COMPANY_NAME                "Demon"
    BUNDLE_ID                   "com.demon.hibana"
    PLUGIN_MANUFACTURER_CODE    Dmon
    PLUGIN_CODE                 Hbn1
    FORMATS                     ${HIBANA_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(Hibana PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Hibana PRIVATE engine)

target_compile_definitions(Hibana PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Hibana
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline renderer / test harness for the engine (no JUCE needed).
add_executable(hibana_render tools/render.cpp)
target_include_directories(hibana_render PRIVATE engine)
