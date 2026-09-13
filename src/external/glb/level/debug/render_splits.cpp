// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/level.hpp"

#include "../coordinates.h"
#include "../normals.h"
#include "../palette.h"
#include "../topology.h"
#include "api/status_boundary.h"
#include "common.h"
#include "external/glb/container.h"
#include "level/debug/access.h"
#include "modules/geometry.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris::glb_level_debug {
namespace {

struct LevelVertexDebug {
  std::uint32_t first_material = 0;
  FaceType first_flags = 0;
  float first_transval = 0.0f;
  bool has_corner = false;
  bool position_split = false;
  bool uv_split = false;
  bool texture_boundary = false;
  bool flags_boundary = false;
  bool transval_boundary = false;
};

std::uint32_t debugMaterialId(TextureIndex texture) { return texture == kNoTexture ? 0U : texture + 1U; }

}  // namespace

}  // namespace pistoris::glb_level_debug

namespace pistoris::level_debug {

ArxReturnCode exportRenderSplitsDebugGlb(const Level& level, std::vector<std::uint8_t>& out, float normal_weld_degrees,
                                         const Level::GlbExportOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::vector<std::uint8_t> tmp;
    ArxReturnCode rc = level.validateMesh();
    if (rc != ARX_OK) return rc;
    const LevelModules& modules = LevelDebugAccess::modules(level);
    const std::vector<Vertex>& vertices = modules.geometry.vertices;
    const std::vector<Face>& faces = modules.geometry.faces;
    glb_level::LevelNormalAnalysis normal_analysis;
    rc = glb_level::analyzeLevelNormals(vertices, faces, normal_weld_degrees, normal_analysis);
    if (rc != ARX_OK) return rc;

    std::vector<glb_level_debug::LevelVertexDebug> debug(vertices.size());
    std::unordered_map<std::uint64_t, glb_level_debug::GlbVec2> uv_by_vertex_material;
    uv_by_vertex_material.reserve(faces.size() * 3U);
    std::vector<std::uint32_t> indices;
    indices.reserve(faces.size() * 3);

    for (const Face& face : faces) {
      for (const auto& corner : face.corners) {
        if (corner.vertex >= vertices.size()) return ARX_GLB_BAD_FORMAT;
        glb_level_debug::LevelVertexDebug& state = debug[corner.vertex];
        glb_level_debug::GlbVec2 uv{corner.u, corner.v};
        const std::uint32_t material = glb_level_debug::debugMaterialId(face.texture);
        if (!state.has_corner) {
          state.first_material = material;
          state.first_flags = face.flags;
          state.first_transval = face.transval;
          state.has_corner = true;
        } else {
          if (state.first_material != material) state.texture_boundary = true;
          if (state.first_flags != face.flags) state.flags_boundary = true;
          if (std::abs(state.first_transval - face.transval) > glb_level::kLevelGlbEpsilon)
            state.transval_boundary = true;
        }

        const std::uint64_t uv_key = (static_cast<std::uint64_t>(corner.vertex) << 32U) | material;
        const auto [material_uv, inserted] = uv_by_vertex_material.emplace(uv_key, uv);
        if (!inserted && !glb_level::sameUv(material_uv->second, uv)) {
          state.uv_split = true;
        }

        indices.push_back(corner.vertex);
      }
    }

    geometry::PositionIndex vertices_by_position(glb_level::kLevelGlbEpsilon);
    std::vector<std::uint32_t> candidates;
    for (std::uint32_t vertex_idx = 0; vertex_idx < vertices.size(); ++vertex_idx) {
      const ArxVector3& position = vertices[vertex_idx].position;
      vertices_by_position.findCandidates(candidates, position);
      for (std::uint32_t other_idx : candidates) {
        debug[vertex_idx].position_split = true;
        debug[other_idx].position_split = true;
      }
      (void)vertices_by_position.tryAdd(vertex_idx, position);
    }

    constexpr glb_level_debug::GlbVec4 kPositionColor{1.0f, 1.0f, 0.0f, 1.0f};
    constexpr glb_level_debug::GlbVec4 kNormalsColor{1.0f, 0.0f, 0.0f, 1.0f};
    constexpr glb_level_debug::GlbVec4 kUvColor{0.0f, 1.0f, 0.0f, 1.0f};
    constexpr glb_level_debug::GlbVec4 kMaterialColor{0.0f, 0.0f, 1.0f, 1.0f};
    constexpr glb_level_debug::GlbVec4 kFlagsColor{1.0f, 0.0f, 1.0f, 1.0f};
    constexpr glb_level_debug::GlbVec4 kTransvalColor{0.0f, 1.0f, 1.0f, 1.0f};

    std::vector<glb_level_debug::GlbVec3> positions;
    std::vector<glb_level_debug::GlbVec3> normals;
    std::vector<glb_level_debug::GlbVec4> position_split;
    std::vector<glb_level_debug::GlbVec4> normals_split;
    std::vector<glb_level_debug::GlbVec4> uv_split;
    std::vector<glb_level_debug::GlbVec4> texture_boundary;
    std::vector<glb_level_debug::GlbVec4> flags_boundary;
    std::vector<glb_level_debug::GlbVec4> transval_boundary;
    positions.reserve(vertices.size());
    normals.reserve(vertices.size());
    position_split.reserve(vertices.size());
    normals_split.reserve(vertices.size());
    uv_split.reserve(vertices.size());
    texture_boundary.reserve(vertices.size());
    flags_boundary.reserve(vertices.size());
    transval_boundary.reserve(vertices.size());

    for (std::size_t i = 0; i < vertices.size(); ++i) {
      positions.push_back(glb_level_debug::toVec3(vertices[i].position));
      normals.push_back(glb_level_debug::toVec3(normal_analysis.vertices[i].representative));
      position_split.push_back(glb_level_debug::debugColor(debug[i].position_split, kPositionColor));
      normals_split.push_back(
          glb_level_debug::debugColor(normal_analysis.vertices[i].cluster_count > 1, kNormalsColor));
      uv_split.push_back(glb_level_debug::debugColor(debug[i].uv_split, kUvColor));
      texture_boundary.push_back(glb_level_debug::debugColor(debug[i].texture_boundary, kMaterialColor));
      flags_boundary.push_back(glb_level_debug::debugColor(debug[i].flags_boundary, kFlagsColor));
      transval_boundary.push_back(glb_level_debug::debugColor(debug[i].transval_boundary, kTransvalColor));
    }

    glb_level_debug::Builder builder;
    glb_level::Palette palette(builder);
    rc = glb_level::configureGlbExportCoordinates(builder, options);
    if (rc != ARX_OK) return rc;
    int material = palette.material(glb_level::PaletteItem::kDebugGeometry);
    glb_level_debug::Primitive primitive;
    primitive.material = material;
    primitive.indices =
        builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
    primitive.attributes = {
        {"POSITION", builder.addVec3Accessor(positions)},
        {"NORMAL", builder.addVec3Accessor(normals)},
        {"_POSITION_SPLIT",
         builder.addAccessor(
             std::span<const glb_level_debug::GlbVec4>(position_split), cgltf_component_type_r_32f, cgltf_type_vec4)},
        {"_NORMALS_SPLIT",
         builder.addAccessor(
             std::span<const glb_level_debug::GlbVec4>(normals_split), cgltf_component_type_r_32f, cgltf_type_vec4)},
        {"_UV_SPLIT",
         builder.addAccessor(
             std::span<const glb_level_debug::GlbVec4>(uv_split), cgltf_component_type_r_32f, cgltf_type_vec4)},
        {"_TEXTURE_BOUNDARY",
         builder.addAccessor(
             std::span<const glb_level_debug::GlbVec4>(texture_boundary), cgltf_component_type_r_32f, cgltf_type_vec4)},
        {"_FLAGS_BOUNDARY",
         builder.addAccessor(
             std::span<const glb_level_debug::GlbVec4>(flags_boundary), cgltf_component_type_r_32f, cgltf_type_vec4)},
        {"_TRANSVAL_BOUNDARY",
         builder.addAccessor(std::span<const glb_level_debug::GlbVec4>(transval_boundary),
                             cgltf_component_type_r_32f,
                             cgltf_type_vec4)},
    };
    int mesh = builder.addMesh("level_render_splits_debug", {std::move(primitive)});
    builder.addRoot(builder.addNode("level_render_splits_debug", mesh));
    rc = builder.write(tmp);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

}  // namespace pistoris::level_debug
