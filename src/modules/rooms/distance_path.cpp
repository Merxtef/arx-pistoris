// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::rooms {
namespace {

struct QueueItem {
  float distance = 0.0f;
  std::uint32_t node = 0;
  bool operator>(const QueueItem& other) const { return distance > other.distance; }
};

void appendUnique(std::vector<PortalIndex>& values, PortalIndex value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

std::vector<std::vector<PortalIndex>> connectedPortalGroups(const RoomGraph& graph) {
  std::vector<std::vector<PortalIndex>> groups;
  std::vector<bool> visited(graph.nodes.size(), false);
  for (std::uint32_t portal_node : graph.portal_nodes) {
    if (portal_node >= graph.nodes.size() || visited[portal_node]) continue;
    std::vector<PortalIndex> group;
    std::queue<std::uint32_t> queue;
    visited[portal_node] = true;
    queue.push(portal_node);
    while (!queue.empty()) {
      const std::uint32_t current = queue.front();
      queue.pop();
      if (graph.nodes[current].portal != kInvalidPortalIndex) appendUnique(group, graph.nodes[current].portal);
      for (const RoomDistanceEdge& edge : graph.adjacency[current]) {
        if (edge.to >= graph.nodes.size() || visited[edge.to]) continue;
        visited[edge.to] = true;
        queue.push(edge.to);
      }
    }
    if (!group.empty()) groups.push_back(std::move(group));
  }
  return groups;
}

float indexedPathDistance(const RoomGraph& graph, std::span<const std::uint32_t> path) {
  float distance = 0.0f;
  for (std::size_t i = 1; i < path.size(); ++i) {
    distance += std::sqrt(
        static_cast<float>(math::lengthSquared(graph.nodes[path[i]].position - graph.nodes[path[i - 1U]].position)));
  }
  return distance;
}

}  // namespace

DijkstraPath shortestPath(const RoomGraph& graph, std::uint32_t start, std::uint32_t goal) {
  if (start >= graph.nodes.size() || goal >= graph.nodes.size()) return {};
  if (start == goal) return {true, 0.0f, {start}};

  std::vector<float> distances(graph.nodes.size(), std::numeric_limits<float>::infinity());
  std::vector<std::uint32_t> previous(graph.nodes.size(), kInvalidRoomDistanceIndex);
  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
  distances[start] = 0.0f;
  queue.push({0.0f, start});

  while (!queue.empty()) {
    QueueItem current = queue.top();
    queue.pop();
    if (current.distance != distances[current.node]) continue;
    if (current.node == goal) break;
    for (const RoomDistanceEdge& edge : graph.adjacency[current.node]) {
      const float next_distance = current.distance + edge.cost;
      if (next_distance >= distances[edge.to]) continue;
      distances[edge.to] = next_distance;
      previous[edge.to] = current.node;
      queue.push({next_distance, edge.to});
    }
  }

  if (!std::isfinite(distances[goal])) return {};
  std::vector<std::uint32_t> path;
  for (std::uint32_t node = goal; node != kInvalidRoomDistanceIndex; node = previous[node]) path.push_back(node);
  std::reverse(path.begin(), path.end());
  return {true, distances[goal], std::move(path)};
}

std::vector<std::uint32_t> shortcutPath(const RoomsData& rooms, const GeometryData& geometry, const RoomGraph& graph,
                                        RoomIndex room, std::span<const std::uint32_t> path) {
  if (path.size() <= 2U) return {path.begin(), path.end()};
  std::vector<std::uint32_t> out;
  std::size_t current = 0;
  out.push_back(path[current]);
  while (current + 1U < path.size()) {
    std::size_t next = current + 1U;
    for (std::size_t candidate = path.size() - 1U; candidate > current + 1U; --candidate) {
      if (!blockedByRoomGeometry(
              rooms, geometry, room, graph.nodes[path[current]].position, graph.nodes[path[candidate]].position)) {
        next = candidate;
        break;
      }
    }
    out.push_back(path[next]);
    current = next;
  }
  return out;
}

std::vector<ArxVector3> pathPositions(const RoomGraph& graph, std::span<const std::uint32_t> path) {
  std::vector<ArxVector3> points;
  points.reserve(path.size());
  for (std::uint32_t node : path) points.push_back(graph.nodes[node].position);
  return points;
}

float pathDistance(std::span<const ArxVector3> points) {
  float distance = 0.0f;
  for (std::size_t i = 1; i < points.size(); ++i)
    distance += std::sqrt(static_cast<float>(math::lengthSquared(points[i] - points[i - 1U])));
  return distance;
}

void addInRoomPortalPaths(const RoomsData& rooms, const GeometryData& geometry, RoomDistanceGenerationGraph& graph,
                          RoomDistanceGenWarnings& warnings, RoomDistanceGenDiagnostics* diagnostics) {
  if (diagnostics) diagnostics->in_room_portal_paths_by_room.assign(rooms.definitions.size(), {});
  for (RoomIndex room = 0; room < graph.rooms.size(); ++room) {
    RoomGraph& room_graph = graph.rooms[room];
    std::vector<std::vector<PortalIndex>> portal_groups = connectedPortalGroups(room_graph);
    if (portal_groups.size() > 1U) {
      ++warnings.disconnected_portal_group_rooms;
      if (warnings.disconnected_portal_group_examples.size() < kRoomDistanceWarningExampleLimit) {
        warnings.disconnected_portal_group_examples.push_back({room, portal_groups.size()});
      }
    }

    for (std::size_t i = 0; i < room_graph.portal_nodes.size(); ++i) {
      for (std::size_t j = i + 1U; j < room_graph.portal_nodes.size(); ++j) {
        const std::uint32_t start = room_graph.portal_nodes[i];
        const std::uint32_t goal = room_graph.portal_nodes[j];
        DijkstraPath path = shortestPath(room_graph, start, goal);
        if (!path.found) continue;
        std::vector<std::uint32_t> shortened = shortcutPath(rooms, geometry, room_graph, room, path.nodes);
        const float distance = indexedPathDistance(room_graph, shortened);
        const RoomDistanceNode& start_node = room_graph.nodes[start];
        const RoomDistanceNode& end_node = room_graph.nodes[goal];
        graph.in_room_paths.push_back({globalPortalSide(start_node.portal, start_node.portal_front),
                                       globalPortalSide(end_node.portal, end_node.portal_front),
                                       distance});
        if (diagnostics && room < diagnostics->in_room_portal_paths_by_room.size()) {
          std::vector<ArxVector3> points = pathPositions(room_graph, shortened);
          diagnostics->in_room_portal_paths_by_room[room].push_back(
              {std::move(points), room, room, start_node.portal, end_node.portal});
        }
      }
    }
  }
}

std::vector<std::vector<RoomDistanceEdge>> buildGlobalPortalGraph(const RoomsData& rooms,
                                                                  const RoomDistanceGenerationGraph& graph) {
  std::vector<std::vector<RoomDistanceEdge>> adjacency(rooms.portals.size() * 2U);
  for (PortalIndex portal = 0; portal < rooms.portals.size(); ++portal)
    addGlobalEdge(adjacency, globalPortalSide(portal, true), globalPortalSide(portal, false), 0.0f);
  for (const InRoomPortalPath& path : graph.in_room_paths)
    addGlobalEdge(adjacency, path.start_global_side, path.end_global_side, path.distance);
  return adjacency;
}

GlobalPath shortestGlobalPath(const std::vector<std::vector<RoomDistanceEdge>>& adjacency,
                              std::span<const std::uint32_t> starts, std::span<const std::uint32_t> goals) {
  if (starts.empty() || goals.empty()) return {};
  std::vector<bool> is_goal(adjacency.size(), false);
  for (std::uint32_t goal : goals)
    if (goal < is_goal.size()) is_goal[goal] = true;

  std::vector<float> distances(adjacency.size(), std::numeric_limits<float>::infinity());
  std::vector<std::uint32_t> previous(adjacency.size(), kInvalidRoomDistanceIndex);
  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
  for (std::uint32_t start : starts) {
    if (start >= adjacency.size()) continue;
    distances[start] = 0.0f;
    queue.push({0.0f, start});
  }

  std::uint32_t reached = kInvalidRoomDistanceIndex;
  while (!queue.empty()) {
    QueueItem current = queue.top();
    queue.pop();
    if (current.distance != distances[current.node]) continue;
    if (is_goal[current.node]) {
      reached = current.node;
      break;
    }
    for (const RoomDistanceEdge& edge : adjacency[current.node]) {
      const float next_distance = current.distance + edge.cost;
      if (next_distance >= distances[edge.to]) continue;
      distances[edge.to] = next_distance;
      previous[edge.to] = current.node;
      queue.push({next_distance, edge.to});
    }
  }

  if (reached == kInvalidRoomDistanceIndex) return {};
  std::vector<std::uint32_t> path;
  for (std::uint32_t node = reached; node != kInvalidRoomDistanceIndex; node = previous[node]) path.push_back(node);
  std::reverse(path.begin(), path.end());
  return {true, distances[reached], std::move(path)};
}

}  // namespace pistoris::rooms
