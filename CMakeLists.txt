cmake_minimum_required(VERSION 3.24)

project(somavr LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(SOMAVR_BUILD_VERSION "0.35.0-dual-render-probe")

include(FetchContent)

FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.4
)

option(SOMAVR_ENABLE_OPENXR "Build and link the OpenXR loader for the VR bootstrap path." OFF)
set(SOMAVR_OPENXR_GIT_TAG "release-1.1.60" CACHE STRING "OpenXR-SDK release tag")
if(SOMAVR_ENABLE_OPENXR)
    set(SOMAVR_BUILD_OPENXR 1)
    set(SOMAVR_BUILD_FLAVOR "openxr")
else()
    set(SOMAVR_BUILD_OPENXR 0)
    set(SOMAVR_BUILD_FLAVOR "opengl")
endif()
configure_file(
    cmake/somavr_build_flavor.txt.in
    "${CMAKE_CURRENT_BINARY_DIR}/somavr_build_flavor.txt"
    @ONLY
)

if(SOMAVR_ENABLE_OPENXR)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
    set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
    set(DYNAMIC_LOADER ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        openxr
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
        GIT_TAG ${SOMAVR_OPENXR_GIT_TAG}
    )
endif()

if(SOMAVR_ENABLE_OPENXR)
    FetchContent_MakeAvailable(minhook openxr)
else()
    FetchContent_MakeAvailable(minhook)
endif()

add_library(somavr_common STATIC
    src/common/Config.cpp
    src/common/Logger.cpp
)
target_include_directories(somavr_common PUBLIC src/common)
target_compile_definitions(somavr_common PUBLIC SOMAVR_WORK_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(somavr_common PUBLIC shlwapi)

add_library(somavr_render_math STATIC
    src/dll/HPLCameraMath.cpp
    src/dll/HPLComfortMath.cpp
    src/dll/HPLHandsMath.cpp
    src/dll/HPLFlashlightMath.cpp
    src/dll/HPLGrabMath.cpp
    src/dll/HPLPhysicalCrouchMath.cpp
    src/dll/HPLHudMath.cpp
    src/dll/HPLInputMath.cpp
    src/dll/HPLMenuMath.cpp
    src/dll/HPLScreenEffectMath.cpp
    src/dll/HPLSubtitleMath.cpp
    src/dll/HPLDualRenderMath.cpp
    src/dll/OpenGLMatrixAnalysis.cpp
    src/dll/OpenXRSpectatorMath.cpp
    src/dll/OpenXRDepthMath.cpp
)
target_include_directories(somavr_render_math PUBLIC src/dll)
target_compile_definitions(somavr_render_math PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

add_library(somavr SHARED
    src/dll/DllMain.cpp
    src/dll/HPLCompatibilityProbe.cpp
    src/dll/HPLCameraBridge.cpp
    src/dll/HPLComfortBridge.cpp
    src/dll/HPLHudBridge.cpp
    src/dll/HPLLifecycle.cpp
    src/dll/HPLInputBridge.cpp
    src/dll/HPLMenuBridge.cpp
    src/dll/HPLPresentationBridge.cpp
    src/dll/HPLScreenEffectBridge.cpp
    src/dll/HPLSubtitleBridge.cpp
    src/dll/HPLNativeLocomotion.cpp
    src/dll/HPLInteractionBridge.cpp
    src/dll/HPLCrosshairBridge.cpp
    src/dll/HPLHandsBridge.cpp
    src/dll/HPLGrabBridge.cpp
    src/dll/HPLPlayerState.cpp
    src/dll/OpenGLHooks.cpp
    src/dll/OpenXRGLBridge.cpp
    src/dll/OpenXRRuntime.cpp
)
target_include_directories(somavr PRIVATE src/dll)
target_compile_definitions(somavr PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_definitions(somavr PRIVATE SOMAVR_BUILD_VERSION="${SOMAVR_BUILD_VERSION}")
target_link_libraries(somavr PRIVATE
    somavr_common
    somavr_render_math
    minhook
    opengl32
    gdi32
    user32
    shlwapi
)
add_custom_command(TARGET somavr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/somavr_build_flavor.txt"
        "$<TARGET_FILE_DIR:somavr>/somavr_build_flavor.txt"
    VERBATIM
)
add_custom_command(TARGET somavr POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSOMAVR_BINARY=$<TARGET_FILE:somavr>
        -DSOMAVR_MANIFEST=$<TARGET_FILE_DIR:somavr>/somavr_build_manifest.txt
        -DSOMAVR_VERSION=${SOMAVR_BUILD_VERSION}
        -DSOMAVR_FLAVOR=${SOMAVR_BUILD_FLAVOR}
        -DSOMAVR_OPENXR=${SOMAVR_BUILD_OPENXR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteBuildManifest.cmake"
    VERBATIM
)

add_library(somavr_injector_support STATIC
    src/injector/CompatibilityScan.cpp
)
target_include_directories(somavr_injector_support PUBLIC src/injector)
target_compile_definitions(somavr_injector_support PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

add_executable(somavr_injector src/injector/main.cpp)
target_link_libraries(somavr_injector PRIVATE somavr_common somavr_injector_support)

add_executable(somavr_dumper
    src/dumper/main.cpp
)
target_link_libraries(somavr_dumper PRIVATE somavr_common dbghelp)

if(BUILD_TESTING)
    add_executable(somavr_render_math_tests
        tests/RenderMathTests.cpp
    )
    target_link_libraries(somavr_render_math_tests PRIVATE somavr_render_math)
    add_test(NAME somavr_render_math COMMAND somavr_render_math_tests)

    add_executable(somavr_injector_compatibility_tests
        tests/InjectorCompatibilityTests.cpp
    )
    target_link_libraries(somavr_injector_compatibility_tests PRIVATE somavr_injector_support)
    add_test(NAME somavr_injector_compatibility COMMAND somavr_injector_compatibility_tests)

    add_test(
        NAME somavr_installer_lifecycle
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/InstallerLifecycleTests.ps1
            -Installer ${CMAKE_CURRENT_SOURCE_DIR}/scripts/Install-Or-Update-SOMAVR.ps1
            -Uninstaller ${CMAKE_CURRENT_SOURCE_DIR}/scripts/Uninstall-SOMAVR.ps1
    )
endif()

if(SOMAVR_ENABLE_OPENXR)
    target_sources(somavr PRIVATE
        src/dll/OpenXRHelpers.cpp
        src/dll/OpenXRInput.cpp
    )
    target_compile_definitions(somavr PRIVATE
        SOMAVR_ENABLE_OPENXR=1
        XR_USE_PLATFORM_WIN32
        XR_USE_GRAPHICS_API_OPENGL
    )
    target_link_libraries(somavr PRIVATE openxr_loader delayimp)
    target_link_options(somavr PRIVATE "/DELAYLOAD:$<TARGET_FILE_NAME:openxr_loader>")

    add_custom_command(TARGET somavr POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:openxr_loader>
            "$<TARGET_FILE_DIR:somavr>/$<TARGET_FILE_NAME:openxr_loader>"
        VERBATIM
    )
endif()
