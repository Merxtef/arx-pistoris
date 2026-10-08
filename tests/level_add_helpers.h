// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/texture.h"

#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "modules/textures.h"

#include <algorithm>
#include <cmath>
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
  out.normal = value.normal;
  return out;
}

inline ArxLevelRoom publicRoom(const pistoris::Room& value) { return {stringView(value.name)}; }

inline ArxTextureView publicTexture(const pistoris::Texture& value) {
  return {stringView(value.path),
          {value.encoded_image.data(), value.encoded_image.size()},
          stringView(value.external_image_extension)};
}

inline ArxLevelPortal publicPortal(const pistoris::Portal& value) {
  ArxLevelPortal out{};
  out.name = stringView(value.name);
  out.room_front = value.room_1;
  out.room_back = value.room_2;
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

struct GeometrySnapshot {
  std::vector<pistoris::Vertex> vertices;
  std::vector<pistoris::Face> faces;
  std::vector<pistoris::Texture> textures;
  std::vector<pistoris::RoomIndex> face_rooms;
  std::vector<ArxColor3> corner_colors;
  std::vector<ArxVector3> face_normals;
};

struct AnchorsSnapshot {
  std::vector<pistoris::Anchor> anchors;
  std::vector<pistoris::AnchorConnection> connections;
};

inline ArxReturnCode replaceGeometry(pistoris::Level& level, const GeometrySnapshot& value) {
  const auto normalized_or = [](ArxVector3 vector, ArxVector3 fallback) {
    const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    if (std::isfinite(length) && length > 1.0e-10f) {
      return ArxVector3{vector.x / length, vector.y / length, vector.z / length};
    }
    return fallback;
  };
  std::vector<float> positions;
  positions.reserve(value.vertices.size() * 3U);
  for (const pistoris::Vertex& vertex : value.vertices) {
    positions.insert(positions.end(), {vertex.position.x, vertex.position.y, vertex.position.z});
  }
  auto status = level.replaceVertices(positions);
  if (!status) return status.code();

  level.clearTextures();
  for (const pistoris::Texture& texture : value.textures) {
    auto added = level.addTexture(detail::publicTexture(texture));
    if (!added) return added.code();
  }

  std::vector<std::uint32_t> vertex_indices;
  std::vector<float> uvs;
  std::vector<float> corner_normals;
  std::vector<std::uint32_t> textures;
  std::vector<float> transvals;
  std::vector<float> corner_colors;
  std::vector<float> face_normals;
  std::vector<pistoris::FaceType> flags;
  std::vector<pistoris::RoomIndex> face_rooms;
  vertex_indices.reserve(value.faces.size() * 3U);
  uvs.reserve(value.faces.size() * 6U);
  corner_normals.reserve(value.faces.size() * 9U);
  textures.reserve(value.faces.size());
  transvals.reserve(value.faces.size());
  corner_colors.reserve(value.faces.size() * 9U);
  if (value.face_normals.size() == value.faces.size()) face_normals.reserve(value.faces.size() * 3U);
  flags.reserve(value.faces.size());
  face_rooms.reserve(value.faces.size());
  for (std::size_t face_index = 0; face_index < value.faces.size(); ++face_index) {
    const pistoris::Face& face = value.faces[face_index];
    ArxVector3 derived_normal{0.0f, -1.0f, 0.0f};
    if (face.corners[0].vertex < value.vertices.size() && face.corners[1].vertex < value.vertices.size() &&
        face.corners[2].vertex < value.vertices.size()) {
      const ArxVector3 a = value.vertices[face.corners[0].vertex].position;
      const ArxVector3 b = value.vertices[face.corners[1].vertex].position;
      const ArxVector3 c = value.vertices[face.corners[2].vertex].position;
      const ArxVector3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
      const ArxVector3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
      derived_normal = normalized_or({ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x},
                                     derived_normal);
    }
    for (std::size_t corner = 0; corner < face.corners.size(); ++corner) {
      const auto& item = face.corners[corner];
      vertex_indices.push_back(item.vertex);
      uvs.insert(uvs.end(), {item.u, item.v});
      const ArxVector3 normal = normalized_or(item.normal, derived_normal);
      corner_normals.insert(corner_normals.end(), {normal.x, normal.y, normal.z});
      const ArxColor3 color = value.corner_colors.size() == value.faces.size() * 3U
                                  ? value.corner_colors[face_index * 3U + corner]
                                  : ArxColor3{0.5f, 0.5f, 0.5f};
      corner_colors.insert(corner_colors.end(), {color.r, color.g, color.b});
    }
    textures.push_back(face.texture);
    transvals.push_back(face.transval);
    if (value.face_normals.size() == value.faces.size()) {
      const ArxVector3 normal = value.face_normals[face_index];
      face_normals.insert(face_normals.end(), {normal.x, normal.y, normal.z});
    }
    flags.push_back(face.flags);
    face_rooms.push_back(face_index < value.face_rooms.size() ? value.face_rooms[face_index] : 0);
  }
  status =
      level.replaceFaces(vertex_indices, uvs, corner_normals, textures, transvals, corner_colors, face_normals, flags);
  if (!status) return status.code();
  return level.replaceFaceRooms(face_rooms).code();
}

inline ArxReturnCode copyGeometry(const pistoris::Level& level, GeometrySnapshot& out) {
  const auto vertices = level.vertices();
  const auto faces = level.faces();
  const auto textures = level.textures();

  GeometrySnapshot copied;
  copied.vertices.reserve(vertices.size());
  for (const ArxLevelVertex& vertex : vertices) copied.vertices.push_back({vertex.position});
  copied.faces.reserve(faces.size());
  copied.face_rooms.reserve(faces.size());
  copied.corner_colors.reserve(faces.size() * 3U);
  copied.face_normals.reserve(faces.size());
  for (const ArxLevelFace& face : faces) {
    pistoris::Face internal{};
    for (std::size_t corner = 0; corner < internal.corners.size(); ++corner) {
      internal.corners[corner] = {
          face.corners[corner].vertex, face.corners[corner].normal, face.corners[corner].u, face.corners[corner].v};
      copied.corner_colors.push_back(face.corners[corner].color);
    }
    internal.texture = face.texture;
    internal.normal = face.normal;
    internal.flags = face.flags;
    internal.transval = face.transval;
    copied.faces.push_back(internal);
    copied.face_rooms.push_back(face.room);
    copied.face_normals.push_back(face.normal);
  }
  copied.textures.reserve(textures.size());
  for (const ArxTextureView& texture : textures) {
    pistoris::Texture internal(detail::string(texture.path));
    internal.encoded_image = detail::bytes(texture.encoded_image);
    internal.external_image_extension = detail::string(texture.external_image_extension);
    copied.textures.push_back(std::move(internal));
  }
  out = std::move(copied);
  return ARX_OK;
}

inline ArxReturnCode replaceGeometry(pistoris::Level& level, std::span<const ArxLevelVertex> source_vertices,
                                     std::span<const ArxLevelFace> source_faces,
                                     std::span<const ArxTextureView> source_textures = {}) {
  std::vector<float> positions;
  positions.reserve(source_vertices.size() * 3U);
  for (const ArxLevelVertex& vertex : source_vertices) {
    positions.insert(positions.end(), {vertex.position.x, vertex.position.y, vertex.position.z});
  }
  auto status = level.replaceVertices(positions);
  if (!status) return status.code();
  level.clearTextures();
  for (const ArxTextureView& texture : source_textures) {
    auto added = level.addTexture(texture);
    if (!added) return added.code();
  }

  std::vector<std::uint32_t> vertex_indices;
  std::vector<float> uvs;
  std::vector<float> corner_normals;
  std::vector<std::uint32_t> texture_indices;
  std::vector<float> transvals;
  std::vector<float> corner_colors;
  std::vector<float> face_normals;
  std::vector<pistoris::FaceType> flags;
  std::vector<pistoris::RoomIndex> rooms;
  bool all_face_normals_valid = true;
  for (const ArxLevelFace& face : source_faces) {
    const float norm = face.normal.x * face.normal.x + face.normal.y * face.normal.y + face.normal.z * face.normal.z;
    if (!std::isfinite(norm) || norm < 0.99f || norm > 1.01f) all_face_normals_valid = false;
  }
  vertex_indices.reserve(source_faces.size() * 3U);
  uvs.reserve(source_faces.size() * 6U);
  corner_normals.reserve(source_faces.size() * 9U);
  texture_indices.reserve(source_faces.size());
  transvals.reserve(source_faces.size());
  corner_colors.reserve(source_faces.size() * 9U);
  flags.reserve(source_faces.size());
  rooms.reserve(source_faces.size());
  if (all_face_normals_valid) face_normals.reserve(source_faces.size() * 3U);
  for (const ArxLevelFace& face : source_faces) {
    for (const ArxLevelCorner& corner : face.corners) {
      vertex_indices.push_back(corner.vertex);
      uvs.insert(uvs.end(), {corner.u, corner.v});
      corner_normals.insert(corner_normals.end(), {corner.normal.x, corner.normal.y, corner.normal.z});
      corner_colors.insert(corner_colors.end(), {corner.color.r, corner.color.g, corner.color.b});
    }
    texture_indices.push_back(face.texture);
    transvals.push_back(face.transval);
    if (all_face_normals_valid) {
      face_normals.insert(face_normals.end(), {face.normal.x, face.normal.y, face.normal.z});
    }
    flags.push_back(face.flags);
    rooms.push_back(face.room);
  }
  status = level.replaceFaces(
      vertex_indices, uvs, corner_normals, texture_indices, transvals, corner_colors, face_normals, flags);
  if (!status) return status.code();
  return level.replaceFaceRooms(rooms).code();
}

inline ArxReturnCode replaceAnchors(pistoris::Level& level, const AnchorsSnapshot& value) {
  std::vector<float> positions;
  std::vector<float> radii;
  std::vector<float> heights;
  std::vector<std::uint32_t> flags;
  positions.reserve(value.anchors.size() * 3U);
  radii.reserve(value.anchors.size());
  heights.reserve(value.anchors.size());
  flags.reserve(value.anchors.size());
  for (const pistoris::Anchor& anchor : value.anchors) {
    positions.insert(positions.end(), {anchor.position.x, anchor.position.y, anchor.position.z});
    radii.push_back(anchor.radius);
    heights.push_back(anchor.height);
    flags.push_back(anchor.flags);
  }
  auto status = level.replaceAnchors(positions, radii, heights, flags);
  if (!status) return status.code();

  std::vector<pistoris::AnchorIndex> endpoints;
  endpoints.reserve(value.connections.size() * 2U);
  for (const pistoris::AnchorConnection& connection : value.connections) {
    endpoints.push_back(connection.first);
    endpoints.push_back(connection.second);
  }
  return level.replaceAnchorConnections(endpoints).code();
}

inline ArxReturnCode replaceRoomDistances(pistoris::Level& level, std::span<const pistoris::RoomDistance> values) {
  std::vector<float> distances;
  std::vector<pistoris::PortalIndex> endpoints;
  distances.reserve(values.size());
  endpoints.reserve(values.size() * 2U);
  for (const pistoris::RoomDistance& value : values) {
    distances.push_back(value.distance);
    endpoints.push_back(value.low_room_portal);
    endpoints.push_back(value.high_room_portal);
  }
  return level.replaceRoomDistances(distances, endpoints).code();
}

inline ArxReturnCode setNavSurface(pistoris::Level& level, const pistoris::NavSurface& value) {
  std::vector<float> positions;
  positions.reserve(value.vertices.size() * 3U);
  for (const pistoris::Vertex& vertex : value.vertices)
    positions.insert(positions.end(), {vertex.position.x, vertex.position.y, vertex.position.z});
  std::vector<std::uint32_t> triangle_indices;
  triangle_indices.reserve(value.triangles.size() * 3U);
  for (const pistoris::NavSurfaceTriangle& triangle : value.triangles) {
    triangle_indices.insert(triangle_indices.end(), {triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]});
  }
  return level.setNavSurface(positions, triangle_indices).code();
}

