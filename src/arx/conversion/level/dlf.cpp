// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx/dlf.h"

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/conversion/level/internal.h"
#include "level/validation.h"
#include "modules/scene.h"
#include "utils/log.h"
#include "utils/math/quat.h"

#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::arx_level_conversion {
namespace {

constexpr float kZoneZeroEdgeEpsilon = 1.0e-4f;

bool sameZonePoint(const ArxVector2& a, const ArxVector2& b) {
  return std::abs(a.x - b.x) <= kZoneZeroEdgeEpsilon && std::abs(a.y - b.y) <= kZoneZeroEdgeEpsilon;
}

void collapseZonePoints(std::vector<ArxVector2>& points, NativeBuildWarnings& warnings) {
  if (points.empty()) return;
  std::vector<ArxVector2> collapsed;
  collapsed.reserve(points.size());
  for (const ArxVector2& point : points) {
    if (!collapsed.empty() && sameZonePoint(collapsed.back(), point)) {
      ++warnings.collapsed_zone_points;
      continue;
    }
    collapsed.push_back(point);
  }
  while (collapsed.size() > 1 && sameZonePoint(collapsed.front(), collapsed.back())) {
    collapsed.pop_back();
    ++warnings.collapsed_zone_points;
  }
  points = std::move(collapsed);
}

PathNodeType pathNodeType(dlf::PathNodeType type) {
  switch (type) {
    case dlf::PathNodeType::kBezier:
      return PathNodeType::kBezier;
    case dlf::PathNodeType::kControlPoint:
      return PathNodeType::kControlPoint;
    case dlf::PathNodeType::kStandard:
      return PathNodeType::kStandard;
  }
  return PathNodeType::kStandard;
}

dlf::PathNodeType nativePathNodeType(PathNodeType type) {
  switch (type) {
    case PathNodeType::kBezier:
      return dlf::PathNodeType::kBezier;
    case PathNodeType::kControlPoint:
      return dlf::PathNodeType::kControlPoint;
    case PathNodeType::kStandard:
      return dlf::PathNodeType::kStandard;
  }
  return dlf::PathNodeType::kStandard;
}

ArxReturnCode addZone(const dlf::Zone& source, const ArxVector3& offset, SceneData& out,
                      NativeBuildWarnings& warnings) {
  ArxVector3 root = source.position + offset;
  Zone zone;
  zone.name = source.name;
  zone.reference_y = root.y;
  zone.height_mode = source.height < 0 ? ZoneHeightMode::kInfinite : ZoneHeightMode::kFinite;
  zone.height = source.height > 0 ? static_cast<float>(source.height) : 0.0f;
  zone.color = source.color;
  zone.farclip = source.farclip;
  const auto& source_ambiance = source.ambiance;
  if (source_ambiance) {
    if (source_ambiance->name.empty()) {
      ++warnings.empty_zone_ambiances;
    } else {
      std::string ambiance;
      if (!paths::normalizeZoneAmbiance(source_ambiance->name, ambiance)) {
        log(ARX_LOG_ERROR,
            std::format("Invalid native zone ambiance: zone '{}', name '{}'", source.name, source_ambiance->name));
        return ARX_LEVEL_BAD_ZONE_AMBIANCE;
      }
      zone.ambiance = ZoneAmbiance{std::move(ambiance), source_ambiance->volume};
    }
  }
  zone.perimeter_xz.reserve(source.points.size());
  for (const ArxVector3& point : source.points) zone.perimeter_xz.push_back({root.x + point.x, root.z + point.z});
  collapseZonePoints(zone.perimeter_xz, warnings);
  out.zones.push_back(std::move(zone));
  return ARX_OK;
}

}  // namespace

