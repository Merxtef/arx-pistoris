// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace pistoris::rooms {
namespace {

void addPortalAccessPoint(RoomDistanceGenerationDiagnostics* diagnostics, RoomDistanceGenerationGraph& graph,
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
  return index;
}

void addGraphEdge(RoomGraph& graph, std::uint32_t first, std::uint32_t second, float cost) {
  graph.edges.push_back({first, second, cost});
}

RoomDistanceAdjacency buildAdjacency(std::size_t node_count, std::span<const RoomDistanceUndirectedEdge> edges) {
  RoomDistanceAdjacency adjacency;
  adjacency.offsets.assign(node_count + 1U, 0);
  for (const RoomDistanceUndirectedEdge& edge : edges) {
    if (edge.first >= node_count || edge.second >= node_count) continue;
    ++adjacency.offsets[edge.first + 1U];
    ++adjacency.offsets[edge.second + 1U];
  }
  std::partial_sum(adjacency.offsets.begin(), adjacency.offsets.end(), adjacency.offsets.begin());
  adjacency.edges.resize(adjacency.offsets.back());
  std::vector<std::size_t> write_offsets(adjacency.offsets.begin(), adjacency.offsets.end() - 1);
  for (const RoomDistanceUndirectedEdge& edge : edges) {
    if (edge.first >= node_count || edge.second >= node_count) continue;
    adjacency.edges[write_offsets[edge.first]++] = {edge.second, edge.cost};
    adjacency.edges[write_offsets[edge.second]++] = {edge.first, edge.cost};
  }
  return adjacency;
}

std::size_t adjacencyNodeCount(const RoomDistanceAdjacency& adjacency) noexcept {
  return adjacency.offsets.empty() ? 0 : adjacency.offsets.size() - 1U;
}

std::span<const RoomDistanceEdge> adjacentEdges(const RoomDistanceAdjacency& adjacency, std::uint32_t node) noexcept {
  if (node >= adjacencyNodeCount(adjacency)) return {};
  const std::size_t first = adjacency.offsets[node];
  return std::span<const RoomDistanceEdge>(adjacency.edges).subspan(first, adjacency.offsets[node + 1U] - first);
}

std::uint32_t globalPortalSide(PortalIndex portal, bool front) { return portal * 2U + (front ? 0U : 1U); }

RoomPortalSideIndex::RoomPortalSideIndex(const RoomsData& rooms) {
  offsets_.assign(rooms.definitions.size() + 1U, 0);
  for (PortalIndex portal = 0; portal < rooms.portals.size(); ++portal) {
    const RoomIndex front = portalSideRoom(rooms.portals[portal], PortalSide::kFront);
    const RoomIndex back = portalSideRoom(rooms.portals[portal], PortalSide::kBack);
    if (front < rooms.definitions.size()) ++offsets_[static_cast<std::size_t>(front) + 1U];
    if (back < rooms.definitions.size()) ++offsets_[static_cast<std::size_t>(back) + 1U];
  }
  for (std::size_t index = 1; index < offsets_.size(); ++index) offsets_[index] += offsets_[index - 1U];

  sides_.resize(offsets_.back());
  std::vector<std::size_t> write_offsets = offsets_;
  for (PortalIndex portal = 0; portal < rooms.portals.size(); ++portal) {
    const RoomIndex front = portalSideRoom(rooms.portals[portal], PortalSide::kFront);
    const RoomIndex back = portalSideRoom(rooms.portals[portal], PortalSide::kBack);
    if (front < rooms.definitions.size()) sides_[write_offsets[front]++] = globalPortalSide(portal, true);
    if (back < rooms.definitions.size()) sides_[write_offsets[back]++] = globalPortalSide(portal, false);
  }
}

std::span<const std::uint32_t> RoomPortalSideIndex::roomSides(RoomIndex room) const noexcept {
  if (static_cast<std::size_t>(room) + 1U >= offsets_.size()) return {};
  return std::span<const std::uint32_t>(sides_).subspan(offsets_[room], offsets_[room + 1U] - offsets_[room]);
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
                               RoomDistanceGenerationGraph& graph, RoomDistanceGenerationDiagnostics* diagnostics) {
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
