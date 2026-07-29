// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "../writer.h"
#include "api.h"
#include "coordinates.h"
#include "entities.h"
#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/utils/image.h"
#include "external/mat_name.h"
#include "fogs.h"
#include "level/data.h"
#include "level/level.h"
#include "level/validation.h"
#include "lighting.h"
#include "material.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "names.h"
#include "objects.h"
#include "palette.h"
#include "paths.h"
#include "player_spawn.h"
#include "utils/log.h"
#include "zones.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

using glb::Builder;
using glb::Primitive;
using glb_level::anchorNodeName;
using glb_level::bottomCenter;
using glb_level::hasEffectFields;
using glb_level::hasSettingsFields;
using glb_level::kAnchorParentOffset;
using glb_level::kLightParentOffset;
using glb_level::kPortalParentOffset;
using glb_level::kRoomParentOffset;
using glb_level::LevelRenderKey;
using glb_level::lightEffectHelperName;
using glb_level::lightFlagHelperName;
using glb_level::lightNodeName;
using glb_level::lightSettingsHelperName;
using glb_level::materialName;
using glb_level::normalizeTexturePath;
using glb_level::portalNodeName;
using glb_level::roomNodeName;
using glb_level::sanitizeMaterialStem;
using GlbVec2 = glb::Vec2;
using GlbVec3 = glb::Vec3;

struct TextureRegistry {
  enum class Alpha : std::uint8_t {
    kUnknown,
    kAbsent,
    kPresent,
  };

  struct Group {
    std::string stem;
    std::size_t source_texture = 0;
    int writer_texture = -1;
    Alpha alpha = Alpha::kUnknown;
  };

  std::vector<bool> referenced;
  std::vector<std::size_t> group_by_texture;
  std::vector<Group> groups;
};

struct RoomProjection {
  std::vector<bool> has_faces;
  std::uint64_t empty_rooms = 0;
  std::uint64_t exported_portals = 0;
  std::uint64_t discarded_portals = 0;
  std::uint64_t discarded_positive_distances = 0;
};

bool exportedPortal(const Portal& portal, const RoomProjection& projection) {
  return projection.has_faces[portal.room_1] && projection.has_faces[portal.room_2];
}

RoomProjection buildRoomProjection(const LevelModules& level) {
  RoomProjection projection;
  projection.has_faces.assign(level.rooms.definitions.size(), false);
  for (RoomIndex room : level.rooms.face_rooms) projection.has_faces[room] = true;
  for (bool has_faces : projection.has_faces)
    if (!has_faces) ++projection.empty_rooms;

  for (const Portal& portal : level.rooms.portals) {
    if (exportedPortal(portal, projection))
      ++projection.exported_portals;
    else
      ++projection.discarded_portals;
  }

  if (rooms::hasCompleteRoomDistances(level.rooms.distances, level.rooms.definitions.size())) {
    for (std::size_t first = 0; first < level.rooms.definitions.size(); ++first) {
      for (std::size_t second = first + 1; second < level.rooms.definitions.size(); ++second) {
        if (projection.has_faces[first] && projection.has_faces[second]) continue;
        const RoomDistance& distance = level.rooms.distances[rooms::roomDistancePairIndex(first, second)];
        if (distance.distance > 0.0f) ++projection.discarded_positive_distances;
      }
    }
  }
  return projection;
}

GlbVec3 toVec3(const ArxVector3& value) { return {value.x, value.y, value.z}; }

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

