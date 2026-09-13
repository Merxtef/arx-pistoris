// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

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

enum class PortalSide : std::uint8_t {
  kFront,
  kBack,
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

struct RoomDistanceGenerationDiagnostics {
  std::vector<std::vector<RoomDistanceSupportTriangle>> support_by_room;
  std::vector<std::vector<RoomDistanceDebugPoint>> portal_access_points_by_room;
  std::vector<RoomDistanceDebugSegment> portal_access_segments;
  std::vector<std::vector<RoomDistanceDebugPoint>> sampled_points_by_room;
  std::vector<RoomDistanceDebugSegment> in_room_visibility_edges;
  std::vector<std::vector<RoomDistanceDebugPath>> in_room_portal_paths_by_room;
  std::vector<RoomDistanceDebugPath> room_pair_paths;
};

struct VertexWeldSegments {
  std::vector<VertexIndex> vertices;
  std::vector<std::size_t> offsets;
  std::vector<VertexIndex> protected_vertices;
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
  kBadIndex,
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

// --- Validation ---

bool validRoomDistanceOptions(const RoomDistanceOptions& options) noexcept;
Error validateRoomCount(std::size_t count) noexcept;
Error validateRoom(const Room& room) noexcept;
Error validateRoomRemoval(const RoomsData& rooms, RoomIndex index) noexcept;
Error validateRoomDefinitions(const RoomsData& rooms);
Error validateFaceRoomIndices(std::span<const RoomIndex> face_rooms, std::size_t room_count) noexcept;
Error validateFaceRooms(std::span<const RoomIndex> face_rooms, std::size_t face_count, std::size_t room_count);
Error validateFaceRooms(const RoomsData& rooms, std::size_t face_count);
Error validateRoomDistance(const RoomDistance& distance, const RoomsData& rooms, RoomIndex low_room,
                           RoomIndex high_room) noexcept;
Error validateRoomDistances(const RoomDistances& distances, const RoomsData& rooms);
Error validateRoomDistances(const RoomsData& rooms);
Error validatePortalCount(std::size_t count) noexcept;
Error validatePortalDefinitions(const RoomsData& rooms);
Error validatePortalRoomRefs(const RoomsData& rooms);
Error validatePortalRoomRefs(const Portal& portal, std::size_t room_count);
PortalValidation validatePortalGeometry(const Portal& portal);
Error validatePortal(const Portal& portal, std::size_t room_count);
Error validatePortals(const RoomsData& rooms);
Error validate(const RoomsData& rooms, std::size_t face_count);

// --- Queries ---

std::size_t portalVertexCount(PortalShape shape);
ArxVector3 portalCentroid(const Portal& portal);
ArxVector3 portalNormal(const Portal& portal);
double pointPortalDistanceSquared(const ArxVector3& point, const Portal& portal);
RoomIndex portalSideRoom(const Portal& portal, PortalSide side);
bool connectsRooms(const Portal& portal, RoomIndex first, RoomIndex second);
std::optional<PortalIndex> firstDirectPortal(const RoomsData& rooms, RoomIndex first_room, RoomIndex second_room);
std::size_t roomDistancePairCount(std::size_t room_count);
std::size_t roomDistancePairIndex(std::size_t first_room, std::size_t second_room);
bool hasCompleteRoomDistances(const RoomDistances& distances, std::size_t room_count);

// --- Mutation ---

RoomIndex addRoom(RoomsData& rooms, Room room);
void setRoom(RoomsData& rooms, RoomIndex index, Room room) noexcept;
void removeRoom(RoomsData& rooms, RoomIndex index) noexcept;
void appendFaceRooms(RoomsData& rooms, std::span<const RoomIndex> face_rooms);
void reserveFaceRoomCapacity(RoomsData& rooms, std::size_t capacity);
void truncateFaceRooms(RoomsData& rooms, std::size_t size) noexcept;
void setFaceRoom(RoomsData& rooms, FaceIndex face, RoomIndex room) noexcept;
void removeFaceRoom(RoomsData& rooms, FaceIndex face) noexcept;
void replaceFaceRooms(RoomsData& rooms, std::vector<RoomIndex>&& face_rooms) noexcept;
void clearFaceRooms(RoomsData& rooms) noexcept;
// Empty means identity; otherwise the map must describe an order-preserving compaction
void remapFaceRooms(RoomsData& rooms, std::span<const FaceIndex> face_remap) noexcept;
PortalIndex addPortal(RoomsData& rooms, Portal portal);
void setPortal(RoomsData& rooms, PortalIndex index, Portal portal) noexcept;
void removePortal(RoomsData& rooms, PortalIndex index) noexcept;
void setRoomDistance(RoomsData& rooms, RoomIndex low_room, RoomIndex high_room, RoomDistance distance);
void replaceRoomDistances(RoomsData& rooms, RoomDistances&& distances) noexcept;
void clearRoomDistances(RoomsData& rooms) noexcept;

// --- Repair ---

void repairRoomName(const RoomsData& rooms, Room& room, RoomIndex ignored = kInvalidRoomIndex);
void repairPortalName(const RoomsData& rooms, Portal& portal, PortalIndex ignored = kInvalidPortalIndex);
std::size_t repairPortalNames(std::span<Portal> portals);

// --- Generation ---

Error collectVertexWeldSegments(const GeometryData& geometry, const RoomsData& rooms, float radius,
                                VertexWeldSegments& out);
void resetRoomDistances(RoomDistances& distances, std::size_t room_count);

Error generateRoomDistances(RoomDistances& out, const RoomsData& rooms, const GeometryData& geometry,
                            const RoomDistanceOptions& options,
                            RoomDistanceGenerationDiagnostics* diagnostics = nullptr);

}  // namespace rooms
}  // namespace pistoris
