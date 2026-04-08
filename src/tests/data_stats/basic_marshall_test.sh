#!/bin/bash

# --- Configuration ---
TEST_NAME="MarshallMultiFileDirectLinksTest" # Updated test name
NIDAS_INSTALL_DIR=~/nidas
ISFS_BASE=/net/isf/isfs
PROJECT_NAME="MARSHALL2023"
SAMPLE_PLAN="noqc_geo"

# Pick a specific .dat file from the project for this test
# Replace with an actual, specific filename you've identified from:
# ls -lhtr /scr/isfs/projects/${PROJECT_NAME}/raw_data/ttt*
TEST_DAT_FILE="/scr/isfs/projects/${PROJECT_NAME}/raw_data/ttt_20250527_120000.dat" 
# ^^^ UPDATE THIS FILENAME TO AN ACTUAL EXISTING ONE IF NEEDED ^^^

DSM_FILTER_ID="1,-1" # Filter for DSM ID 1 (e.g., ttt), adjust if needed

OUTPUT_BASE_DIR="./test_output" 
# Create a unique directory for this test run's output
OUTPUT_DIR_NAME="${OUTPUT_BASE_DIR}/${TEST_NAME}_$(date +%Y%m%d_%H%M%S)"
LOG_FILE="${OUTPUT_DIR_NAME}/${TEST_NAME}.log"


cleanup_previous_run() {
    # This function might not be strictly necessary if OUTPUT_DIR_NAME is always unique,
    # but it's good practice if you might re-run with a fixed OUTPUT_DIR_NAME for debugging.
    # For now, with unique names, we might not need to call it.
    if [ -d "${OUTPUT_DIR_NAME}" ]; then
        echo "INFO: Cleaning up previous output directory: ${OUTPUT_DIR_NAME}"
        rm -rf "${OUTPUT_DIR_NAME}"
    fi
}

setup_nidas_environment() {
    echo "INFO: Setting up NIDAS environment for ${PROJECT_NAME}..."
    # Ensure this script is run within a bash shell or subshell
    if [ -z "$BASH_VERSION" ]; then
        echo "ERROR: This script and NIDAS environment setup expect to be run in bash."
        echo "       Please run 'bash' first, then execute this script."
        exit 1
    fi

    export ISFS="${ISFS_BASE}"
    echo "INFO: Sourcing ISFS functions and selecting project..."
    source "${ISFS}/scripts/isfs_functions.sh"
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to source isfs_functions.sh. Path: ${ISFS}/scripts/isfs_functions.sh"
        exit 1
    fi

    sp "${PROJECT_NAME}" "${SAMPLE_PLAN}"
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to set up project environment with 'sp ${PROJECT_NAME} ${SAMPLE_PLAN}'. Exiting."
        exit 1
    fi

    echo "INFO: Configuring PATH and LD_LIBRARY_PATH for local NIDAS install at ${NIDAS_INSTALL_DIR}..."
    export PATH="${NIDAS_INSTALL_DIR}/bin:${PATH}"
    export LD_LIBRARY_PATH="${NIDAS_INSTALL_DIR}/lib:${LD_LIBRARY_PATH}"
    hash -r # Clear command cache

    local data_stats_path=$(which data_stats)
    echo "INFO: Using data_stats from: ${data_stats_path}"
    if [ "${data_stats_path}" != "${NIDAS_INSTALL_DIR}/bin/data_stats" ]; then
        echo "WARNING: Not using the locally installed data_stats from ${NIDAS_INSTALL_DIR}/bin."
        echo "         Current data_stats resolves to: ${data_stats_path}"
        echo "         Current PATH: $PATH"
        echo "         Please ensure your local NIDAS installation is correctly in PATH and scons install was successful."
        # Consider making this a fatal error: 
        # echo "ERROR: Halting test."
        # exit 1
    fi
}

