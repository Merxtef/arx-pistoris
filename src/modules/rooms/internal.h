// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/rooms.h"

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

struct RoomDistanceGenWarnings {
  std::vector<std::vector<RoomIndex>> global_room_components;
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

struct RoomGraph {
  std::vector<RoomDistanceNode> nodes;
  std::vector<std::vector<RoomDistanceEdge>> adjacency;
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

struct DijkstraPath {
  bool found = false;
  float distance = 0.0f;
  std::vector<std::uint32_t> nodes;
};

struct GlobalPath {
  bool found = false;
  float distance = 0.0f;
  std::vector<std::uint32_t> nodes;
};

std::uint32_t addRoomNode(RoomGraph& graph, const RoomDistanceNode& node);
void addGraphEdge(RoomGraph& graph, std::uint32_t first, std::uint32_t second, float cost);
void addGlobalEdge(std::vector<std::vector<RoomDistanceEdge>>& adjacency, std::uint32_t first, std::uint32_t second,
                   float cost);
std::uint32_t globalPortalSide(PortalIndex portal, bool front);

DijkstraPath shortestPath(const RoomGraph& graph, std::uint32_t start, std::uint32_t goal);
std::vector<std::uint32_t> shortcutPath(const RoomsData& rooms, const GeometryData& geometry, const RoomGraph& graph,
                                        RoomIndex room, std::span<const std::uint32_t> path);
std::vector<ArxVector3> pathPositions(const RoomGraph& graph, std::span<const std::uint32_t> path);
float pathDistance(std::span<const ArxVector3> points);

void addInRoomPortalPaths(const RoomsData& rooms, const GeometryData& geometry, RoomDistanceGenerationGraph& graph,
                          RoomDistanceGenWarnings& warnings, RoomDistanceGenDiagnostics* diagnostics = nullptr);
std::vector<std::vector<RoomDistanceEdge>> buildGlobalPortalGraph(const RoomsData& rooms,
                                                                  const RoomDistanceGenerationGraph& graph);
GlobalPath shortestGlobalPath(const std::vector<std::vector<RoomDistanceEdge>>& adjacency,
                              std::span<const std::uint32_t> starts, std::span<const std::uint32_t> goals);
std::vector<std::uint32_t> portalSidesForRoom(const RoomsData& rooms, RoomIndex room);
ArxVector3 portalSidePosition(const RoomDistanceGenerationGraph& graph, std::uint32_t portal_side);
std::vector<ArxVector3> globalPathPositions(const RoomDistanceGenerationGraph& graph,
                                            std::span<const std::uint32_t> path);
void buildGeneratedRoomDistances(RoomDistances& out, const RoomsData& rooms, const RoomDistanceGenerationGraph& graph,
                                 const RoomDistanceOptions& options, RoomDistanceGenWarnings& warnings,
                                 RoomDistanceGenDiagnostics* diagnostics = nullptr);
void addPortalSideAccessPoints(const RoomsData& rooms, const RoomDistanceOptions& options,
                               RoomDistanceGenerationGraph& graph, RoomDistanceGenDiagnostics* diagnostics = nullptr);

std::vector<FaceIndex> faceIndicesForRoom(const RoomsData& rooms, const GeometryData& geometry, RoomIndex room,
                                          FaceType ignore_flags = kRoomDistanceIgnoreFlags);
geometry::SurfaceSupportIndex buildRoomSupportIndex(const RoomsData& rooms, const GeometryData& geometry,
                                                    RoomIndex room, FaceType ignore_flags = kRoomDistanceIgnoreFlags);
ArxVector3 offsetRoomDistanceSample(const geometry::SurfaceSupportHit& hit, const RoomDistanceOptions& options);
bool sampleOffsetClear(const RoomsData& rooms, const GeometryData& geometry, RoomIndex room,
                       const ArxVector3& support_position, const ArxVector3& sample_position);
bool segmentIntersectsRoomDistanceTriangle(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a,
                                           const ArxVector3& b, const ArxVector3& c);
bool blockedByRoomGeometry(const RoomsData& rooms, const GeometryData& geometry, RoomIndex room,
                           const ArxVector3& start, const ArxVector3& end);
void addSampleNodes(const RoomsData& rooms, const GeometryData& geometry, const RoomDistanceOptions& options,
                    RoomDistanceGenerationGraph& graph, RoomDistanceGenDiagnostics* diagnostics = nullptr);
void addVisibilityEdges(const RoomsData& rooms, const GeometryData& geometry, const RoomDistanceOptions& options,
                        RoomDistanceGenerationGraph& graph, RoomDistanceGenDiagnostics* diagnostics = nullptr);

}  // namespace pistoris::rooms
