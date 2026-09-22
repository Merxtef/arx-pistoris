// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/text.hpp"

#include "modules/geometry.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace pistoris {

struct LevelModules;
struct TexturesData;
struct LightingData;
struct SceneData;

namespace level_native {

struct NativeBuildWarnings {
  std::uint64_t regenerated_normals = 0;
  std::uint64_t normalized_normals = 0;
  std::uint64_t discarded_faces = 0;
  std::uint64_t skipped_room0_faces = 0;
  std::uint64_t skipped_room0_portals = 0;
  std::uint64_t collapsed_zone_points = 0;
  std::uint64_t empty_zone_ambiances = 0;
  std::uint64_t equal_light_falloffs = 0;
  std::uint64_t outside_geometry_anchors = 0;
  std::uint64_t discarded_out_of_bounds_anchors = 0;
  std::uint64_t room_distance_one_sided = 0;
  std::uint64_t room_distance_asymmetric = 0;
  std::uint64_t room_distance_mismatched_portals = 0;
  std::uint64_t room_distance_room0_positive = 0;
  std::uint64_t room_distance_reassigned_endpoints = 0;
  std::uint64_t room_distance_discarded_positive_directions = 0;
  std::uint64_t room_distance_direct_portal_fallbacks = 0;
  std::uint64_t room_distance_missing_connections = 0;
  std::uint64_t repaired_portal_names = 0;
  std::uint64_t repaired_light_names = 0;
  std::uint64_t repaired_fog_names = 0;
  std::uint64_t repaired_zone_names = 0;
  std::uint64_t repaired_path_names = 0;
  bool room_distance_no_positive_real_pairs = false;
};

struct NativeBakeWarnings {
  std::uint64_t discarded_fragments = 0;
  std::uint64_t fully_discarded_faces = 0;
  std::uint64_t rescaled_texture_images = 0;
};

struct NativeBakeStatistics {
  std::uint64_t clipped_fragment_quads = 0;
  std::uint64_t cross_face_quads = 0;
  std::uint64_t triangles = 0;
};

struct NativeTextureShard {
  std::int32_t fts_id = 0;
  std::string resource_path;
};

struct NativeTextureFamily {
  std::vector<NativeTextureShard> shards;
  std::string image_extension;
  std::vector<std::uint8_t> encoded_image;
};

struct NativeTextureResources {
  std::vector<NativeTextureFamily> families;
  std::unordered_set<std::string> unavailable;
  std::int32_t next_fts_id = 1;
  bool include_texture_files = true;
};

std::size_t expectedFtsColorCount(const fts::Data& fts);
ArxReturnCode buildFtsModules(const fts::Data& fts, std::span<const ArxColor3> colors, LevelModules& out,
                              NativeBuildWarnings& warnings, std::vector<std::string>* texture_source_paths,
                              NativeTextMode text_mode);
void buildLlfModules(const fts::Data& fts, const llf::Data& llf, LightingData& out, NativeBuildWarnings& warnings);
ArxReturnCode buildDlfModules(const dlf::Data& dlf, const ArxVector3& offset, SceneData& out,
                              NativeBuildWarnings& warnings, NativeTextMode text_mode);

ArxReturnCode projectNativeTextures(const TexturesData& texture_data, const Level::NativeBakeOptions& options,
                                    NativeTextureResources& out, NativeBakeWarnings& warnings);
ArxReturnCode addNativeTextureShard(NativeTextureResources& textures, TextureIndex source_texture,
                                    std::size_t& out_shard);
void buildNativeTextureFiles(NativeTextureResources& textures, std::vector<NativeTextureFile>& out);
ArxReturnCode bakeFts(const LevelModules& level, NativeTextureResources& textures, bool reconstruct_quads,
                      NativeTextMode text_mode, fts::Data& out, std::vector<ArxColor3>& baked_colors,
                      NativeBakeWarnings& warnings, NativeBakeStatistics& statistics);
ArxReturnCode bakeLlf(const LightingData& lighting, std::vector<ArxColor3>&& baked_colors, llf::Data& out);
ArxReturnCode bakeDlf(const SceneData& scene, std::string_view scene_path, const ArxVector3& target_fts_offset,
                      NativeTextMode text_mode, dlf::Data& out);

void logRoomDistanceBakeWarnings(const LevelModules& level);

}  // namespace level_native
}  // namespace pistoris
