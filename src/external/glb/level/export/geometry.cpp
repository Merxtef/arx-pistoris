// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/geometry.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/geometry_material.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/export/internal.h"
#include "external/glb/level/material.h"
#include "external/glb/level/objects.h"
#include "external/glb/utils/texture.h"
#include "external/glb/writer.h"
#include "external/material_name.h"
#include "level/validation.h"
#include "modules/lights.h"
#include "modules/textures.h"
#include "utils/encoded_image.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris::glb_level_export {
namespace {

using glb::Builder;
using glb::Primitive;
using glb_level::bottomCenter;
using glb_level::kRoomParentOffset;
using glb_level::LevelRenderKey;
using glb_level::roomNodeName;
using GlbVec2 = glb::Vec2;
using GlbVec3 = glb::Vec3;

inline GlbVec3 toVec3(const ArxVector3& value) { return {value.x, value.y, value.z}; }

ArxVector3 centerPositionsOnAabb(std::span<GlbVec3> positions) {
  if (positions.empty()) return {};
  GlbVec3 min = positions.front();
  GlbVec3 max = min;
  for (const GlbVec3& position : positions) {
    min.x = std::min(min.x, position.x);
    min.y = std::min(min.y, position.y);
    min.z = std::min(min.z, position.z);
    max.x = std::max(max.x, position.x);
    max.y = std::max(max.y, position.y);
    max.z = std::max(max.z, position.z);
  }
  const ArxVector3 center = {
      std::midpoint(min.x, max.x),
      std::midpoint(min.y, max.y),
      std::midpoint(min.z, max.z),
  };
  for (GlbVec3& position : positions) {
    position.x -= center.x;
    position.y -= center.y;
    position.z -= center.z;
  }
  return center;
}

LevelRenderKey renderKey(const Face& face, const TextureRegistry& registry) {
  const std::size_t texture_group = face.texture == kNoTexture
                                        ? std::numeric_limits<std::size_t>::max()
                                        : registry.group_by_texture[static_cast<std::size_t>(face.texture)];
  return {texture_group, face.flags, (face.flags & kFaceBitTrans) != 0 ? face.transval : 0.0f};
}

std::uint32_t floatKey(float value) noexcept { return value == 0.0f ? 0U : std::bit_cast<std::uint32_t>(value); }

struct RenderVertexKey {
  std::uint32_t vertex = 0;
  std::array<std::uint32_t, 8> attributes{};

  bool operator==(const RenderVertexKey&) const = default;
};

struct RenderVertexKeyHash {
  std::size_t operator()(const RenderVertexKey& key) const noexcept {
    std::size_t hash = std::hash<std::uint32_t>{}(key.vertex);
    for (std::uint32_t value : key.attributes)
      hash ^= std::hash<std::uint32_t>{}(value) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    return hash;
  }
};

}  // namespace

ArxReturnCode buildTextureRegistry(const LevelModules& level, TextureRegistry& out) {
  TextureRegistry registry;
  registry.referenced.assign(level.textures.textures.size(), 0);
  registry.group_by_texture.assign(level.textures.textures.size(), std::numeric_limits<std::size_t>::max());
  for (const Face& face : level.geometry.faces)
    if (face.texture != kNoTexture) registry.referenced[static_cast<std::size_t>(face.texture)] = 1;

  constexpr std::array<std::string_view, 4> kReserved = {"no_tex", "arx_portal", "arx_zone", "arx_nav_surface"};
  std::vector<std::string> stems =
      material_names::fallbackStems(level.textures.textures, registry.referenced, kReserved, "Level -> GLB");
  for (std::size_t i = 0; i < level.textures.textures.size(); ++i) {
    if (!registry.referenced[i]) continue;
    registry.group_by_texture[i] = registry.groups.size();
    registry.groups.push_back({std::move(stems[i]), i, {}});
  }
  out = std::move(registry);
  return ARX_OK;
}

