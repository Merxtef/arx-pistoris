// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"

#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace test {
namespace detail {

inline ArxStringView stringView(std::string_view value) { return {value.data(), value.size()}; }

inline std::string string(ArxStringView value) {
  return value.size == 0 ? std::string{} : std::string(value.data, value.size);
}

inline std::vector<std::uint8_t> bytes(ArxEncodedImageView value) {
  return value.size == 0 ? std::vector<std::uint8_t>{} : std::vector<std::uint8_t>(value.data, value.data + value.size);
}

inline ArxLevelVertex publicVertex(const pistoris::Vertex& value) { return {value.position}; }

inline ArxLevelFace publicFace(const pistoris::Face& value, pistoris::RoomIndex room) {
  ArxLevelFace out{};
  for (std::size_t corner = 0; corner < value.corners.size(); ++corner) {
    out.corners[corner].vertex = value.corners[corner].vertex;
    out.corners[corner].normal = value.corners[corner].normal;
    out.corners[corner].u = value.corners[corner].u;
    out.corners[corner].v = value.corners[corner].v;
  }
  out.texture = value.texture;
  out.room = room;
  out.flags = value.flags;
  out.transval = value.transval;
  return out;
}

inline ArxLevelRoom publicRoom(const pistoris::Room& value) { return {stringView(value.name)}; }

inline ArxLevelTextureView publicTexture(const pistoris::Texture& value) {
  return {stringView(value.path), {value.encoded_image.data(), value.encoded_image.size()}};
}

inline ArxLevelPortal publicPortal(const pistoris::Portal& value) {
  ArxLevelPortal out{};
  out.name = stringView(value.name);
  out.room_1 = value.room_1;
  out.room_2 = value.room_2;
  out.shape = static_cast<ArxPortalShape>(value.shape);
  for (std::size_t vertex = 0; vertex < value.vertices.size(); ++vertex) out.vertices[vertex] = value.vertices[vertex];
  return out;
}

inline ArxLevelAnchor publicAnchor(const pistoris::Anchor& value) {
  return {value.position, value.radius, value.height, value.flags, stringView(value.name)};
}

inline ArxLevelLight publicLight(const pistoris::Light& value) {
  return {stringView(value.name),
          value.position,
          value.color,
          value.fallstart,
          value.fallend,
          value.intensity,
          value.flicker,
          value.effect_radius,
          value.effect_frequency,
          value.effect_size,
          value.effect_speed,
          value.flare_size,
          value.flags};
}

inline ArxLevelEntity publicEntity(const pistoris::Entity& value) {
  return {stringView(value.class_path), value.ident, value.position, value.rotation, stringView(value.name)};
}

inline ArxLevelFog publicFog(const pistoris::Fog& value) {
  return {value.position,
          value.color,
          value.size,
          static_cast<std::uint8_t>(value.directional),
          value.scale,
          value.rotation,
          value.speed,
          value.rotate_speed,
          value.lifetime_ms,
          value.frequency,
          stringView(value.name)};
}

inline ArxLevelZoneInput publicZone(const pistoris::Zone& value) {
  ArxLevelZoneInput out{};
  out.value.name = stringView(value.name);
  out.value.perimeter_count = value.perimeter_xz.size();
  out.value.reference_y = value.reference_y;
  out.value.height_mode = static_cast<ArxZoneHeightMode>(value.height_mode);
  out.value.height = value.height;
  const auto& color = value.color;
  if (color) {
    out.value.has_color = 1;
    out.value.color = *color;
  }
  const auto& farclip = value.farclip;
  if (farclip) {
    out.value.has_farclip = 1;
    out.value.farclip = *farclip;
  }
  const auto& ambiance = value.ambiance;
  if (ambiance) {
    out.value.has_ambiance = 1;
    out.value.ambiance.name = stringView(ambiance->name);
    out.value.ambiance.volume = ambiance->volume;
  }
  out.perimeter_xz = value.perimeter_xz.data();
  return out;
}

inline ArxLevelPathInput publicPath(const pistoris::Path& value, std::vector<ArxLevelPathNode>& nodes) {
  nodes.reserve(value.nodes.size());
  for (const pistoris::PathNode& node : value.nodes) {
    nodes.push_back({node.relative_position, static_cast<ArxPathNodeType>(node.type), node.time_ms});
  }
  return {stringView(value.name), value.position, nodes.data(), nodes.size()};
}

}  // namespace detail