ArxReturnCode buildTextureRegistry(const LevelModules& level, TextureRegistry& out) {
  TextureRegistry registry;
  registry.referenced.assign(level.geometry.textures.size(), false);
  registry.group_by_texture.assign(level.geometry.textures.size(), std::numeric_limits<std::size_t>::max());
  for (const Face& face : level.geometry.faces)
    if (face.texture != kNoTexture) registry.referenced[static_cast<std::size_t>(face.texture)] = true;

  std::map<std::string, std::size_t> group_by_stem;
  for (std::size_t i = 0; i < level.geometry.textures.size(); ++i) {
    if (!registry.referenced[i]) continue;
    const std::string& path = level.geometry.textures[i].path;
    std::string stem = sanitizeMaterialStem(path);
    if (stem.empty()) return ARX_GLB_BAD_LEVEL_MATERIAL;
    if (stem == "no_tex" || glb_level::isReservedPaletteStem(stem)) return ARX_GLB_BAD_LEVEL_MATERIAL_RESERVED_STEM;
    std::string normalized = normalizeTexturePath(path);
    auto [entry, inserted] = group_by_stem.emplace(stem, registry.groups.size());
    if (inserted) {
      registry.groups.push_back({stem, i});
    } else {
      TextureRegistry::Group& group = registry.groups[entry->second];
      const Texture& representative = level.geometry.textures[group.source_texture];
      if (normalizeTexturePath(representative.path) != normalized) return ARX_GLB_BAD_LEVEL_MATERIAL_STEM_COLLISION;
      if (!representative.encoded_image.empty() && !level.geometry.textures[i].encoded_image.empty() &&
          representative.encoded_image != level.geometry.textures[i].encoded_image) {
        return ARX_GLB_BAD_LEVEL_MATERIAL_STEM_COLLISION;
      }
      if (representative.encoded_image.empty() && !level.geometry.textures[i].encoded_image.empty())
        group.source_texture = i;
    }
    registry.group_by_texture[i] = entry->second;
  }
  out = std::move(registry);
  return ARX_OK;
}

ArxReturnCode registerTextures(const LevelModules& level, TextureRegistry& registry, Builder& builder) {
  for (TextureRegistry::Group& group : registry.groups) {
    const Texture& texture = level.geometry.textures[group.source_texture];
    if (texture.encoded_image.empty()) {
      group.writer_texture = builder.addExternalTexture(std::string(pathFilename(texture.path)), texture.path);
      continue;
    }

    glb::PreparedImage image;
    ArxReturnCode rc = level_validation::imageError(glb::prepareImage(texture.encoded_image, image));
    if (rc != ARX_OK) return rc;
    group.alpha =
        geometry::imageHasAlpha(image.info) ? TextureRegistry::Alpha::kPresent : TextureRegistry::Alpha::kAbsent;
    std::string name = group.stem;
    name += geometry::imageExtension(image.info.format);
    group.writer_texture =
        builder.addEmbeddedTexture(std::move(name), std::string(glb::imageMimeType(image.info.format)), image.encoded);
  }
  return ARX_OK;
}

void logUnusedTextures(const LevelModules& level, const TextureRegistry& registry) {
  std::set<std::string> warned;
  for (std::size_t i = 0; i < registry.referenced.size(); ++i) {
    if (!registry.referenced[i] && warned.insert(level.geometry.textures[i].path).second)
      log(ARX_LOG_WARN, std::format("Level -> GLB: unused texture omitted: {}", level.geometry.textures[i].path));
  }
}

}  // namespace

