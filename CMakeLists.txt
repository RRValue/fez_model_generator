cmake_minimum_required(VERSION 3.25)

# --- vcpkg (classic mode) -----------------------------------------------------
# The vcpkg root is supplied through the cache variable VCPKG_FEZ_MODEL_BASE
# (CMakeUserPresets.json or -DVCPKG_FEZ_MODEL_BASE=<path>). It must be resolved
# before project() so the toolchain file takes effect.
set(VCPKG_FEZ_MODEL_BASE "" CACHE PATH "Root of the vcpkg installation providing assimp, eigen3 and catch2")

if(NOT VCPKG_FEZ_MODEL_BASE)
    message(FATAL_ERROR
        "VCPKG_FEZ_MODEL_BASE is not set. Set it to your vcpkg root in CMakeUserPresets.json "
        "(see CMakeUserPresets.example.json) or pass -DVCPKG_FEZ_MODEL_BASE=<path>.")
endif()

cmake_path(CONVERT "${VCPKG_FEZ_MODEL_BASE}" TO_CMAKE_PATH_LIST _fez_vcpkg_root NORMALIZE)
set(_fez_vcpkg_toolchain "${_fez_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")

if(NOT EXISTS "${_fez_vcpkg_toolchain}")
    message(FATAL_ERROR
        "VCPKG_FEZ_MODEL_BASE='${VCPKG_FEZ_MODEL_BASE}' is not a vcpkg root: "
        "'${_fez_vcpkg_toolchain}' does not exist.")
endif()

set(VCPKG_ROOT "${_fez_vcpkg_root}")
set(ENV{VCPKG_ROOT} "${_fez_vcpkg_root}")
set(CMAKE_TOOLCHAIN_FILE "${_fez_vcpkg_toolchain}" CACHE FILEPATH "vcpkg toolchain file" FORCE)

project(FezModelGenerator LANGUAGES CXX)

# --- Compiler -----------------------------------------------------------------
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake 4.4 knows no C++26 flag for MSVC (its newest is CXX23 -> /std:c++latest).
# Map C++26 to /std:c++latest as well, and forward the mapping into try_compile
# projects (e.g. FindThreads run by Qt6Config), which reload the compiler rules.
if(MSVC AND NOT DEFINED CMAKE_CXX26_STANDARD_COMPILE_OPTION)
    set(CMAKE_CXX26_STANDARD_COMPILE_OPTION "-std:c++latest")
    set(CMAKE_CXX26_EXTENSION_COMPILE_OPTION "-std:c++latest")
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        CMAKE_CXX26_STANDARD_COMPILE_OPTION
        CMAKE_CXX26_EXTENSION_COMPILE_OPTION)
endif()

include(cmake/CompilerWarnings.cmake)

# --- Output layout: build/<preset>/bin/<Config>/ ------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Dependencies -------------------------------------------------------------
find_package(assimp CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Xml Concurrent)
find_package(Catch2 3 CONFIG REQUIRED)

qt_standard_project_setup()

# Runtime DLL directories, used to run tests without Qt or vcpkg on PATH.
get_target_property(_fez_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(FEZ_QT_BIN_DIR "${_fez_qmake}" DIRECTORY)
set(FEZ_VCPKG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin")

# --- Options ------------------------------------------------------------------
set(FEZ_DEBUG_ARGS "" CACHE STRING
    "Command-line arguments for running fez_model_generator from the Visual Studio debugger (F5), e.g. --input <export> --output <fresh dir>")

# --- Targets: fez_core (src/core), fez_model_generator (src/cli) ---------------
add_subdirectory(src)

# --- Tests --------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

qt_finalize_project()
