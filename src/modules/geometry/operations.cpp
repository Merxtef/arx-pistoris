// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::geometry {

VertexIndex addVertex(GeometryData& geometry, const ArxVector3& position) {
  if (geometry.vertices.size() >= static_cast<std::size_t>(kInvalidVertexIndex)) return kInvalidVertexIndex;
  VertexIndex index = static_cast<VertexIndex>(geometry.vertices.size());
  geometry.vertices.push_back({position});
  return index;
}

void addVertices(GeometryData& geometry, std::span<const ArxVector3> positions, std::vector<VertexIndex>* out_indices) {
  if (out_indices) out_indices->reserve(out_indices->size() + positions.size());
  for (const ArxVector3& position : positions) {
    VertexIndex index = addVertex(geometry, position);
    if (out_indices) out_indices->push_back(index);
    if (index == kInvalidVertexIndex) break;
  }
}

FaceIndex addFace(GeometryData& geometry, Face face) {
  if (geometry.faces.size() >= static_cast<std::size_t>(kInvalidFaceIndex)) return kInvalidFaceIndex;
  FaceIndex index = static_cast<FaceIndex>(geometry.faces.size());
  geometry.faces.push_back(face);
  return index;
}

TextureIndex addTexture(GeometryData& geometry, Texture texture) {
  if (geometry.textures.size() >= static_cast<std::size_t>(kNoTexture)) return kNoTexture;
  const TextureIndex index = static_cast<TextureIndex>(geometry.textures.size());
  geometry.textures.push_back(std::move(texture));
  return index;
}

std::size_t compactVertices(GeometryData& geometry, VertexIndexRemap* out_vertex_remap) {
  if (out_vertex_remap) out_vertex_remap->clear();

  const std::size_t original_size = geometry.vertices.size();
  VertexIndexRemap remap(original_size, kInvalidVertexIndex);
  std::vector<Vertex> compact;
  compact.reserve(original_size);

  for (Face& face : geometry.faces) {
    for (Corner& corner : face.corners) {
      VertexIndex& mapped = remap[corner.vertex];
      if (mapped == kInvalidVertexIndex) {
        mapped = static_cast<VertexIndex>(compact.size());
        compact.push_back(geometry.vertices[corner.vertex]);
      }
      corner.vertex = mapped;
    }
  }

  geometry.vertices = std::move(compact);
  if (out_vertex_remap) {
    const bool identity = remap.size() == original_size &&
                          std::all_of(remap.begin(), remap.end(), [index = VertexIndex{0}](VertexIndex mapped) mutable {
                            return mapped == index++;
                          });
    if (!identity) *out_vertex_remap = std::move(remap);
  }
  return original_size - geometry.vertices.size();
}

std::size_t compactTextures(GeometryData& geometry) {
  const std::size_t original_size = geometry.textures.size();
  std::vector<TextureIndex> remap(original_size, kNoTexture);

  for (const Face& face : geometry.faces) {
    if (face.texture != kNoTexture) remap[face.texture] = face.texture;
  }

  std::size_t next = 0;
  for (std::size_t index = 0; index < original_size; ++index) {
    if (remap[index] == kNoTexture) continue;
    if (index != next) geometry.textures[next] = std::move(geometry.textures[index]);
    remap[index] = static_cast<TextureIndex>(next++);
  }
  geometry.textures.resize(next);

  for (Face& face : geometry.faces) {
    if (face.texture != kNoTexture) face.texture = remap[face.texture];
  }

  return original_size - geometry.textures.size();
}

}  // namespace pistoris::geometry
