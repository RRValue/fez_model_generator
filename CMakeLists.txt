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

# --- Executable ---------------------------------------------------------------
set(PROJECT_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source)

file(GLOB_RECURSE PROGRAMMFILES CONFIGURE_DEPENDS ${PROJECT_SRC_PATH}/*.cpp ${PROJECT_SRC_PATH}/*.h)

qt_add_executable(FezModelGenerator ${PROGRAMMFILES})

source_group(TREE ${PROJECT_SRC_PATH} PREFIX source FILES ${PROGRAMMFILES})

target_include_directories(FezModelGenerator PRIVATE ${PROJECT_SRC_PATH})

target_link_libraries(FezModelGenerator PRIVATE
    assimp::assimp
    Eigen3::Eigen
    Qt6::Core
    Qt6::Gui
    Qt6::Xml
    Qt6::Concurrent
)

fez_set_project_warnings(FezModelGenerator)

set_target_properties(FezModelGenerator PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:FezModelGenerator>"
)

# Make the build-tree executable runnable: assimp DLLs are copied by vcpkg
# app-local deployment; Qt DLLs and plugins are deployed into the output
# directory (never into the source tree). Install-time deployment follows later.
if(WIN32)
    find_program(FEZ_WINDEPLOYQT windeployqt HINTS "${FEZ_QT_BIN_DIR}" NO_DEFAULT_PATH)
    if(NOT FEZ_WINDEPLOYQT)
        message(FATAL_ERROR "windeployqt not found next to qmake in '${FEZ_QT_BIN_DIR}'.")
    endif()

    add_custom_command(TARGET FezModelGenerator POST_BUILD
        COMMAND "${FEZ_WINDEPLOYQT}"
                $<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>
                --no-translations --no-compiler-runtime --verbose 0
                "$<TARGET_FILE:FezModelGenerator>"
        COMMENT "Deploying Qt runtime next to FezModelGenerator"
        VERBATIM
    )
endif()

# --- Tests --------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

qt_finalize_project()