struct MeshSnapshot {
  std::vector<pistoris::Vertex> vertices;
  std::vector<pistoris::Face> faces;
  std::vector<pistoris::Texture> textures;
  std::vector<pistoris::RoomIndex> face_rooms;
  std::vector<ArxColor3> corner_colors;
};

struct AnchorsSnapshot {
  std::vector<pistoris::Anchor> anchors;
  std::vector<pistoris::AnchorConnection> connections;
};

inline ArxReturnCode replaceMesh(pistoris::Level& level, const MeshSnapshot& value) {
  std::vector<ArxLevelVertex> vertices;
  vertices.reserve(value.vertices.size());
  for (const pistoris::Vertex& vertex : value.vertices) vertices.push_back(detail::publicVertex(vertex));

  std::vector<ArxLevelFace> faces;
  faces.reserve(value.faces.size());
  for (std::size_t face = 0; face < value.faces.size(); ++face) {
    const pistoris::RoomIndex room = face < value.face_rooms.size() ? value.face_rooms[face] : 0;
    ArxLevelFace projected = detail::publicFace(value.faces[face], room);
    if (value.corner_colors.size() == value.faces.size() * 3U) {
      projected.has_corner_colors = 1;
      for (std::size_t corner = 0; corner < 3; ++corner) {
        projected.corners[corner].color = value.corner_colors[face * 3U + corner];
      }
    }
    faces.push_back(projected);
  }

  std::vector<ArxLevelTextureView> textures;
  textures.reserve(value.textures.size());
  for (const pistoris::Texture& texture : value.textures) textures.push_back(detail::publicTexture(texture));
  return level.replaceMesh(
      {vertices.data(), vertices.size(), faces.data(), faces.size(), textures.data(), textures.size()});
}

inline ArxReturnCode copyMesh(const pistoris::Level& level, MeshSnapshot& out) {
  std::vector<ArxLevelVertex> vertices(level.vertexCount());
  std::vector<ArxLevelFace> faces(level.faceCount());
  std::vector<ArxLevelTextureView> textures(level.textureCount());
  ArxReturnCode rc = level.copyVertices(0, vertices.size(), vertices.data());
  if (rc != ARX_OK) return rc;
  rc = level.copyFaces(0, faces.size(), faces.data());
  if (rc != ARX_OK) return rc;
  rc = level.copyTextureViews(0, textures.size(), textures.data());
  if (rc != ARX_OK) return rc;

  MeshSnapshot copied;
  copied.vertices.reserve(vertices.size());
  for (const ArxLevelVertex& vertex : vertices) copied.vertices.push_back({vertex.position});
  copied.faces.reserve(faces.size());
  copied.face_rooms.reserve(faces.size());
  const bool has_corner_colors = !faces.empty() && faces.front().has_corner_colors != 0;
  if (has_corner_colors) copied.corner_colors.reserve(faces.size() * 3U);
  for (const ArxLevelFace& face : faces) {
    pistoris::Face internal{};
    for (std::size_t corner = 0; corner < internal.corners.size(); ++corner) {
      internal.corners[corner] = {
          face.corners[corner].vertex, face.corners[corner].normal, face.corners[corner].u, face.corners[corner].v};
      if (has_corner_colors) copied.corner_colors.push_back(face.corners[corner].color);
    }
    internal.texture = face.texture;
    internal.flags = face.flags;
    internal.transval = face.transval;
    copied.faces.push_back(internal);
    copied.face_rooms.push_back(face.room);
  }
  copied.textures.reserve(textures.size());
  for (const ArxLevelTextureView& texture : textures) {
    pistoris::Texture internal(detail::string(texture.path));
    internal.encoded_image = detail::bytes(texture.encoded_image);
    copied.textures.push_back(std::move(internal));
  }
  out = std::move(copied);
  return ARX_OK;
}

