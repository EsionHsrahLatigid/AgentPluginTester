cmake_minimum_required(VERSION 3.22)

project(AgentPluginHost VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS version")

option(AGENTPLUGINHOST_BUILD_APP "Build the AgentPluginHost app and scanner" ON)
option(AGENTPLUGINHOST_BUILD_TESTS "Build unit tests and fixture VST3s" ON)
option(AGENTPLUGINHOST_VERIFY_ARTIFACTS "Register the staged artifact contract test" OFF)
set(AGENTPLUGINHOST_ARTIFACT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH "Stable product staging root")
set(EHL_JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE source checkout; avoids network when set")

if(EHL_JUCE_SOURCE_DIR)
    if(NOT EXISTS "${EHL_JUCE_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "EHL_JUCE_SOURCE_DIR does not contain JUCE: ${EHL_JUCE_SOURCE_DIR}")
    endif()
    add_subdirectory("${EHL_JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2 # JUCE 8.0.13
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(JUCE)
endif()

function(agentpluginhost_set_common_target_options target)
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_compile_definitions(${target} PUBLIC
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_AU=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        APH_VERSION="${PROJECT_VERSION}"
        APH_JUCE_REVISION="7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

file(GLOB_RECURSE AGENTPLUGINHOST_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/audio/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/midi/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/transport/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/plugins/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/capture/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/osc/*.cpp")

add_library(AgentPluginHostCore STATIC ${AGENTPLUGINHOST_CORE_SOURCES})
add_library(AgentPluginHost::Core ALIAS AgentPluginHostCore)
target_include_directories(AgentPluginHostCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_link_libraries(AgentPluginHostCore PUBLIC
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_osc)
agentpluginhost_set_common_target_options(AgentPluginHostCore)

if(AGENTPLUGINHOST_BUILD_APP)
    file(GLOB_RECURSE AGENTPLUGINHOST_APP_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/app/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/ui/*.cpp")

    juce_add_gui_app(AgentPluginHost
        PRODUCT_NAME "AgentPluginHost"
        COMPANY_NAME "EsionHsrahLatigid"
        BUNDLE_ID "jp.ehl.agentpluginhost"
        VERSION "${PROJECT_VERSION}")
    juce_generate_juce_header(AgentPluginHost)
    target_sources(AgentPluginHost PRIVATE ${AGENTPLUGINHOST_APP_SOURCES})
    target_include_directories(AgentPluginHost PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_link_libraries(AgentPluginHost PRIVATE
        AgentPluginHost::Core
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
    agentpluginhost_set_common_target_options(AgentPluginHost)

    juce_add_console_app(AgentPluginScanner
        PRODUCT_NAME "AgentPluginScanner"
        COMPANY_NAME "EsionHsrahLatigid")
    juce_generate_juce_header(AgentPluginScanner)
    target_sources(AgentPluginScanner PRIVATE Source/scanner/Main.cpp)
    target_include_directories(AgentPluginScanner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_link_libraries(AgentPluginScanner PRIVATE AgentPluginHost::Core juce::juce_gui_basics juce::juce_gui_extra)
    agentpluginhost_set_common_target_options(AgentPluginScanner)
endif()

if(AGENTPLUGINHOST_BUILD_TESTS)
    include(CTest)
    enable_testing()

    file(GLOB_RECURSE AGENTPLUGINHOST_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/unit/*.cpp")
    juce_add_console_app(AgentPluginHostTests PRODUCT_NAME "AgentPluginHostTests")
    juce_generate_juce_header(AgentPluginHostTests)
    target_sources(AgentPluginHostTests PRIVATE
        Tests/TestMain.cpp
        ${AGENTPLUGINHOST_TEST_SOURCES}
        Source/app/HostRuntime.cpp
        Source/ui/AgentPluginHostUI.cpp)
    target_include_directories(AgentPluginHostTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(AgentPluginHostTests PRIVATE APH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(AgentPluginHostTests PRIVATE AgentPluginHost::Core juce::juce_gui_basics juce::juce_gui_extra)
    agentpluginhost_set_common_target_options(AgentPluginHostTests)
    add_test(NAME AgentPluginHostTests COMMAND AgentPluginHostTests)

    if(APPLE)
        add_test(
            NAME AgentPluginTesterDownloader
            COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/Tests/downloader/test-posix.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/skills/use-agent-plugin-tester/scripts/agent-plugin-tester")
    elseif(WIN32)
        add_test(
            NAME AgentPluginTesterDownloader
            COMMAND powershell -NoLogo -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/Tests/downloader/test-windows.ps1"
                -Launcher "${CMAKE_CURRENT_SOURCE_DIR}/skills/use-agent-plugin-tester/scripts/agent-plugin-tester.ps1")
    endif()

    juce_add_plugin(TestGainVST3
        COMPANY_NAME "EsionHsrahLatigid"
        BUNDLE_ID "jp.ehl.agentpluginhost.testgain"
        PLUGIN_MANUFACTURER_CODE EHL_
        PLUGIN_CODE ApGn
        FORMATS VST3
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PRODUCT_NAME "APH Test Gain")
    target_sources(TestGainVST3 PRIVATE Tests/fixtures/TestGainPlugin.cpp)
    target_link_libraries(TestGainVST3 PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
    agentpluginhost_set_common_target_options(TestGainVST3)

    juce_add_plugin(TestSynthVST3
        COMPANY_NAME "EsionHsrahLatigid"
        BUNDLE_ID "jp.ehl.agentpluginhost.testsynth"
        PLUGIN_MANUFACTURER_CODE EHL_
        PLUGIN_CODE ApSy
        FORMATS VST3
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PRODUCT_NAME "APH Test Synth")
    target_sources(TestSynthVST3 PRIVATE Tests/fixtures/TestSynthPlugin.cpp)
    target_link_libraries(TestSynthVST3 PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
    agentpluginhost_set_common_target_options(TestSynthVST3)
endif()

if(AGENTPLUGINHOST_BUILD_APP AND AGENTPLUGINHOST_BUILD_TESTS)
    set(ehl_stage_platform "linux-x64")
    if(APPLE)
        set(ehl_stage_platform "macos-arm64")
    elseif(WIN32)
        set(ehl_stage_platform "windows-x64")
    endif()

    set(ehl_stage_dir "${AGENTPLUGINHOST_ARTIFACT_ROOT}/host-release/${ehl_stage_platform}")
    set(ehl_app_products "${CMAKE_CURRENT_BINARY_DIR}/AgentPluginHost_artefacts/$<CONFIG>")
    set(ehl_scanner_products "${CMAKE_CURRENT_BINARY_DIR}/AgentPluginScanner_artefacts/$<CONFIG>")
    set(ehl_gain_products "${CMAKE_CURRENT_BINARY_DIR}/TestGainVST3_artefacts/$<CONFIG>")
    set(ehl_synth_products "${CMAKE_CURRENT_BINARY_DIR}/TestSynthVST3_artefacts/$<CONFIG>")

    set(ehl_stage_commands
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_stage_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_stage_dir}/app"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_stage_dir}/scanner"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_stage_dir}/fixtures")

    if(APPLE)
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_app_products}/AgentPluginHost.app" "${ehl_stage_dir}/app/agent_plugin_host.app"
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_scanner_products}/AgentPluginScanner" "${ehl_stage_dir}/scanner/agent_plugin_scanner"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_gain_products}/VST3/APH Test Gain.vst3" "${ehl_stage_dir}/fixtures/aph_test_gain.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_synth_products}/VST3/APH Test Synth.vst3" "${ehl_stage_dir}/fixtures/aph_test_synth.vst3"
            COMMAND codesign --force --deep --sign - "${ehl_stage_dir}/app/agent_plugin_host.app"
            COMMAND codesign --force --deep --sign - "${ehl_stage_dir}/fixtures/aph_test_gain.vst3"
            COMMAND codesign --force --deep --sign - "${ehl_stage_dir}/fixtures/aph_test_synth.vst3")
    elseif(WIN32)
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_app_products}/AgentPluginHost.exe" "${ehl_stage_dir}/app/agent_plugin_host.exe"
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_scanner_products}/AgentPluginScanner.exe" "${ehl_stage_dir}/scanner/agent_plugin_scanner.exe"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_gain_products}/VST3/APH Test Gain.vst3" "${ehl_stage_dir}/fixtures/aph_test_gain.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_synth_products}/VST3/APH Test Synth.vst3" "${ehl_stage_dir}/fixtures/aph_test_synth.vst3")
    else()
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_app_products}/AgentPluginHost" "${ehl_stage_dir}/app/agent_plugin_host"
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_scanner_products}/AgentPluginScanner" "${ehl_stage_dir}/scanner/agent_plugin_scanner"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_gain_products}/VST3/APH Test Gain.vst3" "${ehl_stage_dir}/fixtures/aph_test_gain.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_synth_products}/VST3/APH Test Synth.vst3" "${ehl_stage_dir}/fixtures/aph_test_synth.vst3")
    endif()

    list(APPEND ehl_stage_commands
        COMMAND ${CMAKE_COMMAND}
            -DPRODUCT_NAME=AgentPluginHost
            -DBUNDLE_ID=jp.ehl.agentpluginhost
            -DSTAGE_DIR=${ehl_stage_dir}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteArtifactsManifest.cmake")

    add_custom_target(ehl_stage_products
        ${ehl_stage_commands}
        DEPENDS AgentPluginHost AgentPluginScanner TestGainVST3_VST3 TestSynthVST3_VST3
        COMMENT "Staging AgentPluginHost products under ${AGENTPLUGINHOST_ARTIFACT_ROOT}/host-release")

    if(AGENTPLUGINHOST_VERIFY_ARTIFACTS)
        add_test(
            NAME agent_plugin_host_artifact_contract
            COMMAND ${CMAKE_COMMAND}
                -DSTAGE_DIR=${ehl_stage_dir}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake")
    endif()
endif()
