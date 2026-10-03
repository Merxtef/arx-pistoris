// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::rooms {
namespace {

using QueueItem = std::pair<float, std::uint32_t>;

struct QueueGreater {
  bool operator()(const QueueItem& left, const QueueItem& right) const noexcept { return left.first > right.first; }
};

void pushQueue(std::vector<QueueItem>& queue, QueueItem value) {
  queue.push_back(value);
  std::push_heap(queue.begin(), queue.end(), QueueGreater{});
}

QueueItem popQueue(std::vector<QueueItem>& queue) {
  std::pop_heap(queue.begin(), queue.end(), QueueGreater{});
  QueueItem value = queue.back();
  queue.pop_back();
  return value;
}

DijkstraPath findShortestPath(const RoomDistanceAdjacency& adjacency, std::span<const std::uint32_t> starts,
                              std::span<const std::uint32_t> goals, DijkstraScratch& scratch) {
  scratch.path.clear();
  if (starts.empty() || goals.empty()) return {};

  const std::size_t node_count = adjacencyNodeCount(adjacency);
  reserveDijkstraScratch(scratch, node_count, node_count);
  scratch.distances.assign(node_count, std::numeric_limits<float>::infinity());
  scratch.previous.assign(node_count, kInvalidRoomDistanceIndex);
  scratch.goals.assign(node_count, 0);
  scratch.queue.clear();

  bool has_goal = false;
  for (std::uint32_t goal : goals) {
    if (goal >= node_count) continue;
    scratch.goals[goal] = 1;
    has_goal = true;
  }
  if (!has_goal) return {};

  for (std::uint32_t start : starts) {
    if (start >= node_count || scratch.distances[start] == 0.0f) continue;
    scratch.distances[start] = 0.0f;
    pushQueue(scratch.queue, {0.0f, start});
  }
  if (scratch.queue.empty()) return {};

  std::uint32_t reached = kInvalidRoomDistanceIndex;
  while (!scratch.queue.empty()) {
    const QueueItem current = popQueue(scratch.queue);
    if (current.first != scratch.distances[current.second]) continue;
    if (scratch.goals[current.second] != 0) {
      reached = current.second;
      break;
    }
    for (const RoomDistanceEdge& edge : adjacentEdges(adjacency, current.second)) {
      const float next_distance = current.first + edge.cost;
      if (next_distance >= scratch.distances[edge.to]) continue;
      scratch.distances[edge.to] = next_distance;
      scratch.previous[edge.to] = current.second;
      pushQueue(scratch.queue, {next_distance, edge.to});
    }
  }

  if (reached == kInvalidRoomDistanceIndex) return {};
  for (std::uint32_t node = reached; node != kInvalidRoomDistanceIndex; node = scratch.previous[node])
    scratch.path.push_back(node);
  std::reverse(scratch.path.begin(), scratch.path.end());
  return {true, scratch.distances[reached], std::span<const std::uint32_t>(scratch.path)};
}

std::size_t connectedPortalGroupCount(const RoomGraph& graph, const RoomDistanceAdjacency& adjacency,
                                      std::vector<std::uint8_t>& visited, std::vector<std::uint32_t>& worklist) {
  std::fill_n(visited.begin(), graph.nodes.size(), std::uint8_t{0});
  std::size_t groups = 0;
  for (std::uint32_t portal_node : graph.portal_nodes) {
    if (portal_node >= graph.nodes.size() || visited[portal_node]) continue;
    bool contains_portal = false;
    worklist.clear();
    visited[portal_node] = 1;
    worklist.push_back(portal_node);
    for (std::size_t current_index = 0; current_index < worklist.size(); ++current_index) {
      const std::uint32_t current = worklist[current_index];
      contains_portal = contains_portal || graph.nodes[current].portal != kInvalidPortalIndex;
      for (const RoomDistanceEdge& edge : adjacentEdges(adjacency, current)) {
        if (edge.to >= graph.nodes.size() || visited[edge.to]) continue;
        visited[edge.to] = 1;
        worklist.push_back(edge.to);
      }
    }
    groups += static_cast<std::size_t>(contains_portal);
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

void reserveDijkstraScratch(DijkstraScratch& scratch, std::size_t node_count, std::size_t queue_capacity) {
  scratch.distances.reserve(node_count);
  scratch.previous.reserve(node_count);
  scratch.goals.reserve(node_count);
  scratch.queue.reserve(queue_capacity);
  scratch.path.reserve(node_count);
  scratch.shortened_path.reserve(node_count);
}

DijkstraPath shortestPath(const RoomDistanceAdjacency& adjacency, std::uint32_t start, std::uint32_t goal,
                          DijkstraScratch& scratch) {
  const std::array<std::uint32_t, 1> starts = {start};
  const std::array<std::uint32_t, 1> goals = {goal};
  return findShortestPath(adjacency, starts, goals, scratch);
}

void shortcutPath(std::vector<std::uint32_t>& out, const RoomGeometryIndex& room_geometry, const RoomGraph& graph,
                  RoomIndex room, std::span<const std::uint32_t> path, std::vector<std::uint32_t>& candidate_scratch) {
  out.clear();
  if (path.size() <= 2U) {
    out.insert(out.end(), path.begin(), path.end());
    return;
  }
  std::size_t current = 0;
  out.push_back(path[current]);
  while (current + 1U < path.size()) {
    std::size_t next = current + 1U;
    for (std::size_t candidate = path.size() - 1U; candidate > current + 1U; --candidate) {
      if (!blockedByRoomGeometry(room_geometry,
                                 room,
                                 graph.nodes[path[current]].position,
                                 graph.nodes[path[candidate]].position,
                                 candidate_scratch)) {
        next = candidate;
        break;
      }
    }
    out.push_back(path[next]);
    current = next;
  }
}

std::vector<ArxVector3> pathPositions(const RoomGraph& graph, std::span<const std::uint32_t> path) {
  std::vector<ArxVector3> points;
  points.reserve(path.size());
  for (std::uint32_t node : path) points.push_back(graph.nodes[node].position);
  return points;
}

void addInRoomPortalPaths(const RoomsData& rooms, const RoomGeometryIndex& room_geometry,
                          RoomDistanceGenerationGraph& graph, RoomDistanceGenerationWarnings& warnings,
                          RoomDistanceGenerationDiagnostics* diagnostics) {
  if (diagnostics) diagnostics->in_room_portal_paths_by_room.assign(rooms.definitions.size(), {});
  std::vector<std::uint32_t> candidate_scratch;
  DijkstraScratch path_scratch;
  std::size_t max_nodes = 0;
  std::size_t max_queue = 0;
  for (const RoomGraph& room_graph : graph.rooms) {
    max_nodes = std::max(max_nodes, room_graph.nodes.size());
    const std::size_t queue_capacity = room_graph.nodes.size() + room_graph.edges.size() * 2U;
    max_queue = std::max(max_queue, queue_capacity);
  }
  reserveDijkstraScratch(path_scratch, max_nodes, max_queue);
  std::vector<std::uint8_t> component_visited(max_nodes, 0);
  std::vector<std::uint32_t> component_worklist;
  component_worklist.reserve(max_nodes);
  for (RoomIndex room = 0; room < graph.rooms.size(); ++room) {
    const RoomGraph& room_graph = graph.rooms[room];
    const RoomDistanceAdjacency adjacency = buildAdjacency(room_graph.nodes.size(), room_graph.edges);
    const std::size_t portal_groups =
        connectedPortalGroupCount(room_graph, adjacency, component_visited, component_worklist);
    if (portal_groups > 1U) {
      ++warnings.disconnected_portal_group_rooms;
      if (warnings.disconnected_portal_group_examples.size() < kRoomDistanceWarningExampleLimit) {
        warnings.disconnected_portal_group_examples.push_back({room, portal_groups});
      }
    }

    for (std::size_t i = 0; i < room_graph.portal_nodes.size(); ++i) {
      for (std::size_t j = i + 1U; j < room_graph.portal_nodes.size(); ++j) {
        const std::uint32_t start = room_graph.portal_nodes[i];
        const std::uint32_t goal = room_graph.portal_nodes[j];
        DijkstraPath path = shortestPath(adjacency, start, goal, path_scratch);
        if (!path.found) continue;
        shortcutPath(path_scratch.shortened_path, room_geometry, room_graph, room, path.nodes, candidate_scratch);
        const float distance = indexedPathDistance(room_graph, path_scratch.shortened_path);
        const RoomDistanceNode& start_node = room_graph.nodes[start];
        const RoomDistanceNode& end_node = room_graph.nodes[goal];
        graph.in_room_paths.push_back({globalPortalSide(start_node.portal, start_node.portal_front),
                                       globalPortalSide(end_node.portal, end_node.portal_front),
                                       distance});
        if (diagnostics && room < diagnostics->in_room_portal_paths_by_room.size()) {
          std::vector<ArxVector3> points = pathPositions(room_graph, path_scratch.shortened_path);
          diagnostics->in_room_portal_paths_by_room[room].push_back(
              {std::move(points), room, room, start_node.portal, end_node.portal});
        }
      }
    }
  }
}

RoomDistanceAdjacency buildGlobalPortalGraph(const RoomsData& rooms, const RoomDistanceGenerationGraph& graph) {
  std::vector<RoomDistanceUndirectedEdge> edges;
  edges.reserve(rooms.portals.size() + graph.in_room_paths.size());
  for (PortalIndex portal = 0; portal < rooms.portals.size(); ++portal)
    edges.push_back({globalPortalSide(portal, true), globalPortalSide(portal, false), 0.0f});
  for (const InRoomPortalPath& path : graph.in_room_paths)
    edges.push_back({path.start_global_side, path.end_global_side, path.distance});
  return buildAdjacency(rooms.portals.size() * 2U, edges);
}

GlobalPath shortestGlobalPath(const RoomDistanceAdjacency& adjacency, std::span<const std::uint32_t> starts,
                              std::span<const std::uint32_t> goals, DijkstraScratch& scratch) {
  return findShortestPath(adjacency, starts, goals, scratch);
}

}  // namespace pistoris::rooms