ArxReturnCode registerTextures(const LevelModules& level, TextureRegistry& registry, Builder& builder) {
  std::vector<textures::ImagePreparationRequest> requests;
  requests.reserve(registry.groups.size());
  for (const TextureRegistry::Group& group : registry.groups) {
    const Texture& texture = level.textures.textures[group.source_texture];
    if (texture.encoded_image.empty()) continue;
    requests.push_back(
        {static_cast<TextureIndex>(group.source_texture),
         {.accepted_formats = image::formatFlag(image::Format::kPng) | image::formatFlag(image::Format::kJpeg),
          .fallback_format = image::Format::kPng,
          .require_power_of_two = false}});
  }
  std::vector<textures::PreparedImage> prepared;
  ArxReturnCode rc = level_validation::textureError(textures::prepareImages(level.textures, requests, prepared));
  if (rc != ARX_OK) return rc;

  std::size_t prepared_index = 0;
  for (TextureRegistry::Group& group : registry.groups) {
    const Texture& texture = level.textures.textures[group.source_texture];
    const textures::PreparedImage* image = texture.encoded_image.empty() ? nullptr : &prepared[prepared_index++];
    glb::exportTexture(builder, texture, image, group.exported);
    if (group.exported.assumed_png)
      log(ARX_LOG_WARN, "Level -> GLB: external texture format unknown; assuming PNG: {}.png", texture.path);
  }
  return ARX_OK;
}

void logUnusedTextures(const LevelModules& level, const TextureRegistry& registry) {
  std::set<std::string> warned;
  for (std::size_t i = 0; i < registry.referenced.size(); ++i) {
    if (!registry.referenced[i] && warned.insert(level.textures.textures[i].path).second)
      log(ARX_LOG_WARN, "Level -> GLB: unused texture omitted: {}", level.textures.textures[i].path);
  }
}

