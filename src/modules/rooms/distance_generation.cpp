// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris_types.h"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::rooms {
namespace {

inline constexpr float kMinSampleSpacing = 20.0f;
inline constexpr float kMaxPortalSideOffset = 50.0f;
inline constexpr float kMinSampleHeightOffset = 50.0f;
inline constexpr float kMinLinkDistanceSampleSpacingRate = 1.1f;

struct GlobalRoomConnectivity {
  std::vector<std::vector<RoomIndex>> components;
  std::vector<RoomIndex> unreachable_rooms;
  std::vector<std::vector<std::uint32_t>> room_components;
};

void appendUnique(std::vector<RoomIndex>& values, RoomIndex value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

RoomIndex globalPortalSideRoom(const RoomsData& rooms, std::uint32_t portal_side) {
  const PortalIndex portal = portal_side / 2U;
  const bool front = portal_side % 2U == 0U;
  if (portal >= rooms.portals.size()) return kInvalidRoomIndex;
  return portalSideRoom(rooms.portals[portal], front);
}

GlobalRoomConnectivity analyzeGlobalRoomConnectivity(const RoomsData& rooms,
                                                     const std::vector<std::vector<RoomDistanceEdge>>& adjacency) {
  GlobalRoomConnectivity out;
  out.room_components.resize(rooms.definitions.size());

  std::vector<bool> visited(adjacency.size(), false);
  for (std::uint32_t start = 0; start < adjacency.size(); ++start) {
    if (visited[start]) continue;
    std::vector<RoomIndex> component_rooms;
    std::queue<std::uint32_t> queue;
    visited[start] = true;
    queue.push(start);
    while (!queue.empty()) {
      const std::uint32_t current = queue.front();
      queue.pop();
      const RoomIndex room = globalPortalSideRoom(rooms, current);
      if (room < rooms.definitions.size()) appendUnique(component_rooms, room);
      for (const RoomDistanceEdge& edge : adjacency[current]) {
        if (edge.to >= adjacency.size() || visited[edge.to]) continue;
        visited[edge.to] = true;
        queue.push(edge.to);
      }
    }

    if (component_rooms.empty()) continue;
    const std::uint32_t component = static_cast<std::uint32_t>(out.components.size());
    for (RoomIndex room : component_rooms) out.room_components[room].push_back(component);
    out.components.push_back(std::move(component_rooms));
  }

  for (RoomIndex room = 0; room < out.room_components.size(); ++room)
    if (out.room_components[room].empty()) out.unreachable_rooms.push_back(room);
  return out;
}

bool roomsShareComponent(const GlobalRoomConnectivity& connectivity, RoomIndex first_room, RoomIndex second_room) {
  if (first_room >= connectivity.room_components.size() || second_room >= connectivity.room_components.size())
    return false;
  for (std::uint32_t first_component : connectivity.room_components[first_room]) {
    const std::vector<std::uint32_t>& second_components = connectivity.room_components[second_room];
    if (std::find(second_components.begin(), second_components.end(), first_component) != second_components.end())
      return true;
  }
  return false;
}

void recordGlobalConnectivity(const GlobalRoomConnectivity& connectivity, RoomDistanceGenWarnings& warnings) {
  warnings.global_room_components = connectivity.components;
  warnings.unreachable_rooms = connectivity.unreachable_rooms;
}

std::string roomNameList(const RoomsData& rooms, const std::vector<RoomIndex>& indices) {
  std::string out;
  for (RoomIndex room : indices) {
    if (room >= rooms.definitions.size()) continue;
    if (!out.empty()) out += ", ";
    out += rooms.definitions[room].name;
  }
  return out;
}

std::string roomNameGroups(const RoomsData& rooms, const std::vector<std::vector<RoomIndex>>& groups) {
  std::string out;
  for (const std::vector<RoomIndex>& group : groups) {
    if (!out.empty()) out += " ";
    out += "[";
    out += roomNameList(rooms, group);
    out += "]";
  }
  return out;
}

std::string disconnectedPortalGroupExamples(const RoomsData& rooms, const RoomDistanceGenWarnings& warnings) {
  std::string out;
  for (const RoomDistanceDisconnectedPortalGroupsWarning& example : warnings.disconnected_portal_group_examples) {
    if (example.room >= rooms.definitions.size()) continue;
    if (!out.empty()) out += "; ";
    out += std::format("'{}' ({} groups)", rooms.definitions[example.room].name, example.group_count);
  }
  if (warnings.disconnected_portal_group_rooms > warnings.disconnected_portal_group_examples.size()) {
    if (!out.empty()) out += "; ";
    out += std::format("{} more",
                       warnings.disconnected_portal_group_rooms - warnings.disconnected_portal_group_examples.size());
  }
  return out;
}

std::string failedRoomPairExamples(const RoomsData& rooms, const RoomDistanceGenWarnings& warnings) {
  std::string out;
  for (const RoomDistanceRoomPairWarning& example : warnings.failed_connected_room_pair_examples) {
    if (example.room_1 >= rooms.definitions.size() || example.room_2 >= rooms.definitions.size()) continue;
    if (!out.empty()) out += "; ";
    out += std::format("'{}' -> '{}'", rooms.definitions[example.room_1].name, rooms.definitions[example.room_2].name);
  }
  if (warnings.failed_connected_room_pairs > warnings.failed_connected_room_pair_examples.size()) {
    if (!out.empty()) out += "; ";
    out += std::format("{} more",
                       warnings.failed_connected_room_pairs - warnings.failed_connected_room_pair_examples.size());
  }
  return out;
}

void logWarnings(const RoomsData& rooms, const RoomDistanceGenWarnings& warnings) {
  if (!warnings.unreachable_rooms.empty()) {
    log(ARX_LOG_WARN,
        std::format("Level room-distance generation: unreachable room(s): {}",
                    roomNameList(rooms, warnings.unreachable_rooms)));
  }
  if (warnings.global_room_components.size() >= 2U) {
    log(ARX_LOG_WARN,
        std::format("Level room-distance generation: disconnected room graph: {}",
                    roomNameGroups(rooms, warnings.global_room_components)));
  }
  if (warnings.disconnected_portal_group_rooms != 0) {
    log(ARX_LOG_WARN,
        std::format("Level room-distance generation: disconnected in-room portal graph in {} room(s): {}",
                    warnings.disconnected_portal_group_rooms,
                    disconnectedPortalGroupExamples(rooms, warnings)));
  }
  if (warnings.failed_connected_room_pairs != 0) {
    log(ARX_LOG_WARN,
        std::format("Level room-distance generation: internal path search failed for {} connected room pair(s): {}",
                    warnings.failed_connected_room_pairs,
                    failedRoomPairExamples(rooms, warnings)));
  }
}

RoomDistanceSupportTriangle toSupportTriangle(const geometry::SurfaceSupportTriangle& triangle) {
  return {.vertices = triangle.vertices};
}

void captureSupportDiagnostics(const RoomsData& rooms, const GeometryData& geometry,
                               RoomDistanceGenDiagnostics& diagnostics) {
  diagnostics.support_by_room.assign(rooms.definitions.size(), {});
  for (RoomIndex room = 0; room < rooms.definitions.size(); ++room) {
    std::vector<geometry::SurfaceSupportTriangle> triangles = buildRoomSupportIndex(rooms, geometry, room).triangles();
    std::vector<RoomDistanceSupportTriangle>& support = diagnostics.support_by_room[room];
    support.reserve(triangles.size());
    for (const geometry::SurfaceSupportTriangle& triangle : triangles) support.push_back(toSupportTriangle(triangle));
  }
}

}  // namespace

bool validateRoomDistanceOptions(const RoomDistanceOptions& options) {
  return math::finite(options.portal_side_offset) && options.portal_side_offset > 0.0f &&
         options.portal_side_offset <= kMaxPortalSideOffset && math::finite(options.sample_spacing) &&
         options.sample_spacing >= kMinSampleSpacing && math::finite(options.sample_height_offset) &&
         options.sample_height_offset >= kMinSampleHeightOffset && math::finite(options.max_link_distance) &&
         options.max_link_distance >= options.sample_spacing * kMinLinkDistanceSampleSpacingRate;
}

void buildGeneratedRoomDistances(RoomDistances& out, const RoomsData& rooms, const RoomDistanceGenerationGraph& graph,
                                 const RoomDistanceOptions& options, RoomDistanceGenWarnings& warnings,
                                 RoomDistanceGenDiagnostics* diagnostics) {
  initializeRoomDistances(out, rooms.definitions.size());
  const std::vector<std::vector<RoomDistanceEdge>> adjacency = buildGlobalPortalGraph(rooms, graph);
  const GlobalRoomConnectivity connectivity = analyzeGlobalRoomConnectivity(rooms, adjacency);
  recordGlobalConnectivity(connectivity, warnings);

  for (RoomIndex first_room = 0; first_room < rooms.definitions.size(); ++first_room) {
    for (RoomIndex second_room = first_room + 1U; second_room < rooms.definitions.size(); ++second_room) {
      const std::size_t distance_index = roomDistancePairIndex(first_room, second_room);
      RoomDistance& distance = out[distance_index];
      if (std::optional<PortalIndex> direct = firstDirectPortal(rooms, first_room, second_room)) {
        distance = {.distance = -1.0f, .low_room_portal = *direct, .high_room_portal = *direct};
        continue;
      }

      std::vector<std::uint32_t> starts = portalSidesForRoom(rooms, first_room);
      std::vector<std::uint32_t> goals = portalSidesForRoom(rooms, second_room);
      GlobalPath path = shortestGlobalPath(adjacency, starts, goals);
      if (!path.found) {
        if (roomsShareComponent(connectivity, first_room, second_room)) {
          ++warnings.failed_connected_room_pairs;
          if (warnings.failed_connected_room_pair_examples.size() < kRoomDistanceWarningExampleLimit) {
            warnings.failed_connected_room_pair_examples.push_back({first_room, second_room});
          }
        }
        distance = {};
        continue;
      }

      const PortalIndex start_portal = path.nodes.empty() ? kInvalidPortalIndex : path.nodes.front() / 2U;
      const PortalIndex end_portal = path.nodes.empty() ? kInvalidPortalIndex : path.nodes.back() / 2U;
      distance.distance = path.distance + options.portal_side_offset * 2.0f;
      distance.low_room_portal = start_portal;
      distance.high_room_portal = end_portal;
      if (diagnostics) {
        std::vector<ArxVector3> points = globalPathPositions(graph, path.nodes);
        diagnostics->room_pair_paths.push_back({std::move(points), first_room, second_room, start_portal, end_portal});
      }
    }
  }
}

Error generateRoomDistances(RoomDistances& out, const RoomsData& rooms, const GeometryData& geometry,
                            const RoomDistanceOptions& options, RoomDistanceGenDiagnostics* diagnostics) {
  if (diagnostics) *diagnostics = {};
  if (!validateRoomDistanceOptions(options)) return Error::kInvalidOptions;
  if (diagnostics) captureSupportDiagnostics(rooms, geometry, *diagnostics);

  RoomDistanceGenerationGraph graph;
  RoomDistanceGenWarnings warnings;
  graph.rooms.resize(rooms.definitions.size());
  addPortalSideAccessPoints(rooms, options, graph, diagnostics);
  addSampleNodes(rooms, geometry, options, graph, diagnostics);
  addVisibilityEdges(rooms, geometry, options, graph, diagnostics);
  addInRoomPortalPaths(rooms, geometry, graph, warnings, diagnostics);
  buildGeneratedRoomDistances(out, rooms, graph, options, warnings, diagnostics);
  Error error = validateRoomDistances(out, rooms);
  if (error != Error::kNone) return error;
  logWarnings(rooms, warnings);
  return Error::kNone;
}

}  // namespace pistoris::rooms
