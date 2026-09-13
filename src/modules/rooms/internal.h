// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/rooms.h"

#include <utility>

namespace pistoris::rooms {

inline constexpr std::uint32_t kInvalidRoomDistanceIndex = std::numeric_limits<std::uint32_t>::max();
inline constexpr FaceType kRoomDistanceIgnoreFlags = kFaceBitWater | kFaceBitNocol | kFaceBitTrans;
inline constexpr float kVisibilityEndpointEpsilon = 1.0e-4f;
inline constexpr float kSampleOffsetEndpointEpsilon = 1.0e-3f;
inline constexpr std::size_t kRoomDistanceWarningExampleLimit = 5U;

struct RoomDistanceRoomPairWarning {
  RoomIndex room_1 = 0;
  RoomIndex room_2 = 0;
};

struct RoomDistanceDisconnectedPortalGroupsWarning {
  RoomIndex room = 0;
  std::size_t group_count = 0;
};

struct RoomDistanceGenerationWarnings {
  std::vector<RoomIndex> global_room_component_rooms;
  std::vector<std::size_t> global_room_component_offsets;
  std::vector<RoomIndex> unreachable_rooms;
  std::size_t disconnected_portal_group_rooms = 0;
  std::vector<RoomDistanceDisconnectedPortalGroupsWarning> disconnected_portal_group_examples;
  std::size_t failed_connected_room_pairs = 0;
  std::vector<RoomDistanceRoomPairWarning> failed_connected_room_pair_examples;
};

struct RoomDistanceNode {
  ArxVector3 position = {};
  RoomIndex room = 0;
  PortalIndex portal = kInvalidPortalIndex;
  bool portal_front = true;
};

struct RoomDistanceEdge {
  std::uint32_t to = 0;
  float cost = 0.0f;
};

struct RoomDistanceUndirectedEdge {
  std::uint32_t first = 0;
  std::uint32_t second = 0;
  float cost = 0.0f;
};

struct RoomDistanceAdjacency {
  std::vector<std::size_t> offsets;
  std::vector<RoomDistanceEdge> edges;
};

struct RoomGraph {
  std::vector<RoomDistanceNode> nodes;
  std::vector<RoomDistanceUndirectedEdge> edges;
  std::vector<std::uint32_t> portal_nodes;
};

struct PortalSideRef {
  ArxVector3 position = {};
};

struct InRoomPortalPath {
  std::uint32_t start_global_side = 0;
  std::uint32_t end_global_side = 0;
  float distance = 0.0f;
};

struct RoomDistanceGenerationGraph {
  std::vector<RoomGraph> rooms;
  std::vector<std::array<PortalSideRef, 2>> portal_sides;
  std::vector<InRoomPortalPath> in_room_paths;
};

class RoomGeometryIndex {
 public:
  RoomGeometryIndex(const RoomsData& rooms, const GeometryData& geometry);

  RoomGeometryIndex(const RoomGeometryIndex&) = delete;
  RoomGeometryIndex& operator=(const RoomGeometryIndex&) = delete;
  RoomGeometryIndex(RoomGeometryIndex&&) noexcept = default;
  RoomGeometryIndex& operator=(RoomGeometryIndex&&) noexcept = default;

  [[nodiscard]] std::span<const FaceIndex> roomFaces(RoomIndex room) const noexcept;
  [[nodiscard]] bool hasRoomBounds(RoomIndex room) const noexcept;
  [[nodiscard]] const ArxAabb& roomBounds(RoomIndex room) const;
  [[nodiscard]] std::array<ArxVector3, 3> triangle(FaceIndex face) const;
  void findSupportHits(std::vector<geometry::SurfaceSupportHit>& out, RoomIndex room, float x, float z) const;
  [[nodiscard]] bool segmentBlocked(RoomIndex room, const ArxVector3& start, const ArxVector3& end,
                                    float endpoint_epsilon, std::vector<std::uint32_t>& scratch) const;

 private:
  geometry::TriangleIndex triangles_;
  std::vector<RoomIndex> face_rooms_;
  std::vector<FaceType> face_flags_;
  std::vector<ArxVector3> face_normals_;
  std::vector<FaceIndex> room_faces_;
  std::vector<std::size_t> room_face_offsets_;
  std::vector<ArxAabb> room_bounds_;
  std::vector<std::uint8_t> room_has_bounds_;
};

struct DijkstraPath {
  bool found = false;
  float distance = 0.0f;
  std::span<const std::uint32_t> nodes;
};

using GlobalPath = DijkstraPath;

struct DijkstraScratch {
  std::vector<float> distances;
  std::vector<std::uint32_t> previous;
  std::vector<std::uint8_t> goals;
  std::vector<std::pair<float, std::uint32_t>> queue;
  std::vector<std::uint32_t> path;
  std::vector<std::uint32_t> shortened_path;
};

class RoomPortalSideIndex {
 public:
  explicit RoomPortalSideIndex(const RoomsData& rooms);