ArxReturnCode exportRoomGeometry(const LevelModules& level, const ArxAabb& referenced_bounds,
                                 const TextureRegistry& texture_registry, const RoomProjection& room_projection,
                                 Builder& builder, std::uint64_t& nonstandard_transval) {
  std::map<LevelRenderKey, int> material_map;
  std::set<std::size_t> unknown_alpha_groups;
  for (const Face& face : level.geometry.faces) {
    const bool transparent = (face.flags & kFaceBitTrans) != 0;
    if (transparent && !glb::isStandardGeometryTransparency(face.flags, face.transval)) ++nonstandard_transval;
    LevelRenderKey key = renderKey(face, texture_registry);
    if (material_map.contains(key)) continue;
    int texture = -1;
    glb::TextureAlpha texture_alpha = glb::TextureAlpha::kAbsent;
    std::string stem = "no_tex";
    if (key.texture_group != std::numeric_limits<std::size_t>::max()) {
      const TextureRegistry::Group& group = texture_registry.groups[key.texture_group];
      texture = group.exported.index;
      stem = group.stem;
      texture_alpha = group.exported.alpha;
    }
    glb::ExportedGeometryMaterial material = glb::exportGeometryMaterial(stem, key.flags, key.transval, texture_alpha);
    if (material.unknown_alpha) unknown_alpha_groups.insert(key.texture_group);
    material_map.emplace(
        key, builder.addMaterial(std::move(material.name), texture, key.flags, material.alpha, material.alpha_cutout));
  }
  if (!unknown_alpha_groups.empty()) {
    log(ARX_LOG_WARN,
        "Level -> GLB: alpha presence unknown for {} external texture(s); non-TRANS materials exported "
        "as OPAQUE",
        unknown_alpha_groups.size());
  }

  ArxVector3 room_root = bottomCenter(referenced_bounds);
  room_root.y += kRoomParentOffset;
  int room_parent = builder.addNode("rooms_parent");
  builder.setNodeTranslation(room_parent, toVec3(room_root));
  builder.addRoot(room_parent);

  std::vector<std::size_t> room_face_offsets(level.rooms.definitions.size() + 1U, 0);
  for (std::uint32_t room : level.rooms.face_rooms) ++room_face_offsets[static_cast<std::size_t>(room) + 1U];
  std::partial_sum(room_face_offsets.begin(), room_face_offsets.end(), room_face_offsets.begin());
  std::vector<std::size_t> room_faces(room_face_offsets.back());
  std::vector<std::size_t> room_face_cursors(room_face_offsets.begin(), room_face_offsets.end() - 1);
  for (std::size_t face = 0; face < level.rooms.face_rooms.size(); ++face) {
    const std::size_t room = level.rooms.face_rooms[face];
    room_faces[room_face_cursors[room]++] = face;
  }

  for (std::size_t room_index = 0; room_index < level.rooms.definitions.size(); ++room_index) {
    if (!room_projection.has_faces[room_index]) continue;
    std::vector<GlbVec3> positions;
    std::vector<GlbVec3> normals;
    std::vector<GlbVec3> colors;
    std::vector<GlbVec2> uv;
    std::unordered_map<RenderVertexKey, std::uint32_t, RenderVertexKeyHash> render_vertex_by_value;
    std::map<LevelRenderKey, std::vector<std::uint32_t>> indices_by_key;

    const std::span<const std::size_t> faces(room_faces.data() + room_face_offsets[room_index],
                                             room_face_offsets[room_index + 1U] - room_face_offsets[room_index]);
    const std::size_t corner_capacity = faces.size() * 3U;
    positions.reserve(corner_capacity);
    normals.reserve(corner_capacity);
    colors.reserve(corner_capacity);
    uv.reserve(corner_capacity);
    render_vertex_by_value.reserve(corner_capacity);
    for (std::size_t face_index : faces) {
      const Face& face = level.geometry.faces[face_index];
      LevelRenderKey material_key = renderKey(face, texture_registry);
      std::vector<std::uint32_t>& indices = indices_by_key[material_key];
      for (std::size_t corner_index = 0; corner_index < face.corners.size(); ++corner_index) {
        const Corner& corner = face.corners[corner_index];
        const ArxVector3& normal = corner.normal;
        const ArxColor3 color = lights::cornerColorOr(
            level.lighting, static_cast<FaceIndex>(face_index), corner_index, lights::kDefaultCornerColor);
        const RenderVertexKey key = {corner.vertex,
                                     {floatKey(normal.x),
                                      floatKey(normal.y),
                                      floatKey(normal.z),
                                      floatKey(corner.u),
                                      floatKey(corner.v),
                                      floatKey(color.r),
                                      floatKey(color.g),
                                      floatKey(color.b)}};
        auto [entry, inserted] = render_vertex_by_value.emplace(key, 0);
        if (inserted) {
          entry->second = static_cast<std::uint32_t>(positions.size());
          const ArxVector3& position = level.geometry.vertices[corner.vertex].position;
          positions.push_back(toVec3(position));
          normals.push_back(toVec3(normal));
          colors.push_back({color.r, color.g, color.b});
          uv.push_back({corner.u, corner.v});
        }
        indices.push_back(entry->second);
      }
    }
    const ArxVector3 room_center = centerPositionsOnAabb(positions);
    int position_accessor = builder.addVec3Accessor(positions);
    int normal_accessor = builder.addVec3Accessor(normals);
    int color_accessor =
        builder.addAccessor(std::span<const GlbVec3>(colors), cgltf_component_type_r_32f, cgltf_type_vec3);
    int uv_accessor = builder.addAccessor(std::span<const GlbVec2>(uv), cgltf_component_type_r_32f, cgltf_type_vec2);
    std::vector<Primitive> primitives;
    primitives.reserve(indices_by_key.size());
    for (const auto& [key, indices] : indices_by_key) {
      Primitive primitive;
      primitive.indices =
          builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
      primitive.material = material_map.at(key);
      primitive.attributes = {
          {"POSITION", position_accessor},
          {"NORMAL", normal_accessor},
          {"TEXCOORD_0", uv_accessor},
          {"COLOR_0", color_accessor},
      };
      primitives.push_back(std::move(primitive));
    }
    std::string name = roomNodeName(level.rooms.definitions[room_index]);
    int mesh = builder.addMesh(name, std::move(primitives));
    int node = builder.addNode(std::move(name), mesh);
    builder.setNodeTranslation(node,
                               {room_center.x - room_root.x, room_center.y - room_root.y, room_center.z - room_root.z});
    builder.addChild(room_parent, node);
  }
  return ARX_OK;
}

}  // namespace pistoris::glb_level_export
