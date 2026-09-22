// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/container.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/zones.h"
#include "external/glb/utils/names.h"
#include "external/glb/utils/node.h"
#include "external/glb/utils/tokens.h"
#include "internal.h"
#include "modules/scene.h"
#include "paths/ambiance.h"
#include "utils/log.h"
#include "utils/math/mat4.h"
#include "utils/name_tokens.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level {

using glb::parseUnsignedToken;
namespace {

ArxReturnCode parseHelpers(const cgltf_data& data, const cgltf_node& root, std::size_t node_index,
                           std::string_view zone_name, Zone& zone) {
  constexpr std::string_view kSettings = "SETTINGS__";
  constexpr std::string_view kAmbiance = "AMBIANCE_";
  bool settings_seen = false;
  bool ambiance_seen = false;
  bool volume_seen = false;
  zone_internal::Settings settings;
  for (std::size_t i = 0; i < root.children_count; ++i) {
    const cgltf_node* child = root.children[i];
    if (child == nullptr) return ARX_GLB_BAD_FORMAT;
    const std::ptrdiff_t index = child - data.nodes;
    if (index < 0 || static_cast<std::size_t>(index) >= data.nodes_count) return ARX_GLB_BAD_FORMAT;
    const std::string_view name = child->name != nullptr ? child->name : "";
    if (name.starts_with(kSettings)) {
      zone_internal::Settings candidate;
      glb::ParsedLabel label;
      if (!glb::simpleEmptyNode(*child) ||
          !glb::parseRecoverableLabel(name,
                                      candidate,
                                      &label,
                                      glb::ConventionOptions{{}, {"RGB_", "FARCLIP_", "VOLUME_"}},
                                      [](std::span<const std::string_view> tokens, zone_internal::Settings& value) {
                                        if (tokens.size() < 2 || tokens.front() != "SETTINGS") return false;
                                        return zone_internal::parseSettings(tokens.subspan(1), value) == ARX_OK;
                                      })) {
        zone_internal::logFailure(node_index, zone_name, "has invalid settings helper node {} '{}'", index, name);
        return ARX_GLB_BAD_LEVEL_ZONE;
      }
      glb::reportConventionLabel("GLB -> Level zone settings", name, label);
      volume_seen = volume_seen || candidate.volume.has_value();
      if (!settings_seen) settings = candidate;
      settings_seen = true;
    }
    if (name.starts_with(kAmbiance)) {
      const std::optional<glb::LabeledValue> labeled = glb::splitRequiredLabel(name.substr(kAmbiance.size()));
      if (!labeled || !glb::simpleEmptyNode(*child)) {
        zone_internal::logFailure(node_index, zone_name, "has invalid ambiance helper node {} '{}'", index, name);
        return ARX_GLB_BAD_LEVEL_ZONE;
      }
      std::string ambiance;
      if (!paths::parseZoneAmbianceReference(labeled->value, ambiance)) {
        zone_internal::logFailure(node_index, zone_name, "has malformed ambiance helper node {} '{}'", index, name);
        return ARX_GLB_BAD_LEVEL_ZONE;
      }
      if (!ambiance_seen) zone.ambiance = ZoneAmbiance{std::move(ambiance), 100.0f};
      ambiance_seen = true;
    }
  }
  if (volume_seen && !zone.ambiance) {
    zone_internal::logFailure(node_index, zone_name, "has VOLUME setting without ambiance helper");
    return ARX_GLB_BAD_LEVEL_ZONE;
  }
  zone.color = settings.color;
  zone.farclip = settings.farclip;
  const auto& volume = settings.volume;
  if (volume) {
    auto& ambiance = zone.ambiance;
    if (!ambiance) return ARX_GLB_BAD_LEVEL_ZONE;
    ambiance->volume = *volume;
  }
  return ARX_OK;
}

}  // namespace

