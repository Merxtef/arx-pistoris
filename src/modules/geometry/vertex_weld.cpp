// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::geometry {
namespace {

constexpr std::uint32_t kProtectedVertexRole = std::numeric_limits<std::uint32_t>::max() - 1U;
constexpr std::uint32_t kUnlistedVertexRole = std::numeric_limits<std::uint32_t>::max();

bool validMetric(PositionWeldMetric metric) noexcept {
  switch (metric) {
    case PositionWeldMetric::kEuclidean:
    case PositionWeldMetric::kAxisAligned:
      return true;
  }
  return false;
}

bool validDegenerateFacePolicy(DegenerateFacePolicy policy) noexcept {
  switch (policy) {
    case DegenerateFacePolicy::kReject:
    case DegenerateFacePolicy::kDiscard:
    case DegenerateFacePolicy::kPreserve:
      return true;
  }
  return false;
}

bool validOptions(const VertexWeldOptions& options) noexcept {
  return options.radius > 0.0f && std::isfinite(options.radius) && validMetric(options.metric) &&
         validDegenerateFacePolicy(options.degenerate_faces);
}

Error validateIndexingPreconditions(const GeometryData& geometry) noexcept {
  if (geometry.vertices.size() > static_cast<std::size_t>(kInvalidVertexIndex)) return Error::kTooManyVertices;
  if (geometry.faces.size() > static_cast<std::size_t>(kInvalidFaceIndex)) return Error::kTooManyFaces;
  for (const Face& face : geometry.faces) {
    for (const Corner& corner : face.corners) {
      if (corner.vertex >= geometry.vertices.size()) return Error::kBadFaceVertex;
    }
  }
  return Error::kNone;
}

Error classifyVertices(std::size_t vertex_count, const SegmentedVertexWeldInput& input,
                       std::vector<std::uint32_t>& roles) {
  if (input.segments.size() > static_cast<std::size_t>(kProtectedVertexRole)) return Error::kInvalidOptions;

  roles.assign(vertex_count, kUnlistedVertexRole);
  for (VertexIndex vertex : input.protected_vertices) {
    if (vertex >= vertex_count) return Error::kBadVertexWeldSegment;
    roles[vertex] = kProtectedVertexRole;
  }

  for (std::size_t segment = 0; segment < input.segments.size(); ++segment) {
    for (VertexIndex vertex : input.segments[segment].vertices) {
      if (vertex >= vertex_count) return Error::kBadVertexWeldSegment;
      std::uint32_t& role = roles[vertex];
      if (role == kProtectedVertexRole) continue;
      if (role == kUnlistedVertexRole) {
        role = static_cast<std::uint32_t>(segment);
      } else if (role != static_cast<std::uint32_t>(segment)) {
        return Error::kOverlappingVertexWeldSegments;
      }
    }
  }
  return Error::kNone;
}

bool protectedVertex(VertexIndex vertex, std::span<const std::uint32_t> roles) noexcept {
  return roles[vertex] == kProtectedVertexRole;
}

class RepresentativeQueue {
 public:
  explicit RepresentativeQueue(std::vector<std::size_t> counts)
      : counts_(std::move(counts)), positions_(counts_.size()), heap_(counts_.size()) {
    for (std::size_t i = 0; i < heap_.size(); ++i) {
      heap_[i] = static_cast<VertexIndex>(i);
      positions_[i] = i;
    }
    for (std::size_t i = heap_.size() / 2; i > 0; --i) siftDown(i - 1);
  }

  [[nodiscard]] bool empty() const noexcept { return heap_.empty(); }
  [[nodiscard]] VertexIndex best() const noexcept { return heap_.front(); }
  [[nodiscard]] bool contains(VertexIndex vertex) const noexcept {
    return vertex < positions_.size() && positions_[vertex] != kNotQueued;
  }

  void remove(VertexIndex vertex) {
    const std::size_t position = positions_[vertex];
    const std::size_t last = heap_.size() - 1;
    swapEntries(position, last);
    heap_.pop_back();
    positions_[vertex] = kNotQueued;
    if (position == heap_.size()) return;
    if (position > 0 && higherPriority(heap_[position], heap_[(position - 1) / 2])) {
      siftUp(position);
    } else {
      siftDown(position);
    }
  }

  void decrementCount(VertexIndex vertex) {
    if (!contains(vertex) || counts_[vertex] == 0) return;
    --counts_[vertex];
    siftDown(positions_[vertex]);
  }

 private:
  static constexpr std::size_t kNotQueued = std::numeric_limits<std::size_t>::max();

  [[nodiscard]] bool higherPriority(VertexIndex lhs, VertexIndex rhs) const noexcept {
    return counts_[lhs] > counts_[rhs] || (counts_[lhs] == counts_[rhs] && lhs < rhs);
  }

  void swapEntries(std::size_t lhs, std::size_t rhs) {
    if (lhs == rhs) return;
    std::swap(heap_[lhs], heap_[rhs]);
    positions_[heap_[lhs]] = lhs;
    positions_[heap_[rhs]] = rhs;
  }

