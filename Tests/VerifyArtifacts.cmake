if(NOT DEFINED STAGE_DIR)
    message(FATAL_ERROR "STAGE_DIR is required")
endif()

if(APPLE)
    set(host "${STAGE_DIR}/app/agent_plugin_host.app")
    set(scanner "${STAGE_DIR}/scanner/agent_plugin_scanner")
elseif(WIN32)
    set(host "${STAGE_DIR}/app/agent_plugin_host.exe")
    set(scanner "${STAGE_DIR}/scanner/agent_plugin_scanner.exe")
else()
    set(host "${STAGE_DIR}/app/agent_plugin_host")
    set(scanner "${STAGE_DIR}/scanner/agent_plugin_scanner")
endif()

set(gain "${STAGE_DIR}/fixtures/aph_test_gain.vst3")
set(synth "${STAGE_DIR}/fixtures/aph_test_synth.vst3")
set(manifest "${STAGE_DIR}/ARTIFACTS.txt")

foreach(path IN ITEMS "${host}" "${scanner}" "${gain}" "${synth}" "${manifest}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing staged artifact: ${path}")
    endif()
endforeach()

if(APPLE)
    if(NOT IS_SYMLINK "${scanner}")
        message(FATAL_ERROR "The staged macOS scanner compatibility path must be a symlink: ${scanner}")
    endif()
    file(READ_SYMLINK "${scanner}" scanner_target)
    if(NOT scanner_target STREQUAL "../app/agent_plugin_host.app/Contents/Helpers/agent_plugin_scanner")
        message(FATAL_ERROR "Unexpected staged macOS scanner target: ${scanner_target}")
    endif()

    set(gain_au "${STAGE_DIR}/fixtures/aph_test_gain.component")
    set(synth_au "${STAGE_DIR}/fixtures/aph_test_synth.component")
    foreach(path IN ITEMS "${gain_au}" "${synth_au}")
        if(NOT EXISTS "${path}")
            message(FATAL_ERROR "Missing staged artifact: ${path}")
        endif()
    endforeach()

    foreach(bundle IN ITEMS "${host}" "${gain}" "${synth}" "${gain_au}" "${synth_au}")
        execute_process(COMMAND codesign --verify --deep --strict "${bundle}"
                        RESULT_VARIABLE result ERROR_VARIABLE error)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Invalid ad-hoc signature for ${bundle}: ${error}")
        endif()
    endforeach()

    execute_process(COMMAND codesign --verify --strict "${scanner}"
                    RESULT_VARIABLE scanner_result ERROR_VARIABLE scanner_error)
    if(NOT scanner_result EQUAL 0)
        message(FATAL_ERROR "Invalid ad-hoc signature for ${scanner}: ${scanner_error}")
    endif()

    set(universal_binaries
        "${host}/Contents/MacOS/AgentPluginHost"
        "${scanner}"
        "${gain}/Contents/MacOS/APH Test Gain"
        "${synth}/Contents/MacOS/APH Test Synth"
        "${gain_au}/Contents/MacOS/APH Test Gain"
        "${synth_au}/Contents/MacOS/APH Test Synth")
    foreach(binary IN LISTS universal_binaries)
        execute_process(COMMAND lipo -archs "${binary}"
                        RESULT_VARIABLE lipo_result
                        OUTPUT_VARIABLE binary_architectures
                        ERROR_VARIABLE lipo_error
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT lipo_result EQUAL 0)
            message(FATAL_ERROR "Could not inspect architectures for ${binary}: ${lipo_error}")
        endif()
        string(REPLACE " " ";" binary_architectures "${binary_architectures}")
        list(SORT binary_architectures)
        if(NOT binary_architectures STREQUAL "arm64;x86_64")
            message(FATAL_ERROR "Expected universal 2 binary at ${binary}; got ${binary_architectures}")
        endif()
    endforeach()
endif()