inline ArxReturnCode replaceAnchors(pistoris::Level& level, const AnchorsSnapshot& value) {
  std::vector<ArxLevelAnchor> anchors;
  anchors.reserve(value.anchors.size());
  for (const pistoris::Anchor& anchor : value.anchors) anchors.push_back(detail::publicAnchor(anchor));

  std::vector<ArxLevelAnchorConnection> connections;
  connections.reserve(value.connections.size());
  for (const pistoris::AnchorConnection& connection : value.connections) {
    connections.push_back({connection.first, connection.second});
  }
  return level.replaceAnchors({anchors.data(), anchors.size(), connections.data(), connections.size()});
}

inline ArxReturnCode replaceRoomDistances(pistoris::Level& level, std::span<const pistoris::RoomDistance> values) {
  std::vector<ArxLevelRoomDistance> distances;
  distances.reserve(values.size());
  std::size_t source = 0;
  for (pistoris::RoomIndex second = 1; second < level.roomCount() && source < values.size(); ++second) {
    for (pistoris::RoomIndex first = 0; first < second && source < values.size(); ++first, ++source) {
      distances.push_back(
          {first, second, values[source].distance, values[source].low_room_portal, values[source].high_room_portal});
    }
  }
  if (source != values.size()) distances.resize(values.size());
  return level.replaceRoomDistances(distances.data(), distances.size());
}

inline ArxReturnCode setNavSurface(pistoris::Level& level, const pistoris::NavSurface& value) {
  std::vector<ArxLevelVertex> vertices;
  vertices.reserve(value.vertices.size());
  for (const pistoris::Vertex& vertex : value.vertices) vertices.push_back(detail::publicVertex(vertex));
  std::vector<ArxLevelNavSurfaceTriangle> triangles;
  triangles.reserve(value.triangles.size());
  for (const pistoris::NavSurfaceTriangle& triangle : value.triangles) {
    triangles.push_back({triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]});
  }
  return level.setNavSurface({vertices.data(), vertices.size(), triangles.data(), triangles.size()});
}

inline ArxReturnCode setVertex(pistoris::Level& level, pistoris::VertexIndex index, const pistoris::Vertex& value) {
  return level.setVertex(index, detail::publicVertex(value));
}

inline ArxReturnCode setTexture(pistoris::Level& level, pistoris::TextureIndex index, const pistoris::Texture& value) {
  return level.setTexture(index, detail::publicTexture(value));
}

inline ArxReturnCode setRoom(pistoris::Level& level, pistoris::RoomIndex index, const pistoris::Room& value) {
  return level.setRoom(index, detail::publicRoom(value));
}

inline ArxReturnCode setPortal(pistoris::Level& level, pistoris::PortalIndex index, const pistoris::Portal& value) {
  return level.setPortal(index, detail::publicPortal(value));
}

inline ArxReturnCode setFace(pistoris::Level& level, pistoris::FaceIndex index, const pistoris::Face& value) {
  ArxLevelFace current{};
  ArxReturnCode rc = level.copyFaces(index, 1, &current);
  if (rc != ARX_OK) return rc;
  ArxLevelFace projected = detail::publicFace(value, current.room);
  if (current.has_corner_colors != 0) {
    projected.has_corner_colors = 1;
    for (std::size_t corner = 0; corner < 3; ++corner) projected.corners[corner].color = current.corners[corner].color;
  }
  return level.setFace(index, projected);
}

inline ArxReturnCode setAnchor(pistoris::Level& level, pistoris::AnchorIndex index, const pistoris::Anchor& value) {
  return level.setAnchor(index, detail::publicAnchor(value));
}

inline ArxReturnCode setLight(pistoris::Level& level, pistoris::LightIndex index, const pistoris::Light& value) {
  return level.setLight(index, detail::publicLight(value));
}

inline ArxReturnCode setPlayerSpawn(pistoris::Level& level, const pistoris::PlayerSpawn& value) {
  return level.setPlayerSpawn({value.position, value.rotation, 1});
}

inline ArxReturnCode setEntity(pistoris::Level& level, pistoris::EntityIndex index, const pistoris::Entity& value) {
  return level.setEntity(index, detail::publicEntity(value));
}

inline ArxReturnCode setFog(pistoris::Level& level, pistoris::FogIndex index, const pistoris::Fog& value) {
  return level.setFog(index, detail::publicFog(value));
}

