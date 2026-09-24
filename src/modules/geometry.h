// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "utils/spatial/arx_level_grid_index.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace pistoris {

struct Vertex {
  ArxVector3 position = {};
};

struct Corner {
  VertexIndex vertex = 0;
  ArxVector3 normal = {};
  float u = 0.0f;
  float v = 0.0f;
};

struct Face {
  std::array<Corner, 3> corners = {};
  TextureIndex texture = kNoTexture;
  FaceType flags = 0;
  float transval = 0.0f;
  ArxVector3 normal = {};
};

struct GeometryData {
  std::vector<Vertex> vertices;
  std::vector<Face> faces;
};

struct GeometryDerived {
  ArxAabb bounds = {};
  ArxAabb referenced_bounds = {};
};

namespace geometry {

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kBadIndex,
  kNoGeometry,
  kTooManyVertices,
  kTooManyFaces,
  kBadVertex,
  kOutOfMemory,
  kBadFaceTexture,
  kBadFaceVertex,
  kBadCornerNormal,
  kBadFaceType,
  kBadFaceTransval,
  kBadFaceNormal,
  kBadFaceUv,
  kDegenerateFace,
  kBadVertexWeldSegment,
  kOverlappingVertexWeldSegments,
};

using FacePredicateFn = bool (*)(FaceIndex face, const void* user_data) noexcept;

struct FacePredicate {
  FacePredicateFn function = nullptr;
  const void* user_data = nullptr;
};

enum class PositionWeldMetric : std::uint8_t {
  kEuclidean,
  kAxisAligned,
};

enum class DegenerateFacePolicy : std::uint8_t {
  kReject,
  kDiscard,
  kPreserve,
};

struct VertexWeldOptions {
  float radius = 1.0e-4f;
  PositionWeldMetric metric = PositionWeldMetric::kEuclidean;
  DegenerateFacePolicy degenerate_faces = DegenerateFacePolicy::kReject;
};

struct VertexWeldSegment {
  std::span<const VertexIndex> vertices;
};

struct SegmentedVertexWeldInput {
  std::span<const VertexWeldSegment> segments;
  std::span<const VertexIndex> protected_vertices;
};

using VertexIndexRemap = std::vector<VertexIndex>;
using FaceIndexRemap = std::vector<FaceIndex>;

struct PositionKey {
  std::int64_t x = 0;
  std::int64_t y = 0;
  std::int64_t z = 0;

  bool operator==(const PositionKey& other) const = default;
};

struct PositionKeyHash {
  std::size_t operator()(const PositionKey& key) const;
};

class PositionIndex {
 public:
  explicit PositionIndex(float radius, PositionWeldMetric metric = PositionWeldMetric::kAxisAligned);

  [[nodiscard]] bool enabled() const noexcept { return valid(); }
  void reservePositionCapacity(std::size_t capacity);
  [[nodiscard]] bool tryAdd(std::uint32_t index, const ArxVector3& position);
  void findCandidates(std::vector<std::uint32_t>& out, const ArxVector3& position) const;
  [[nodiscard]] std::optional<std::uint32_t> find(const ArxVector3& position) const;

 private:
  struct Entry {
    std::uint32_t index = 0;
    ArxVector3 position = {};
  };

  float radius_ = 0.0f;
  PositionWeldMetric metric_ = PositionWeldMetric::kAxisAligned;
  std::unordered_map<PositionKey, std::vector<Entry>, PositionKeyHash> by_position_;

  [[nodiscard]] PositionKey key(const ArxVector3& position) const;
  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] bool samePosition(const ArxVector3& a, const ArxVector3& b) const noexcept;
};

struct IndexedTriangle {
  std::array<ArxVector3, 3> vertices = {};
  ArxAabb bounds = {};
};

class TriangleIndex {
 public:
  TriangleIndex() = default;
  explicit TriangleIndex(std::span<const IndexedTriangle> triangles);
  explicit TriangleIndex(std::vector<IndexedTriangle>&& triangles);