ArxReturnCode buildDlfModules(const dlf::Data& dlf, const ArxVector3& offset, SceneData& out,
                              NativeBuildWarnings& warnings) {
  PlayerSpawn player_spawn{dlf.player_spawn.position + offset, math::angleToQuat(dlf.player_spawn.angle)};
  ArxReturnCode rc = level_validation::sceneError(scene::setPlayerSpawn(out, player_spawn));
  if (rc != ARX_OK) return rc;
  out.entities.reserve(dlf.entities.size());
  for (const dlf::Entity& source : dlf.entities) {
    Entity entity{source.class_path, source.ident, source.position + offset, math::angleToQuat(source.angle), {}};
    if (!scene::normalizeRotation(entity.rotation)) return ARX_LEVEL_BAD_ENTITY_ROTATION;
    out.entities.push_back(std::move(entity));
  }
  scene::makeEntityNamesUnique(out.entities);
  out.fogs.reserve(dlf.fogs.size());
  for (const dlf::Fog& source : dlf.fogs) {
    Fog fog{source.position + offset,
            source.color,
            source.size,
            source.directional,
            source.scale,
            math::angleToQuat(source.angle),
            source.speed,
            source.rotate_speed,
            source.lifetime_ms,
            source.frequency,
            {}};
    if (!scene::normalizeRotation(fog.rotation)) return ARX_LEVEL_BAD_FOG_ROTATION;
    out.fogs.push_back(fog);
  }
  out.zones.reserve(dlf.zones.size() + 1);
  for (const dlf::Zone& source : dlf.zones) {
    rc = addZone(source, offset, out, warnings);
    if (rc != ARX_OK) return rc;
  }

  out.paths.reserve(dlf.paths.size());
  for (const dlf::Path& source : dlf.paths) {
    if (source.name == "level11_sewer1") {
      dlf::Zone patched;
      patched.name = source.name;
      patched.position = source.position;
      patched.height = -1;
      patched.points.reserve(source.nodes.size());
      for (const dlf::PathNode& node : source.nodes) patched.points.push_back(node.relative_position);
      rc = addZone(patched, offset, out, warnings);
      if (rc != ARX_OK) return rc;
      continue;
    }
    Path path;
    path.name = source.name;
    path.position = source.position + offset;
    path.nodes.reserve(source.nodes.size());
    for (const dlf::PathNode& node : source.nodes)
      path.nodes.push_back({node.relative_position, pathNodeType(node.type), node.time_ms});
    out.paths.push_back(std::move(path));
  }
  warnings.renamed_duplicate_paths += scene::makePathNamesUnique(out.paths);
  return ARX_OK;
}

ArxReturnCode bakeDlf(const SceneData& scene, std::string_view scene_path, const ArxVector3& target_fts_offset,
                      dlf::Data& out) {
  dlf::Data dlf;
  dlf.version = kDlfVersion;
  dlf.player_spawn = {scene.player_spawn.position - target_fts_offset, math::quatToAngle(scene.player_spawn.rotation)};
  dlf.scene_path = scene_path;
  dlf.entities.reserve(scene.entities.size());
  for (const Entity& source : scene.entities)
    dlf.entities.push_back(
        {source.class_path, source.ident, source.position - target_fts_offset, math::quatToAngle(source.rotation)});
  dlf.fogs.reserve(scene.fogs.size());
  for (const Fog& source : scene.fogs)
    dlf.fogs.push_back({source.position - target_fts_offset,
                        source.color,
                        source.size,
                        source.directional,
                        source.scale,
                        math::quatToAngle(source.rotation),
                        source.speed,
                        source.rotate_speed,
                        source.lifetime_ms,
                        source.frequency});
  dlf.zones.reserve(scene.zones.size());
  for (const Zone& source : scene.zones) {
    dlf::Zone zone;
    zone.name = source.name;
    zone.position = {-target_fts_offset.x, source.reference_y - target_fts_offset.y, -target_fts_offset.z};
    zone.points.reserve(source.perimeter_xz.size());
    for (const ArxVector2& point : source.perimeter_xz) zone.points.push_back({point.x, 0.0f, point.y});
    if (source.height_mode == ZoneHeightMode::kInfinite) {
      zone.height = -1;
    } else {
      if (static_cast<double>(source.height) > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        return ARX_DLF_BAD_ZONE_HEIGHT;
      const long rounded_height = std::lround(static_cast<double>(source.height));
      if (rounded_height < std::numeric_limits<std::int32_t>::min() ||
          rounded_height > std::numeric_limits<std::int32_t>::max())
        return ARX_DLF_BAD_ZONE_HEIGHT;
      zone.height = static_cast<std::int32_t>(rounded_height);
    }
    zone.color = source.color;
    zone.farclip = source.farclip;
    if (const auto& ambiance = source.ambiance; ambiance.has_value()) {
      const ZoneAmbiance& value = *ambiance;
      zone.ambiance = dlf::ZoneAmbiance{value.name.empty() ? std::string("none") : value.name, value.volume};
    }
    dlf.zones.push_back(std::move(zone));
  }
  dlf.paths.reserve(scene.paths.size());
  for (const Path& source : scene.paths) {
    dlf::Path path;
    path.name = source.name;
    path.position = source.position - target_fts_offset;
    path.nodes.reserve(source.nodes.size());
    for (const PathNode& node : source.nodes)
      path.nodes.push_back({node.relative_position, nativePathNodeType(node.type), node.time_ms});
    dlf.paths.push_back(std::move(path));
  }
  ArxReturnCode rc = validateDlf(&dlf);
  if (rc != ARX_OK) return rc;
  out = std::move(dlf);
  return ARX_OK;
}

}  // namespace pistoris::arx_level_conversion
