if(NOT DEFINED SOMAVR_BINARY OR NOT EXISTS "${SOMAVR_BINARY}")
    message(FATAL_ERROR "SOMAVR_BINARY must name an existing build artifact")
endif()

file(SHA256 "${SOMAVR_BINARY}" SOMAVR_SHA256)
get_filename_component(SOMAVR_BINARY_NAME "${SOMAVR_BINARY}" NAME)
file(WRITE "${SOMAVR_MANIFEST}"
    "version=${SOMAVR_VERSION}\n"
    "flavor=${SOMAVR_FLAVOR}\n"
    "openxr=${SOMAVR_OPENXR}\n"
    "artifact=${SOMAVR_BINARY_NAME}\n"
    "sha256=${SOMAVR_SHA256}\n")
