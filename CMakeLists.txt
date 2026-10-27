cmake_minimum_required (VERSION 3.22)

project (TapeGrainAndBitrot VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    message (FATAL_ERROR "JUCE not found in third_party/JUCE - run tools/build.ps1 after restoring third_party.")
endif ()

if (MINGW)
    # GCC 15+ dropped the implicit <cstring>/<cstdint>/... includes that JUCE 7 relies on.
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include cstring -include cstdint -include cstdio -include cstdlib")
endif ()

add_subdirectory (third_party/JUCE EXCLUDE_FROM_ALL)

#===============================================================================
# shared settings
set (DS_PLUGIN_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

function (ds_finish_plugin target)
    juce_generate_juce_header (${target})

    target_compile_definitions (${target} PUBLIC ${DS_PLUGIN_DEFINITIONS})

    target_link_libraries (${target} PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)

    if (MINGW)
        # Ship a self-contained binary: no libstdc++-6.dll / libwinpthread-1.dll hunting in the DAW.
        target_link_options (${target} PRIVATE
            -static-libgcc
            -static-libstdc++
            -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
    endif ()
endfunction ()

#===============================================================================
# TapeGrain - tape saturation / bit crush / wow & flutter
juce_add_plugin (TapeGrain
    COMPANY_NAME "DSh Audio"
    BUNDLE_ID "com.dshaudio.tapegrain"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Dsha
    PLUGIN_CODE Tg01
    FORMATS VST3 Standalone
    PRODUCT_NAME "TapeGrain")

target_sources (TapeGrain PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

ds_finish_plugin (TapeGrain)

#===============================================================================
# BADHERTS Losser999 - lossy digital decay, pixel-art ui
juce_add_plugin (BadHertsLooser999
    COMPANY_NAME "DSh Audio"
    BUNDLE_ID "com.dshaudio.badhertslooser999"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Dsha
    PLUGIN_CODE Btr1
    FORMATS VST3 Standalone
    PRODUCT_NAME "BADHERTS Losser999")

target_sources (BadHertsLooser999 PRIVATE
    src/BitrotProcessor.cpp
    src/BitrotEditor.cpp)

ds_finish_plugin (BadHertsLooser999)
