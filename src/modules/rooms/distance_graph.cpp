// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/indices.h"

#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pistoris::rooms {
namespace {

void addPortalAccessPoint(RoomDistanceGenDiagnostics* diagnostics, RoomDistanceGenerationGraph& graph,
                          PortalIndex portal_index, bool front, RoomIndex room, const ArxVector3& position) {
  if (room >= graph.rooms.size()) return;
  if (diagnostics && room < diagnostics->portal_access_points_by_room.size())
    diagnostics->portal_access_points_by_room[room].push_back({position, room});
  const std::size_t side_index = front ? 0U : 1U;
  RoomGraph& room_graph = graph.rooms[room];
  const std::uint32_t node = addRoomNode(room_graph, {position, room, portal_index, front});
  room_graph.portal_nodes.push_back(node);
  if (portal_index < graph.portal_sides.size()) graph.portal_sides[portal_index][side_index].position = position;
}

}  // namespace

std::uint32_t addRoomNode(RoomGraph& graph, const RoomDistanceNode& node) {
  const std::uint32_t index = static_cast<std::uint32_t>(graph.nodes.size());
  graph.nodes.push_back(node);
  graph.adjacency.emplace_back();
  return index;
}

void addGraphEdge(RoomGraph& graph, std::uint32_t first, std::uint32_t second, float cost) {
  graph.adjacency[first].push_back({second, cost});
  graph.adjacency[second].push_back({first, cost});
}

void addGlobalEdge(std::vector<std::vector<RoomDistanceEdge>>& adjacency, std::uint32_t first, std::uint32_t second,
                   float cost) {
  adjacency[first].push_back({second, cost});
  adjacency[second].push_back({first, cost});
}

std::uint32_t globalPortalSide(PortalIndex portal, bool front) { return portal * 2U + (front ? 0U : 1U); }

std::vector<std::uint32_t> portalSidesForRoom(const RoomsData& rooms, RoomIndex room) {
  std::vector<std::uint32_t> sides;
  for (PortalIndex portal = 0; portal < rooms.portals.size(); ++portal) {
    if (portalSideRoom(rooms.portals[portal], true) == room) sides.push_back(globalPortalSide(portal, true));
    if (portalSideRoom(rooms.portals[portal], false) == room) sides.push_back(globalPortalSide(portal, false));
  }
  return sides;
}

ArxVector3 portalSidePosition(const RoomDistanceGenerationGraph& graph, std::uint32_t portal_side) {
  const PortalIndex portal = portal_side / 2U;
  const std::size_t side_index = portal_side % 2U;
  if (portal >= graph.portal_sides.size()) return {};
  return graph.portal_sides[portal][side_index].position;
}

std::vector<ArxVector3> globalPathPositions(const RoomDistanceGenerationGraph& graph,
                                            std::span<const std::uint32_t> path) {
  std::vector<ArxVector3> points;
  points.reserve(path.size());
  for (std::uint32_t portal_side : path) points.push_back(portalSidePosition(graph, portal_side));
  return points;
}

void addPortalSideAccessPoints(const RoomsData& rooms, const RoomDistanceOptions& options,
                               RoomDistanceGenerationGraph& graph, RoomDistanceGenDiagnostics* diagnostics) {
  if (diagnostics) diagnostics->portal_access_points_by_room.assign(rooms.definitions.size(), {});
  graph.portal_sides.assign(rooms.portals.size(), {});
  for (PortalIndex portal_index = 0; portal_index < rooms.portals.size(); ++portal_index) {
    const Portal& portal = rooms.portals[portal_index];
    const ArxVector3 center = portalCentroid(portal);
    const ArxVector3 normal = portalNormal(portal);
    const ArxVector3 room_1_position = center + normal * options.portal_side_offset;
    const ArxVector3 room_2_position = center - normal * options.portal_side_offset;
    addPortalAccessPoint(diagnostics, graph, portal_index, true, portal.room_1, room_1_position);
    addPortalAccessPoint(diagnostics, graph, portal_index, false, portal.room_2, room_2_position);
    if (diagnostics && portal.room_1 < rooms.definitions.size())
      diagnostics->portal_access_segments.push_back({center, room_1_position, portal.room_1, portal.room_1});
    if (diagnostics && portal.room_2 < rooms.definitions.size())
      diagnostics->portal_access_segments.push_back({center, room_2_position, portal.room_2, portal.room_2});
  }
}

}  // namespace pistoris::rooms