ArxReturnCode importZones(const glb::Asset& asset, const std::vector<math::Mat4>& world,
                          std::span<const std::size_t> roots, const ImportUnits& units,
                          std::vector<PendingZone>& zones) {
  const cgltf_data& data = *asset.data();
  std::vector<std::uint32_t> ordinals;
  for (std::size_t node_index : roots) {
    if (node_index >= data.nodes_count || node_index >= world.size()) return ARX_GLB_BAD_FORMAT;
    const cgltf_node& node = data.nodes[node_index];
    const std::string_view name = node.name != nullptr ? node.name : "";

    constexpr std::string_view kZonePrefix = "arx_zone__";
    const std::string_view zone_payload = name.substr(kZonePrefix.size());
    std::vector<std::string_view> name_tokens;
    splitDoubleUnderscore(zone_payload, name_tokens);
    if (zone_payload.empty() || name_tokens.size() > 2 || name_tokens.back().empty() ||
        (name_tokens.size() == 2 && !parseUnsignedToken(name_tokens.front()).has_value())) {
      zone_internal::logFailure(node_index, name, "has malformed reserved name");
      return ARX_GLB_BAD_LEVEL_ZONE;
    }

    zone_internal::ParsedName parsed = zone_internal::parseName(name, node_index);
    const auto& ordinal = parsed.ordinal;
    if (ordinal) {
      if (std::find(ordinals.begin(), ordinals.end(), *ordinal) != ordinals.end()) {
        zone_internal::logFailure(node_index, name, "duplicates zone ordinal {}", *ordinal);
        return ARX_GLB_BAD_LEVEL_ZONE;
      }
      ordinals.push_back(*ordinal);
    }
    if (zones.size() >= static_cast<std::size_t>(kInvalidZoneIndex)) return ARX_GLB_BAD_LEVEL_ZONE;
    PendingZone pending;
    pending.ordinal = parsed.ordinal;
    pending.node_index = node_index;
    pending.zone.name = std::move(parsed.name);
    log(ARX_LOG_DEBUG, "GLB -> Level: importing zone node {} '{}' as '{}'", node_index, name, pending.zone.name);
    ArxReturnCode rc = parseHelpers(data, node, node_index, name, pending.zone);
    if (rc != ARX_OK) return rc;
    const auto& source_farclip = pending.zone.farclip;
    if (source_farclip) {
      const std::optional<float> farclip = toArxLength(*source_farclip, units);
      if (!farclip) return ARX_GLB_BAD_FORMAT;
      pending.zone.farclip = farclip;
    }
    zone_internal::Mesh mesh;
    rc = zone_internal::readMesh(asset, node, world[node_index], units, node_index, name, mesh);
    if (rc != ARX_OK) return rc;
    log(ARX_LOG_DEBUG,
        "GLB -> Level: zone node {} '{}' mesh has {} vertex/vertices and {} triangle(s)",
        node_index,
        name,
        mesh.positions.size(),
        mesh.triangles.size());
    float top_movement = 0.0f;
    float bottom_movement = 0.0f;
    rc = zone_internal::reconstruct(
        mesh, pending.zone, pending.top_y, pending.bottom_y, top_movement, bottom_movement, node_index, name);
    if (rc != ARX_OK) return rc;
    log(ARX_LOG_DEBUG,
        "GLB -> Level: zone node {} '{}' reconstructed {} perimeter point(s), height {}, reference_y {}",
        node_index,
        name,
        pending.zone.perimeter_xz.size(),
        pending.zone.height,
        pending.zone.reference_y);
    if (top_movement > zone_internal::kPlaneEpsilon || bottom_movement > zone_internal::kPlaneEpsilon) {
      log(ARX_LOG_WARN,
          "GLB -> Level: zone '{}' planes flattened; top moved {}, bottom moved {}",
          pending.zone.name,
          top_movement,
          bottom_movement);
    }
    zones.push_back(std::move(pending));
  }
  return ARX_OK;
}

std::vector<Zone> finalizeImportedZones(std::vector<PendingZone> pending, const ArxAabb& referenced_bounds) {
  std::sort(pending.begin(), pending.end(), [](const PendingZone& a, const PendingZone& b) {
    const auto& a_ordinal = a.ordinal;
    const auto& b_ordinal = b.ordinal;
    if (a_ordinal.has_value() != b_ordinal.has_value()) return a_ordinal.has_value();
    if (a_ordinal && b_ordinal && *a_ordinal != *b_ordinal) return *a_ordinal < *b_ordinal;
    return a.node_index < b.node_index;
  });
  std::vector<Zone> zones;
  zones.reserve(pending.size());
  for (PendingZone& item : pending) {
    const bool infinite = item.top_y <= referenced_bounds.min.y + zone_internal::kInfinityEpsilon &&
                          item.bottom_y >= referenced_bounds.max.y - zone_internal::kInfinityEpsilon;
    if (infinite) {
      item.zone.height_mode = ZoneHeightMode::kInfinite;
      item.zone.height = 0.0f;
    }
    zones.push_back(std::move(item.zone));
  }
  return zones;
}

}  // namespace pistoris::glb_level
