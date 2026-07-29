// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris_types.h"

#include "modules/geometry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

struct Room {
  std::string name;
};

enum class PortalShape : std::uint8_t {
  kTriangle = 3,
  kQuad = 4,
};

struct Portal {
  std::string name;
  RoomIndex room_1 = 0;
  RoomIndex room_2 = 0;
  PortalShape shape = PortalShape::kQuad;
  std::array<ArxVector3, 4> vertices = {};
};

struct RoomDistance {
  float distance = -1.0f;
  PortalIndex low_room_portal = kInvalidPortalIndex;
  PortalIndex high_room_portal = kInvalidPortalIndex;
};

using RoomDistances = std::vector<RoomDistance>;

struct RoomsData {
  std::vector<Room> definitions;
  std::vector<RoomIndex> face_rooms;
  std::vector<Portal> portals;
  RoomDistances distances;
};

namespace rooms {

struct RoomDistanceDebugPoint {
  ArxVector3 position = {};
  RoomIndex room = 0;
};

struct RoomDistanceDebugSegment {
  ArxVector3 start = {};
  ArxVector3 end = {};
  RoomIndex room_1 = 0;
  RoomIndex room_2 = 0;
};

struct RoomDistanceDebugPath {
  std::vector<ArxVector3> points;
  RoomIndex room_1 = 0;
  RoomIndex room_2 = 0;
  PortalIndex portal_1 = kInvalidPortalIndex;
  PortalIndex portal_2 = kInvalidPortalIndex;
};

struct RoomDistanceSupportTriangle {
  std::array<ArxVector3, 3> vertices = {};
};

struct RoomDistanceGenDiagnostics {
  std::vector<std::vector<RoomDistanceSupportTriangle>> support_by_room;
  std::vector<std::vector<RoomDistanceDebugPoint>> portal_access_points_by_room;
  std::vector<RoomDistanceDebugSegment> portal_access_segments;
  std::vector<std::vector<RoomDistanceDebugPoint>> sampled_points_by_room;
  std::vector<RoomDistanceDebugSegment> in_room_visibility_edges;
  std::vector<std::vector<RoomDistanceDebugPath>> in_room_portal_paths_by_room;
  std::vector<RoomDistanceDebugPath> room_pair_paths;
};

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kNoRooms,
  kTooManyRooms,
  kBadRoomName,
  kDuplicateRoomName,
  kBadFaceRoomCount,
  kBadFaceRoomIndex,
  kBadRoomDistanceCount,
  kBadRoomDistance,
  kTooManyPortals,
  kBadPortalName,
  kDuplicatePortalName,
  kBadPortalRoom,
  kBadPortalShape,
  kBadPortalVertex,
  kDegeneratePortal,
  kNonPlanarPortal,
  kInconsistentPortalOrientation,
  kSelfIntersectingPortal,
  kBadFaceVertex,
};

enum class PortalValidation : std::uint8_t {
  kValid,
  kBadShape,
  kBadVertex,
  kDegenerate,
  kNonPlanar,
  kInconsistentOrientation,
  kSelfIntersecting,
};

struct RoomDistanceOptions {
  float portal_side_offset = 10.0f;
  float sample_spacing = 100.0f;
  float sample_height_offset = 82.5f;
  float max_link_distance = 150.0f;
};

RoomIndex addRoom(RoomsData& rooms, std::string name);
void addFaceRooms(RoomsData& rooms, std::span<const RoomIndex> face_rooms);
// Empty means identity; otherwise the map must describe an order-preserving compaction
void remapFaceRooms(RoomsData& rooms, std::span<const FaceIndex> face_remap) noexcept;
Error collectVertexWeldSegments(const GeometryData& geometry, const RoomsData& rooms, float radius,
                                std::vector<std::vector<VertexIndex>>& segments,
                                std::vector<VertexIndex>& protected_vertices);
PortalIndex addPortal(RoomsData& rooms, Portal portal);
std::size_t makePortalNamesUnique(std::span<Portal> portals);

std::size_t portalVertexCount(PortalShape shape);
ArxVector3 portalCentroid(const Portal& portal);
ArxVector3 portalNormal(const Portal& portal);
double pointPortalDistanceSquared(const ArxVector3& point, const Portal& portal);
RoomIndex portalSideRoom(const Portal& portal, bool front);
bool connectsRooms(const Portal& portal, RoomIndex first, RoomIndex second);
Error validatePortalRoomRefs(const Portal& portal, std::size_t room_count);
PortalValidation validatePortalGeometry(const Portal& portal);
std::optional<PortalIndex> firstDirectPortal(const RoomsData& rooms, RoomIndex first_room, RoomIndex second_room);

bool validateRoomDistanceOptions(const RoomDistanceOptions& options);
Error validateRoom(const Room& room) noexcept;
Error validateRooms(const RoomsData& rooms);
Error validateFaceRooms(std::span<const RoomIndex> face_rooms, std::size_t face_count, std::size_t room_count);
Error validateFaceRooms(const RoomsData& rooms, std::size_t face_count);
Error validateRoomDistances(const RoomDistances& distances, const RoomsData& rooms);
Error validateRoomDistances(const RoomsData& rooms);
Error validatePortalDefinitions(const RoomsData& rooms);
Error validatePortalRoomRefs(const RoomsData& rooms);
Error validatePortal(const Portal& portal, std::size_t room_count);
Error validatePortals(const RoomsData& rooms);
Error validate(const RoomsData& rooms, std::size_t face_count);

std::size_t roomDistancePairCount(std::size_t room_count);
std::size_t roomDistancePairIndex(std::size_t first_room, std::size_t second_room);
bool hasCompleteRoomDistances(const RoomDistances& distances, std::size_t room_count);
void initializeRoomDistances(RoomDistances& distances, std::size_t room_count);

Error generateRoomDistances(RoomDistances& out, const RoomsData& rooms, const GeometryData& geometry,
                            const RoomDistanceOptions& options, RoomDistanceGenDiagnostics* diagnostics = nullptr);

}  // namespace rooms
}  // namespace pistoris
