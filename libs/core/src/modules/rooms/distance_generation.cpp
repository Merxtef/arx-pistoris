// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/runtime/types.h"

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
#include <span>
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
  std::vector<std::uint32_t> component_by_node;
};

RoomIndex globalPortalSideRoom(const RoomsData& rooms, std::uint32_t portal_side) {
  const PortalIndex portal = portal_side / 2U;
  const bool front = portal_side % 2U == 0U;
  if (portal >= rooms.portals.size()) return kInvalidRoomIndex;
  return portalSideRoom(rooms.portals[portal], front ? PortalSide::kFront : PortalSide::kBack);
}

GlobalRoomConnectivity analyzeGlobalRoomConnectivity(const RoomsData& rooms, const RoomDistanceAdjacency& adjacency,
                                                     RoomDistanceGenerationWarnings& warnings) {
  GlobalRoomConnectivity out;
  const std::size_t node_count = adjacencyNodeCount(adjacency);
  out.component_by_node.assign(node_count, kInvalidRoomDistanceIndex);
  warnings.global_room_component_rooms.clear();
  warnings.global_room_component_offsets.clear();
  warnings.global_room_component_offsets.push_back(0);
  warnings.unreachable_rooms.clear();

  std::vector<std::size_t> room_generation(rooms.definitions.size(), 0);
  std::vector<std::uint32_t> worklist;
  worklist.reserve(node_count);
  std::uint32_t component = 0;
  for (std::uint32_t start = 0; start < node_count; ++start) {
    if (out.component_by_node[start] != kInvalidRoomDistanceIndex) continue;
    const std::size_t generation = static_cast<std::size_t>(start) + 1U;
    const std::size_t room_begin = warnings.global_room_component_rooms.size();
    worklist.clear();
    out.component_by_node[start] = component;
    worklist.push_back(start);
    for (std::size_t current_index = 0; current_index < worklist.size(); ++current_index) {
      const std::uint32_t current = worklist[current_index];
      const RoomIndex room = globalPortalSideRoom(rooms, current);
      if (room < rooms.definitions.size() && room_generation[room] != generation) {
        room_generation[room] = generation;
        warnings.global_room_component_rooms.push_back(room);
      }
      for (const RoomDistanceEdge& edge : adjacentEdges(adjacency, current)) {
        if (edge.to >= node_count || out.component_by_node[edge.to] != kInvalidRoomDistanceIndex) continue;
        out.component_by_node[edge.to] = component;
        worklist.push_back(edge.to);
      }
    }
    if (warnings.global_room_component_rooms.size() != room_begin)
      warnings.global_room_component_offsets.push_back(warnings.global_room_component_rooms.size());
    ++component;
  }

  for (RoomIndex room = 0; room < room_generation.size(); ++room)
    if (room_generation[room] == 0) warnings.unreachable_rooms.push_back(room);
  return out;
}

bool roomsShareComponent(const GlobalRoomConnectivity& connectivity, const RoomPortalSideIndex& portal_sides,
                         RoomIndex first_room, RoomIndex second_room) {
  const std::span<const std::uint32_t> first_sides = portal_sides.roomSides(first_room);
  const std::span<const std::uint32_t> second_sides = portal_sides.roomSides(second_room);
  for (std::uint32_t first : first_sides) {
    if (first >= connectivity.component_by_node.size()) continue;
    const std::uint32_t component = connectivity.component_by_node[first];
    if (component == kInvalidRoomDistanceIndex) continue;
    for (std::uint32_t second : second_sides)
      if (second < connectivity.component_by_node.size() && connectivity.component_by_node[second] == component)
        return true;
  }
  return false;
}

std::string roomNameList(const RoomsData& rooms, std::span<const RoomIndex> indices) {
  std::string out;
  for (RoomIndex room : indices) {
    if (room >= rooms.definitions.size()) continue;
    if (!out.empty()) out += ", ";
    out += rooms.definitions[room].name;
  }
  return out;
}

