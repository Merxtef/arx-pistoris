// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/debug/level/diagnostics.hpp"

#include "../../writer.h"
#include "modules/geometry.h"
#include "modules/navigation.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb_level {
class Palette;
}

namespace pistoris::glb_level_debug {

using Builder = glb::Builder;
using Primitive = glb::Primitive;
using GlbVec2 = glb::Vec2;
using GlbVec3 = glb::Vec3;
using GlbVec4 = glb::Vec4;

struct DebugSegment {
  ArxVector3 start = {};
  ArxVector3 end = {};
};

GlbVec3 toVec3(const ArxVector3& value);
GlbVec4 debugColor(bool active, const GlbVec4& color);

int addMarkerMesh(Builder& builder, const std::string& name, int material, float size);
void addMarkerNode(Builder& builder, int parent, int mesh, const std::string& name, const ArxVector3& position);
int addDebugMeshChild(Builder& builder, int parent, const std::string& name, std::span<const GlbVec3> positions,
                      std::span<const std::uint32_t> indices, int material);
int addDebugMeshChildAtCenter(Builder& builder, int parent, const std::string& name, std::span<const GlbVec3> positions,
                              std::span<const std::uint32_t> indices, int material, const ArxVector3& parent_origin);
int addDebugGroupUnderMap(Builder& builder, int parent, const std::string& name, const ArxAabb& bounds,
                          ArxVector3& out_origin);
void appendSurfaceSupportMeshData(std::span<const geometry::SurfaceSupportTriangle> faces,
                                  std::vector<GlbVec3>& positions, std::vector<std::uint32_t>& indices);
void appendMarkerMeshData(const ArxVector3& position, float size, std::vector<GlbVec3>& positions,
                          std::vector<std::uint32_t>& indices);
void addSurfaceSupportMeshChild(Builder& builder, int parent, const std::string& name,
                                std::span<const geometry::SurfaceSupportTriangle> faces, int material);
void addGeometryContextMesh(Builder& builder, int parent, const GeometryData& geometry, glb_level::Palette& palette);
void appendSegmentQuad(const ArxVector3& a, const ArxVector3& b, float half_width, std::vector<GlbVec3>& positions,
                       std::vector<std::uint32_t>& indices);

}  // namespace pistoris::glb_level_debug
