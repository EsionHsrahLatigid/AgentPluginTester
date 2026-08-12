if(NOT DEFINED STAGE_DIR)
    message(FATAL_ERROR "STAGE_DIR is required")
endif()

file(WRITE "${STAGE_DIR}/ARTIFACTS.txt"
"Product: ${PRODUCT_NAME}
Bundle ID: ${BUNDLE_ID}
Staged artifact contract:
- app/agent_plugin_host.app or .exe
- scanner/agent_plugin_scanner or .exe
- fixtures/aph_test_gain.vst3
- fixtures/aph_test_synth.vst3
")
