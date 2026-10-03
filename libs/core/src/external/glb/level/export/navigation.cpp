// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/navigation.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/level.hpp"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/anchor_metadata.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/export/internal.h"
#include "external/glb/level/names.h"
#include "external/glb/level/objects.h"
#include "external/glb/level/palette.h"
#include "external/glb/writer.h"
#include "modules/geometry.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb_level_export {
namespace {
using glb::Builder;
using glb::Primitive;
using glb_level::anchorNodeName;
using glb_level::bottomCenter;
using glb_level::kAnchorParentOffset;
using GlbVec3 = glb::Vec3;

inline GlbVec3 toVec3(const ArxVector3& value) { return {value.x, value.y, value.z}; }
}  // namespace

void exportNavigation(const LevelModules& level, const ArxAabb& referenced_bounds,
                      const Level::GlbExportOptions& options, glb_level::Palette& palette, Builder& builder) {
  if (!level.navigation.anchors.empty()) {
    ArxVector3 anchor_root = bottomCenter(referenced_bounds);
    anchor_root.y += kAnchorParentOffset;
    int anchor_parent = builder.addNode("anchors_parent");
    builder.setNodeTranslation(anchor_parent, toVec3(anchor_root));
    builder.addRoot(anchor_parent);
    std::size_t next_connection = 0;
    for (std::size_t anchor_index = 0; anchor_index < level.navigation.anchors.size(); ++anchor_index) {
      const Anchor& anchor = level.navigation.anchors[anchor_index];
      const AnchorIndex anchor_id = static_cast<AnchorIndex>(anchor_index);
      const std::size_t connection_begin = next_connection;
      while (next_connection < level.navigation.connections.size() &&
             level.navigation.connections[next_connection].first == anchor_id)
        ++next_connection;
      int node = builder.addNode(anchorNodeName(anchor, anchor_index, options));
      builder.setNodeExtrasJson(
          node,
          glb_level::anchorMetadataJson(
              anchor_id,
              std::span(level.navigation.connections).subspan(connection_begin, next_connection - connection_begin)));
      builder.setNodeTranslation(
          node,
          {anchor.position.x - anchor_root.x, anchor.position.y - anchor_root.y, anchor.position.z - anchor_root.z});
      builder.addChild(anchor_parent, node);
    }
    assert(next_connection == level.navigation.connections.size());
  }

  const auto& surface = level.navigation.surface;
  if (surface.has_value()) {
    std::vector<GlbVec3> positions;
    positions.reserve(surface->vertices.size());
    for (const Vertex& vertex : surface->vertices) positions.push_back(toVec3(vertex.position));

    std::vector<std::uint32_t> indices;
    indices.reserve(surface->triangles.size() * 3U);
    for (const NavSurfaceTriangle& triangle : surface->triangles) {
      indices.push_back(triangle.vertices[0]);
      indices.push_back(triangle.vertices[1]);
      indices.push_back(triangle.vertices[2]);
    }

    Primitive primitive;
    primitive.indices =
        builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
    primitive.material = palette.material(glb_level::PaletteItem::kNavigationSurface);
    primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(positions));
    int mesh = builder.addMesh(glb_level::kExportNavSurfaceRootName, {std::move(primitive)});
    int node = builder.addNode(glb_level::kExportNavSurfaceRootName, mesh);
    builder.addRoot(node);
  }
}

}  // namespace pistoris::glb_level_export
