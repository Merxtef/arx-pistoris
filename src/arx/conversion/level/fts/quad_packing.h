// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pistoris::arx_level_conversion::fts_bake {

struct Corner {
  ArxVector3 position = {};
  ArxVector3 normal = {};
  float u = 0.0f;
  float v = 0.0f;
  ArxColor3 color = {};
  std::uint8_t source_edge_mask = 0;
};

struct Triangle {
  std::array<Corner, 3> corners = {};
  std::array<VertexIndex, 3> source_vertices = {};
  ArxVector3 face_normal = {};
  TextureIndex texture = kNoTexture;
  FaceType flags = 0;
  float transval = 0.0f;
  std::int16_t room = 0;
  std::size_t source_face = 0;
};

enum class PolygonKind : std::uint8_t {
  kTriangle,
  kClippedFragmentQuad,
  kCrossFaceQuad,
};

struct Polygon {
  std::array<Corner, 4> corners = {};
  ArxVector3 norm = {};
  ArxVector3 norm2 = {};
  TextureIndex texture = kNoTexture;
  FaceType flags = 0;
  float transval = 0.0f;
  std::int16_t room = 0;
  PolygonKind kind = PolygonKind::kTriangle;
};

std::vector<Polygon> packCellPolygons(std::span<const Triangle> triangles, bool reconstruct_quads);

}  // namespace pistoris::arx_level_conversion::fts_bake