  void siftUp(std::size_t position) {
    while (position > 0) {
      const std::size_t parent = (position - 1) / 2;
      if (!higherPriority(heap_[position], heap_[parent])) return;
      swapEntries(position, parent);
      position = parent;
    }
  }

  void siftDown(std::size_t position) {
    while (true) {
      const std::size_t left = position * 2 + 1;
      if (left >= heap_.size()) return;
      const std::size_t right = left + 1;
      std::size_t best_child = left;
      if (right < heap_.size() && higherPriority(heap_[right], heap_[left])) best_child = right;
      if (!higherPriority(heap_[best_child], heap_[position])) return;
      swapEntries(position, best_child);
      position = best_child;
    }
  }

  std::vector<std::size_t> counts_;
  std::vector<std::size_t> positions_;
  std::vector<VertexIndex> heap_;
};

double distanceSquared(const ArxVector3& lhs, const ArxVector3& rhs) noexcept {
  const double x = static_cast<double>(lhs.x) - rhs.x;
  const double y = static_cast<double>(lhs.y) - rhs.y;
  const double z = static_cast<double>(lhs.z) - rhs.z;
  return x * x + y * y + z * z;
}

void planSegmentRepresentatives(const GeometryData& geometry, std::span<const VertexIndex> segment,
                                std::span<const std::uint32_t> roles, const VertexWeldOptions& options,
                                std::vector<VertexIndex>& representatives) {
  std::vector<VertexIndex> members(segment.begin(), segment.end());
  std::sort(members.begin(), members.end());
  members.erase(std::unique(members.begin(), members.end()), members.end());
  if (members.empty()) return;

  PositionIndex index(options.radius, options.metric);
  for (std::size_t local = 0; local < members.size(); ++local) {
    index.add(static_cast<std::uint32_t>(local), geometry.vertices[members[local]].position);
  }

  std::vector<std::size_t> candidate_counts(members.size(), 0);
  for (std::size_t local = 0; local < members.size(); ++local) {
    for (std::uint32_t candidate : index.candidates(geometry.vertices[members[local]].position)) {
      if (!protectedVertex(members[candidate], roles)) ++candidate_counts[local];
    }
  }

  RepresentativeQueue queue(std::move(candidate_counts));
  std::vector<std::uint8_t> assigned(members.size(), 0);
  while (!queue.empty()) {
    const VertexIndex center_local = queue.best();
    const VertexIndex center_vertex = members[center_local];
    const ArxVector3& center = geometry.vertices[center_vertex].position;

    std::vector<VertexIndex> group;
    std::optional<VertexIndex> closest_protected;
    double closest_distance = std::numeric_limits<double>::infinity();
    for (std::uint32_t candidate_local : index.candidates(center)) {
      const VertexIndex candidate = members[candidate_local];
      if (protectedVertex(candidate, roles)) {
        const double distance = distanceSquared(center, geometry.vertices[candidate].position);
        if (distance < closest_distance ||
            (distance == closest_distance && (!closest_protected || candidate < *closest_protected))) {
          closest_protected = candidate;
          closest_distance = distance;
        }
      } else if (assigned[candidate_local] == 0) {
        group.push_back(static_cast<VertexIndex>(candidate_local));
      }
    }

    if (group.empty()) {
      queue.remove(center_local);
      continue;
    }

    const VertexIndex representative = closest_protected.value_or(center_vertex);
    for (VertexIndex candidate_local : group) {
      representatives[members[candidate_local]] = representative;
      assigned[candidate_local] = 1;
      queue.remove(candidate_local);
    }
    if (protectedVertex(center_vertex, roles) && queue.contains(center_local)) queue.remove(center_local);

    for (VertexIndex candidate_local : group) {
      const ArxVector3& position = geometry.vertices[members[candidate_local]].position;
      for (std::uint32_t neighbor : index.candidates(position)) {
        queue.decrementCount(static_cast<VertexIndex>(neighbor));
      }
    }
  }
}

bool faceDegenerateAfterRemap(const GeometryData& geometry, const Face& face,
                              std::span<const VertexIndex> representatives) {
  std::array<ArxVector3, 3> positions{};
  for (std::size_t corner = 0; corner < face.corners.size(); ++corner) {
    positions[corner] = geometry.vertices[representatives[face.corners[corner].vertex]].position;
  }
  return degenerateTriangle(positions[0], positions[1], positions[2]);
}

Error preserveNondegenerateFaces(const GeometryData& geometry, std::vector<VertexIndex>& representatives) {
  bool changed = false;
  do {
    changed = false;
    for (const Face& face : geometry.faces) {
      if (!faceDegenerateAfterRemap(geometry, face, representatives)) continue;
      bool restored = false;
      for (const Corner& corner : face.corners) {
        if (representatives[corner.vertex] == corner.vertex) continue;
        representatives[corner.vertex] = corner.vertex;
        restored = true;
        changed = true;
      }
      if (!restored) return Error::kDegenerateFace;
    }
  } while (changed);
  return Error::kNone;
}

VertexIndexRemap buildVertexRemap(std::span<const VertexIndex> representatives) {
  VertexIndexRemap remap(representatives.size(), kInvalidVertexIndex);
  VertexIndex next = 0;
  for (std::size_t old = 0; old < representatives.size(); ++old) {
    if (representatives[old] == old) remap[old] = next++;
  }
  for (std::size_t old = 0; old < representatives.size(); ++old) {
    if (representatives[old] != old) remap[old] = remap[representatives[old]];
  }
  return remap;
}

Error buildFaceRemap(const GeometryData& geometry, std::span<const VertexIndex> representatives,
                     DegenerateFacePolicy policy, FaceIndexRemap& face_remap) {
  if (policy != DegenerateFacePolicy::kDiscard) {
    for (const Face& face : geometry.faces)
      if (faceDegenerateAfterRemap(geometry, face, representatives)) return Error::kDegenerateFace;
    return Error::kNone;
  }

  face_remap.assign(geometry.faces.size(), kInvalidFaceIndex);
  FaceIndex next = 0;
  bool discarded = false;
  for (std::size_t old = 0; old < geometry.faces.size(); ++old) {
    if (faceDegenerateAfterRemap(geometry, geometry.faces[old], representatives)) {
      discarded = true;
      continue;
    }
    face_remap[old] = next++;
  }
  if (!discarded) face_remap.clear();
  return Error::kNone;
}

void applyVertexRemap(GeometryData& geometry, std::span<const VertexIndex> representatives,
                      std::span<const VertexIndex> vertex_remap) noexcept {
  std::size_t next = 0;
  for (std::size_t old = 0; old < representatives.size(); ++old) {
    if (representatives[old] != old) continue;
    if (next != old) geometry.vertices[next] = geometry.vertices[old];
    ++next;
  }
  while (geometry.vertices.size() > next) geometry.vertices.pop_back();

  for (Face& face : geometry.faces)
    for (Corner& corner : face.corners) corner.vertex = vertex_remap[corner.vertex];
}

void applyFaceRemap(GeometryData& geometry, std::span<const FaceIndex> face_remap) noexcept {
  if (face_remap.empty()) return;
  std::size_t next = 0;
  for (std::size_t old = 0; old < face_remap.size(); ++old) {
    if (face_remap[old] == kInvalidFaceIndex) continue;
    if (next != old) geometry.faces[next] = geometry.faces[old];
    ++next;
  }
  while (geometry.faces.size() > next) geometry.faces.pop_back();
}

template <typename Id>
bool identityRemap(std::span<const Id> remap) {
  Id expected = 0;
  for (Id mapped : remap) {
    if (mapped != expected) return false;
    ++expected;
  }
  return true;
}

Error weldVerticesImpl(GeometryData& geometry, const SegmentedVertexWeldInput& input, const VertexWeldOptions& options,
                       GeometryRemap* out_remap) {
  if (out_remap) *out_remap = {};
  if (!validOptions(options)) return Error::kInvalidOptions;

  Error error = validateIndexingPreconditions(geometry);
  if (error != Error::kNone) return error;

  std::vector<std::uint32_t> roles;
  error = classifyVertices(geometry.vertices.size(), input, roles);
  if (error != Error::kNone) return error;

  std::vector<VertexIndex> representatives(geometry.vertices.size());
  std::iota(representatives.begin(), representatives.end(), VertexIndex{0});
  for (const VertexWeldSegment& segment : input.segments) {
    planSegmentRepresentatives(geometry, segment.vertices, roles, options, representatives);
  }
  if (options.degenerate_faces == DegenerateFacePolicy::kPreserve) {
    error = preserveNondegenerateFaces(geometry, representatives);
    if (error != Error::kNone) return error;
  }

  VertexIndexRemap vertex_remap = buildVertexRemap(representatives);
  FaceIndexRemap face_remap;
  error = buildFaceRemap(geometry, representatives, options.degenerate_faces, face_remap);
  if (error != Error::kNone) return error;

  applyVertexRemap(geometry, representatives, vertex_remap);
  applyFaceRemap(geometry, face_remap);
  if (out_remap && !identityRemap<VertexIndex>(vertex_remap)) out_remap->vertices = std::move(vertex_remap);
  if (out_remap && !face_remap.empty()) out_remap->faces = std::move(face_remap);
  return Error::kNone;
}

}  // namespace

Error weldVertices(GeometryData& geometry, const VertexWeldOptions& options, GeometryRemap* out_remap) {
  if (out_remap) *out_remap = {};
  if (!validOptions(options)) return Error::kInvalidOptions;
  if (geometry.vertices.size() > static_cast<std::size_t>(kInvalidVertexIndex)) return Error::kTooManyVertices;

  std::vector<VertexIndex> vertices(geometry.vertices.size());
  std::iota(vertices.begin(), vertices.end(), VertexIndex{0});
  const VertexWeldSegment segment{vertices};
  const std::array segments{segment};
  return weldVerticesImpl(geometry, {.segments = segments, .protected_vertices = {}}, options, out_remap);
}

Error weldVerticesSegmented(GeometryData& geometry, const SegmentedVertexWeldInput& input,
                            const VertexWeldOptions& options, GeometryRemap* out_remap) {
  return weldVerticesImpl(geometry, input, options, out_remap);
}

}  // namespace pistoris::geometry