run_data_stats_test() {
    echo "INFO: Running ${TEST_NAME} with input ${TEST_DAT_FILE}..."
    # Ensure output directory for logs and JSON exists
    mkdir -p "${OUTPUT_DIR_NAME}" 
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to create output directory ${OUTPUT_DIR_NAME}. Exiting."
        exit 1
    fi

    echo "INFO: Executing data_stats. Outputting to ${OUTPUT_DIR_NAME}"
    # Using absolute path to the locally installed data_stats for robustness
    "${NIDAS_INSTALL_DIR}/bin/data_stats" \
        -p \
        -a \
        -D \
        --json-output-dir "${OUTPUT_DIR_NAME}" \
        -i "${DSM_FILTER_ID}" \
        -P 15 -n 1 \
        "${TEST_DAT_FILE}" \
        > "${LOG_FILE}" 2>&1 # Capture stdout and stderr

    local exit_code=$?
    if [ ${exit_code} -eq 0 ]; then
        echo "INFO: data_stats command completed successfully."
    else
        echo "ERROR: data_stats command failed with exit code ${exit_code}."
        echo "--- Log File Content (${LOG_FILE}) ---"
        cat "${LOG_FILE}"
        echo "--- End Log File Content ---"
        exit 1
    fi
}

verify_output_structure() {
    echo "INFO: Verifying output structure for ${TEST_NAME} in ${OUTPUT_DIR_NAME}..."
    local test_passed=true

    # Check for top-level files
    if [ ! -f "${OUTPUT_DIR_NAME}/manifest.json" ]; then
        echo "ERROR: manifest.json not found!"
        test_passed=false
    fi
    if [ ! -f "${OUTPUT_DIR_NAME}/problems.json" ]; then
        echo "ERROR: problems.json not found!"
        test_passed=false
    fi

    # Check for subdirectories
    # Note: "data_values" is included because we use -D in the run_data_stats_test
    for subdir in metadata statistics data_values; do 
        if [ ! -d "${OUTPUT_DIR_NAME}/${subdir}" ]; then
            echo "ERROR: ${subdir}/ directory not found!"
            test_passed=false
        fi
    done

    # --- VERIFYING NEW MANIFEST STRUCTURE ---
    if command -v jq &> /dev/null && [ -f "${OUTPUT_DIR_NAME}/manifest.json" ]; then
        echo "INFO: Performing detailed manifest.json checks with jq..."

        # 1. Check for the top-level "streams" object and that it's not empty if streams are expected
        #    (Adjust the key ".streams" if you named it differently, e.g., ".streams_info")
        if ! jq -e '.streams | (type == "object" and (keys_unsorted | length) > 0)' "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null; then
            if jq -e '.streams | (type == "object" and (keys_unsorted | length) == 0)' "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null; then
                echo "WARNING: manifest.json .streams object is present but empty (no streams reported for filter ${DSM_FILTER_ID})."
                # This could be okay if the filter yields no streams, but often indicates an issue or unexpected filter.
            else
                echo "ERROR: manifest.json does not have a non-empty '.streams' object, or it's not an object."
                test_passed=false
            fi
        else
            local stream_count=$(jq '.streams | keys_unsorted | length' "${OUTPUT_DIR_NAME}/manifest.json")
            echo "INFO: Found ${stream_count} stream(s) under manifest.json .streams object."
        fi

        # 2. Check for new processing_info field names
        #    (Timestamps here are still expected to be default ISO8601 strings,
        #     as configurable timestamps for manifest fields are a separate later step)
        if ! jq -e '.processing_info.timeperiod' "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null ; then
             echo "ERROR: manifest.json .processing_info missing 'timeperiod'."
             test_passed=false
        fi
        if ! jq -e '.processing_info.starttime' "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null ; then
             echo "ERROR: manifest.json .processing_info missing 'starttime'."
             test_passed=false
        fi
        if ! jq -e '.processing_info.period' "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null ; then
             echo "ERROR: manifest.json .processing_info missing 'period'."
             test_passed=false
        fi
        # Adjust this key if you used "update" instead of "update_interval" to match original schema exactly
        if ! jq -e '.processing_info.update_interval' "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null ; then 
             echo "ERROR: manifest.json .processing_info missing 'update_interval'. (Did you use 'update' instead?)"
             test_passed=false
        fi

        # 3. Check that "component_references" object is GONE
        if jq -e '.component_references' "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null ; then
             echo "ERROR: manifest.json still contains 'component_references'. It should be removed."
             test_passed=false
        fi
        
        # 4. Check link structure within the first stream object under .streams
        local first_stream_key=$(jq -r '.streams | keys_unsorted | .[0]' "${OUTPUT_DIR_NAME}/manifest.json")
        if [ "$first_stream_key" != "null" ] && [ -n "$first_stream_key" ]; then
            echo "INFO: Checking link structure for first stream: $first_stream_key"
            if ! jq -e --arg sk "$first_stream_key" \
                '.streams[$sk] | .metadata_file and .statistics_file and .data_values_file' \
                "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null; then
                echo "ERROR: First stream object in manifest.json .streams ('$first_stream_key') does not have expected link keys (metadata_file, statistics_file, data_values_file)."
                echo "DEBUG: Content of .streams['$first_stream_key']:"
                jq --arg sk "$first_stream_key" '.streams[$sk]' "${OUTPUT_DIR_NAME}/manifest.json"
                test_passed=false
            else
                echo "INFO: Link structure for first stream object ('$first_stream_key') looks OK."
                # Verify the actual paths for the first stream
                jq -r --arg sk "$first_stream_key" '.streams[$sk] | .metadata_file, .statistics_file, .data_values_file' "${OUTPUT_DIR_NAME}/manifest.json" | while read -r file_path; do
                    if [ ! -f "${OUTPUT_DIR_NAME}/${file_path}" ]; then
                        echo "ERROR: Linked file does not exist: ${OUTPUT_DIR_NAME}/${file_path} (linked from manifest for stream $first_stream_key)"
                        test_passed=false
                    fi
                done

            fi
        elif jq -e '.streams | (keys_unsorted | length) > 0' "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null; then
            echo "WARNING: Could not extract first stream key for detailed link check, but .streams object has entries."
        fi
    else
        echo "WARNING: jq not found or manifest.json missing. Skipping detailed manifest content checks."
        test_passed=false # Consider this a failure if jq is expected
    fi
    
    # Check if subdirectories for streams have any .json files
    for subdir in metadata statistics data_values; do
        if [ -d "${OUTPUT_DIR_NAME}/${subdir}" ]; then
            local file_count=$(find "${OUTPUT_DIR_NAME}/${subdir}" -maxdepth 1 -name '*.json' -type f -print | wc -l)
            if [ "${file_count}" -eq 0 ] && jq -e '.streams | (keys_unsorted | length) > 0' "${OUTPUT_DIR_NAME}/manifest.json" > /dev/null ; then
                # Only warn if manifest actually listed streams but subdir is empty
                echo "WARNING: ${subdir}/ directory is empty but manifest lists streams. This might indicate an issue."
            elif [ "${file_count}" -gt 0 ]; then
                echo "INFO: ${subdir}/ contains ${file_count} JSON file(s)."
            fi
        fi
    done

    if $test_passed; then
        echo "INFO: VERIFICATION PASSED for ${TEST_NAME}."
    else
        echo "ERROR: VERIFICATION FAILED for ${TEST_NAME}. Check errors above and log file: ${LOG_FILE}"
        echo "To debug, inspect:"
        echo "  Log file: ${LOG_FILE}"
        echo "  Manifest: ${OUTPUT_DIR_NAME}/manifest.json"
        exit 1
    fi
}

# --- Main Execution ---
# cleanup_previous_run # Not strictly needed if OUTPUT_DIR_NAME is always unique due to date
setup_nidas_environment
run_data_stats_test
verify_output_structure

echo "INFO: ${TEST_NAME} test script completed successfully."