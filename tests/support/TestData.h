#pragma once

#include <QtCore/QDir>
#include <QtCore/QString>

#ifndef FEZ_TEST_DATA_DIR
#error "FEZ_TEST_DATA_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

namespace fez::testing
{
    // Absolute path of the committed fixture root (tests/data), injected by CMake.
    inline QString testDataDir()
    {
        return QDir::cleanPath(QStringLiteral(FEZ_TEST_DATA_DIR));
    }

    // Absolute path of a fixture below tests/data, e.g. testDataPath("tree_compare/sample").
    inline QString testDataPath(const QString& relativePath)
    {
        return QDir::cleanPath(testDataDir() + QLatin1Char('/') + relativePath);
    }
} // namespace fez::testing
