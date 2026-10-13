#include "support/TestData.h"
#include "support/TreeCompare.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using fez::testing::compareTrees;
using fez::testing::TreeComparison;

namespace
{
    void writeFile(const QString& root, const QString& relativePath, const QByteArray& content)
    {
        const QString path = root + QLatin1Char('/') + relativePath;
        REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        REQUIRE(file.write(content) == content.size());
    }

    void copyTree(const QString& from, const QString& to)
    {
        QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            const QString source = it.next();
            const QString target = to + QLatin1Char('/') + QDir(from).relativeFilePath(source);
            REQUIRE(QDir().mkpath(QFileInfo(target).absolutePath()));
            REQUIRE(QFile::copy(source, target));
        }
    }

    const QByteArray kObj = "# model\n"
                            "mtllib model.mtl\n"
                            "v 0.0 0.0 0.0\n"
                            "v 1.0 0.0 0.0\n"
                            "v 0.0 1.0 0.0\n"
                            "f 1 2 3\n";

    const QByteArray kMtl = "newmtl material_0\r\n"
                            "Kd 1.000000 1.000000 1.000000\r\n"
                            "map_Kd tex.png\r\n";

    const QByteArray kPng = QByteArray::fromHex("89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c489");

    // A small tree with nested directories and text and binary files.
    void writeSampleTree(const QString& root)
    {
        writeFile(root, "level/model.obj", kObj);
        writeFile(root, "level/model.mtl", kMtl);
        writeFile(root, "level/textures/tex.png", kPng);
        writeFile(root, "top.txt", "top level\n");
    }

    struct Trees
    {
        QTemporaryDir expected;
        QTemporaryDir actual;

        Trees()
        {
            REQUIRE(expected.isValid());
            REQUIRE(actual.isValid());
            writeSampleTree(expected.path());
            writeSampleTree(actual.path());
        }
    };
} // namespace

TEST_CASE("compareTrees reports identical trees as identical", "[TreeCompare]")
{
    Trees trees;

    const TreeComparison result = compareTrees(trees.expected.path(), trees.actual.path());

    CHECK(result.missing.isEmpty());
    CHECK(result.extra.isEmpty());
    CHECK(result.differing.empty());
    CHECK(result.identical());
    CHECK(result.describe().isEmpty());
}

TEST_CASE("compareTrees reports a file missing from the actual tree", "[TreeCompare]")
{
    Trees trees;
    REQUIRE(QFile::remove(trees.actual.path() + "/level/textures/tex.png"));

    const TreeComparison result = compareTrees(trees.expected.path(), trees.actual.path());

    CHECK(result.missing == QStringList{"level/textures/tex.png"});
    CHECK(result.extra.isEmpty());
    CHECK(result.differing.empty());
    CHECK_FALSE(result.identical());
    CHECK(result.describe().contains("missing"));
    CHECK(result.describe().contains("level/textures/tex.png"));
}

TEST_CASE("compareTrees reports a file only in the actual tree as extra", "[TreeCompare]")
{
    Trees trees;
    writeFile(trees.actual.path(), "level/stale.obj", "v 0 0 0\n");
    writeFile(trees.actual.path(), "new_dir/b.txt", "b\n");

    const TreeComparison result = compareTrees(trees.expected.path(), trees.actual.path());

    CHECK(result.missing.isEmpty());
    CHECK(result.extra == QStringList{"level/stale.obj", "new_dir/b.txt"});
    CHECK(result.differing.empty());
    CHECK_FALSE(result.identical());
    CHECK(result.describe().contains("extra"));
    CHECK(result.describe().contains("new_dir/b.txt"));
}

TEST_CASE("compareTrees reports a binary difference without a line number", "[TreeCompare]")
{
    Trees trees;
    QByteArray changed = kPng;
    changed[20] = '\x7f';
    writeFile(trees.actual.path(), "level/textures/tex.png", changed);

    const TreeComparison result = compareTrees(trees.expected.path(), trees.actual.path());

    CHECK(result.missing.isEmpty());
    CHECK(result.extra.isEmpty());
    REQUIRE(result.differing.size() == 1);
    CHECK(result.differing[0].relativePath == "level/textures/tex.png");
    CHECK_FALSE(result.differing[0].firstDifferingLine.has_value());
    CHECK_FALSE(result.identical());
    CHECK(result.describe().contains("level/textures/tex.png"));
}

TEST_CASE("compareTrees reports the first differing line of an .obj file", "[TreeCompare]")
{
    Trees trees;
    QByteArray changed = kObj;
    changed.replace("v 1.0 0.0 0.0", "v 1.0 0.5 0.0");
    changed.replace("f 1 2 3", "f 3 2 1"); // a later difference must not hide the first one
    writeFile(trees.actual.path(), "level/model.obj", changed);

    const TreeComparison result = compareTrees(trees.expected.path(), trees.actual.path());

    REQUIRE(result.differing.size() == 1);
    const auto& diff = result.differing[0];
    CHECK(diff.relativePath == "level/model.obj");
    REQUIRE(diff.firstDifferingLine.has_value());
    CHECK(*diff.firstDifferingLine == 4);
    CHECK(diff.expectedLine == "v 1.0 0.0 0.0");
    CHECK(diff.actualLine == "v 1.0 0.5 0.0");
    CHECK(result.describe().contains("level/model.obj"));
    CHECK(result.describe().contains("line 4"));
}