inline ArxReturnCode setVertex(pistoris::Level& level, pistoris::VertexIndex index, const pistoris::Vertex& value) {
  return level.setVertex(index, detail::publicVertex(value)).code();
}

inline ArxReturnCode setTexture(pistoris::Level& level, pistoris::TextureIndex index, const pistoris::Texture& value) {
  return level.setTexture(index, detail::publicTexture(value)).code();
}

inline ArxReturnCode setRoom(pistoris::Level& level, pistoris::RoomIndex index, const pistoris::Room& value) {
  return level.setRoom(index, detail::publicRoom(value)).code();
}

inline ArxReturnCode setPortal(pistoris::Level& level, pistoris::PortalIndex index, const pistoris::Portal& value) {
  return level.setPortal(index, detail::publicPortal(value)).code();
}

inline ArxReturnCode setFace(pistoris::Level& level, pistoris::FaceIndex index, const pistoris::Face& value) {
  if (index >= level.faces().size()) return ARX_INDEX_OUT_OF_RANGE;
  const ArxLevelFace current = level.faces()[index];
  ArxLevelFace projected = detail::publicFace(value, current.room);
  const float normal_length = projected.normal.x * projected.normal.x + projected.normal.y * projected.normal.y +
                              projected.normal.z * projected.normal.z;
  if (!std::isfinite(normal_length) || normal_length < 0.99f || normal_length > 1.01f)
    projected.normal = current.normal;
  for (std::size_t corner = 0; corner < 3; ++corner) projected.corners[corner].color = current.corners[corner].color;
  return level.setFace(index, projected).code();
}