  TriangleIndex(const TriangleIndex&) = delete;
  TriangleIndex& operator=(const TriangleIndex&) = delete;
  TriangleIndex(TriangleIndex&&) noexcept = default;  // NOLINT(bugprone-exception-escape): container moves are noexcept
  TriangleIndex& operator=(TriangleIndex&&) noexcept = default;

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] const IndexedTriangle& triangle(std::uint32_t index) const;
  void findCandidatesForXz(std::vector<std::uint32_t>& out, float x, float z) const;
  void findCandidatesForAabb(std::vector<std::uint32_t>& out, const ArxAabb& bounds) const;
  void findCandidatesForSegment(std::vector<std::uint32_t>& out, const ArxVector3& a, const ArxVector3& b) const;

  using CandidateVisitor = void (*)(std::uint32_t index, void* user_data);
  void visitCandidatesForXz(float x, float z, CandidateVisitor visitor, void* user_data) const;

 private:
  void buildIndex();

  std::vector<IndexedTriangle> triangles_;
  spatial::ArxLevelGridIndex grid_index_;
};

struct SurfaceSupportTriangle {
  FaceIndex face = kInvalidFaceIndex;
  std::array<ArxVector3, 3> vertices = {};
};

struct SurfaceSupportHit {
  FaceIndex face = kInvalidFaceIndex;
  ArxVector3 position = {};
  ArxVector3 normal = {};
};

class SurfaceSupportIndexBuilder;

class SurfaceSupportIndex {
 public:
  using HitVisitor = void (*)(const SurfaceSupportHit& hit, void* user_data);

  SurfaceSupportIndex(const SurfaceSupportIndex&) = delete;
  SurfaceSupportIndex& operator=(const SurfaceSupportIndex&) = delete;
  // NOLINTNEXTLINE(bugprone-exception-escape): standard container moves are noexcept
  SurfaceSupportIndex(SurfaceSupportIndex&&) noexcept = default;
  SurfaceSupportIndex& operator=(SurfaceSupportIndex&&) noexcept = default;

  [[nodiscard]] bool empty() const;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool hasBounds() const;
  [[nodiscard]] const ArxAabb& bounds() const;
  [[nodiscard]] SurfaceSupportTriangle triangle(std::size_t index) const;
  [[nodiscard]] SurfaceSupportIndex subset(std::span<const std::uint32_t> triangle_indices) const;
  [[nodiscard]] bool mayHaveHitsInAabb(const ArxAabb& bounds) const;
  void visitHitsAt(float x, float z, HitVisitor visitor, void* user_data) const;
  void findHitsAt(std::vector<SurfaceSupportHit>& out, float x, float z) const;
  void findDownwardHitsAt(std::vector<SurfaceSupportHit>& out, float x, float z, float origin_y) const;
  [[nodiscard]] std::optional<SurfaceSupportHit> closestDownwardHit(float x, float z, float origin_y) const;
  [[nodiscard]] std::optional<SurfaceSupportHit> closestHit(float x, float z, float reference_y, float max_delta) const;

 private:
  friend class SurfaceSupportIndexBuilder;

  struct Record {
    FaceIndex face = kInvalidFaceIndex;
    ArxVector3 normal = {};
    IndexedTriangle triangle = {};
  };

  struct Storage {
    std::vector<Record> records;
  };

  [[nodiscard]] std::optional<SurfaceSupportHit> hitAt(std::uint32_t index, float x, float z) const;
  void addTriangleToIndex(std::uint32_t index);
  void visitCandidatesForXz(float x, float z, TriangleIndex::CandidateVisitor visitor, void* user_data) const;

  SurfaceSupportIndex() = default;

  std::shared_ptr<const Storage> storage_;
  std::vector<std::uint32_t> triangle_indices_;
  spatial::ArxLevelGridIndex grid_index_;
  ArxAabb bounds_ = {};
  bool has_bounds_ = false;
};

class SurfaceSupportIndexBuilder {
 public:
  explicit SurfaceSupportIndexBuilder(std::size_t capacity);

  void addTriangle(FaceIndex face, const std::array<ArxVector3, 3>& vertices);
  SurfaceSupportIndex build() &&;

 private:
  std::vector<SurfaceSupportIndex::Record> records_;
};

struct GeometryRemap {
  VertexIndexRemap vertices;
  FaceIndexRemap faces;
};

class VertexFaceIndex {
 public:
  [[nodiscard]] std::span<const FaceIndex> incidentFaces(VertexIndex vertex) const noexcept;

 private:
  std::vector<std::size_t> offsets_;
  std::vector<FaceIndex> faces_;

  friend Error buildVertexFaceIndex(const GeometryData& geometry, VertexFaceIndex& out);
};

// --- Validation ---

