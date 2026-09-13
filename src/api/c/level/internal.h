// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.h"
#include "arx_pistoris/level.hpp"

#include "api/c/internal.h"

struct arx_pistoris_level {
  pistoris::Level value;
};

namespace pistoris::c_api {

inline bool valid(const ArxLevelRoom& value) noexcept { return valid(value.name); }

inline bool valid(const ArxLevelPortal& value) noexcept { return valid(value.name); }

inline bool valid(const ArxLevelAnchor& value) noexcept { return valid(value.name); }

inline bool valid(const ArxLevelLight& value) noexcept { return valid(value.name); }

inline bool valid(const ArxLevelEntity& value) noexcept { return valid(value.class_path) && valid(value.name); }

inline bool valid(const ArxLevelFog& value) noexcept { return valid(value.name); }

inline bool valid(const ArxLevelZoneInput& value) noexcept {
  return valid(value.value.name) && valid(value.perimeter_xz, value.value.perimeter_count) &&
         (value.value.has_ambiance == 0 || valid(value.value.ambiance.name));
}

inline bool valid(const ArxLevelPathInput& value) noexcept {
  return valid(value.name) && valid(value.nodes, value.node_count);
}

inline Level::VertexWeldOptions weldOptions(const ArxLevelVertexWeldOptions& value) noexcept {
  return {value.radius,
          static_cast<Level::PositionWeldMetric>(value.metric),
          static_cast<Level::DegenerateFacePolicy>(value.degenerate_faces)};
}

inline Level::NavSurfaceSourceOptions navSourceOptions(const ArxLevelNavSurfaceSourceOptions& value) noexcept {
  return {value.clearance, value.support_min_up_cos, value.support_ignore_flags};
}

inline Level::NavSurfaceGenOptions navGenOptions(const ArxLevelNavSurfaceGenOptions& value) noexcept {
  Level::NavSurfaceGenOptions result;
  result.clearance = value.clearance;
  result.support_min_up_cos = value.support_min_up_cos;
  result.support_ignore_flags = value.support_ignore_flags;
  result.radius = value.radius;
  result.height = value.height;
  result.max_step_up = value.max_step_up;
  return result;
}

inline Level::NavSurfacePruneOptions navPruneOptions(const ArxLevelNavSurfacePruneOptions& value) noexcept {
  return {value.min_component_area_ratio, value.min_component_area};
}

inline Level::AnchorGenOptions anchorGenOptions(const ArxLevelAnchorGenOptions& value) noexcept {
  return {value.sample_spacing, value.radius, value.height};
}

inline Level::AnchorPruneOptions anchorPruneOptions(const ArxLevelAnchorPruneOptions& value) noexcept {
  return {value.min_component_anchor_ratio, value.min_component_anchor_count};
}

inline Level::AnchorConnectionGenOptions anchorConnectionOptions(
    const ArxLevelAnchorConnectionGenOptions& value) noexcept {
  return {value.max_distance, value.max_step_distance, value.max_step_up, value.radius_scale, value.max_steps};
}

inline Level::RoomDistanceGenOptions roomDistanceOptions(const ArxLevelRoomDistanceGenOptions& value) noexcept {
  return {value.portal_side_offset, value.sample_spacing, value.sample_height_offset, value.max_link_distance};
}

inline Level::StaticLightingGenOptions lightingOptions(const ArxLevelStaticLightingGenOptions& value) noexcept {
  return {value.ambient_color, value.global_factor, value.use_normals != 0, value.use_shadows != 0};
}

inline Level::MinimapGenerationOptions minimapGenerationOptions(
    const ArxLevelMinimapGenerationOptions& value) noexcept {
  return {
      .foreground = {value.foreground.image, value.foreground.color},
      .background = {value.background.image, value.background.color},
      .water = {value.water.image, value.water.color},
      .lava = {value.lava.image, value.lava.color},
      .halo_color = value.halo_color,
      .halo_radius = value.halo_radius,
  };
}

}  // namespace pistoris::c_api
