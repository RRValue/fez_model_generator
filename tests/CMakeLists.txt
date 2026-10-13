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
    unit/ModelHeaderTests.cpp
    unit/XmlTests.cpp
    unit/FaceDecodingTests.cpp
    unit/TexturedQuadTests.cpp
    unit/ImageTests.cpp
    unit/GeometryParserTests.cpp
    unit/model_headers/ArtObjectHeader.cpp
    unit/model_headers/BackgroundPlaneHeader.cpp
    unit/model_headers/CharacterHeader.cpp
    unit/model_headers/GeometryHeader.cpp
    unit/model_headers/LevelHeader.cpp
    unit/model_headers/TrileEmplacementHeader.cpp
    unit/model_headers/VertexHeader.cpp
    e2e/MiniExportTests.cpp
    golden/GoldenTests.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX tests
    FILES support/main.cpp support/TestData.h support/TreeCompare.h support/TreeCompare.cpp
          unit/SupportTests.cpp unit/TreeCompareTests.cpp unit/ModelHeaderTests.cpp unit/XmlTests.cpp
          unit/FaceDecodingTests.cpp unit/TexturedQuadTests.cpp unit/ImageTests.cpp unit/GeometryParserTests.cpp
          unit/model_headers/ArtObjectHeader.cpp unit/model_headers/BackgroundPlaneHeader.cpp
          unit/model_headers/CharacterHeader.cpp unit/model_headers/GeometryHeader.cpp
          unit/model_headers/LevelHeader.cpp unit/model_headers/TrileEmplacementHeader.cpp
          unit/model_headers/VertexHeader.cpp
          e2e/MiniExportTests.cpp golden/GoldenTests.cpp)

target_include_directories(fez_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(fez_tests PRIVATE
    fez_core
    Catch2::Catch2
    Qt6::Core
)

# Fixture root: tests never depend on the working directory.
target_compile_definitions(fez_tests PRIVATE FEZ_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

# The mini end-to-end test and the golden test run the converter of the same configuration; building fez_tests builds it too.
target_compile_definitions(fez_tests PRIVATE FEZ_MODEL_GENERATOR_EXE="$<TARGET_FILE:fez_model_generator>")
add_dependencies(fez_tests fez_model_generator)

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

# Golden regression test (tests/golden/GoldenTests.cpp). The Catch2 test is hidden ("[.golden]"), so
# catch_discover_tests does not register it; it gets its own CTest entry with label "golden". It reads FEZ_DATA_DIR
# and FEZ_GOLDEN_DIR from the environment (set by the test presets in CMakeUserPresets.json) and SKIPs (Catch2 exit
# code 4) when they are unset. The default test presets exclude label "golden"; run it with
# ctest --preset windows-debug-golden (or windows-release-golden, ~9 min instead of ~1 h).
# Because it is registered by hand it does not get DL_PATHS, so Qt and vcpkg bin are prepended to PATH for fez_tests
# and the converter child process. A full Debug conversion takes about an hour, hence the large TIMEOUT.
add_test(NAME fez_golden COMMAND fez_tests "[golden]")
set_tests_properties(fez_golden PROPERTIES
    LABELS golden
    SKIP_RETURN_CODE 4
    TIMEOUT 7200
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${FEZ_VCPKG_BIN_DIR};PATH=path_list_prepend:${FEZ_QT_BIN_DIR}"
)
