// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/geometry.h"
#include "modules/geometry/internal.h"
#include "utils/log.h"

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
  return options.radius > 0.0f && std::isfinite(options.radius) && validPositionWeldMetric(options.metric) &&
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
  RepresentativeQueue(std::vector<std::size_t>& counts, std::vector<std::size_t>& positions,
                      std::vector<VertexIndex>& heap)
      : counts_(counts), positions_(positions), heap_(heap) {
    positions_.resize(counts_.size());
    heap_.resize(counts_.size());
    for (std::size_t i = 0; i < heap_.size(); ++i) {
      heap_[i] = static_cast<VertexIndex>(i);
      positions_[i] = i;
    }
    for (std::size_t i = heap_.size() / 2; i > 0; --i) siftDown(i - 1);
  }

  [[nodiscard]] bool empty() const noexcept { return heap_.empty(); }
  [[nodiscard]] VertexIndex best() const noexcept { return heap_.front(); }
  [[nodiscard]] bool contains(VertexIndex vertex) const noexcept {
    if (vertex >= positions_.size()) return false;
    const std::size_t position = positions_[vertex];
    return position < heap_.size() && heap_[position] == vertex;
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

  std::vector<std::size_t>& counts_;
  std::vector<std::size_t>& positions_;
  std::vector<VertexIndex>& heap_;
};

struct WeldScratch {
  WeldScratch(std::size_t vertex_count, std::size_t max_segment_size) {
    candidates.reserve(vertex_count);
    candidate_counts.reserve(max_segment_size);
    queue_positions.reserve(max_segment_size);
    queue_heap.reserve(max_segment_size);
    assigned.reserve(max_segment_size);
    group.reserve(max_segment_size);
  }

  std::vector<std::uint32_t> candidates;
  std::vector<std::size_t> candidate_counts;
  std::vector<std::size_t> queue_positions;
  std::vector<VertexIndex> queue_heap;
  std::vector<std::uint8_t> assigned;
  std::vector<VertexIndex> group;
};

struct SegmentedWeldScratch {
  SegmentedWeldScratch(std::size_t vertex_count, std::size_t max_segment_size)
      : plan(vertex_count, max_segment_size),
        local_indices(vertex_count, kInvalidVertexIndex),
        segment_membership(vertex_count, kUnlistedVertexRole) {
    members.reserve(max_segment_size);
  }

  WeldScratch plan;
  std::vector<VertexIndex> members;
  std::vector<VertexIndex> local_indices;
  std::vector<std::uint32_t> segment_membership;
};

double distanceSquared(const ArxVector3& lhs, const ArxVector3& rhs) noexcept {
  const double x = static_cast<double>(lhs.x) - rhs.x;
  const double y = static_cast<double>(lhs.y) - rhs.y;
  const double z = static_cast<double>(lhs.z) - rhs.z;
  return x * x + y * y + z * z;
}

bool strictlyIncreasing(std::span<const VertexIndex> values) noexcept {
  for (std::size_t index = 1; index < values.size(); ++index)
    if (values[index - 1U] >= values[index]) return false;
  return true;
}

std::span<const VertexIndex> normalizedMembers(std::span<const VertexIndex> segment,
                                               std::vector<VertexIndex>& scratch) {
  if (strictlyIncreasing(segment)) return segment;
  scratch.assign(segment.begin(), segment.end());
  std::sort(scratch.begin(), scratch.end());
  scratch.erase(std::unique(scratch.begin(), scratch.end()), scratch.end());
  return scratch;
}

struct AllVertexDomain {
  std::size_t count = 0;

  [[nodiscard]] std::size_t size() const noexcept { return count; }
  [[nodiscard]] bool empty() const noexcept { return count == 0; }
  [[nodiscard]] VertexIndex vertex(std::size_t local) const noexcept { return static_cast<VertexIndex>(local); }
  [[nodiscard]] VertexIndex local(std::uint32_t vertex_index) const noexcept { return vertex_index; }
  [[nodiscard]] bool contains(std::uint32_t vertex_index) const noexcept { return vertex_index < count; }
  [[nodiscard]] bool protectedVertex(std::uint32_t) const noexcept { return false; }
};

struct SegmentedVertexDomain {
  std::span<const VertexIndex> members;
  std::uint32_t segment = 0;
  std::span<const std::uint32_t> roles;
  std::span<const VertexIndex> local_indices;
  std::span<const std::uint32_t> segment_membership;

  [[nodiscard]] std::size_t size() const noexcept { return members.size(); }
  [[nodiscard]] bool empty() const noexcept { return members.empty(); }
  [[nodiscard]] VertexIndex vertex(std::size_t local) const noexcept { return members[local]; }
  [[nodiscard]] VertexIndex local(std::uint32_t vertex_index) const noexcept { return local_indices[vertex_index]; }
  [[nodiscard]] bool contains(std::uint32_t vertex_index) const noexcept {
    return segment_membership[vertex_index] == segment;
  }
  [[nodiscard]] bool protectedVertex(std::uint32_t vertex_index) const noexcept {
    return geometry::protectedVertex(static_cast<VertexIndex>(vertex_index), roles);
  }
};