inline ArxReturnCode setAnchor(pistoris::Level& level, pistoris::AnchorIndex index, const pistoris::Anchor& value) {
  return level.setAnchor(index, detail::publicAnchor(value)).code();
}

inline ArxReturnCode setLight(pistoris::Level& level, pistoris::LightIndex index, const pistoris::Light& value) {
  return level.setLight(index, detail::publicLight(value)).code();
}

inline ArxReturnCode setPlayerSpawn(pistoris::Level& level, const pistoris::PlayerSpawn& value) {
  return level.setPlayerSpawn({value.position, value.rotation, 1}).code();
}

inline ArxReturnCode setEntity(pistoris::Level& level, pistoris::EntityIndex index, const pistoris::Entity& value) {
  return level.setEntity(index, detail::publicEntity(value)).code();
}

inline ArxReturnCode setFog(pistoris::Level& level, pistoris::FogIndex index, const pistoris::Fog& value) {
  return level.setFog(index, detail::publicFog(value)).code();
}

inline ArxReturnCode setZone(pistoris::Level& level, pistoris::ZoneIndex index, const pistoris::Zone& value) {
  const ArxLevelZoneInput input = detail::publicZone(value);
  return level.setZone(index, input).code();
}

inline ArxReturnCode setPath(pistoris::Level& level, pistoris::PathIndex index, const pistoris::Path& value) {
  std::vector<ArxLevelPathNode> nodes;
  const ArxLevelPathInput input = detail::publicPath(value, nodes);
  return level.setPath(index, input).code();
}

