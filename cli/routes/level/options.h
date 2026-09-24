// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/glb.hpp"
#include "arx_pistoris/level.hpp"

#include "formats/modifiers.h"
#include "routes/options.h"

#include <optional>
#include <string>

namespace cli::level {

static_assert(kMinGlbArxUnitsPerUnit == pistoris::glb::kMinArxUnitsPerUnit);
static_assert(kMaxGlbArxUnitsPerUnit == pistoris::glb::kMaxArxUnitsPerUnit);

inline constexpr float kAnchorSpacingRadiusFactor = 2.0f;
inline constexpr float kAnchorLinkDistanceSpacingFactor = 1.5f;

struct MinimapSamplerPaths {
  std::string foreground;
  std::string background;
  std::string water;
  std::string lava;
};

struct LevelOptions final : RouteOptions {
  pistoris::Level::GlbImportOptions glb_import;
  pistoris::Level::GlbExportOptions glb_export;
  bool load_previews = false;
  bool weld_vertices = false;
  pistoris::Level::VertexWeldOptions vertex_welding;
  bool flatten_portals = false;
  bool snap_to_portals = false;
  pistoris::Level::PortalSnapOptions portal_snapping;
  bool dlf_only = false;
  bool reconstruct_quads = true;
  bool fts_scene_directory_specified = false;
  std::string fts_scene_directory;
  std::string signer;
  bool generate_nav_surface = false;
  bool nav_surface_from_floor = false;
  pistoris::Level::NavSurfaceGenOptions nav_surface_generation;
  bool prune_nav_surface_islands = false;
  pistoris::Level::NavSurfacePruneOptions nav_surface_pruning;
  bool generate_room_distances = false;
  pistoris::Level::RoomDistanceGenOptions room_distance_generation;
  bool room_distance_link_distance_specified = false;
  bool generate_anchors = false;
  bool connect_anchors = false;
  bool prune_anchor_islands = false;
  pistoris::Level::AnchorGenOptions anchor_generation;
  pistoris::Level::AnchorConnectionGenOptions anchor_connection;
  pistoris::Level::AnchorPruneOptions anchor_pruning;
  bool anchor_spacing_specified = false;
  bool anchor_link_distance_specified = false;
  bool generate_static_lighting = false;
  pistoris::Level::StaticLightingGenOptions static_lighting_generation;
  bool generate_minimap = false;
  pistoris::Level::MinimapGenerationOptions minimap_generation;
  MinimapSamplerPaths minimap_sampler_paths;
  std::optional<pistoris::ArxColor3> minimap_border_color;
};

inline pistoris::ArxColor3 effectiveMinimapBorderColor(const LevelOptions& options) noexcept {
  if (options.minimap_border_color) return *options.minimap_border_color;
  if (options.generate_minimap && options.minimap_generation.halo_radius != 0)
    return options.minimap_generation.halo_color;
  return {1.0f, 1.0f, 1.0f};
}

inline void applyFormatModifiers(LevelOptions& options, const FormatModifierOptions& modifiers) {
  const auto& units = modifiers.glb.arx_units_per_unit;
  if (units) {
    options.glb_import.arx_units_per_glb_unit = *units;
    options.glb_export.arx_units_per_glb_unit = *units;
  }
  const auto& offset = modifiers.glb.arx_offset;
  if (offset) {
    options.glb_import.arx_offset = offset;
    options.glb_export.arx_offset = *offset;
  }
}

inline pistoris::Level::AnchorGenOptions effectiveAnchorGenerationOptions(const LevelOptions& options) {
  pistoris::Level::AnchorGenOptions result = options.anchor_generation;
  if (!options.anchor_spacing_specified) {
    result.sample_spacing = result.radius * kAnchorSpacingRadiusFactor;
  }
  return result;
}

inline pistoris::Level::AnchorConnectionGenOptions effectiveAnchorConnectionOptions(
    const LevelOptions& options, const pistoris::Level::AnchorGenOptions& generation) {
  pistoris::Level::AnchorConnectionGenOptions result = options.anchor_connection;
  if (!options.anchor_link_distance_specified) {
    result.max_distance = generation.sample_spacing * kAnchorLinkDistanceSpacingFactor;
  }
  return result;
}

}  // namespace cli::level
