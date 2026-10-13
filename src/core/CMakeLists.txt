# fez_core: static library with everything except the command-line front end.
# Sources are listed explicitly (no file(GLOB)); add new files here.

set(FEZ_CORE_SOURCES
    error/Error.h
    format/QStringFormatter.h
    geometry/FaceDecoding.cpp
    geometry/FaceDecoding.h
    math/Quaternion.h
    math/Vector.h
    model/ArtObject.h
    model/BackgroundPlane.h
    model/Character.h
    model/Geometry.h
    model/Level.h
    model/Texture.h
    model/TrileEmplacement.h
    model/Vertex.h
    parser/ArtObjectParser.cpp
    parser/ArtObjectParser.h
    parser/GeometryParser.cpp
    parser/GeometryParser.h
    parser/LevelParser.cpp
    parser/LevelParser.h
    parser/TextureParser.cpp
    parser/TextureParser.h
    parser/TrileSetParser.cpp
    parser/TrileSetParser.h
    writer/GeometryWriter.cpp
    writer/GeometryWriter.h
    writer/LevelWriter.cpp
    writer/LevelWriter.h
    writer/Writer.cpp
    writer/Writer.h
    xml/Xml.cpp
    xml/Xml.h
)

add_library(fez_core STATIC ${FEZ_CORE_SOURCES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX core FILES ${FEZ_CORE_SOURCES})

# Headers are included relative to src/core (e.g. "parser/LevelParser.h").
target_include_directories(fez_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Public headers use Qt Core/Xml types and Eigen; assimp and QImage stay internal.
target_link_libraries(fez_core
    PUBLIC
        Eigen3::Eigen
        Qt6::Core
        Qt6::Xml
    PRIVATE
        assimp::assimp
        Qt6::Gui
)

fez_set_project_warnings(fez_core)
