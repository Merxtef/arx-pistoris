// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"

#include "external/glb/level/material.h"
#include "external/glb/utils/texture.h"
#include "level/data.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

namespace glb {
class Builder;
struct Primitive;
}  // namespace glb

struct LevelModules;

namespace glb_level {
class Palette;
}

namespace glb_level_export {

struct TextureRegistry {
  struct Group {
    std::string stem;
    std::size_t source_texture = 0;
    glb::ExportedTexture exported;
  };

  std::vector<std::uint8_t> referenced;
  std::vector<std::size_t> group_by_texture;
  std::vector<Group> groups;
};

struct RoomProjection {
  std::vector<bool> has_faces;
  std::uint64_t empty_rooms = 0;
  std::uint64_t exported_portals = 0;
  std::uint64_t discarded_portals = 0;
  std::uint64_t discarded_positive_distances = 0;
};

ArxReturnCode buildTextureRegistry(const LevelModules& level, TextureRegistry& out);
ArxReturnCode registerTextures(const LevelModules& level, TextureRegistry& registry, glb::Builder& builder);
void logUnusedTextures(const LevelModules& level, const TextureRegistry& registry);
RoomProjection buildRoomProjection(const LevelModules& level);
ArxReturnCode exportRoomGeometry(const LevelModules& level, const ArxAabb& referenced_bounds,
                                 const TextureRegistry& registry, const RoomProjection& projection,
                                 glb::Builder& builder, std::uint64_t& nonstandard_transval);
void exportPortals(const LevelModules& level, const ArxAabb& referenced_bounds, const RoomProjection& projection,
                   glb_level::Palette& palette, glb::Builder& builder);
void exportNavigation(const LevelModules& level, const ArxAabb& referenced_bounds,
                      const Level::GlbExportOptions& options, glb_level::Palette& palette, glb::Builder& builder);
std::uint64_t exportLights(const LevelModules& level, const ArxAabb& referenced_bounds,
                           const Level::GlbExportOptions& options, glb::Builder& builder);

}  // namespace glb_level_export
}  // namespace pistoris
