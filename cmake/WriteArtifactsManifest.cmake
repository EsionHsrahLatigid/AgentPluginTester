if(NOT DEFINED STAGE_DIR)
    message(FATAL_ERROR "STAGE_DIR is required")
endif()

set(audio_unit_artifacts "")
if(INCLUDE_AUDIO_UNIT)
    string(APPEND audio_unit_artifacts
"- fixtures/aph_test_gain.component
- fixtures/aph_test_synth.component
")
endif()

file(WRITE "${STAGE_DIR}/ARTIFACTS.txt"
"Product: ${PRODUCT_NAME}
Bundle ID: ${BUNDLE_ID}
Staged artifact contract:
- app/agent_plugin_host.app or .exe
- scanner/agent_plugin_scanner or .exe
- fixtures/aph_test_gain.vst3
- fixtures/aph_test_synth.vst3
${audio_unit_artifacts}
")
