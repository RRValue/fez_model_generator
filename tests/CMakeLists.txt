# fez_tests: Catch2 v3 test executable. It has its own main (support/main.cpp)
# that constructs a QCoreApplication before running Catch::Session, so it links
# Catch2::Catch2 instead of Catch2::Catch2WithMain.

add_executable(fez_tests
    support/main.cpp
    support/TestData.h
    support/TreeCompare.h
    support/TreeCompare.cpp
    unit/SupportTests.cpp
    unit/TreeCompareTests.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX tests
    FILES support/main.cpp support/TestData.h support/TreeCompare.h support/TreeCompare.cpp
          unit/SupportTests.cpp unit/TreeCompareTests.cpp)

target_include_directories(fez_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(fez_tests PRIVATE
    Catch2::Catch2
    Qt6::Core
)

# Fixture root: tests never depend on the working directory.
target_compile_definitions(fez_tests PRIVATE FEZ_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

fez_set_project_warnings(fez_tests)

set_target_properties(fez_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:fez_tests>"
    VS_DEBUGGER_ENVIRONMENT "PATH=${FEZ_QT_BIN_DIR};${FEZ_VCPKG_BIN_DIR};%PATH%"
)

# PRE_TEST discovery keeps one test list per configuration (multi-config
# generator) and runs the executable with DL_PATHS on PATH; the discovered tests
# get the same directories prepended via ENVIRONMENT_MODIFICATION.
include(Catch)
catch_discover_tests(fez_tests
    DISCOVERY_MODE PRE_TEST
    DL_PATHS "${FEZ_QT_BIN_DIR}" "${FEZ_VCPKG_BIN_DIR}"
)