ArxReturnCode exportLevelToGlb(const LevelModules& level, const ArxAabb& referenced_bounds,
                               const Level::GlbExportOptions& options, std::vector<std::uint8_t>& out) {
  TextureRegistry texture_registry;
  ArxReturnCode rc = buildTextureRegistry(level, texture_registry);
  if (rc != ARX_OK) return rc;

  Builder builder;
  glb_level::Palette palette(builder);
  rc = registerTextures(level, texture_registry, builder);
  if (rc != ARX_OK) return rc;
  rc = glb_level::configureGlbExportCoordinates(builder, options);
  if (rc != ARX_OK) return rc;
  const RoomProjection room_projection = buildRoomProjection(level);
  std::map<LevelRenderKey, int> material_map;
  std::set<std::size_t> unknown_alpha_groups;
  std::uint64_t nonstandard_transval = 0;
  for (const Face& face : level.geometry.faces) {
    const bool transparent = (face.flags & kFaceBitTrans) != 0;
    const bool standard_transparency = transparent && face.transval > 0.0f && face.transval < 1.0f;
    if (transparent && !standard_transparency) ++nonstandard_transval;
    LevelRenderKey key = renderKey(face, texture_registry);
    if (material_map.contains(key)) continue;
    int texture = -1;
    bool alpha_cutout = false;
    std::string stem = "no_tex";
    if (key.texture_group != std::numeric_limits<std::size_t>::max()) {
      const TextureRegistry::Group& group = texture_registry.groups[key.texture_group];
      texture = group.writer_texture;
      stem = group.stem;
      if (!transparent) {
        alpha_cutout = group.alpha == TextureRegistry::Alpha::kPresent;
        if (group.alpha == TextureRegistry::Alpha::kUnknown) unknown_alpha_groups.insert(key.texture_group);
      }
    }
    float alpha = 1.0f;
    if (standard_transparency) alpha = 1.0f - face.transval;
    material_map.emplace(
        key, builder.addMaterial(materialName(stem, key.flags, key.transval), texture, key.flags, alpha, alpha_cutout));
  }
  if (!unknown_alpha_groups.empty()) {
    log(ARX_LOG_WARN,
        std::format("Level -> GLB: alpha presence unknown for {} external texture(s); non-TRANS materials exported "
                    "as OPAQUE",
                    unknown_alpha_groups.size()));
  }

  using RenderVertexKey =
      std::tuple<LevelRenderKey, std::uint32_t, float, float, float, float, float, float, float, float>;
  ArxVector3 room_root = bottomCenter(referenced_bounds);
  room_root.y += kRoomParentOffset;
  int room_parent = builder.addNode("rooms_parent");
  builder.setNodeTranslation(room_parent, toVec3(room_root));
  builder.addRoot(room_parent);

  for (std::size_t room_index = 0; room_index < level.rooms.definitions.size(); ++room_index) {
    if (!room_projection.has_faces[room_index]) continue;
    std::vector<GlbVec3> positions;
    std::vector<GlbVec3> normals;
    std::vector<GlbVec3> colors;
    std::vector<GlbVec2> uv;
    std::map<RenderVertexKey, std::uint32_t> render_vertex_by_value;
    std::map<LevelRenderKey, std::vector<std::uint32_t>> indices_by_key;

    for (std::size_t face_index = 0; face_index < level.geometry.faces.size(); ++face_index) {
      if (level.rooms.face_rooms[face_index] != room_index) continue;
      const Face& face = level.geometry.faces[face_index];
      LevelRenderKey material_key = renderKey(face, texture_registry);
      std::vector<std::uint32_t>& indices = indices_by_key[material_key];
      for (std::size_t corner_index = 0; corner_index < face.corners.size(); ++corner_index) {
        const Corner& corner = face.corners[corner_index];
        const ArxColor3 color = lights::cornerColorOr(
            level.lighting, static_cast<FaceIndex>(face_index), corner_index, lights::kDefaultCornerColor);
        RenderVertexKey key = {material_key,
                               corner.vertex,
                               corner.normal.x,
                               corner.normal.y,
                               corner.normal.z,
                               corner.u,
                               corner.v,
                               color.r,
                               color.g,
                               color.b};
        auto [entry, inserted] = render_vertex_by_value.emplace(key, 0);
        if (inserted) {
          entry->second = static_cast<std::uint32_t>(positions.size());
          const ArxVector3& position = level.geometry.vertices[corner.vertex].position;
          positions.push_back(toVec3(position));
          normals.push_back(toVec3(corner.normal));
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

  int portal_material = palette.material(glb_level::PaletteItem::kPortal);
  int portal_parent = -1;
  ArxVector3 portal_root{};
  if (room_projection.exported_portals != 0) {
    portal_root = bottomCenter(referenced_bounds);
    portal_root.y += kPortalParentOffset;
    portal_parent = builder.addNode("portals_parent");
    builder.setNodeTranslation(portal_parent, toVec3(portal_root));
    builder.addRoot(portal_parent);
  }
  for (const Portal& portal : level.rooms.portals) {
    if (!exportedPortal(portal, room_projection)) continue;
    const std::size_t count = rooms::portalVertexCount(portal.shape);
    ArxVector3 centroid = rooms::portalCentroid(portal);
    std::vector<GlbVec3> portal_positions;
    portal_positions.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      portal_positions.push_back(
          {portal.vertices[i].x - centroid.x, portal.vertices[i].y - centroid.y, portal.vertices[i].z - centroid.z});
    }
    constexpr std::array<std::uint16_t, 6> kQuadIndices = {0, 1, 3, 2, 3, 1};
    constexpr std::array<std::uint16_t, 3> kTriIndices = {0, 1, 2};
    Primitive primitive;
    primitive.indices =
        count == 4 ? builder.addAccessor(
                         std::span<const std::uint16_t>(kQuadIndices), cgltf_component_type_r_16u, cgltf_type_scalar)
                   : builder.addAccessor(
                         std::span<const std::uint16_t>(kTriIndices), cgltf_component_type_r_16u, cgltf_type_scalar);
    primitive.material = portal_material;
    primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(portal_positions));
    std::string name = portalNodeName(portal, level.rooms);
    int mesh = builder.addMesh(name, {std::move(primitive)});
    int node = builder.addNode(std::move(name), mesh);
    builder.setNodeTranslation(node,
                               {centroid.x - portal_root.x, centroid.y - portal_root.y, centroid.z - portal_root.z});
    builder.addChild(portal_parent, node);
  }

  if (!level.navigation.anchors.empty()) {
    ArxVector3 anchor_root = bottomCenter(referenced_bounds);
    anchor_root.y += kAnchorParentOffset;
    int anchor_parent = builder.addNode("anchors_parent");
    builder.setNodeTranslation(anchor_parent, toVec3(anchor_root));
    builder.addRoot(anchor_parent);
    for (std::size_t anchor_index = 0; anchor_index < level.navigation.anchors.size(); ++anchor_index) {
      const Anchor& anchor = level.navigation.anchors[anchor_index];
      int node = builder.addNode(anchorNodeName(anchor, anchor_index, options));
      builder.setNodeTranslation(
          node,
          {anchor.position.x - anchor_root.x, anchor.position.y - anchor_root.y, anchor.position.z - anchor_root.z});
      builder.addChild(anchor_parent, node);
    }
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

  std::uint64_t defaulted_light_fallstarts = 0;
  if (!level.lighting.lights.empty()) {
    ArxVector3 light_root = bottomCenter(referenced_bounds);
    light_root.y += kLightParentOffset;
    int light_parent = builder.addNode("lights_parent");
    builder.setNodeTranslation(light_parent, toVec3(light_root));
    builder.addRoot(light_parent);
    for (const Light& light : level.lighting.lights) {
      bool real_light = light.fallend > 0.0f;
      bool default_fallstart = real_light && light.fallstart >= light.fallend;
      Light normalized_light = light;
      if (default_fallstart) normalized_light.fallstart = normalized_light.fallend * 0.5f;
      std::string name = lightNodeName(normalized_light, options);
      if (default_fallstart) ++defaulted_light_fallstarts;
      int node = builder.addNode(name);
      if (real_light) {
        int resource = builder.addPointLight(name,
                                             {light.color.r, light.color.g, light.color.b},
                                             light.intensity,
                                             glb_level::toGlbLength(normalized_light.fallend, options));
        builder.setNodeLight(node, resource);
      }
      builder.setNodeTranslation(
          node, {light.position.x - light_root.x, light.position.y - light_root.y, light.position.z - light_root.z});
      builder.addChild(light_parent, node);
      if (hasSettingsFields(normalized_light))
        builder.addChild(node, builder.addNode(lightSettingsHelperName(light.name, normalized_light)));
      if (light.flags != 0) builder.addChild(node, builder.addNode(lightFlagHelperName(light.name, light.flags)));
      if (hasEffectFields(normalized_light))
        builder.addChild(node, builder.addNode(lightEffectHelperName(light.name, normalized_light, options)));
    }
  }

  glb_level::exportPlayerSpawn(level, builder);
  glb_level::exportEntities(level, referenced_bounds, builder);
  rc = glb_level::exportZones(level, referenced_bounds, options, builder, palette);
  if (rc != ARX_OK) return rc;
  glb_level::exportPaths(level, referenced_bounds, builder);
  glb_level::exportFogs(level, referenced_bounds, options, builder);

  if (defaulted_light_fallstarts != 0 || room_projection.discarded_portals != 0 ||
      room_projection.discarded_positive_distances != 0) {
    std::string warning = "Level -> GLB repairs:";
    if (defaulted_light_fallstarts != 0)
      warning += std::format(" {} light fallstart value(s) defaulted;", defaulted_light_fallstarts);
    if (room_projection.discarded_portals != 0 || room_projection.discarded_positive_distances != 0)
      warning += std::format(
          " {} empty room(s), {} referencing portal(s), and {} positive room-distance pair(s) "
          "discarded;",
          room_projection.empty_rooms,
          room_projection.discarded_portals,
          room_projection.discarded_positive_distances);
    warning.pop_back();
    log(ARX_LOG_WARN, warning);
  }
  if (nonstandard_transval != 0)
    log(ARX_LOG_WARN,
        std::format("Level -> GLB: {} transparent face(s) use nonstandard Arx blend modes; raw transval is "
                    "preserved in material names and previewed with alpha 1",
                    nonstandard_transval));
  logUnusedTextures(level, texture_registry);
  return builder.write(out);
}

}  // namespace pistoris