Error validateVertex(const Vertex& vertex) noexcept;
Error validateCounts(std::size_t vertex_count, std::size_t face_count) noexcept;
Error validateVertexAppend(const GeometryData& geometry, std::size_t count) noexcept;
Error validateVertices(std::span<const Vertex> vertices, ArxAabb* out_bounds = nullptr) noexcept;
Error validateFaceReferences(const Face& face, std::size_t vertex_count, std::size_t texture_count) noexcept;
Error validateFaces(std::span<const Face> faces, std::span<const Vertex> vertices, std::size_t texture_count,
                    ArxAabb* out_referenced_bounds = nullptr) noexcept;
Error validate(const GeometryData& geometry, std::size_t texture_count, GeometryDerived* out = nullptr);
Error validateScale(const GeometryData& geometry, float factor) noexcept;
Error validateRotation(const GeometryData& geometry, const ArxMat3& rotation) noexcept;
Error validateTranslation(const GeometryData& geometry, const ArxVector3& offset) noexcept;

// --- Queries ---

std::size_t vertexCapacityForAppend(const GeometryData& geometry, std::size_t count, std::size_t limit) noexcept;
Error collectTextureUsage(const GeometryData& geometry, std::size_t texture_count, std::vector<std::uint8_t>& out);
Error buildVertexFaceIndex(const GeometryData& geometry, VertexFaceIndex& out);
std::array<ArxVector3, 3> facePositions(const GeometryData& geometry, const Face& face);
ArxVector3 faceNormalOr(const GeometryData& geometry, const Face& face, ArxVector3 fallback);
ArxAabb triangleBounds(const std::array<ArxVector3, 3>& vertices);
bool degenerateTriangle(const ArxVector3& a, const ArxVector3& b, const ArxVector3& c);
bool segmentTriangleIntersectionT(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a,
                                  const ArxVector3& b, const ArxVector3& c, double& out_t);
bool segmentIntersectsTriangle(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a, const ArxVector3& b,
                               const ArxVector3& c);

// --- Mutation ---

void reserveVertexCapacity(GeometryData& geometry, std::size_t capacity);
void reserveFaceCapacity(GeometryData& geometry, std::size_t capacity);
void setVertex(GeometryData& geometry, VertexIndex index, Vertex vertex) noexcept;
VertexIndex addVertex(GeometryData& geometry, Vertex vertex);
VertexIndex appendVertices(GeometryData& geometry, std::span<const Vertex> vertices);
void truncateVertices(GeometryData& geometry, std::size_t size) noexcept;
void setFace(GeometryData& geometry, FaceIndex index, Face face) noexcept;
FaceIndex addFace(GeometryData& geometry, Face face);
void removeFace(GeometryData& geometry, FaceIndex index) noexcept;
void replace(GeometryData& geometry, GeometryData&& replacement) noexcept;
void clear(GeometryData& geometry) noexcept;
VertexIndex addOrFindVertex(GeometryData& geometry, PositionIndex& index, const ArxVector3& position);

// --- Repair ---

void refreshFaceNormals(GeometryData& geometry) noexcept;
// Nonempty output is an order-preserving compaction map
std::size_t compactVertices(GeometryData& geometry, VertexIndexRemap* out_vertex_remap = nullptr);
Error weldVertices(GeometryData& geometry, const VertexWeldOptions& options = {}, GeometryRemap* out_remap = nullptr);
Error weldVerticesSegmented(GeometryData& geometry, const SegmentedVertexWeldInput& input,
                            const VertexWeldOptions& options = {}, GeometryRemap* out_remap = nullptr);
void mergeSurfaceSupportHits(std::vector<SurfaceSupportHit>& hits, float merge_distance = 1.0f);
void mergeSortedSurfaceSupportHits(std::vector<SurfaceSupportHit>& hits, float merge_distance = 1.0f);

// --- Transformation ---

void applyScale(GeometryData& geometry, float factor) noexcept;
void applyRotation(GeometryData& geometry, const ArxMat3& rotation) noexcept;
void applyTranslation(GeometryData& geometry, const ArxVector3& offset) noexcept;
void remapTextureReferences(GeometryData& geometry, std::span<const TextureIndex> remap) noexcept;

// --- Generation ---

SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry);
SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry, FacePredicate predicate,
                                             std::vector<FaceIndex>& face_scratch);
SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry, std::span<const FaceIndex> face_indices);

}  // namespace geometry
}  // namespace pistoris
