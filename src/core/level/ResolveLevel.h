#pragma once

#include "error/Error.h"
#include "model/Level.h"
#include "model/LevelDescription.h"

#include <expected>

namespace fez
{

class AssetRepository;

// Builds a Level from a LevelDescription by loading everything it references from the repository (shared by all
// levels of a run, thread-safe). Steps, in this order (the first error is returned and the level gets no output):
//  - triles: the trile set m_TrileSetName (its repository error; BadValue when it has no triles, even if the level
//    places none), then one geometry per used trile id; id -1 is skipped (meaning unknown, Task 30), any other id
//    that is not in the set fails (BadValue)
//  - art objects: one geometry per used name (the repository error of the first one that fails)
//  - background planes: all textures "background planes/<texture name>" (static or animated as described), then one
//    quad each of size (width / 16.0, height / 16.0), depth offset 0.05
//  - NPCs: all textures "character animations/<npc name>/<animation of the first action>" (animated; an NPC without
//    actions is MissingElement), then one quad each of size (width / 16.0, height / 16.0), no depth offset
// Plane and NPC quads cover the first animation frame of an animated texture, otherwise the whole texture; their
// geometry is named after the texture. Emplacements (including id -1) and art object instances are copied as described.
std::expected<Level, Error> resolveLevel(const LevelDescription& description, AssetRepository& repository);

} // namespace fez
