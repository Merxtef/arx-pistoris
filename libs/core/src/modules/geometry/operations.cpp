// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/geometry.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::geometry {

void reserveVertexCapacity(GeometryData& geometry, std::size_t capacity) { geometry.vertices.reserve(capacity); }

void reserveFaceCapacity(GeometryData& geometry, std::size_t capacity) { geometry.faces.reserve(capacity); }

std::size_t vertexCapacityForAppend(const GeometryData& geometry, std::size_t count, std::size_t limit) noexcept {
  const std::size_t size = geometry.vertices.size();
  const std::size_t required = size + count;
  const std::size_t grown = size + std::max(size / 2U, std::size_t{1});
  return std::min(limit, std::max(required, grown));
}

void setVertex(GeometryData& geometry, VertexIndex index, Vertex vertex) noexcept {
  assert(static_cast<std::size_t>(index) < geometry.vertices.size());
  geometry.vertices[index] = vertex;
}

VertexIndex addVertex(GeometryData& geometry, Vertex vertex) {
  assert(geometry.vertices.size() < static_cast<std::size_t>(kInvalidVertexIndex));
  const VertexIndex index = static_cast<VertexIndex>(geometry.vertices.size());
  geometry.vertices.push_back(vertex);
  return index;
}

VertexIndex appendVertices(GeometryData& geometry, std::span<const Vertex> vertices) {
  assert(!vertices.empty());
  const VertexIndex first = static_cast<VertexIndex>(geometry.vertices.size());
  geometry.vertices.insert(geometry.vertices.end(), vertices.begin(), vertices.end());
  return first;
}

void truncateVertices(GeometryData& geometry, std::size_t size) noexcept {
  while (geometry.vertices.size() > size) geometry.vertices.pop_back();
}

void setFace(GeometryData& geometry, FaceIndex index, Face face) noexcept {
  assert(static_cast<std::size_t>(index) < geometry.faces.size());
  geometry.faces[index] = face;
}

FaceIndex addFace(GeometryData& geometry, Face face) {
  assert(geometry.faces.size() < static_cast<std::size_t>(kInvalidFaceIndex));
  const FaceIndex index = static_cast<FaceIndex>(geometry.faces.size());
  geometry.faces.push_back(face);
  return index;
}

void removeFace(GeometryData& geometry, FaceIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < geometry.faces.size());
  geometry.faces.erase(geometry.faces.begin() + static_cast<std::ptrdiff_t>(index));
}

void replace(GeometryData& geometry, GeometryData&& replacement) noexcept { geometry = std::move(replacement); }

void clear(GeometryData& geometry) noexcept {
  geometry.vertices.clear();
  geometry.faces.clear();
}

std::size_t compactVertices(GeometryData& geometry, VertexIndexRemap* out_vertex_remap) {
  if (out_vertex_remap) out_vertex_remap->clear();

  const std::size_t original_size = geometry.vertices.size();
  VertexIndexRemap remap(original_size, kInvalidVertexIndex);

  for (const Face& face : geometry.faces) {
    for (const Corner& corner : face.corners) {
      remap[corner.vertex] = 0;
    }
  }

  VertexIndex next = 0;
  for (std::size_t old = 0; old < original_size; ++old) {
    if (remap[old] == kInvalidVertexIndex) continue;
    remap[old] = next;
    if (next != old) geometry.vertices[next] = geometry.vertices[old];
    ++next;
  }

  for (Face& face : geometry.faces)
    for (Corner& corner : face.corners) corner.vertex = remap[corner.vertex];
  while (geometry.vertices.size() > next) geometry.vertices.pop_back();

  const std::size_t removed = original_size - geometry.vertices.size();
  if (out_vertex_remap && removed != 0) *out_vertex_remap = std::move(remap);
  return removed;
}

Error collectTextureUsage(const GeometryData& geometry, std::size_t texture_count, std::vector<std::uint8_t>& out) {
  std::vector<std::uint8_t> used;
  try {
    used.assign(texture_count, 0);
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
  for (const Face& face : geometry.faces) {
    if (face.texture == kNoTexture) continue;
    if (static_cast<std::size_t>(face.texture) >= texture_count) return Error::kBadFaceTexture;
    used[face.texture] = 1;
  }
  out = std::move(used);
  return Error::kNone;
}

void remapTextureReferences(GeometryData& geometry, std::span<const TextureIndex> remap) noexcept {
  for (Face& face : geometry.faces)
    if (face.texture != kNoTexture) face.texture = remap[face.texture];
}

}  // namespace pistoris::geometry
