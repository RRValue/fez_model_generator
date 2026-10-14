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

# Make the build-tree executable runnable: assimp DLLs are copied by vcpkg
# app-local deployment; Qt DLLs and plugins are deployed into the output
# directory (never into the source tree). Install-time deployment follows later.
if(WIN32)
    find_program(FEZ_WINDEPLOYQT windeployqt HINTS "${FEZ_QT_BIN_DIR}" NO_DEFAULT_PATH)
    if(NOT FEZ_WINDEPLOYQT)
        message(FATAL_ERROR "windeployqt not found next to qmake in '${FEZ_QT_BIN_DIR}'.")
    endif()

    add_custom_command(TARGET fez_model_generator POST_BUILD
        COMMAND "${FEZ_WINDEPLOYQT}"
                $<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>
                --no-translations --no-compiler-runtime --verbose 0
                "$<TARGET_FILE:fez_model_generator>"
        COMMENT "Deploying Qt runtime next to fez_model_generator"
        VERBATIM
    )
endif()
