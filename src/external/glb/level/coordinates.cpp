// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "coordinates.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "external/glb/accessor.h"
#include "external/glb/writer.h"
#include "level/data.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/math/quat.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>

namespace pistoris::glb_level {
namespace {

constexpr double kAutoPlacementStep = 100.0;
constexpr double kBoundaryEpsilon = 1.0e-4;
constexpr ArxQuat kLevelGlbBasisRotation = {0.0f, 1.0f, 0.0f, 0.0f};

struct XzEnvelope {
  double min_x = std::numeric_limits<double>::infinity();
  double max_x = -std::numeric_limits<double>::infinity();
  double min_z = std::numeric_limits<double>::infinity();
  double max_z = -std::numeric_limits<double>::infinity();
};

std::optional<float> scaledValue(float value, double scale) {
  const double result = static_cast<double>(value) * scale;
  if (!std::isfinite(result) || result < std::numeric_limits<float>::lowest() ||
      result > std::numeric_limits<float>::max())
    return std::nullopt;
  return static_cast<float>(result);
}

std::optional<ArxVector3> scaledVector(const ArxVector3& value, double scale) {
  const std::optional<float> x = scaledValue(value.x, scale);
  const std::optional<float> y = scaledValue(value.y, scale);
  const std::optional<float> z = scaledValue(value.z, scale);
  if (!x || !y || !z) return std::nullopt;
  return ArxVector3{*x, *y, *z};
}

std::optional<float> translatedValue(float value, float offset) {
  const double result = static_cast<double>(value) + offset;
  if (!std::isfinite(result) || result < std::numeric_limits<float>::lowest() ||
      result > std::numeric_limits<float>::max())
    return std::nullopt;
  return static_cast<float>(result);
}

std::optional<ArxVector3> translatedPoint(const ArxVector3& value, const ArxVector3& offset) {
  const std::optional<float> x = translatedValue(value.x, offset.x);
  const std::optional<float> y = translatedValue(value.y, offset.y);
  const std::optional<float> z = translatedValue(value.z, offset.z);
  if (!x || !y || !z) return std::nullopt;
  return ArxVector3{*x, *y, *z};
}

std::size_t portalVertexCount(const Portal& portal) { return portal.shape == PortalShape::kTriangle ? 3U : 4U; }

bool translateZone(Zone& zone, const ArxVector3& offset) {
  for (ArxVector2& point : zone.perimeter_xz) {
    const std::optional<float> x = translatedValue(point.x, offset.x);
    const std::optional<float> z = translatedValue(point.y, offset.z);
    if (!x || !z) return false;
    point.x = *x;
    point.y = *z;
  }
  const std::optional<float> y = translatedValue(zone.reference_y, offset.y);
  if (!y) return false;
  zone.reference_y = *y;
  return true;
}

bool translateLevel(LevelModules& level, const ArxVector3& offset) {
  auto translate = [&](ArxVector3& value) {
    const std::optional<ArxVector3> translated = translatedPoint(value, offset);
    if (!translated) return false;
    value = *translated;
    return true;
  };
  for (Vertex& vertex : level.geometry.vertices)
    if (!translate(vertex.position)) return false;
  for (Portal& portal : level.rooms.portals)
    for (std::size_t i = 0; i < portalVertexCount(portal); ++i)
      if (!translate(portal.vertices[i])) return false;
  auto& surface = level.navigation.surface;
  if (surface)
    for (Vertex& vertex : surface->vertices)
      if (!translate(vertex.position)) return false;
  for (Anchor& anchor : level.navigation.anchors)
    if (!translate(anchor.position)) return false;
  for (Light& light : level.lighting.lights)
    if (!translate(light.position)) return false;
  if (!level.scene.player_spawn_is_fallback)
    if (!translate(level.scene.player_spawn.position)) return false;
  for (Entity& entity : level.scene.entities)
    if (!translate(entity.position)) return false;
  for (Fog& fog : level.scene.fogs)
    if (!translate(fog.position)) return false;
  for (Zone& zone : level.scene.zones)
    if (!translateZone(zone, offset)) return false;
  for (Path& path : level.scene.paths)
    if (!translate(path.position)) return false;
  return true;
}

void include(XzEnvelope& bounds, const ArxVector3& position) {
  bounds.min_x = std::min(bounds.min_x, static_cast<double>(position.x));
  bounds.max_x = std::max(bounds.max_x, static_cast<double>(position.x));
  bounds.min_z = std::min(bounds.min_z, static_cast<double>(position.z));
  bounds.max_z = std::max(bounds.max_z, static_cast<double>(position.z));
}

XzEnvelope placementEnvelope(const LevelModules& level) {
  XzEnvelope bounds;
  for (const Vertex& vertex : level.geometry.vertices) include(bounds, vertex.position);
  for (const Portal& portal : level.rooms.portals)
    for (std::size_t i = 0; i < portalVertexCount(portal); ++i) include(bounds, portal.vertices[i]);
  for (const Anchor& anchor : level.navigation.anchors) include(bounds, anchor.position);
  return bounds;
}

std::optional<double> autoAxisOffset(double min, double max) {
  if (!std::isfinite(min) || !std::isfinite(max) || max - min > kLevelMaxXZ + kBoundaryEpsilon) return std::nullopt;
  const double lower = kLevelMinXZ - min;
  const double upper = kLevelMaxXZ - max;
  if (lower <= kBoundaryEpsilon && upper >= -kBoundaryEpsilon) return 0.0;

  const double first = std::ceil((lower - kBoundaryEpsilon) / kAutoPlacementStep);
  const double last = std::floor((upper + kBoundaryEpsilon) / kAutoPlacementStep);
  if (first > last) return std::nullopt;
  return std::clamp(0.0, first, last) * kAutoPlacementStep;
}

std::optional<ArxVector3> automaticOffset(const LevelModules& level) {
  const XzEnvelope bounds = placementEnvelope(level);
  std::optional<double> x = autoAxisOffset(bounds.min_x, bounds.max_x);
  std::optional<double> z = autoAxisOffset(bounds.min_z, bounds.max_z);
  if (!x || !z) {
    log(ARX_LOG_ERROR,
        std::format("GLB -> Level automatic placement failed for XZ bounds [{}, {}] x [{}, {}]",
                    bounds.min_x,
                    bounds.max_x,
                    bounds.min_z,
                    bounds.max_z));
    return std::nullopt;
  }
  return ArxVector3{static_cast<float>(*x), 0.0f, static_cast<float>(*z)};
}

float snapBoundary(float value) {
  if (std::abs(static_cast<double>(value) - kLevelMinXZ) <= kBoundaryEpsilon) return kLevelMinXZ;
  if (std::abs(static_cast<double>(value) - kLevelMaxXZ) <= kBoundaryEpsilon) return kLevelMaxXZ;
  return value;
}

void snapMapEnvelope(LevelModules& level) {
  auto snap = [](ArxVector3& position) {
    position.x = snapBoundary(position.x);
    position.z = snapBoundary(position.z);
  };
  for (Vertex& vertex : level.geometry.vertices) snap(vertex.position);
  for (Portal& portal : level.rooms.portals)
    for (std::size_t i = 0; i < portalVertexCount(portal); ++i) snap(portal.vertices[i]);
  for (Anchor& anchor : level.navigation.anchors) snap(anchor.position);
}

bool validScale(float scale) {
  return std::isfinite(scale) && scale >= kMinArxUnitsPerGlbUnit && scale <= kMaxArxUnitsPerGlbUnit;
}

}  // namespace

ArxReturnCode validateGlbImportOptions(const Level::GlbImportOptions& options) {
  const auto& offset = options.arx_offset;
  if (!validScale(options.arx_units_per_glb_unit) || (offset && !math::finite(*offset))) return ARX_INVALID_OPTIONS;
  return ARX_OK;
}

std::optional<ArxVector3> toArxPoint(const ArxVector3& value, const ImportUnits& units) noexcept {
  return scaledVector({value.x, -value.y, -value.z}, units.arx_per_glb_unit);
}

std::optional<ArxVector3> toArxVector(const ArxVector3& value, const ImportUnits& units) noexcept {
  return scaledVector({value.x, -value.y, -value.z}, units.arx_per_glb_unit);
}

ArxQuat toArxRotation(const ArxQuat& value) noexcept {
  return math::normalize(math::conjugate(kLevelGlbBasisRotation) * value * kLevelGlbBasisRotation);
}

std::optional<float> toArxLength(float value, const ImportUnits& units) noexcept {
  return scaledValue(value, units.arx_per_glb_unit);
}

ArxReturnCode applyGlbImportPlacement(LevelModules& level, const Level::GlbImportOptions& options,
                                      Level::GlbImportInfo& info) {
  ArxReturnCode rc = validateGlbImportOptions(options);
  if (rc != ARX_OK) return rc;

  std::optional<ArxVector3> offset = options.arx_offset;
  if (!offset) offset = automaticOffset(level);
  if (!offset) return ARX_GLB_BAD_LEVEL_GEOMETRY;
  if (!translateLevel(level, *offset)) return options.arx_offset ? ARX_INVALID_OPTIONS : ARX_GLB_BAD_FORMAT;
  snapMapEnvelope(level);

  info.applied_arx_offset = *offset;
  if (!options.arx_offset && (offset->x != 0.0f || offset->z != 0.0f))
    log(ARX_LOG_INFO, std::format("GLB -> Level automatic XZ offset: ({}, {})", offset->x, offset->z));
  return ARX_OK;
}

ArxReturnCode configureGlbExportCoordinates(glb::Builder& builder, const Level::GlbExportOptions& options) {
  if (!validScale(options.arx_units_per_glb_unit) || !math::finite(options.arx_offset)) return ARX_INVALID_OPTIONS;
  const double inverse = 1.0 / static_cast<double>(options.arx_units_per_glb_unit);
  const glb::Vec3 translation = {
      static_cast<float>(-static_cast<double>(options.arx_offset.x) * inverse),
      static_cast<float>(static_cast<double>(options.arx_offset.y) * inverse),
      static_cast<float>(static_cast<double>(options.arx_offset.z) * inverse),
  };
  const float scale = static_cast<float>(inverse);
  if (!std::isfinite(scale) || scale <= 0.0f || !std::isfinite(translation.x) || !std::isfinite(translation.y) ||
      !std::isfinite(translation.z))
    return ARX_INVALID_OPTIONS;
  builder.setContentBasisRotation(kLevelGlbBasisRotation);
  builder.setRootTransform("level_space", translation, math::kIdentityQuat, scale);
  return ARX_OK;
}

float toGlbLength(float value, const Level::GlbExportOptions& options) {
  return static_cast<float>(static_cast<double>(value) / options.arx_units_per_glb_unit);
}

}  // namespace pistoris::glb_level