TEST_CASE("compareTrees reports the first differing line of an .mtl file with CRLF line ends", "[TreeCompare]")
{
    Trees trees;
    QByteArray changed = kMtl;
    changed.replace("map_Kd tex.png", "map_Kd tex2.png");
    writeFile(trees.actual.path(), "level/model.mtl", changed);

    const TreeComparison result = compareTrees(trees.expected.path(), trees.actual.path());

    REQUIRE(result.differing.size() == 1);
    const auto& diff = result.differing[0];
    CHECK(diff.relativePath == "level/model.mtl");
    REQUIRE(diff.firstDifferingLine.has_value());
    CHECK(*diff.firstDifferingLine == 3);
    CHECK(diff.expectedLine == "map_Kd tex.png");
    CHECK(diff.actualLine == "map_Kd tex2.png");
}

TEST_CASE("compareTrees reports the first line past the end of a truncated .obj file", "[TreeCompare]")
{
    Trees trees;
    writeFile(trees.actual.path(), "level/model.obj", kObj.left(kObj.indexOf("f 1 2 3")));

    const TreeComparison result = compareTrees(trees.expected.path(), trees.actual.path());

    REQUIRE(result.differing.size() == 1);
    const auto& diff = result.differing[0];
    REQUIRE(diff.firstDifferingLine.has_value());
    CHECK(*diff.firstDifferingLine == 6);
    CHECK(diff.expectedLine == "f 1 2 3");
    CHECK(diff.actualLine.isEmpty());
}

TEST_CASE("compareTrees treats .OBJ and .MTL suffixes case-insensitively", "[TreeCompare]")
{
    QTemporaryDir expected;
    QTemporaryDir actual;
    REQUIRE(expected.isValid());
    REQUIRE(actual.isValid());
    writeFile(expected.path(), "A.OBJ", "v 0 0 0\nv 1 1 1\n");
    writeFile(actual.path(), "A.OBJ", "v 0 0 0\nv 1 1 2\n");

    const TreeComparison result = compareTrees(expected.path(), actual.path());

    REQUIRE(result.differing.size() == 1);
    REQUIRE(result.differing[0].firstDifferingLine.has_value());
    CHECK(*result.differing[0].firstDifferingLine == 2);
}

TEST_CASE("compareTrees reports no line number for other text files", "[TreeCompare]")
{
    Trees trees;
    writeFile(trees.actual.path(), "top.txt", "top level changed\n");

    const TreeComparison result = compareTrees(trees.expected.path(), trees.actual.path());

    REQUIRE(result.differing.size() == 1);
    CHECK(result.differing[0].relativePath == "top.txt");
    CHECK_FALSE(result.differing[0].firstDifferingLine.has_value());
}

TEST_CASE("compareTrees reports missing, extra and differing files together, sorted", "[TreeCompare]")
{
    Trees trees;
    REQUIRE(QFile::remove(trees.actual.path() + "/top.txt"));
    REQUIRE(QFile::remove(trees.actual.path() + "/level/model.mtl"));
    writeFile(trees.actual.path(), "z.txt", "z\n");
    writeFile(trees.actual.path(), "a.txt", "a\n");
    writeFile(trees.actual.path(), "level/textures/tex.png", "not a png");
    writeFile(trees.actual.path(), "level/model.obj", "# other\n");

    const TreeComparison result = compareTrees(trees.expected.path(), trees.actual.path());

    CHECK(result.missing == QStringList{"level/model.mtl", "top.txt"});
    CHECK(result.extra == QStringList{"a.txt", "z.txt"});
    REQUIRE(result.differing.size() == 2);
    CHECK(result.differing[0].relativePath == "level/model.obj");
    CHECK(result.differing[1].relativePath == "level/textures/tex.png");
    REQUIRE(result.differing[0].firstDifferingLine.has_value());
    CHECK(*result.differing[0].firstDifferingLine == 1);
}

TEST_CASE("compareTrees treats a nonexistent root as an empty tree", "[TreeCompare]")
{
    Trees trees;
    const QString nowhere = trees.actual.path() + "/does_not_exist";

    const TreeComparison result = compareTrees(trees.expected.path(), nowhere);

    CHECK(result.missing.size() == 4);
    CHECK(result.extra.isEmpty());
    CHECK_FALSE(result.identical());
}

TEST_CASE("compareTrees finds a copy of the committed fixture identical and detects an edited line", "[TreeCompare][fixture]")
{
    const QString fixture = fez::testing::testDataPath("tree_compare/sample");
    REQUIRE(QFileInfo(fixture).isAbsolute());
    REQUIRE(QFileInfo::exists(fixture + "/level.mtl"));

    QTemporaryDir copy;
    REQUIRE(copy.isValid());
    copyTree(fixture, copy.path());

    CHECK(compareTrees(fixture, copy.path()).identical());

    QFile mtl(copy.path() + "/level.mtl");
    REQUIRE(mtl.open(QIODevice::ReadOnly));
    QByteArray content = mtl.readAll();
    mtl.close();
    content.replace("\nd 1.000000", "\nd 0.500000"); // line 5; the leading '\n' avoids matching "Kd 1.000000"
    REQUIRE(QFile::remove(copy.path() + "/level.mtl"));
    writeFile(copy.path(), "level.mtl", content);

    const TreeComparison result = compareTrees(fixture, copy.path());
    REQUIRE(result.differing.size() == 1);
    CHECK(result.differing[0].relativePath == "level.mtl");
    REQUIRE(result.differing[0].firstDifferingLine.has_value());
    CHECK(*result.differing[0].firstDifferingLine == 5);
}
