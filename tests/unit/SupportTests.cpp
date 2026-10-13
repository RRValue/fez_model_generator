#include "support/TestData.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("fez_tests runs inside a live QCoreApplication", "[support]")
{
    REQUIRE(QCoreApplication::instance() != nullptr);
    CHECK_FALSE(QCoreApplication::applicationFilePath().isEmpty());
}

TEST_CASE("FEZ_TEST_DATA_DIR points at the committed fixture root", "[support]")
{
    const QFileInfo root(fez::testing::testDataDir());
    CHECK(root.isAbsolute());
    CHECK(root.isDir());
    CHECK(QFileInfo(fez::testing::testDataPath("tree_compare/sample/level.mtl")).isFile());
}
