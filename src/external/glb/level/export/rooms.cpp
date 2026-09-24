// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/rooms.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/export/internal.h"
#include "external/glb/level/objects.h"
#include "external/glb/level/palette.h"
#include "external/glb/level/room_distance_metadata.h"
#include "external/glb/writer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb_level_export {
namespace {

using glb::Builder;
using glb::Primitive;
using glb_level::bottomCenter;
using glb_level::kPortalParentOffset;
using glb_level::portalNodeName;
using GlbVec3 = glb::Vec3;

inline GlbVec3 toVec3(const ArxVector3& value) { return {value.x, value.y, value.z}; }

bool exportedPortal(const Portal& portal, const RoomProjection& projection) {
  return projection.has_faces[portal.room_1] && projection.has_faces[portal.room_2];
}

}  // namespace

RoomProjection buildRoomProjection(const LevelModules& level) {
  RoomProjection projection;
  projection.has_faces.assign(level.rooms.definitions.size(), false);
  for (RoomIndex room : level.rooms.face_rooms) projection.has_faces[room] = true;
  for (bool has_faces : projection.has_faces)
    if (!has_faces) ++projection.empty_rooms;

  for (const Portal& portal : level.rooms.portals) {
    if (exportedPortal(portal, projection))
      ++projection.exported_portals;
    else
      ++projection.discarded_portals;
  }

  if (rooms::hasCompleteRoomDistances(level.rooms.distances, level.rooms.definitions.size())) {
    for (std::size_t first = 0; first < level.rooms.definitions.size(); ++first) {
      for (std::size_t second = first + 1; second < level.rooms.definitions.size(); ++second) {
        if (projection.has_faces[first] && projection.has_faces[second]) continue;
        const RoomDistance& distance = level.rooms.distances[rooms::roomDistancePairIndex(first, second)];
        if (distance.distance > 0.0f) ++projection.discarded_positive_distances;
      }
    }
  }
  projection.preserve_distances =
      projection.empty_rooms == 0 && !level.rooms.distances.empty() &&
      rooms::hasCompleteRoomDistances(level.rooms.distances, level.rooms.definitions.size());
  return projection;
}

void exportPortals(const LevelModules& level, const ArxAabb& referenced_bounds, const RoomProjection& room_projection,
                   glb_level::Palette& palette, Builder& builder) {
  int portal_material = palette.material(glb_level::PaletteItem::kPortal);
  int portal_parent = -1;
  ArxVector3 portal_root{};
  if (room_projection.exported_portals != 0) {
    portal_root = bottomCenter(referenced_bounds);
    portal_root.y += kPortalParentOffset;
    portal_parent = builder.addNode("portals_parent");
    builder.setNodeTranslation(portal_parent, toVec3(portal_root));
    builder.addRoot(portal_parent);
  }
  for (std::size_t portal_index = 0; portal_index < level.rooms.portals.size(); ++portal_index) {
    const Portal& portal = level.rooms.portals[portal_index];
    if (!exportedPortal(portal, room_projection)) continue;
    const std::size_t count = rooms::portalVertexCount(portal.shape);
    ArxVector3 centroid = rooms::portalCentroid(portal);
    std::vector<GlbVec3> portal_positions;
    portal_positions.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      portal_positions.push_back(
          {portal.vertices[i].x - centroid.x, portal.vertices[i].y - centroid.y, portal.vertices[i].z - centroid.z});
    }
    constexpr std::array<std::uint16_t, 6> kQuadIndices = {0, 1, 3, 2, 3, 1};
    constexpr std::array<std::uint16_t, 3> kTriIndices = {0, 1, 2};
    Primitive primitive;
    primitive.indices =
        count == 4 ? builder.addAccessor(
                         std::span<const std::uint16_t>(kQuadIndices), cgltf_component_type_r_16u, cgltf_type_scalar)
                   : builder.addAccessor(
                         std::span<const std::uint16_t>(kTriIndices), cgltf_component_type_r_16u, cgltf_type_scalar);
    primitive.material = portal_material;
    primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(portal_positions));
    std::string name = portalNodeName(portal, level.rooms);
    int mesh = builder.addMesh(name, {std::move(primitive)});
    int node = builder.addNode(std::move(name), mesh);
    if (room_projection.preserve_distances)
      builder.setNodeExtrasJson(node, glb_level::roomDistancePortalMetadataJson(portal_index));
    builder.setNodeTranslation(node,
                               {centroid.x - portal_root.x, centroid.y - portal_root.y, centroid.z - portal_root.z});
    builder.addChild(portal_parent, node);
  }
}

}  // namespace pistoris::glb_level_export
