# fez_model_generator: command-line converter (QCoreApplication + QCommandLineParser).
# Sources are listed explicitly (no file(GLOB)); add new files here.
# fez_cli holds the testable parts (option parsing, failure summary); fez_tests links it too.

set(FEZ_CLI_LIB_SOURCES
    Options.h
    Options.cpp
    Summary.h
    Summary.cpp
)

add_library(fez_cli STATIC ${FEZ_CLI_LIB_SOURCES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX cli FILES ${FEZ_CLI_LIB_SOURCES})

# Headers are included relative to src/cli (e.g. "Options.h").
target_include_directories(fez_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(fez_cli PUBLIC
    fez_core
    Qt6::Core
)

fez_set_project_warnings(fez_cli)

set(FEZ_CLI_SOURCES
    main.cpp
)

qt_add_executable(fez_model_generator ${FEZ_CLI_SOURCES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX cli FILES ${FEZ_CLI_SOURCES})

target_link_libraries(fez_model_generator PRIVATE
    fez_cli
    fez_core
    Qt6::Core
)

fez_set_project_warnings(fez_model_generator)

# F5 in Visual Studio runs the converter with FEZ_DEBUG_ARGS (cache variable, e.g. set in CMakeUserPresets.json).
set_target_properties(fez_model_generator PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:fez_model_generator>"
    VS_DEBUGGER_COMMAND_ARGUMENTS "${FEZ_DEBUG_ARGS}"
)

# --- Build tree ---------------------------------------------------------------
# Keep build/<preset>/bin/<Config>/fez_model_generator.exe runnable without Qt on PATH: vcpkg app-local deployment
# copies assimp and its dependencies; this copies the DLLs of the linked imported targets (Qt6Core, Qt6Gui, Qt6Xml,
# Qt6Concurrent, assimp). No Qt plugins are needed: the converter is a QCoreApplication and PNG support is built into
# Qt6Gui. Tests do not depend on this copy (they get Qt and vcpkg bin via DL_PATHS / ENVIRONMENT_MODIFICATION).
add_custom_command(TARGET fez_model_generator POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:fez_model_generator> "$<TARGET_FILE_DIR:fez_model_generator>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying runtime DLLs next to fez_model_generator"
    VERBATIM
)

# --- Install: cmake --install build/windows-vs --config Release --prefix <dir> -----------------------------------
# <dir>/bin holds a self-contained converter: the executable, the Qt runtime (windeployqt via Qt's deploy script),
# assimp and its vcpkg dependencies (runtime dependency set, resolved in the vcpkg bin directory of the installed
# configuration) and the MSVC runtime (InstallRequiredSystemLibraries).
include(GNUInstallDirs)

install(TARGETS fez_model_generator
    RUNTIME_DEPENDENCY_SET fez_runtime_dependencies
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Qt libraries are deployed by the Qt deploy script below; Windows API sets and system DLLs are never copied.
install(RUNTIME_DEPENDENCY_SET fez_runtime_dependencies
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    DIRECTORIES "${FEZ_VCPKG_BIN_DIR}"
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-" "^[Qq]t6"
    POST_EXCLUDE_REGEXES "[/\\\\][Ss]ystem32[/\\\\]" "[/\\\\][Ww]indows[/\\\\]"
)

qt_generate_deploy_app_script(
    TARGET fez_model_generator
    OUTPUT_SCRIPT FEZ_QT_DEPLOY_SCRIPT
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
)
install(SCRIPT "${FEZ_QT_DEPLOY_SCRIPT}")

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
include(InstallRequiredSystemLibraries)