template <typename Domain>
Error planRepresentatives(const GeometryData& geometry, const Domain& domain, const PositionIndex& position_index,
                          WeldScratch& scratch, std::vector<VertexIndex>& representatives) {
  if (domain.empty()) return Error::kNone;

  scratch.candidate_counts.assign(domain.size(), 0);
  for (std::size_t local = 0; local < domain.size(); ++local) {
    position_index.findCandidates(scratch.candidates, geometry.vertices[domain.vertex(local)].position);
    for (std::uint32_t candidate : scratch.candidates) {
      if (!domain.contains(candidate)) continue;
      if (!domain.protectedVertex(candidate)) ++scratch.candidate_counts[local];
    }
  }

  RepresentativeQueue queue(scratch.candidate_counts, scratch.queue_positions, scratch.queue_heap);
  scratch.assigned.assign(domain.size(), 0);
  while (!queue.empty()) {
    const VertexIndex center_local = queue.best();
    const VertexIndex center_vertex = domain.vertex(center_local);
    const ArxVector3& center = geometry.vertices[center_vertex].position;

    scratch.group.clear();
    std::optional<VertexIndex> closest_protected;
    double closest_distance = std::numeric_limits<double>::infinity();
    position_index.findCandidates(scratch.candidates, center);
    for (std::uint32_t candidate_index : scratch.candidates) {
      if (!domain.contains(candidate_index)) continue;
      const VertexIndex candidate = static_cast<VertexIndex>(candidate_index);
      const VertexIndex candidate_local = domain.local(candidate);
      if (domain.protectedVertex(candidate)) {
        const double distance = distanceSquared(center, geometry.vertices[candidate].position);
        if (distance < closest_distance ||
            (distance == closest_distance && (!closest_protected || candidate < *closest_protected))) {
          closest_protected = candidate;
          closest_distance = distance;
        }
      } else if (scratch.assigned[candidate_local] == 0) {
        scratch.group.push_back(candidate_local);
      }
    }

    if (scratch.group.empty()) {
      queue.remove(center_local);
      continue;
    }

    const VertexIndex representative = closest_protected.value_or(center_vertex);
    for (VertexIndex candidate_local : scratch.group) {
      representatives[domain.vertex(candidate_local)] = representative;
      scratch.assigned[candidate_local] = 1;
      queue.remove(candidate_local);
    }
    if (domain.protectedVertex(center_vertex) && queue.contains(center_local)) queue.remove(center_local);

    for (VertexIndex candidate_local : scratch.group) {
      const ArxVector3& position = geometry.vertices[domain.vertex(candidate_local)].position;
      position_index.findCandidates(scratch.candidates, position);
      for (std::uint32_t neighbor : scratch.candidates) {
        if (!domain.contains(neighbor)) continue;
        queue.decrementCount(domain.local(neighbor));
      }
    }
  }
  return Error::kNone;
}

Error planSegmentRepresentatives(const GeometryData& geometry, std::span<const VertexIndex> segment,
                                 std::uint32_t segment_index, std::span<const std::uint32_t> roles,
                                 const PositionIndex& position_index, SegmentedWeldScratch& scratch,
                                 std::vector<VertexIndex>& representatives) {
  const std::span<const VertexIndex> members = normalizedMembers(segment, scratch.members);
  for (std::size_t local = 0; local < members.size(); ++local) {
    scratch.segment_membership[members[local]] = segment_index;
    scratch.local_indices[members[local]] = static_cast<VertexIndex>(local);
  }
  const SegmentedVertexDomain domain{members, segment_index, roles, scratch.local_indices, scratch.segment_membership};
  return planRepresentatives(geometry, domain, position_index, scratch.plan, representatives);
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
  std::vector<FaceIndex> pending;
  pending.reserve(geometry.faces.size());
  std::vector<std::uint8_t> queued(geometry.faces.size(), 0);
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    if (!faceDegenerateAfterRemap(geometry, geometry.faces[face_index], representatives)) continue;
    pending.push_back(static_cast<FaceIndex>(face_index));
    queued[face_index] = 1;
  }
  if (pending.empty()) return Error::kNone;

  std::vector<std::size_t> offsets(geometry.vertices.size() + 1U, 0);
  for (const Face& face : geometry.faces)
    for (const Corner& corner : face.corners) ++offsets[static_cast<std::size_t>(corner.vertex) + 1U];
  std::partial_sum(offsets.begin(), offsets.end(), offsets.begin());
  std::vector<std::size_t> cursors = offsets;
  std::vector<FaceIndex> incident_faces(offsets.back());
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    for (const Corner& corner : geometry.faces[face_index].corners) {
      incident_faces[cursors[corner.vertex]++] = static_cast<FaceIndex>(face_index);
    }
  }

  while (!pending.empty()) {
    const FaceIndex face_index = pending.back();
    pending.pop_back();
    queued[face_index] = 0;
    const Face& face = geometry.faces[face_index];
    if (!faceDegenerateAfterRemap(geometry, face, representatives)) continue;

    std::array<VertexIndex, 3> restored_vertices{};
    std::size_t restored_count = 0;
    for (const Corner& corner : face.corners) {
      if (representatives[corner.vertex] == corner.vertex) continue;
      representatives[corner.vertex] = corner.vertex;
      restored_vertices[restored_count++] = corner.vertex;
    }
    if (restored_count == 0) return Error::kDegenerateFace;

    for (std::size_t restored = 0; restored < restored_count; ++restored) {
      const VertexIndex vertex = restored_vertices[restored];
      for (std::size_t incident = offsets[vertex]; incident < offsets[static_cast<std::size_t>(vertex) + 1U];
           ++incident) {
        const FaceIndex affected = incident_faces[incident];
        if (queued[affected] != 0) continue;
        pending.push_back(affected);
        queued[affected] = 1;
      }
    }
  }
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