inline pistoris::Vertex vertex(const pistoris::Level& level, pistoris::VertexIndex index) {
  const ArxLevelVertex value = level.vertices()[index];
  return {value.position};
}

inline pistoris::Face face(const pistoris::Level& level, pistoris::FaceIndex index) {
  const ArxLevelFace value = level.faces()[index];
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
  const ArxTextureView value = level.textures()[index];
  pistoris::Texture out(detail::string(value.path));
  out.encoded_image = detail::bytes(value.encoded_image);
  out.external_image_extension = detail::string(value.external_image_extension);
  return out;
}

inline pistoris::Room room(const pistoris::Level& level, pistoris::RoomIndex index) {
  const ArxLevelRoom value = level.rooms()[index];
  return {detail::string(value.name)};
}

inline pistoris::Portal portal(const pistoris::Level& level, pistoris::PortalIndex index) {
  const ArxLevelPortal value = level.portals()[index];
  pistoris::Portal out{};
  out.name = detail::string(value.name);
  out.room_1 = value.room_front;
  out.room_2 = value.room_back;
  out.shape = static_cast<pistoris::PortalShape>(value.shape);
  for (std::size_t vertex = 0; vertex < out.vertices.size(); ++vertex) out.vertices[vertex] = value.vertices[vertex];
  return out;
}

inline pistoris::Anchor anchor(const pistoris::Level& level, pistoris::AnchorIndex index) {
  const ArxLevelAnchor value = level.anchors()[index];
  return {value.position, value.radius, value.height, value.flags, detail::string(value.name)};
}

