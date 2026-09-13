// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/runtime/types.h"

#include "../container.h"
#include "api.h"
#include "coordinates.h"
#include "discovery.h"
#include "entities.h"
#include "external/glb/level/anchor_metadata.h"
#include "external/glb/level/import/internal.h"
#include "external/glb/node_graph.h"
#include "external/glb/utils/texture.h"
#include "fogs.h"
#include "level/anchor_bounds.h"
#include "level/data.h"
#include "level/validation.h"
#include "lighting.h"
#include "minimap.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "names.h"
#include "objects.h"
#include "paths.h"
#include "player_spawn.h"
#include "topology.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/bounds.h"
#include "utils/math/mat4.h"
#include "zones.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

using glb::Asset;
using glb::parse;
using glb_level::compactLevelVertices;
using glb_level::ImportUnits;
using glb_level::isReservedLightName;
using glb_level::parseAnchorNodeName;
using glb_level::roomNameFromNode;
using glb_level_import::importNavSurface;
using glb_level_import::importPointLight;
using glb_level_import::importPortal;
using glb_level_import::importReservedLight;
using glb_level_import::ImportWarnings;
using glb_level_import::navSurfaceComponents;
using glb_level_import::readGeometryPrimitive;
using glb_level_import::detail::logLevelObjectFailure;
using glb_level_import::detail::nodeName;

ArxAabb referencedGeometryBounds(const LevelModules& level) {
  ArxAabb bounds{};
  bool initialized = false;
  for (const Face& face : level.geometry.faces) {
    for (const Corner& corner : face.corners) {
      const ArxVector3& position = level.geometry.vertices[corner.vertex].position;
      if (!initialized) {
        bounds.min = position;
        bounds.max = position;
        initialized = true;
      } else {
        math::expand(bounds, position);
      }
    }
  }
  return bounds;
}

void logWarnings(const ImportWarnings& diagnostics) {
  if (diagnostics.regenerated_normals != 0 || diagnostics.normalized_normals != 0 || diagnostics.discarded_faces != 0 ||
      diagnostics.discarded_vertices != 0 || diagnostics.skipped_quad_flags != 0 || diagnostics.portal_materials != 0 ||
      diagnostics.discarded_uv_sets != 0 || diagnostics.defaulted_colors != 0 || diagnostics.discarded_colors != 0 ||
      diagnostics.discarded_tangents != 0 || diagnostics.discarded_custom != 0 ||
      diagnostics.generated_light_names != 0 || diagnostics.normalized_legacy_teo != 0)
    logLazy(ARX_LOG_WARN, [&] {
      std::string warning = "GLB -> Level repairs:";
      if (diagnostics.regenerated_normals != 0)
        warning += std::format(" {} corner normal(s) regenerated;", diagnostics.regenerated_normals);
      if (diagnostics.normalized_normals != 0)
        warning += std::format(" {} corner normal(s) normalized;", diagnostics.normalized_normals);
      if (diagnostics.discarded_faces != 0)
        warning += std::format(" {} degenerate face(s) discarded;", diagnostics.discarded_faces);
      if (diagnostics.discarded_vertices != 0)
        warning += std::format(" {} unreferenced vertex/vertices discarded;", diagnostics.discarded_vertices);
      if (diagnostics.skipped_quad_flags != 0)
        warning += std::format(" {} QUAD material flag(s) intentionally skipped;", diagnostics.skipped_quad_flags);
      if (diagnostics.portal_materials != 0)
        warning += std::format(" {} arx_portal material primitive(s) treated as no_tex;", diagnostics.portal_materials);
      if (diagnostics.discarded_uv_sets != 0)
        warning += std::format(" {} unselected UV set binding(s) discarded;", diagnostics.discarded_uv_sets);
      if (diagnostics.defaulted_colors != 0)
        warning += std::format(" {} baked color(s) defaulted;", diagnostics.defaulted_colors);
      if (diagnostics.discarded_colors != 0)
        warning += std::format(" {} color attribute binding(s) discarded;", diagnostics.discarded_colors);
      if (diagnostics.discarded_tangents != 0)
        warning += std::format(" {} tangent attribute binding(s) discarded;", diagnostics.discarded_tangents);
      if (diagnostics.discarded_custom != 0)
        warning += std::format(" {} custom attribute binding(s) discarded;", diagnostics.discarded_custom);
      if (diagnostics.generated_light_names != 0)
        warning += std::format(" {} light name(s) generated;", diagnostics.generated_light_names);
      if (diagnostics.normalized_legacy_teo != 0)
        warning += std::format(" normalized {} legacy .teo entity class path(s);", diagnostics.normalized_legacy_teo);
      warning.pop_back();
      return warning;
    });
  if (diagnostics.ignored_skinning) {
    log(ARX_LOG_WARN, "GLB -> Level: skinning data was ignored");
  }
  if (diagnostics.nav_surface_components > 1) {
    log(ARX_LOG_WARN,
        "GLB -> Level: navigation surface has {} disconnected component(s)",
        diagnostics.nav_surface_components);
  }
}

