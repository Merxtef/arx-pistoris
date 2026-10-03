// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"
#include "utils/math/bounds.h"
#include "utils/math/geometry_algorithms.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::rooms {

RoomGeometryIndex::RoomGeometryIndex(const RoomsData& rooms, const GeometryData& geometry) {
  std::vector<geometry::IndexedTriangle> triangles;
  triangles.reserve(geometry.faces.size());
  face_rooms_.assign(geometry.faces.size(), kInvalidRoomIndex);
  face_flags_.reserve(geometry.faces.size());
  face_normals_.reserve(geometry.faces.size());

  std::vector<std::size_t> room_face_counts(rooms.definitions.size(), 0);
  room_bounds_.resize(rooms.definitions.size());
  room_has_bounds_.assign(rooms.definitions.size(), 0);
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    const Face& face = geometry.faces[face_index];
    const std::array<ArxVector3, 3> vertices = geometry::facePositions(geometry, face);
    const ArxAabb bounds = geometry::triangleBounds(vertices);
    triangles.push_back({vertices, bounds});
    face_flags_.push_back(face.flags);
    face_normals_.push_back(
        math::normalizeFiniteOr(math::cross(vertices[1] - vertices[0], vertices[2] - vertices[0]), {}));

    if (face_index >= rooms.face_rooms.size()) continue;
    const RoomIndex room = rooms.face_rooms[face_index];
    face_rooms_[face_index] = room;
    if (room >= rooms.definitions.size() || (face.flags & kRoomDistanceIgnoreFlags) != 0) continue;
    ++room_face_counts[room];
    if (room_has_bounds_[room] == 0) {
      room_bounds_[room] = bounds;
      room_has_bounds_[room] = 1;
    } else {
      math::expand(room_bounds_[room], bounds.min);
      math::expand(room_bounds_[room], bounds.max);
    }
  }
  triangles_ = geometry::TriangleIndex(std::move(triangles));

  room_face_offsets_.resize(rooms.definitions.size() + 1U, 0);
  for (std::size_t room = 0; room < room_face_counts.size(); ++room)
    room_face_offsets_[room + 1U] = room_face_offsets_[room] + room_face_counts[room];
  room_faces_.resize(room_face_offsets_.back());
  std::vector<std::size_t> next = room_face_offsets_;
  for (std::size_t face_index = 0; face_index < face_rooms_.size(); ++face_index) {
    const RoomIndex room = face_rooms_[face_index];
    if (room >= rooms.definitions.size() || (face_flags_[face_index] & kRoomDistanceIgnoreFlags) != 0) continue;
    room_faces_[next[room]++] = static_cast<FaceIndex>(face_index);
  }
}

std::span<const FaceIndex> RoomGeometryIndex::roomFaces(RoomIndex room) const noexcept {
  if (static_cast<std::size_t>(room) + 1U >= room_face_offsets_.size()) return {};
  const std::size_t begin = room_face_offsets_[room];
  const std::size_t end = room_face_offsets_[room + 1U];
  if (begin == end) return {};
  return {room_faces_.data() + begin, end - begin};
}

bool RoomGeometryIndex::hasRoomBounds(RoomIndex room) const noexcept {
  return room < room_has_bounds_.size() && room_has_bounds_[room] != 0;
}

const ArxAabb& RoomGeometryIndex::roomBounds(RoomIndex room) const { return room_bounds_[room]; }

std::array<ArxVector3, 3> RoomGeometryIndex::triangle(FaceIndex face) const {
  return triangles_.triangle(face).vertices;
}

void RoomGeometryIndex::findSupportHits(std::vector<geometry::SurfaceSupportHit>& out, RoomIndex room, float x,
                                        float z) const {
  out.clear();
  struct Context {
    const RoomGeometryIndex* index = nullptr;
    std::vector<geometry::SurfaceSupportHit>* out = nullptr;
    RoomIndex room = kInvalidRoomIndex;
    float x = 0.0f;
    float z = 0.0f;
  } context{this, &out, room, x, z};
  triangles_.visitCandidatesForXz(
      x,
      z,
      [](std::uint32_t face, void* raw) {
        auto& context = *static_cast<Context*>(raw);
        if (face >= context.index->face_rooms_.size() || context.index->face_rooms_[face] != context.room ||
            (context.index->face_flags_[face] & kRoomDistanceIgnoreFlags) != 0)
          return;
        const std::array<ArxVector3, 3>& vertices = context.index->triangles_.triangle(face).vertices;
        std::array<double, 3> weights{};
        if (!math::barycentricXz(vertices[0], vertices[1], vertices[2], context.x, context.z, weights)) return;
        context.out->push_back({static_cast<FaceIndex>(face),
                                math::interpolate(vertices[0], vertices[1], vertices[2], weights),
                                context.index->face_normals_[face]});
      },
      &context);
}

bool RoomGeometryIndex::segmentBlocked(RoomIndex room, const ArxVector3& start, const ArxVector3& end,
                                       float endpoint_epsilon, std::vector<std::uint32_t>& scratch) const {
  triangles_.findCandidatesForSegment(scratch, start, end);
  for (std::uint32_t face : scratch) {
    if (face >= face_rooms_.size() || face_rooms_[face] != room || (face_flags_[face] & kRoomDistanceIgnoreFlags) != 0)
      continue;
    const std::array<ArxVector3, 3>& vertices = triangles_.triangle(face).vertices;
    double t = 0.0;
    if (!geometry::segmentTriangleIntersectionT(start, end, vertices[0], vertices[1], vertices[2], t)) continue;
    if (t > endpoint_epsilon && t < 1.0 - endpoint_epsilon) return true;
  }
  return false;
}

}  // namespace pistoris::rooms
