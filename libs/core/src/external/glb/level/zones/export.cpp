// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/objects.h"
#include "external/glb/level/palette.h"
#include "external/glb/level/zones.h"
#include "external/glb/writer.h"
#include "internal.h"
#include "level/data.h"
#include "modules/scene.h"
#include "paths/ambiance.h"
#include "utils/log.h"
#include "utils/math/triangulation.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb_level {

ArxReturnCode exportZones(const LevelModules& level, const ArxAabb& referenced_bounds,
                          const Level::GlbExportOptions& options, glb::Builder& builder, Palette& palette) {
  if (level.scene.zones.empty()) return ARX_OK;
  const int material = palette.material(PaletteItem::kZone);
  ArxVector3 parent_position = bottomCenter(referenced_bounds);
  parent_position.y += kZoneParentOffset;
  const int parent = builder.addNode("zones_parent");
  builder.setNodeTranslation(parent, {parent_position.x, parent_position.y, parent_position.z});
  builder.addRoot(parent);

  for (std::size_t ordinal = 0; ordinal < level.scene.zones.size(); ++ordinal) {
    const Zone& zone = level.scene.zones[ordinal];
    if (zone.perimeter_xz.size() > std::numeric_limits<std::uint32_t>::max() / 2U) return ARX_GLB_BAD_LEVEL_ZONE;
    float top_y = zone.reference_y - zone.height;
    float bottom_y = zone.reference_y;
    if (zone.height_mode == ZoneHeightMode::kInfinite) {
      top_y = referenced_bounds.min.y - zone_internal::kExportMargin;
      bottom_y = referenced_bounds.max.y + zone_internal::kExportMargin;
    }
    double x = 0.0;
    double z = 0.0;
    for (const ArxVector2& point : zone.perimeter_xz) {
      x += point.x;
      z += point.y;
    }
    const float center_x = static_cast<float>(x / static_cast<double>(zone.perimeter_xz.size()));
    const float center_z = static_cast<float>(z / static_cast<double>(zone.perimeter_xz.size()));
    const float center_y = (top_y + bottom_y) * 0.5f;

    std::vector<glb::Vec3> positions;
    positions.reserve(zone.perimeter_xz.size() * 2U);
    for (const ArxVector2& point : zone.perimeter_xz)
      positions.push_back({point.x - center_x, top_y - center_y, point.y - center_z});
    for (const ArxVector2& point : zone.perimeter_xz)
      positions.push_back({point.x - center_x, bottom_y - center_y, point.y - center_z});

    std::vector<std::uint32_t> indices;
    const std::uint32_t count = static_cast<std::uint32_t>(zone.perimeter_xz.size());
    std::vector<std::uint32_t> cap_indices;
    const math::TriangulationResult triangulation = math::triangulateSimplePolygon(zone.perimeter_xz, cap_indices);
    if (triangulation == math::TriangulationResult::kSuccess) {
      for (std::size_t i = 0; i < cap_indices.size(); i += 3) {
        const std::uint32_t a = cap_indices[i];
        const std::uint32_t b = cap_indices[i + 1];
        const std::uint32_t c = cap_indices[i + 2];
        indices.insert(indices.end(), {a, c, b});
        indices.insert(indices.end(), {count + a, count + b, count + c});
      }
    } else {
      const char* reason =
          triangulation == math::TriangulationResult::kNonSimple ? "is not simple" : "could not be triangulated";
      log(ARX_LOG_WARN, "Level -> GLB: zone '{}' perimeter {}; using fan cap triangulation", zone.name, reason);
      for (std::uint32_t i = 1; i + 1 < count; ++i) {
        indices.insert(indices.end(), {0, i + 1, i});
        indices.insert(indices.end(), {count, count + i, count + i + 1});
      }
    }
    for (std::uint32_t i = 0; i < count; ++i) {
      const std::uint32_t next = (i + 1) % count;
      indices.insert(indices.end(), {i, next, count + i, count + i, next, count + next});
    }

    glb::Primitive primitive;
    primitive.indices =
        builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
    primitive.material = material;
    primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(positions));
    const std::string name = zone_internal::nodeName(zone, ordinal);
    const int mesh = builder.addMesh(name, {std::move(primitive)});
    const int node = builder.addNode(name, mesh);
    builder.setNodeTranslation(
        node, {center_x - parent_position.x, center_y - parent_position.y, center_z - parent_position.z});
    builder.addChild(parent, node);

    const auto& ambiance = zone.ambiance;
    zone_internal::Settings settings{
        .color = zone.color,
        .farclip = zone.farclip ? std::optional<float>(toGlbLength(*zone.farclip, options)) : std::nullopt,
        .volume = ambiance ? std::optional<float>(ambiance->volume) : std::nullopt,
    };
    const std::string settings_name = zone_internal::settingsHelperName(settings, zone.name);
    if (!settings_name.empty()) builder.addChild(node, builder.addNode(settings_name));
    if (ambiance) {
      std::string reference;
      if (!paths::formatZoneAmbianceReference(ambiance->name, reference)) return ARX_GLB_BAD_LEVEL_ZONE;
      builder.addChild(node, builder.addNode("AMBIANCE_" + reference + "__" + zone.name));
    }
  }
  return ARX_OK;
}

}  // namespace pistoris::glb_level
