// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
};

struct Texture {
  std::string path;
  std::vector<std::uint8_t> encoded_image;

  Texture() = default;
  Texture(std::string value) : path(std::move(value)) {}
  Texture(const char* value) : path(value) {}
  Texture(std::string_view value) : path(value) {}

  operator std::string&() { return path; }
  operator const std::string&() const { return path; }

  bool operator==(const Texture&) const = default;
  bool operator==(std::string_view value) const { return path == value; }
};

inline bool operator==(std::string_view value, const Texture& texture) { return texture == value; }

struct GeometryData {
  std::vector<Vertex> vertices;
  std::vector<Face> faces;
  std::vector<Texture> textures;
};

struct GeometryDerived {
  ArxAabb bounds = {};
  ArxAabb referenced_bounds = {};
};

namespace geometry {

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kNoGeometry,
  kTooManyVertices,
  kTooManyFaces,
  kTooManyTextures,
  kBadVertex,
  kBadTexture,
  kBadTextureImage,
  kOutOfMemory,
  kBadFaceTexture,
  kBadFaceVertex,
  kBadFaceType,
  kBadFaceTransval,
  kBadFaceNormal,
  kBadFaceUv,
  kDegenerateFace,
  kBadVertexWeldSegment,
  kOverlappingVertexWeldSegments,
};

enum class ImageFormat : std::uint8_t {
  kUnknown = 0,
  kJpeg,
  kPng,
  kBmp,
  kTga,
};

enum class ImageError : std::uint8_t {
  kNone,
  kMalformed,
  kOutOfMemory,
};

struct ImageInfo {
  ImageFormat format = ImageFormat::kUnknown;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint8_t components = 0;
};

ImageFormat detectImageFormat(std::span<const std::uint8_t> encoded) noexcept;
ImageError inspectImage(std::span<const std::uint8_t> encoded, ImageInfo* out = nullptr) noexcept;
ImageError transcodeImageToPng(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out,
                               ImageInfo* out_info = nullptr);
ImageError normalizeImageToPowerOfTwo(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out,
                                      ImageInfo* out_info = nullptr, bool* out_rescaled = nullptr);
bool imageHasAlpha(const ImageInfo& info) noexcept;
std::string_view imageExtension(ImageFormat format) noexcept;

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

  void add(std::uint32_t index, const ArxVector3& position);
  [[nodiscard]] std::vector<std::uint32_t> candidates(const ArxVector3& position) const;
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

  [[nodiscard]] const IndexedTriangle& triangle(std::uint32_t index) const;
  [[nodiscard]] std::vector<std::uint32_t> candidatesForXz(float x, float z) const;
  [[nodiscard]] std::vector<std::uint32_t> candidatesForAabb(const ArxAabb& bounds) const;
  [[nodiscard]] std::vector<std::uint32_t> candidatesForSegment(const ArxVector3& a, const ArxVector3& b) const;

 private:
  std::vector<IndexedTriangle> triangles_;
  std::unordered_map<std::uint16_t, std::vector<std::uint32_t>> buckets_;
  std::vector<std::uint32_t> large_triangle_indices_;
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

class SurfaceSupportIndex {
 public:
  explicit SurfaceSupportIndex(const std::vector<SurfaceSupportTriangle>& triangles);

  [[nodiscard]] bool empty() const;
  [[nodiscard]] bool hasBounds() const;
  [[nodiscard]] const ArxAabb& bounds() const;
  [[nodiscard]] std::vector<SurfaceSupportTriangle> triangles() const;
  [[nodiscard]] std::vector<SurfaceSupportHit> hitsAt(float x, float z) const;
  [[nodiscard]] std::vector<SurfaceSupportHit> downwardHitsAt(float x, float z, float origin_y) const;
  [[nodiscard]] std::optional<SurfaceSupportHit> closestDownwardHit(float x, float z, float origin_y) const;
  [[nodiscard]] std::optional<SurfaceSupportHit> closestHit(float x, float z, float reference_y, float max_delta) const;

 private:
  struct SupportFace {
    FaceIndex face = kInvalidFaceIndex;
    std::array<ArxVector3, 3> vertices = {};
    ArxVector3 normal = {};
  };

  std::vector<SupportFace> faces_;
  TriangleIndex index_;
  ArxAabb bounds_ = {};
  bool has_bounds_ = false;
};

VertexIndex addVertex(GeometryData& geometry, const ArxVector3& position);
void addVertices(GeometryData& geometry, std::span<const ArxVector3> positions,
                 std::vector<VertexIndex>* out_indices = nullptr);
FaceIndex addFace(GeometryData& geometry, Face face);
TextureIndex addTexture(GeometryData& geometry, Texture texture);
std::size_t compactVertices(GeometryData& geometry, VertexIndexRemap* out_vertex_remap = nullptr);
std::size_t compactTextures(GeometryData& geometry);
struct GeometryRemap {
  VertexIndexRemap vertices;
  FaceIndexRemap faces;
};

Error weldVertices(GeometryData& geometry, const VertexWeldOptions& options = {}, GeometryRemap* out_remap = nullptr);
Error weldVerticesSegmented(GeometryData& geometry, const SegmentedVertexWeldInput& input,
                            const VertexWeldOptions& options = {}, GeometryRemap* out_remap = nullptr);

std::array<ArxVector3, 3> facePositions(const GeometryData& geometry, const Face& face);
ArxVector3 faceNormalOr(const GeometryData& geometry, const Face& face, ArxVector3 fallback);
ArxAabb triangleBounds(const std::array<ArxVector3, 3>& vertices);
bool degenerateTriangle(const ArxVector3& a, const ArxVector3& b, const ArxVector3& c);

VertexIndex addOrFindVertex(GeometryData& geometry, PositionIndex& index, const ArxVector3& position);

bool segmentTriangleIntersectionT(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a,
                                  const ArxVector3& b, const ArxVector3& c, double& out_t);
bool segmentIntersectsTriangle(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a, const ArxVector3& b,
                               const ArxVector3& c);

SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry);
SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry, FacePredicate predicate);
SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry, std::span<const FaceIndex> face_indices);
void mergeSurfaceSupportHits(std::vector<SurfaceSupportHit>& hits, float merge_distance = 1.0f);

Error validateVertex(const Vertex& vertex) noexcept;
Error validateVertices(std::span<const Vertex> vertices, ArxAabb* out_bounds = nullptr) noexcept;
Error validateTexture(const Texture& texture) noexcept;
Error validateTextures(std::span<const Texture> textures) noexcept;
Error validateFaceReferences(const Face& face, std::size_t vertex_count, std::size_t texture_count) noexcept;
Error validateFaces(std::span<const Face> faces, std::span<const Vertex> vertices, std::size_t texture_count,
                    ArxAabb* out_referenced_bounds = nullptr) noexcept;
Error validate(const GeometryData& geometry, GeometryDerived* out = nullptr);

}  // namespace geometry
}  // namespace pistoris