std::string roomNameGroups(const RoomsData& rooms, std::span<const RoomIndex> grouped_rooms,
                           std::span<const std::size_t> offsets) {
  std::string out;
  for (std::size_t group = 0; group + 1U < offsets.size(); ++group) {
    const std::size_t first = std::min(offsets[group], grouped_rooms.size());
    const std::size_t last = std::min(offsets[group + 1U], grouped_rooms.size());
    if (!out.empty()) out += " ";
    out += "[";
    out += roomNameList(rooms, grouped_rooms.subspan(first, last - first));
    out += "]";
  }
  return out;
}

std::string disconnectedPortalGroupExamples(const RoomsData& rooms, const RoomDistanceGenerationWarnings& warnings) {
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

std::string failedRoomPairExamples(const RoomsData& rooms, const RoomDistanceGenerationWarnings& warnings) {
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

void logWarnings(const RoomsData& rooms, const RoomDistanceGenerationWarnings& warnings) {
  if (!warnings.unreachable_rooms.empty()) {
    logLazy(ARX_LOG_WARN, [&] {
      return std::format("Room-distance generation: unreachable room(s): {}",
                         roomNameList(rooms, warnings.unreachable_rooms));
    });
  }
  if (warnings.global_room_component_offsets.size() >= 3U) {
    logLazy(ARX_LOG_WARN, [&] {
      return std::format(
          "Room-distance generation: disconnected room graph: {}",
          roomNameGroups(rooms, warnings.global_room_component_rooms, warnings.global_room_component_offsets));
    });
  }
  if (warnings.disconnected_portal_group_rooms != 0) {
    logLazy(ARX_LOG_WARN, [&] {
      return std::format("Room-distance generation: disconnected in-room portal graph in {} room(s): {}",
                         warnings.disconnected_portal_group_rooms,
                         disconnectedPortalGroupExamples(rooms, warnings));
    });
  }
  if (warnings.failed_connected_room_pairs != 0) {
    logLazy(ARX_LOG_WARN, [&] {
      return std::format("Room-distance generation: internal path search failed for {} connected room pair(s): {}",
                         warnings.failed_connected_room_pairs,
                         failedRoomPairExamples(rooms, warnings));
    });
  }
}

void captureSupportDiagnostics(const RoomsData& rooms, const RoomGeometryIndex& room_geometry,
                               RoomDistanceGenerationDiagnostics& diagnostics) {
  diagnostics.support_by_room.assign(rooms.definitions.size(), {});
  for (RoomIndex room = 0; room < rooms.definitions.size(); ++room) {
    std::vector<RoomDistanceSupportTriangle>& support = diagnostics.support_by_room[room];
    const std::span<const FaceIndex> faces = room_geometry.roomFaces(room);
    support.reserve(faces.size());
    for (FaceIndex face : faces) support.push_back({room_geometry.triangle(face)});
  }
}

}  // namespace

bool validRoomDistanceOptions(const RoomDistanceOptions& options) noexcept {
  return math::finite(options.portal_side_offset) && options.portal_side_offset > 0.0f &&
         options.portal_side_offset <= kMaxPortalSideOffset && math::finite(options.sample_spacing) &&
         options.sample_spacing >= kMinSampleSpacing && math::finite(options.sample_height_offset) &&
         options.sample_height_offset >= kMinSampleHeightOffset && math::finite(options.max_link_distance) &&
         options.max_link_distance >= options.sample_spacing * kMinLinkDistanceSampleSpacingRate;
}

void buildGeneratedRoomDistances(RoomDistances& out, const RoomsData& rooms, const RoomDistanceGenerationGraph& graph,
                                 const RoomDistanceOptions& options, RoomDistanceGenerationWarnings& warnings,
                                 RoomDistanceGenerationDiagnostics* diagnostics) {
  resetRoomDistances(out, rooms.definitions.size());
  const RoomDistanceAdjacency adjacency = buildGlobalPortalGraph(rooms, graph);
  const RoomPortalSideIndex portal_sides(rooms);
  const GlobalRoomConnectivity connectivity = analyzeGlobalRoomConnectivity(rooms, adjacency, warnings);
  DijkstraScratch path_scratch;
  const std::size_t node_count = adjacencyNodeCount(adjacency);
  reserveDijkstraScratch(path_scratch, node_count, node_count + adjacency.edges.size());
  for (RoomIndex first_room = 0; first_room < rooms.definitions.size(); ++first_room) {
    for (RoomIndex second_room = first_room + 1U; second_room < rooms.definitions.size(); ++second_room) {
      const std::size_t distance_index = roomDistancePairIndex(first_room, second_room);
      RoomDistance& distance = out[distance_index];
      if (std::optional<PortalIndex> direct = firstDirectPortal(rooms, first_room, second_room)) {
        distance = {.distance = -1.0f, .low_room_portal = *direct, .high_room_portal = *direct};
        continue;
      }

      const std::span<const std::uint32_t> starts = portal_sides.roomSides(first_room);
      const std::span<const std::uint32_t> goals = portal_sides.roomSides(second_room);
      GlobalPath path = shortestGlobalPath(adjacency, starts, goals, path_scratch);
      if (!path.found) {
        if (roomsShareComponent(connectivity, portal_sides, first_room, second_room)) {
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
                            const RoomDistanceOptions& options, RoomDistanceGenerationDiagnostics* diagnostics) {
  if (diagnostics) *diagnostics = {};
  if (!validRoomDistanceOptions(options)) return Error::kInvalidOptions;

  RoomDistanceGenerationDiagnostics generated_diagnostics;
  RoomDistanceGenerationDiagnostics* generated_diagnostics_ptr =
      diagnostics != nullptr ? &generated_diagnostics : nullptr;
  RoomGeometryIndex room_geometry(rooms, geometry);
  if (generated_diagnostics_ptr) captureSupportDiagnostics(rooms, room_geometry, *generated_diagnostics_ptr);

  RoomDistanceGenerationGraph graph;
  RoomDistanceGenerationWarnings warnings;
  graph.rooms.resize(rooms.definitions.size());
  addPortalSideAccessPoints(rooms, options, graph, generated_diagnostics_ptr);
  addSampleNodes(rooms, room_geometry, options, graph, generated_diagnostics_ptr);
  addVisibilityEdges(rooms, room_geometry, options, graph, generated_diagnostics_ptr);
  addInRoomPortalPaths(rooms, room_geometry, graph, warnings, generated_diagnostics_ptr);

  RoomDistances generated;
  buildGeneratedRoomDistances(generated, rooms, graph, options, warnings, generated_diagnostics_ptr);
  Error error = validateRoomDistances(generated, rooms);
  if (error != Error::kNone) return error;
  std::size_t direct_pairs = 0;
  std::size_t routed_pairs = 0;
  std::size_t unavailable_pairs = 0;
  for (const RoomDistance& distance : generated) {
    if (distance.distance > 0.0f) {
      ++routed_pairs;
    } else if (distance.low_room_portal != kInvalidPortalIndex) {
      ++direct_pairs;
    } else {
      ++unavailable_pairs;
    }
  }
  log(ARX_LOG_DEBUG,
      "Room-distance generation: {} rooms, {} portals, {} pairs ({} direct, {} routed, {} unavailable), spacing "
      "{}, height offset {}, max link {}",
      rooms.definitions.size(),
      rooms.portals.size(),
      generated.size(),
      direct_pairs,
      routed_pairs,
      unavailable_pairs,
      options.sample_spacing,
      options.sample_height_offset,
      options.max_link_distance);
  logWarnings(rooms, warnings);
  out = std::move(generated);
  if (diagnostics) *diagnostics = std::move(generated_diagnostics);
  return Error::kNone;
}

}  // namespace pistoris::rooms