inline ArxReturnCode setZone(pistoris::Level& level, pistoris::ZoneIndex index, const pistoris::Zone& value) {
  const ArxLevelZoneInput input = detail::publicZone(value);
  return level.setZone(index, input);
}

inline ArxReturnCode setPath(pistoris::Level& level, pistoris::PathIndex index, const pistoris::Path& value) {
  std::vector<ArxLevelPathNode> nodes;
  const ArxLevelPathInput input = detail::publicPath(value, nodes);
  return level.setPath(index, input);
}

inline pistoris::Vertex vertex(const pistoris::Level& level, pistoris::VertexIndex index) {
  ArxLevelVertex value{};
  static_cast<void>(level.copyVertices(index, 1, &value));
  return {value.position};
}

inline pistoris::Face face(const pistoris::Level& level, pistoris::FaceIndex index) {
  ArxLevelFace value{};
  static_cast<void>(level.copyFaces(index, 1, &value));
  pistoris::Face out{};
  for (std::size_t corner = 0; corner < out.corners.size(); ++corner) {
    out.corners[corner] = {
        value.corners[corner].vertex, value.corners[corner].normal, value.corners[corner].u, value.corners[corner].v};
  }
  out.texture = value.texture;
  out.flags = value.flags;
  out.transval = value.transval;
  return out;
}

inline pistoris::Texture texture(const pistoris::Level& level, pistoris::TextureIndex index) {
  ArxLevelTextureView value{};
  static_cast<void>(level.copyTextureViews(index, 1, &value));
  pistoris::Texture out(detail::string(value.path));
  out.encoded_image = detail::bytes(value.encoded_image);
  return out;
}

inline pistoris::Room room(const pistoris::Level& level, pistoris::RoomIndex index) {
  ArxLevelRoom value{};
  static_cast<void>(level.copyRooms(index, 1, &value));
  return {detail::string(value.name)};
}

inline pistoris::Portal portal(const pistoris::Level& level, pistoris::PortalIndex index) {
  ArxLevelPortal value{};
  static_cast<void>(level.copyPortals(index, 1, &value));
  pistoris::Portal out{};
  out.name = detail::string(value.name);
  out.room_1 = value.room_1;
  out.room_2 = value.room_2;
  out.shape = static_cast<pistoris::PortalShape>(value.shape);
  for (std::size_t vertex = 0; vertex < out.vertices.size(); ++vertex) out.vertices[vertex] = value.vertices[vertex];
  return out;
}

inline pistoris::Anchor anchor(const pistoris::Level& level, pistoris::AnchorIndex index) {
  ArxLevelAnchor value{};
  static_cast<void>(level.copyAnchors(index, 1, &value));
  return {value.position, value.radius, value.height, value.flags, detail::string(value.name)};
}

inline pistoris::AnchorConnection anchorConnection(const pistoris::Level& level,
                                                   pistoris::AnchorConnectionIndex index) {
  ArxLevelAnchorConnection value{};
  static_cast<void>(level.copyAnchorConnections(index, 1, &value));
  return {value.first, value.second};
}

inline pistoris::Light light(const pistoris::Level& level, pistoris::LightIndex index) {
  ArxLevelLight value{};
  static_cast<void>(level.copyLights(index, 1, &value));
  return {detail::string(value.name),
          value.position,
          value.color,
          value.fallstart,
          value.fallend,
          value.intensity,
          value.flicker,
          value.effect_radius,
          value.effect_frequency,
          value.effect_size,
          value.effect_speed,
          value.flare_size,
          value.flags};
}

inline pistoris::Entity entity(const pistoris::Level& level, pistoris::EntityIndex index) {
  ArxLevelEntity value{};
  static_cast<void>(level.copyEntities(index, 1, &value));
  return {detail::string(value.class_path), value.ident, value.position, value.rotation, detail::string(value.name)};
}

inline pistoris::Fog fog(const pistoris::Level& level, pistoris::FogIndex index) {
  ArxLevelFog value{};
  static_cast<void>(level.copyFogs(index, 1, &value));
  return {value.position,
          value.color,
          value.size,
          value.directional != 0,
          value.scale,
          value.rotation,
          value.speed,
          value.rotate_speed,
          value.lifetime_ms,
          value.frequency,
          detail::string(value.name)};
}

