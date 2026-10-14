# fez_core: static library with everything except the command-line front end.
# Sources are listed explicitly (no file(GLOB)); add new files here.

set(FEZ_CORE_SOURCES
    error/Error.h
    export/ObjExport.cpp
    export/ObjExport.h
    export/SceneBuilder.cpp
    export/SceneBuilder.h
    export/SceneComposition.cpp
    export/SceneComposition.h
    export/TextureCopier.cpp
    export/TextureCopier.h
    export/Transforms.cpp
    export/Transforms.h
    format/QStringFormatter.h
    geometry/FaceDecoding.cpp
    geometry/FaceDecoding.h
    geometry/Image.cpp
    geometry/Image.h
    geometry/TexturedQuad.cpp
    geometry/TexturedQuad.h
    level/ResolveLevel.cpp
    level/ResolveLevel.h
    math/Quaternion.h
    math/Vector.h
    model/ArtObject.h
    model/BackgroundPlane.h
    model/Character.h
    model/Geometry.h
    model/Level.h
    model/LevelDescription.h
    model/Texture.h
    model/TrileEmplacement.h
    model/Vertex.h
    parser/ArtObjectParser.cpp
    parser/ArtObjectParser.h
    parser/GeometryParser.cpp
    parser/GeometryParser.h
    parser/level/ArtObjectSection.cpp
    parser/level/BackgroundPlaneSection.cpp
    parser/level/LevelReader.cpp
    parser/level/LevelReader.h
    parser/level/LevelSections.h
    parser/level/NpcSection.cpp
    parser/level/TrileSection.cpp
    parser/TextureParser.cpp
    parser/TextureParser.h
    parser/TrileSetParser.cpp
    parser/TrileSetParser.h
    pipeline/ConversionReport.h
    pipeline/Pipeline.cpp
    pipeline/Pipeline.h
    repository/AssetRepository.cpp
    repository/AssetRepository.h
    xml/Xml.cpp
    xml/Xml.h
)

add_library(fez_core STATIC ${FEZ_CORE_SOURCES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX core FILES ${FEZ_CORE_SOURCES})

# Headers are included relative to src/core (e.g. "parser/level/LevelReader.h").
target_include_directories(fez_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Public headers use Qt Core/Xml types and Eigen; assimp, QImage and QtConcurrent stay internal.
target_link_libraries(fez_core
    PUBLIC
        Eigen3::Eigen
        Qt6::Core
        Qt6::Xml
    PRIVATE
        assimp::assimp
        Qt6::Concurrent
        Qt6::Gui
)

fez_set_project_warnings(fez_core)
