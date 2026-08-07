if(NOT DEFINED SOMAVR_SOURCE_DIR OR NOT DEFINED SOMAVR_OUTPUT)
    message(FATAL_ERROR "GenerateBuildInfo.cmake requires SOMAVR_SOURCE_DIR and SOMAVR_OUTPUT")
endif()

find_program(SOMAVR_GIT_EXECUTABLE git)
set(SOMAVR_GIT_DESCRIBE "unavailable")
set(SOMAVR_GIT_COMMIT "unavailable")
if(SOMAVR_GIT_EXECUTABLE)
    execute_process(
        COMMAND "${SOMAVR_GIT_EXECUTABLE}"
            -c "safe.directory=${SOMAVR_SOURCE_DIR}"
            -C "${SOMAVR_SOURCE_DIR}"
            describe --tags --always --dirty
        RESULT_VARIABLE describe_result
        OUTPUT_VARIABLE describe_output
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(describe_result EQUAL 0 AND NOT describe_output STREQUAL "")
        set(SOMAVR_GIT_DESCRIBE "${describe_output}")
    endif()

    execute_process(
        COMMAND "${SOMAVR_GIT_EXECUTABLE}"
            -c "safe.directory=${SOMAVR_SOURCE_DIR}"
            -C "${SOMAVR_SOURCE_DIR}"
            rev-parse HEAD
        RESULT_VARIABLE commit_result
        OUTPUT_VARIABLE commit_output
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(commit_result EQUAL 0 AND NOT commit_output STREQUAL "")
        set(SOMAVR_GIT_COMMIT "${commit_output}")
    endif()
endif()

string(FIND "${SOMAVR_GIT_DESCRIBE}" "-dirty" dirty_index)
if(dirty_index EQUAL -1)
    set(SOMAVR_GIT_DIRTY 0)
else()
    set(SOMAVR_GIT_DIRTY 1)
endif()
string(TIMESTAMP SOMAVR_BUILD_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)

foreach(value_name
    SOMAVR_VERSION
    SOMAVR_FLAVOR
    SOMAVR_CONFIGURATION
    SOMAVR_GIT_DESCRIBE
    SOMAVR_GIT_COMMIT
    SOMAVR_BUILD_UTC)
    string(REPLACE "\\" "\\\\" ${value_name} "${${value_name}}")
    string(REPLACE "\"" "\\\"" ${value_name} "${${value_name}}")
endforeach()

get_filename_component(output_dir "${SOMAVR_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")
set(temp_output "${SOMAVR_OUTPUT}.tmp")
file(WRITE "${temp_output}"
"#pragma once

#define SOMAVR_GENERATED_VERSION \"${SOMAVR_VERSION}\"
#define SOMAVR_GENERATED_FLAVOR \"${SOMAVR_FLAVOR}\"
#define SOMAVR_GENERATED_CONFIGURATION \"${SOMAVR_CONFIGURATION}\"
#define SOMAVR_GENERATED_OPENXR ${SOMAVR_OPENXR}
#define SOMAVR_GENERATED_GIT_DESCRIBE \"${SOMAVR_GIT_DESCRIBE}\"
#define SOMAVR_GENERATED_GIT_COMMIT \"${SOMAVR_GIT_COMMIT}\"
#define SOMAVR_GENERATED_GIT_DIRTY ${SOMAVR_GIT_DIRTY}
#define SOMAVR_GENERATED_BUILD_UTC \"${SOMAVR_BUILD_UTC}\"
")
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${temp_output}" "${SOMAVR_OUTPUT}")
file(REMOVE "${temp_output}")