inline pistoris::Zone zone(const pistoris::Level& level, pistoris::ZoneIndex index) {
  ArxLevelZone value{};
  static_cast<void>(level.copyZones(index, 1, &value));
  pistoris::Zone out{};
  out.name = detail::string(value.name);
  out.reference_y = value.reference_y;
  out.height_mode = static_cast<pistoris::ZoneHeightMode>(value.height_mode);
  out.height = value.height;
  out.perimeter_xz.resize(value.perimeter_count);
  static_cast<void>(level.copyZonePerimeter(index, 0, out.perimeter_xz.size(), out.perimeter_xz.data()));
  if (value.has_color != 0) out.color = value.color;
  if (value.has_farclip != 0) out.farclip = value.farclip;
  if (value.has_ambiance != 0) {
    out.ambiance = pistoris::ZoneAmbiance{detail::string(value.ambiance.name), value.ambiance.volume};
  }
  return out;
}

inline pistoris::Path path(const pistoris::Level& level, pistoris::PathIndex index) {
  ArxLevelPath value{};
  static_cast<void>(level.copyPaths(index, 1, &value));
  pistoris::Path out{};
  out.name = detail::string(value.name);
  out.position = value.position;
  std::vector<ArxLevelPathNode> nodes(value.node_count);
  static_cast<void>(level.copyPathNodes(index, 0, nodes.size(), nodes.data()));
  for (const ArxLevelPathNode& node : nodes) {
    out.nodes.push_back({node.relative_position, static_cast<pistoris::PathNodeType>(node.type), node.time_ms});
  }
  return out;
}

inline std::vector<pistoris::Vertex> vertices(const pistoris::Level& level) {
  std::vector<ArxLevelVertex> projected(level.vertexCount());
  static_cast<void>(level.copyVertices(0, projected.size(), projected.data()));
  std::vector<pistoris::Vertex> out;
  out.reserve(projected.size());
  for (const ArxLevelVertex& value : projected) out.push_back({value.position});
  return out;
}

inline std::vector<pistoris::Face> faces(const pistoris::Level& level) {
  std::vector<pistoris::Face> out;
  out.reserve(level.faceCount());
  for (pistoris::FaceIndex index = 0; index < level.faceCount(); ++index) out.push_back(face(level, index));
  return out;
}

inline std::vector<pistoris::Texture> textures(const pistoris::Level& level) {
  std::vector<pistoris::Texture> out;
  out.reserve(level.textureCount());
  for (pistoris::TextureIndex index = 0; index < level.textureCount(); ++index) out.push_back(texture(level, index));
  return out;
}

inline std::vector<pistoris::Room> rooms(const pistoris::Level& level) {
  std::vector<pistoris::Room> out;
  out.reserve(level.roomCount());
  for (pistoris::RoomIndex index = 0; index < level.roomCount(); ++index) out.push_back(room(level, index));
  return out;
}

inline std::vector<pistoris::Portal> portals(const pistoris::Level& level) {
  std::vector<pistoris::Portal> out;
  out.reserve(level.portalCount());
  for (pistoris::PortalIndex index = 0; index < level.portalCount(); ++index) out.push_back(portal(level, index));
  return out;
}

inline std::vector<pistoris::RoomDistance> roomDistances(const pistoris::Level& level) {
  std::vector<ArxLevelRoomDistance> projected(level.roomDistanceCount());
  static_cast<void>(level.copyRoomDistances(0, projected.size(), projected.data()));
  std::vector<pistoris::RoomDistance> out;
  out.reserve(projected.size());
  for (const ArxLevelRoomDistance& value : projected) {
    out.push_back({value.distance, value.portal_a, value.portal_b});
  }
  return out;
}

inline std::optional<ArxLevelRoomDistance> roomDistance(const pistoris::Level& level, pistoris::RoomIndex first,
                                                        pistoris::RoomIndex second) {
  std::uint8_t has_distance = 0;
  ArxLevelRoomDistance value{};
  if (level.getRoomDistance(first, second, has_distance, value) != ARX_OK || has_distance == 0) return std::nullopt;
  return value;
}

inline std::vector<pistoris::Anchor> anchors(const pistoris::Level& level) {
  std::vector<pistoris::Anchor> out;
  out.reserve(level.anchorCount());
  for (pistoris::AnchorIndex index = 0; index < level.anchorCount(); ++index) out.push_back(anchor(level, index));
  return out;
}