void logAnchorMetadataWarnings(const glb_level::AnchorMetadataImport& metadata,
                               const glb_level::AnchorMetadataDiagnostics& diagnostics) {
  if (metadata.malformed_records == 0 && metadata.malformed_links == 0 && diagnostics.ambiguous_ids == 0 &&
      diagnostics.ambiguous_links == 0 && diagnostics.dangling_links == 0 && diagnostics.self_links == 0 &&
      !diagnostics.graph_discarded)
    return;
  logLazy(ARX_LOG_WARN, [&] {
    std::string warning = "GLB -> Level anchor metadata:";
    if (metadata.malformed_records != 0)
      warning += std::format(" {} malformed record(s) discarded;", metadata.malformed_records);
    if (metadata.malformed_links != 0)
      warning += std::format(" {} malformed link(s) discarded;", metadata.malformed_links);
    if (diagnostics.ambiguous_ids != 0) warning += std::format(" {} ambiguous ID(s);", diagnostics.ambiguous_ids);
    if (diagnostics.ambiguous_links != 0)
      warning += std::format(" {} ambiguous connection declaration(s) discarded;", diagnostics.ambiguous_links);
    if (diagnostics.dangling_links != 0)
      warning += std::format(" {} dangling connection declaration(s) discarded;", diagnostics.dangling_links);
    if (diagnostics.self_links != 0)
      warning += std::format(" {} self-referencing connection declaration(s) discarded;", diagnostics.self_links);
    if (diagnostics.graph_discarded) warning += " recovered graph exceeded Level limits and was discarded;";
    warning.pop_back();
    return warning;
  });
}

}  // namespace