inline pistoris::AnchorConnection anchorConnection(const pistoris::Level& level,
                                                   pistoris::AnchorConnectionIndex index) {
  const ArxLevelAnchorConnection value = level.anchorConnections()[index];
  return {value.first, value.second};
}

inline pistoris::Light light(const pistoris::Level& level, pistoris::LightIndex index) {
  const ArxLevelLight value = level.lights()[index];
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
  const ArxLevelEntity value = level.entities()[index];
  return {detail::string(value.class_path), value.ident, value.position, value.rotation, detail::string(value.name)};
}

inline pistoris::Fog fog(const pistoris::Level& level, pistoris::FogIndex index) {
  const ArxLevelFog value = level.fogs()[index];
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
  const ArxLevelZone value = level.zones()[index];
  pistoris::Zone out{};
  out.name = detail::string(value.name);
  out.reference_y = value.reference_y;
  out.height_mode = static_cast<pistoris::ZoneHeightMode>(value.height_mode);
  out.height = value.height;
  auto perimeter = level.zonePerimeter(index);
  if (perimeter) out.perimeter_xz.assign(perimeter->begin(), perimeter->end());
  if (value.has_color != 0) out.color = value.color;
  if (value.has_farclip != 0) out.farclip = value.farclip;
  if (value.has_ambiance != 0) {
    out.ambiance = pistoris::ZoneAmbiance{detail::string(value.ambiance.name), value.ambiance.volume};
  }
  return out;
}

inline pistoris::Path path(const pistoris::Level& level, pistoris::PathIndex index) {
  const ArxLevelPath value = level.paths()[index];
  pistoris::Path out{};
  out.name = detail::string(value.name);
  out.position = value.position;
  auto nodes = level.pathNodes(index);
  if (!nodes) return out;
  for (const ArxLevelPathNode& node : *nodes) {
    out.nodes.push_back({node.relative_position, static_cast<pistoris::PathNodeType>(node.type), node.time_ms});
  }
  return out;
}

inline std::vector<pistoris::Vertex> vertices(const pistoris::Level& level) {
  std::vector<pistoris::Vertex> out;
  out.reserve(level.vertices().size());
  for (const ArxLevelVertex value : level.vertices()) out.push_back({value.position});
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
  std::vector<pistoris::RoomDistance> out;
  out.reserve(level.roomDistances().size());
  for (const ArxLevelRoomDistance value : level.roomDistances()) {
    out.push_back({value.distance, value.portal_a, value.portal_b});
  }
  return out;
}

inline std::optional<ArxLevelRoomDistance> roomDistance(const pistoris::Level& level, pistoris::RoomIndex first,
                                                        pistoris::RoomIndex second) {
  auto result = level.roomDistance(first, second);
  if (!result) return std::nullopt;
  return *result;
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
  const auto vertices = level.navSurfaceVertices();
  const auto triangles = level.navSurfaceTriangles();
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
  return level.faces()[index].room;
}

inline std::vector<ArxColor3> cornerColors(const pistoris::Level& level) {
  const auto projected = level.faces();
  std::vector<ArxColor3> out;
  out.reserve(projected.size() * 3U);
  for (const ArxLevelFace& face : projected) {
    for (const ArxLevelCorner& corner : face.corners) out.push_back(corner.color);
  }
  return out;
}

inline ArxColor3 cornerColor(const pistoris::Level& level, pistoris::FaceIndex face_index, std::size_t corner_index) {
  return level.faces()[face_index].corners[corner_index].color;
}

inline ArxReturnCode copyAnchors(const pistoris::Level& level, AnchorsSnapshot& out) {
  AnchorsSnapshot copied;
  copied.anchors = anchors(level);
  copied.connections = anchorConnections(level);
  out = std::move(copied);
  return ARX_OK;
}

inline pistoris::VertexIndex addVertex(pistoris::Level& level, const pistoris::Vertex& value) {
  auto result = level.addVertex(detail::publicVertex(value));
  return result ? *result : pistoris::kInvalidVertexIndex;
}