inline std::vector<pistoris::AnchorConnection> anchorConnections(const pistoris::Level& level) {
  std::vector<pistoris::AnchorConnection> out;
  out.reserve(level.anchorConnectionCount());
  for (pistoris::AnchorConnectionIndex index = 0; index < level.anchorConnectionCount(); ++index) {
    out.push_back(anchorConnection(level, index));
  }
  return out;
}

inline std::vector<pistoris::Light> lights(const pistoris::Level& level) {
  std::vector<pistoris::Light> out;
  out.reserve(level.lightCount());
  for (pistoris::LightIndex index = 0; index < level.lightCount(); ++index) out.push_back(light(level, index));
  return out;
}

inline std::vector<pistoris::Entity> entities(const pistoris::Level& level) {
  std::vector<pistoris::Entity> out;
  out.reserve(level.entityCount());
  for (pistoris::EntityIndex index = 0; index < level.entityCount(); ++index) out.push_back(entity(level, index));
  return out;
}

inline std::vector<pistoris::Fog> fogs(const pistoris::Level& level) {
  std::vector<pistoris::Fog> out;
  out.reserve(level.fogCount());
  for (pistoris::FogIndex index = 0; index < level.fogCount(); ++index) out.push_back(fog(level, index));
  return out;
}

inline std::vector<pistoris::Zone> zones(const pistoris::Level& level) {
  std::vector<pistoris::Zone> out;
  out.reserve(level.zoneCount());
  for (pistoris::ZoneIndex index = 0; index < level.zoneCount(); ++index) out.push_back(zone(level, index));
  return out;
}

inline std::vector<pistoris::Path> paths(const pistoris::Level& level) {
  std::vector<pistoris::Path> out;
  out.reserve(level.pathCount());
  for (pistoris::PathIndex index = 0; index < level.pathCount(); ++index) out.push_back(path(level, index));
  return out;
}

inline std::optional<pistoris::NavSurface> navSurface(const pistoris::Level& level) {
  ArxLevelNavSurfaceInfo info = level.navSurfaceInfo();
  if (info.has_surface == 0) return std::nullopt;
  std::vector<ArxLevelVertex> vertices(info.vertex_count);
  std::vector<ArxLevelNavSurfaceTriangle> triangles(info.triangle_count);
  if (level.copyNavSurfaceVertices(0, vertices.size(), vertices.data()) != ARX_OK ||
      level.copyNavSurfaceTriangles(0, triangles.size(), triangles.data()) != ARX_OK)
    return std::nullopt;
  pistoris::NavSurface out;
  out.vertices.reserve(vertices.size());
  for (const ArxLevelVertex& vertex : vertices) out.vertices.push_back({vertex.position});
  out.triangles.reserve(triangles.size());
  for (const ArxLevelNavSurfaceTriangle& triangle : triangles) {
    out.triangles.push_back({{triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]}});
  }
  return out;
}

inline pistoris::PlayerSpawn playerSpawn(const pistoris::Level& level, bool* out_usable = nullptr) {
  ArxLevelPlayerSpawn value = level.playerSpawn();
  if (out_usable) *out_usable = value.is_usable != 0;
  return {value.position, value.rotation};
}

inline pistoris::RoomIndex faceRoom(const pistoris::Level& level, pistoris::FaceIndex index) {
  ArxLevelFace value{};
  static_cast<void>(level.copyFaces(index, 1, &value));
  return value.room;
}

inline std::vector<ArxColor3> cornerColors(const pistoris::Level& level) {
  std::vector<ArxLevelFace> projected(level.faceCount());
  static_cast<void>(level.copyFaces(0, projected.size(), projected.data()));
  if (projected.empty() || projected.front().has_corner_colors == 0) return {};
  std::vector<ArxColor3> out;
  out.reserve(projected.size() * 3U);
  for (const ArxLevelFace& face : projected) {
    for (const ArxLevelCorner& corner : face.corners) out.push_back(corner.color);
  }
  return out;
}