ArxReturnCode importLevelFromGlb(std::span<const std::uint8_t> bytes, LevelModules& out,
                                 const Level::GlbImportOptions& options, LevelValidationState* out_validation,
                                 ArxLevelGlbImportInfo* info, std::vector<std::string>* texture_source_paths) {
  ArxReturnCode rc = glb_level::validateGlbImportOptions(options);
  if (rc != ARX_OK) return rc;
  const ImportUnits units{options.arx_units_per_glb_unit};
  Asset asset;
  rc = parse(bytes, asset);
  if (rc != ARX_OK) return rc;
  cgltf_data& data = *asset.data();
  for (std::size_t i = 0; i < data.extensions_required_count; ++i) {
    if (data.extensions_required[i] == nullptr) return ARX_GLB_BAD_FORMAT;
    if (std::string_view(data.extensions_required[i]) != "KHR_lights_punctual") return ARX_GLB_UNSUPPORTED_FEATURE;
  }

  glb::NodeGraph graph;
  rc = glb::buildNodeGraph(data, graph);
  if (rc != ARX_OK) return rc;
  glb_level::LevelDiscovery discovery;
  rc = glb_level::discoverLevelNodes(data, graph, discovery);
  if (rc != ARX_OK) return rc;
  const std::vector<math::Mat4>& world = graph.world;

  LevelModules tmp;
  ImportWarnings diagnostics;
  glb::TextureImporter texture_importer(tmp.textures, texture_source_paths, "GLB -> Level");
  std::unordered_map<const cgltf_material*, glb_level_import::ImportedMaterial> imported_materials;
  std::map<std::string, std::uint32_t> rooms_by_name;
  std::vector<std::string> room_source_names;
  std::size_t point_light_ordinal = 0;
  rc = glb_level::importPlayerSpawn(data, world, discovery.player_spawns, units, tmp);
  if (rc != ARX_OK) {
    logLevelObjectFailure("player spawn import", rc);
    return rc;
  }
  rc = glb_level::importEntities(data, world, discovery.entities, units, tmp, diagnostics.normalized_legacy_teo);
  if (rc != ARX_OK) {
    logLevelObjectFailure("entity import", rc);
    return rc;
  }
  std::vector<glb_level::PendingZone> pending_zones;
  rc = glb_level::importZones(asset, world, discovery.zones, units, pending_zones);
  if (rc != ARX_OK) {
    logLevelObjectFailure("zone import", rc);
    return rc;
  }
  rc = glb_level::importPaths(data, world, discovery.paths, units, tmp);
  if (rc != ARX_OK) {
    logLevelObjectFailure("path import", rc);
    return rc;
  }
  rc = glb_level::importFogs(data, world, discovery.fogs, units, tmp);
  if (rc != ARX_OK) {
    logLevelObjectFailure("fog import", rc);
    return rc;
  }
  const bool has_explicit_rooms = !discovery.rooms.empty();
  const bool has_reserved_portals = !discovery.portals.empty();

  auto add_room = [&](std::string name, std::uint32_t& out_room) -> ArxReturnCode {
    IdentifierNormalization normalized = normalizeIdentifier(name);
    if (auto existing = rooms_by_name.find(normalized.value); existing != rooms_by_name.end()) {
      log(ARX_LOG_DEBUG,
          "GLB -> Level object failure: room names '{}' and '{}' normalize to '{}'",
          room_source_names[existing->second],
          name,
          normalized.value);
      return ARX_GLB_BAD_LEVEL_ROOM;
    }
    if (normalized.repair != IdentifierRepair::kNone)
      log(ARX_LOG_WARN, "GLB -> Level: room name '{}' normalized to '{}'", name, normalized.value);
    if (rooms::validateRoom({normalized.value}) != rooms::Error::kNone) return ARX_GLB_BAD_LEVEL_ROOM;
    if (tmp.rooms.definitions.size() >= static_cast<std::size_t>(kInvalidRoomIndex)) return ARX_GLB_BAD_LEVEL_ROOM;
    out_room = static_cast<std::uint32_t>(tmp.rooms.definitions.size());
    tmp.rooms.definitions.push_back({std::move(normalized.value)});
    rooms_by_name.emplace(tmp.rooms.definitions.back().name, out_room);
    room_source_names.push_back(std::move(name));
    return ARX_OK;
  };

  std::vector<std::uint32_t> rooms_by_node(data.nodes_count, kInvalidRoomIndex);
  for (std::size_t node_index : discovery.rooms) {
    const cgltf_node& node = data.nodes[node_index];
    const std::string_view name = nodeName(node);
    log(ARX_LOG_DEBUG, "GLB -> Level: importing room node {} '{}'", node_index, name);
    if (node.camera != nullptr || node.light != nullptr) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: room node {} '{}' has non-geometry payload", node_index, name);
      return ARX_GLB_BAD_LEVEL_ROOM;
    }
    std::optional<std::string> room_name = roomNameFromNode(name);
    if (!room_name) {
      log(ARX_LOG_DEBUG,
          "GLB -> Level object failure: room node {} '{}' has malformed reserved name",
          node_index,
          name);
      return ARX_GLB_BAD_LEVEL_ROOM;
    }
    std::uint32_t room = 0;
    rc = add_room(std::move(*room_name), room);
    if (rc != ARX_OK) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: duplicate or invalid room node {} '{}'", node_index, name);
      return rc;
    }
    rooms_by_node[node_index] = room;
  }

  auto import_geometry_node =
      [&](const cgltf_node& node, std::size_t node_index, std::string_view name, std::uint32_t room) -> ArxReturnCode {
    if (node.mesh == nullptr) return ARX_GLB_BAD_LEVEL_GEOMETRY;
    std::size_t accessor_count = 0;
    for (std::size_t primitive = 0; primitive < node.mesh->primitives_count; ++primitive) {
      const cgltf_primitive& source = node.mesh->primitives[primitive];
      accessor_count += source.attributes_count + (source.indices != nullptr ? 1U : 0U);
    }
    glb_level_import::LevelMeshImportContext context(asset, accessor_count, imported_materials);
    for (std::size_t primitive_index = 0; primitive_index < node.mesh->primitives_count; ++primitive_index) {
      ArxReturnCode node_rc = readGeometryPrimitive(context,
                                                    node,
                                                    node.mesh->primitives[primitive_index],
                                                    world[node_index],
                                                    units,
                                                    room,
                                                    tmp,
                                                    texture_importer,
                                                    diagnostics);
      if (node_rc != ARX_OK) {
        log(ARX_LOG_DEBUG,
            "GLB -> Level geometry failure: node {} '{}' primitive {} returned code {}",
            node_index,
            name,
            primitive_index,
            node_rc);
        return node_rc;
      }
    }
    return ARX_OK;
  };

  if (has_explicit_rooms) {
    for (const glb_level::DiscoveredGeometryNode& geometry : discovery.geometry) {
      if (geometry.room == glb::kInvalidNodeIndex || geometry.room >= rooms_by_node.size() ||
          rooms_by_node[geometry.room] == kInvalidRoomIndex) {
        log(ARX_LOG_DEBUG,
            "GLB -> Level object failure: non-room mesh node {} '{}' found beside explicit rooms",
            geometry.node,
            nodeName(data.nodes[geometry.node]));
        return ARX_GLB_BAD_LEVEL_GEOMETRY;
      }
      rc = import_geometry_node(
          data.nodes[geometry.node], geometry.node, nodeName(data.nodes[geometry.node]), rooms_by_node[geometry.room]);
      if (rc != ARX_OK) return rc;
    }
  } else {
    if (has_reserved_portals) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: room-linked portals require explicit room nodes");
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
    if (!discovery.geometry.empty()) {
      std::uint32_t room = 0;
      rc = add_room("room", room);
      if (rc != ARX_OK) return rc;
      for (const glb_level::DiscoveredGeometryNode& geometry : discovery.geometry) {
        rc = import_geometry_node(data.nodes[geometry.node], geometry.node, nodeName(data.nodes[geometry.node]), room);
        if (rc != ARX_OK) return rc;
      }
    }
  }

  if (discovery.navigation_surfaces.size() > 1) {
    log(ARX_LOG_DEBUG, "GLB -> Level object failure: duplicate navigation surface nodes");
    return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
  }
  if (!discovery.navigation_surfaces.empty()) {
    const std::size_t node_index = discovery.navigation_surfaces.front();
    const cgltf_node& node = data.nodes[node_index];
    const std::string_view name = nodeName(node);
    log(ARX_LOG_DEBUG, "GLB -> Level: importing nav surface node {} '{}'", node_index, name);
    if (!glb_level::isNavSurfaceRootName(name)) {
      logLevelObjectFailure("navigation surface import", node_index, name, ARX_GLB_BAD_LEVEL_NAV_SURFACE);
      return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    }
    NavSurface surface;
    rc = importNavSurface(asset, node, node_index, world[node_index], units, surface);
    if (rc != ARX_OK) {
      logLevelObjectFailure("navigation surface import", node_index, name, rc);
      return rc;
    }
    diagnostics.nav_surface_components = navSurfaceComponents(surface);
    tmp.navigation.surface = std::move(surface);
  }

  if (discovery.minimaps.size() > 1) {
    log(ARX_LOG_WARN, "GLB -> Level: {} minimap nodes found; minimap omitted", discovery.minimaps.size());
  } else if (!discovery.minimaps.empty()) {
    const std::size_t node_index = discovery.minimaps.front();
    const cgltf_node& node = data.nodes[node_index];
    const std::string_view name = nodeName(node);
    if (!glb_level::isMinimapRootName(name)) {
      log(ARX_LOG_WARN, "GLB -> Level: minimap node {} '{}' has a malformed name; minimap omitted", node_index, name);
    } else {
      const glb_level::MinimapImportError error =
          glb_level::importMinimap(asset, node, world[node_index], units, tmp.minimap);
      switch (error) {
        case glb_level::MinimapImportError::kNone:
          break;
        case glb_level::MinimapImportError::kBadData:
          log(ARX_LOG_WARN, "GLB -> Level: minimap node {} '{}' is invalid; minimap omitted", node_index, name);
          break;
        case glb_level::MinimapImportError::kOutOfMemory:
          return ARX_BAD_ALLOC;
      }
    }
  }

  for (std::size_t node_index : discovery.lights) {
    const cgltf_node& node = data.nodes[node_index];
    const std::string_view name = nodeName(node);
    if (isReservedLightName(name)) {
      if (node.mesh != nullptr)
        log(ARX_LOG_WARN,
            "GLB -> Level: reserved light node {} '{}' also has a mesh; mesh discarded",
            node_index,
            name);
      Light light;
      rc = importReservedLight(node, node_index, world[node_index], units, point_light_ordinal++, diagnostics, light);
      if (rc != ARX_OK) {
        logLevelObjectFailure("reserved light import", node_index, name, rc);
        return rc;
      }
      if (glb::hasNonIdentityLocalScale(node))
        log(ARX_LOG_WARN,
            "GLB -> Level: light node {} '{}' has nonidentity local scale; scale ignored",
            node_index,
            name);
      tmp.lighting.lights.push_back(std::move(light));
    } else {
      if (node.mesh != nullptr)
        log(ARX_LOG_WARN,
            "GLB -> Level: generic point light node {} '{}' also has a mesh; mesh discarded",
            node_index,
            name);
      Light light;
      rc = importPointLight(node, node_index, world[node_index], units, point_light_ordinal++, diagnostics, light);
      if (rc != ARX_OK) {
        logLevelObjectFailure("point light import", node_index, name, rc);
        return rc;
      }
      if (glb::hasNonIdentityLocalScale(node))
        log(ARX_LOG_WARN,
            "GLB -> Level: light node {} '{}' has nonidentity local scale; scale ignored",
            node_index,
            name);
      tmp.lighting.lights.push_back(std::move(light));
    }
  }

  glb_level::AnchorMetadataImport anchor_metadata;
  anchor_metadata.records.reserve(discovery.anchors.size());
  for (std::size_t node_index : discovery.anchors) {
    const cgltf_node& node = data.nodes[node_index];
    const std::string_view name = nodeName(node);
    log(ARX_LOG_DEBUG, "GLB -> Level: importing anchor node {} '{}'", node_index, name);
    if (node.mesh != nullptr || node.camera != nullptr || node.light != nullptr) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: anchor node {} '{}' must be an empty node", node_index, name);
      return ARX_GLB_BAD_LEVEL_ANCHOR;
    }
    std::optional<glb_level::ParsedAnchorName> parsed = parseAnchorNodeName(name);
    if (!parsed.has_value()) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: anchor node {} '{}' has invalid name", node_index, name);
      return ARX_GLB_BAD_LEVEL_ANCHOR;
    }
    glb_level::ParsedAnchorName& parsed_name = *parsed;
    Anchor anchor{};
    const std::optional<ArxVector3> position = glb_level::toArxPoint(math::translation(world[node_index]), units);
    if (!position) return ARX_GLB_BAD_FORMAT;
    anchor.position = *position;
    const auto& parsed_radius = parsed_name.radius;
    if (parsed_radius) {
      const std::optional<float> radius = glb_level::toArxLength(*parsed_radius, units);
      if (!radius) return ARX_GLB_BAD_FORMAT;
      anchor.radius = *radius;
    } else {
      anchor.radius = kDefaultAnchorRadius;
    }
    const auto& parsed_height = parsed_name.height;
    if (parsed_height) {
      const std::optional<float> height = glb_level::toArxLength(*parsed_height, units);
      if (!height) return ARX_GLB_BAD_FORMAT;
      anchor.height = -*height;
    } else {
      anchor.height = kDefaultAnchorHeight;
    }
    anchor.flags = parsed_name.flags;
    anchor.name = std::move(parsed_name.name);
    if (glb::hasNonIdentityLocalScale(node))
      log(ARX_LOG_WARN,
          "GLB -> Level: anchor node {} '{}' has nonidentity local scale; scale ignored",
          node_index,
          name);
    tmp.navigation.anchors.push_back(std::move(anchor));
    glb_level::appendAnchorMetadata(node.extras, anchor_metadata);
  }

  const glb_level::AnchorMetadataDiagnostics anchor_metadata_diagnostics =
      glb_level::importAnchorConnections(anchor_metadata, tmp.navigation.anchors.size(), tmp.navigation.connections);

  for (std::size_t node_index : discovery.portals) {
    const cgltf_node& node = data.nodes[node_index];
    const std::string_view name = nodeName(node);
    log(ARX_LOG_DEBUG, "GLB -> Level: importing portal node {} '{}'", node_index, name);
    Portal portal;
    rc = importPortal(asset, node, world[node_index], rooms_by_name, diagnostics, units, portal);
    if (rc != ARX_OK) {
      logLevelObjectFailure("portal import", node_index, name, rc);
      return rc;
    }
    tmp.rooms.portals.push_back(std::move(portal));
  }

  if (tmp.geometry.faces.empty()) return ARX_GLB_NO_LEVEL_GEOMETRY;
  if (!glb::makeTexturePathsUnique(tmp.textures.textures, "GLB -> Level")) return ARX_GLB_BAD_LEVEL_MATERIAL;
  diagnostics.discarded_vertices = compactLevelVertices(tmp);
  if (!pending_zones.empty()) {
    const ArxAabb zone_bounds = referencedGeometryBounds(tmp);
    tmp.scene.zones = glb_level::finalizeImportedZones(std::move(pending_zones), zone_bounds);
  }
  const std::size_t repaired_portals = rooms::repairPortalNames(tmp.rooms.portals);
  const std::size_t repaired_anchors = navigation::repairAnchorNames(tmp.navigation.anchors);
  const std::size_t repaired_lights = lights::repairLightNames(tmp.lighting.lights);
  const std::size_t repaired_fogs = scene::repairFogNames(tmp.scene.fogs);
  const std::size_t repaired_zones = scene::repairZoneNames(tmp.scene.zones);
  if (repaired_portals != 0) log(ARX_LOG_WARN, "GLB -> Level repairs: {} portal name(s) repaired", repaired_portals);
  if (repaired_anchors != 0) log(ARX_LOG_WARN, "GLB -> Level repairs: {} anchor name(s) repaired", repaired_anchors);
  if (repaired_lights != 0) log(ARX_LOG_WARN, "GLB -> Level repairs: {} light name(s) repaired", repaired_lights);
  if (repaired_fogs != 0) log(ARX_LOG_WARN, "GLB -> Level repairs: {} fog name(s) repaired", repaired_fogs);
  if (repaired_zones != 0) log(ARX_LOG_WARN, "GLB -> Level repairs: {} zone name(s) repaired", repaired_zones);
  ArxLevelGlbImportInfo import_info{};
  rc = glb_level::applyGlbImportPlacement(tmp, options, import_info);
  if (rc != ARX_OK) return rc;
  ArxAabb bounds = referencedGeometryBounds(tmp);
  std::size_t outside_geometry_anchors = 0;
  for (std::size_t anchor_index = 0; anchor_index < tmp.navigation.anchors.size(); ++anchor_index) {
    const Anchor& anchor = tmp.navigation.anchors[anchor_index];
    if (!level_anchor_bounds::insideNativeMap(anchor.position)) {
      log(ARX_LOG_DEBUG,
          "GLB -> Level object failure: anchor {} position ({}, {}, {}) is outside native X/Z bounds",
          anchor_index,
          anchor.position.x,
          anchor.position.y,
          anchor.position.z);
      return ARX_GLB_BAD_LEVEL_ANCHOR;
    }
    if (level_anchor_bounds::materiallyOutsideGeometry(bounds, anchor.position)) ++outside_geometry_anchors;
  }
  if (outside_geometry_anchors != 0)
    log(ARX_LOG_WARN,
        "GLB -> Level: {} anchor(s) outside referenced geometry bounds retained",
        outside_geometry_anchors);
  LevelValidationState validation;
  rc = validateLevelModules(tmp, validation);
  if (rc != ARX_OK) {
    log(ARX_LOG_DEBUG, "GLB -> Level constructed Level validation failed with code {}", rc);
    return rc;
  }
  logAnchorMetadataWarnings(anchor_metadata, anchor_metadata_diagnostics);
  logWarnings(diagnostics);
  out = std::move(tmp);
  if (out_validation) *out_validation = validation;
  if (info) *info = import_info;
  return ARX_OK;
}

}  // namespace pistoris