  [[nodiscard]] std::span<const std::uint32_t> roomSides(RoomIndex room) const noexcept;

 private:
  std::vector<std::uint32_t> sides_;
  std::vector<std::size_t> offsets_;
};

std::uint32_t addRoomNode(RoomGraph& graph, const RoomDistanceNode& node);
void addGraphEdge(RoomGraph& graph, std::uint32_t first, std::uint32_t second, float cost);
RoomDistanceAdjacency buildAdjacency(std::size_t node_count, std::span<const RoomDistanceUndirectedEdge> edges);
std::size_t adjacencyNodeCount(const RoomDistanceAdjacency& adjacency) noexcept;
std::span<const RoomDistanceEdge> adjacentEdges(const RoomDistanceAdjacency& adjacency, std::uint32_t node) noexcept;
std::uint32_t globalPortalSide(PortalIndex portal, bool front);

void reserveDijkstraScratch(DijkstraScratch& scratch, std::size_t node_count, std::size_t queue_capacity);
DijkstraPath shortestPath(const RoomDistanceAdjacency& adjacency, std::uint32_t start, std::uint32_t goal,
                          DijkstraScratch& scratch);
void shortcutPath(std::vector<std::uint32_t>& out, const RoomGeometryIndex& room_geometry, const RoomGraph& graph,
                  RoomIndex room, std::span<const std::uint32_t> path, std::vector<std::uint32_t>& candidate_scratch);
std::vector<ArxVector3> pathPositions(const RoomGraph& graph, std::span<const std::uint32_t> path);

void addInRoomPortalPaths(const RoomsData& rooms, const RoomGeometryIndex& room_geometry,
                          RoomDistanceGenerationGraph& graph, RoomDistanceGenerationWarnings& warnings,
                          RoomDistanceGenerationDiagnostics* diagnostics = nullptr);
RoomDistanceAdjacency buildGlobalPortalGraph(const RoomsData& rooms, const RoomDistanceGenerationGraph& graph);
GlobalPath shortestGlobalPath(const RoomDistanceAdjacency& adjacency, std::span<const std::uint32_t> starts,
                              std::span<const std::uint32_t> goals, DijkstraScratch& scratch);
ArxVector3 portalSidePosition(const RoomDistanceGenerationGraph& graph, std::uint32_t portal_side);
std::vector<ArxVector3> globalPathPositions(const RoomDistanceGenerationGraph& graph,
                                            std::span<const std::uint32_t> path);
void buildGeneratedRoomDistances(RoomDistances& out, const RoomsData& rooms, const RoomDistanceGenerationGraph& graph,
                                 const RoomDistanceOptions& options, RoomDistanceGenerationWarnings& warnings,
                                 RoomDistanceGenerationDiagnostics* diagnostics = nullptr);
void addPortalSideAccessPoints(const RoomsData& rooms, const RoomDistanceOptions& options,
                               RoomDistanceGenerationGraph& graph,
                               RoomDistanceGenerationDiagnostics* diagnostics = nullptr);

ArxVector3 offsetRoomDistanceSample(const geometry::SurfaceSupportHit& hit, const RoomDistanceOptions& options);
bool sampleOffsetClear(const RoomGeometryIndex& room_geometry, RoomIndex room, const ArxVector3& support_position,
                       const ArxVector3& sample_position, std::vector<std::uint32_t>& candidate_scratch);
bool segmentIntersectsRoomDistanceTriangle(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a,
                                           const ArxVector3& b, const ArxVector3& c);
bool blockedByRoomGeometry(const RoomGeometryIndex& room_geometry, RoomIndex room, const ArxVector3& start,
                           const ArxVector3& end, std::vector<std::uint32_t>& candidate_scratch);
void addSampleNodes(const RoomsData& rooms, const RoomGeometryIndex& room_geometry, const RoomDistanceOptions& options,
                    RoomDistanceGenerationGraph& graph, RoomDistanceGenerationDiagnostics* diagnostics = nullptr);
void addVisibilityEdges(const RoomsData& rooms, const RoomGeometryIndex& room_geometry,
                        const RoomDistanceOptions& options, RoomDistanceGenerationGraph& graph,
                        RoomDistanceGenerationDiagnostics* diagnostics = nullptr);

}  // namespace pistoris::rooms