inline pistoris::FaceIndex addFace(pistoris::Level& level, const pistoris::Face& value, pistoris::RoomIndex room) {
  ArxLevelFace projected = detail::publicFace(value, room);
  const float normal_length = projected.normal.x * projected.normal.x + projected.normal.y * projected.normal.y +
                              projected.normal.z * projected.normal.z;
  if (!std::isfinite(normal_length) || normal_length < 0.99f || normal_length > 1.01f) {
    const auto vertices = level.vertices();
    bool valid_indices = true;
    for (const ArxLevelCorner& corner : projected.corners) valid_indices &= corner.vertex < vertices.size();
    if (valid_indices) {
      const ArxVector3 a = vertices[projected.corners[0].vertex].position;
      const ArxVector3 b = vertices[projected.corners[1].vertex].position;
      const ArxVector3 c = vertices[projected.corners[2].vertex].position;
      const double abx = static_cast<double>(b.x) - a.x;
      const double aby = static_cast<double>(b.y) - a.y;
      const double abz = static_cast<double>(b.z) - a.z;
      const double acx = static_cast<double>(c.x) - a.x;
      const double acy = static_cast<double>(c.y) - a.y;
      const double acz = static_cast<double>(c.z) - a.z;
      const double nx = aby * acz - abz * acy;
      const double ny = abz * acx - abx * acz;
      const double nz = abx * acy - aby * acx;
      const double length = std::hypot(nx, ny, nz);
      if (length > 0.0 && std::isfinite(length))
        projected.normal = {
            static_cast<float>(nx / length), static_cast<float>(ny / length), static_cast<float>(nz / length)};
    }
  }
  auto result = level.addFace(projected);
  return result ? *result : pistoris::kInvalidFaceIndex;
}

inline pistoris::Corner corner(pistoris::VertexIndex vertex, ArxVector3 normal, float u = 0.0f, float v = 0.0f) {
  return {vertex, normal, u, v};
}

inline pistoris::RoomIndex addRoom(pistoris::Level& level, const pistoris::Room& value) {
  auto result = level.addRoom(detail::publicRoom(value));
  return result ? *result : pistoris::kInvalidRoomIndex;
}

inline pistoris::PortalIndex addPortal(pistoris::Level& level, const pistoris::Portal& value) {
  auto result = level.addPortal(detail::publicPortal(value));
  return result ? *result : pistoris::kInvalidPortalIndex;
}

inline pistoris::AnchorIndex addAnchor(pistoris::Level& level, const pistoris::Anchor& value) {
  auto result = level.addAnchor(detail::publicAnchor(value));
  return result ? *result : pistoris::kInvalidAnchorIndex;
}

inline pistoris::AnchorConnectionIndex addAnchorConnection(pistoris::Level& level, pistoris::AnchorConnection value) {
  const ArxLevelAnchorConnection connection{value.first, value.second};
  auto result = level.addAnchorConnection(connection);
  return result ? *result : pistoris::kInvalidAnchorConnectionIndex;
}

inline pistoris::LightIndex addLight(pistoris::Level& level, const pistoris::Light& value) {
  auto result = level.addLight(detail::publicLight(value));
  return result ? *result : pistoris::kInvalidLightIndex;
}

inline pistoris::EntityIndex addEntity(pistoris::Level& level, const pistoris::Entity& value) {
  auto result = level.addEntity(detail::publicEntity(value));
  return result ? *result : pistoris::kInvalidEntityIndex;
}

inline pistoris::FogIndex addFog(pistoris::Level& level, const pistoris::Fog& value) {
  auto result = level.addFog(detail::publicFog(value));
  return result ? *result : pistoris::kInvalidFogIndex;
}

inline pistoris::ZoneIndex addZone(pistoris::Level& level, const pistoris::Zone& value) {
  const ArxLevelZoneInput input = detail::publicZone(value);
  auto result = level.addZone(input);
  return result ? *result : pistoris::kInvalidZoneIndex;
}

inline pistoris::PathIndex addPath(pistoris::Level& level, const pistoris::Path& value) {
  std::vector<ArxLevelPathNode> nodes;
  const ArxLevelPathInput input = detail::publicPath(value, nodes);
  auto result = level.addPath(input);
  return result ? *result : pistoris::kInvalidPathIndex;
}

}  // namespace test