Error finishWeld(GeometryData& geometry, const VertexWeldOptions& options, std::vector<VertexIndex>& representatives,
                 std::size_t segment_count, std::size_t protected_count, GeometryRemap* out_remap) {
  const std::size_t old_vertex_count = geometry.vertices.size();
  const std::size_t old_face_count = geometry.faces.size();
  Error error = Error::kNone;
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
  const std::size_t discarded_faces = old_face_count - geometry.faces.size();
  log(ARX_LOG_DEBUG,
      "Geometry weld: {} -> {} vertices, {} -> {} faces, radius {}, metric {}, face policy {}, {} segments, {} "
      "protected vertices",
      old_vertex_count,
      geometry.vertices.size(),
      old_face_count,
      geometry.faces.size(),
      options.radius,
      static_cast<int>(options.metric),
      static_cast<int>(options.degenerate_faces),
      segment_count,
      protected_count);
  if (discarded_faces != 0) log(ARX_LOG_WARN, "Geometry weld discarded {} degenerate face(s)", discarded_faces);
  if (out_remap && !identityRemap<VertexIndex>(vertex_remap)) out_remap->vertices = std::move(vertex_remap);
  if (out_remap && !face_remap.empty()) out_remap->faces = std::move(face_remap);
  return Error::kNone;
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

  PositionIndex position_index(options.radius, options.metric);
  position_index.reservePositionCapacity(roles.size());
  for (std::size_t vertex = 0; vertex < roles.size(); ++vertex) {
    if (roles[vertex] == kUnlistedVertexRole) continue;
    if (!position_index.tryAdd(static_cast<VertexIndex>(vertex), geometry.vertices[vertex].position))
      return Error::kBadVertex;
  }

  std::size_t max_segment_size = 0;
  for (const VertexWeldSegment& segment : input.segments)
    max_segment_size = std::max(max_segment_size, segment.vertices.size());
  SegmentedWeldScratch scratch(geometry.vertices.size(), max_segment_size);
  std::vector<VertexIndex> representatives(geometry.vertices.size());
  std::iota(representatives.begin(), representatives.end(), VertexIndex{0});
  for (std::size_t segment = 0; segment < input.segments.size(); ++segment) {
    error = planSegmentRepresentatives(geometry,
                                       input.segments[segment].vertices,
                                       static_cast<std::uint32_t>(segment),
                                       roles,
                                       position_index,
                                       scratch,
                                       representatives);
    if (error != Error::kNone) return error;
  }
  return finishWeld(
      geometry, options, representatives, input.segments.size(), input.protected_vertices.size(), out_remap);
}

}  // namespace

Error weldVertices(GeometryData& geometry, const VertexWeldOptions& options, GeometryRemap* out_remap) {
  if (out_remap) *out_remap = {};
  if (!validOptions(options)) return Error::kInvalidOptions;
  Error error = validateIndexingPreconditions(geometry);
  if (error != Error::kNone) return error;

  PositionIndex position_index(options.radius, options.metric);
  position_index.reservePositionCapacity(geometry.vertices.size());
  for (std::size_t vertex = 0; vertex < geometry.vertices.size(); ++vertex) {
    if (!position_index.tryAdd(static_cast<VertexIndex>(vertex), geometry.vertices[vertex].position))
      return Error::kBadVertex;
  }

  WeldScratch scratch(geometry.vertices.size(), geometry.vertices.size());
  std::vector<VertexIndex> representatives(geometry.vertices.size());
  std::iota(representatives.begin(), representatives.end(), VertexIndex{0});
  error = planRepresentatives(
      geometry, AllVertexDomain{geometry.vertices.size()}, position_index, scratch, representatives);
  if (error != Error::kNone) return error;
  return finishWeld(geometry, options, representatives, 0, 0, out_remap);
}

Error weldVerticesSegmented(GeometryData& geometry, const SegmentedVertexWeldInput& input,
                            const VertexWeldOptions& options, GeometryRemap* out_remap) {
  return weldVerticesImpl(geometry, input, options, out_remap);
}

}  // namespace pistoris::geometry