inline ArxColor3 cornerColor(const pistoris::Level& level, pistoris::FaceIndex face_index, std::size_t corner_index) {
  ArxLevelFace value{};
  static_cast<void>(level.copyFaces(face_index, 1, &value));
  return value.corners[corner_index].color;
}

inline ArxReturnCode copyAnchors(const pistoris::Level& level, AnchorsSnapshot& out) {
  AnchorsSnapshot copied;
  copied.anchors = anchors(level);
  copied.connections = anchorConnections(level);
  out = std::move(copied);
  return ARX_OK;
}

inline pistoris::VertexIndex addVertex(pistoris::Level& level, const pistoris::Vertex& value) {
  pistoris::VertexIndex index = pistoris::kInvalidVertexIndex;
  return level.addVertex(detail::publicVertex(value), index) == ARX_OK ? index : pistoris::kInvalidVertexIndex;
}

inline pistoris::FaceIndex addFace(pistoris::Level& level, const pistoris::Face& value, pistoris::RoomIndex room) {
  pistoris::FaceIndex index = pistoris::kInvalidFaceIndex;
  return level.addFace(detail::publicFace(value, room), index) == ARX_OK ? index : pistoris::kInvalidFaceIndex;
}

inline pistoris::RoomIndex addRoom(pistoris::Level& level, const pistoris::Room& value) {
  pistoris::RoomIndex index = pistoris::kInvalidRoomIndex;
  return level.addRoom(detail::publicRoom(value), index) == ARX_OK ? index : pistoris::kInvalidRoomIndex;
}

inline pistoris::PortalIndex addPortal(pistoris::Level& level, const pistoris::Portal& value) {
  pistoris::PortalIndex index = pistoris::kInvalidPortalIndex;
  return level.addPortal(detail::publicPortal(value), index) == ARX_OK ? index : pistoris::kInvalidPortalIndex;
}

inline pistoris::AnchorIndex addAnchor(pistoris::Level& level, const pistoris::Anchor& value) {
  pistoris::AnchorIndex index = pistoris::kInvalidAnchorIndex;
  return level.addAnchor(detail::publicAnchor(value), index) == ARX_OK ? index : pistoris::kInvalidAnchorIndex;
}

inline pistoris::AnchorConnectionIndex addAnchorConnection(pistoris::Level& level, pistoris::AnchorConnection value) {
  pistoris::AnchorConnectionIndex index = pistoris::kInvalidAnchorConnectionIndex;
  const ArxLevelAnchorConnection connection{value.first, value.second};
  return level.addAnchorConnection(connection, index) == ARX_OK ? index : pistoris::kInvalidAnchorConnectionIndex;
}

inline pistoris::LightIndex addLight(pistoris::Level& level, const pistoris::Light& value) {
  pistoris::LightIndex index = pistoris::kInvalidLightIndex;
  return level.addLight(detail::publicLight(value), index) == ARX_OK ? index : pistoris::kInvalidLightIndex;
}

inline pistoris::EntityIndex addEntity(pistoris::Level& level, const pistoris::Entity& value) {
  pistoris::EntityIndex index = pistoris::kInvalidEntityIndex;
  return level.addEntity(detail::publicEntity(value), index) == ARX_OK ? index : pistoris::kInvalidEntityIndex;
}

inline pistoris::FogIndex addFog(pistoris::Level& level, const pistoris::Fog& value) {
  pistoris::FogIndex index = pistoris::kInvalidFogIndex;
  return level.addFog(detail::publicFog(value), index) == ARX_OK ? index : pistoris::kInvalidFogIndex;
}

inline pistoris::ZoneIndex addZone(pistoris::Level& level, const pistoris::Zone& value) {
  pistoris::ZoneIndex index = pistoris::kInvalidZoneIndex;
  const ArxLevelZoneInput input = detail::publicZone(value);
  return level.addZone(input, index) == ARX_OK ? index : pistoris::kInvalidZoneIndex;
}

inline pistoris::PathIndex addPath(pistoris::Level& level, const pistoris::Path& value) {
  std::vector<ArxLevelPathNode> nodes;
  const ArxLevelPathInput input = detail::publicPath(value, nodes);
  pistoris::PathIndex index = pistoris::kInvalidPathIndex;
  return level.addPath(input, index) == ARX_OK ? index : pistoris::kInvalidPathIndex;
}

}  // namespace test
