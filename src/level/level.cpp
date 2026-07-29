// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "level.h"

#include "arx_pistoris/api.h"
#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/image.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/pistoris_types.h"

#include "level/data.h"
#include "level/validation.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "utils/math/bounds.h"
#include "utils/math/finite.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

template <typename Index>
bool validIndex(Index index, std::size_t size) noexcept {
  return static_cast<std::size_t>(index) < size;
}

template <typename Index>
bool canAppendIndex(std::size_t size) noexcept {
  return size < static_cast<std::size_t>(std::numeric_limits<Index>::max());
}

template <typename T>
ArxReturnCode validateCopyRange(std::size_t size, std::size_t offset, std::size_t count, T* out) noexcept {
  if (offset > size || count > size - offset) return ARX_INDEX_OUT_OF_RANGE;
  if (count != 0 && out == nullptr) return ARX_INVALID_DATA_POINTER;
  return ARX_OK;
}

ArxStringView borrowedString(const std::string& value) noexcept { return {value.data(), value.size()}; }

ArxEncodedImageView borrowedImage(const std::vector<std::uint8_t>& value) noexcept {
  return {value.data(), value.size()};
}

bool copyString(ArxStringView value, std::string& out) {
  if (value.size != 0 && value.data == nullptr) return false;
  out.assign(value.data == nullptr ? "" : value.data, value.size);
  return true;
}

bool copyImage(ArxEncodedImageView value, std::vector<std::uint8_t>& out) {
  if (value.size != 0 && value.data == nullptr) return false;
  if (value.size == 0) {
    out.clear();
    return true;
  }
  out.assign(value.data, value.data + value.size);
  return true;
}

bool validPortalShape(ArxPortalShape shape) noexcept {
  return shape == ARX_PORTAL_TRIANGLE || shape == ARX_PORTAL_QUAD;
}

bool validZoneHeightMode(ArxZoneHeightMode mode) noexcept {
  return mode == ARX_ZONE_HEIGHT_FINITE || mode == ARX_ZONE_HEIGHT_INFINITE;
}

bool validPathNodeTypes(const ArxLevelPathInput& path) noexcept {
  for (std::size_t i = 0; i < path.node_count; ++i) {
    const ArxPathNodeType type = path.nodes[i].type;
    if (type != ARX_PATH_NODE_STANDARD && type != ARX_PATH_NODE_BEZIER && type != ARX_PATH_NODE_CONTROL_POINT)
      return false;
  }
  return true;
}

Vertex internalVertex(const ArxLevelVertex& vertex) noexcept { return {vertex.position}; }

Face internalFace(const ArxLevelFace& face) noexcept {
  Face result;
  for (std::size_t corner = 0; corner < result.corners.size(); ++corner) {
    result.corners[corner] = {
        .vertex = face.corners[corner].vertex,
        .normal = face.corners[corner].normal,
        .u = face.corners[corner].u,
        .v = face.corners[corner].v,
    };
  }
  result.texture = face.texture;
  result.flags = face.flags;
  result.transval = face.transval;
  return result;
}

AnchorConnection internalConnection(ArxLevelAnchorConnection connection) noexcept {
  return {connection.first, connection.second};
}

bool internalTexture(const ArxLevelTextureView& texture, Texture& out) {
  return copyString(texture.path, out.path) && copyImage(texture.encoded_image, out.encoded_image);
}

bool internalRoom(const ArxLevelRoom& room, Room& out) { return copyString(room.name, out.name); }

bool internalPortal(const ArxLevelPortal& portal, Portal& out) {
  if (!copyString(portal.name, out.name)) return false;
  out.room_1 = portal.room_1;
  out.room_2 = portal.room_2;
  out.shape = static_cast<PortalShape>(portal.shape);
  out.vertices = {};
  const std::size_t vertex_count = portal.shape == ARX_PORTAL_TRIANGLE ? 3U : 4U;
  std::copy_n(portal.vertices, vertex_count, out.vertices.begin());
  return true;
}

bool internalAnchor(const ArxLevelAnchor& anchor, Anchor& out) {
  if (!copyString(anchor.name, out.name)) return false;
  out.position = anchor.position;
  out.radius = anchor.radius;
  out.height = anchor.height;
  out.flags = anchor.flags;
  return true;
}

bool internalLight(const ArxLevelLight& light, Light& out) {
  if (!copyString(light.name, out.name)) return false;
  out.position = light.position;
  out.color = light.color;
  out.fallstart = light.fallstart;
  out.fallend = light.fallend;
  out.intensity = light.intensity;
  out.flicker = light.flicker;
  out.effect_radius = light.effect_radius;
  out.effect_frequency = light.effect_frequency;
  out.effect_size = light.effect_size;
  out.effect_speed = light.effect_speed;
  out.flare_size = light.flare_size;
  out.flags = light.flags;
  return true;
}

bool internalEntity(const ArxLevelEntity& entity, Entity& out) {
  if (!copyString(entity.class_path, out.class_path) || !copyString(entity.name, out.name)) return false;
  out.ident = entity.ident;
  out.position = entity.position;
  out.rotation = entity.rotation;
  return true;
}

bool internalFog(const ArxLevelFog& fog, Fog& out) {
  if (!copyString(fog.name, out.name)) return false;
  out.position = fog.position;
  out.color = fog.color;
  out.size = fog.size;
  out.directional = fog.directional != 0;
  out.scale = fog.scale;
  out.rotation = fog.rotation;
  out.speed = fog.speed;
  out.rotate_speed = fog.rotate_speed;
  out.lifetime_ms = fog.lifetime_ms;
  out.frequency = fog.frequency;
  return true;
}

bool internalZone(const ArxLevelZoneInput& zone, Zone& out) {
  if (!copyString(zone.value.name, out.name)) return false;
  if (zone.value.perimeter_count != 0 && zone.perimeter_xz == nullptr) return false;
  if (zone.value.perimeter_count == 0)
    out.perimeter_xz.clear();
  else
    out.perimeter_xz.assign(zone.perimeter_xz, zone.perimeter_xz + zone.value.perimeter_count);
  out.reference_y = zone.value.reference_y;
  out.height_mode = static_cast<ZoneHeightMode>(zone.value.height_mode);
  out.height = zone.value.height;
  out.color = zone.value.has_color != 0 ? std::optional<ArxColor3>(zone.value.color) : std::nullopt;
  out.farclip = zone.value.has_farclip != 0 ? std::optional<float>(zone.value.farclip) : std::nullopt;
  out.ambiance.reset();
  if (zone.value.has_ambiance != 0) {
    ZoneAmbiance ambiance;
    if (!copyString(zone.value.ambiance.name, ambiance.name)) return false;
    ambiance.volume = zone.value.ambiance.volume;
    out.ambiance = std::move(ambiance);
  }
  return true;
}

bool internalPath(const ArxLevelPathInput& path, Path& out) {
  if (!copyString(path.name, out.name)) return false;
  if (path.node_count != 0 && path.nodes == nullptr) return false;
  out.position = path.position;
  out.nodes.resize(path.node_count);
  for (std::size_t i = 0; i < path.node_count; ++i) {
    out.nodes[i].relative_position = path.nodes[i].relative_position;
    out.nodes[i].type = static_cast<PathNodeType>(path.nodes[i].type);
    out.nodes[i].time_ms = path.nodes[i].time_ms;
  }
  return true;
}

ArxReturnCode validateMeshCoherence(const GeometryData& geometry, std::span<const RoomIndex> face_rooms,
                                    std::span<const ArxColor3> corner_colors, std::size_t room_count,
                                    LevelValidationState& state) noexcept {
  ArxAabb bounds;
  ArxReturnCode rc = level_validation::geometryError(geometry::validateVertices(geometry.vertices, &bounds));
  if (rc != ARX_OK) return rc;
  if (bounds.min.x < kLevelMinXZ || bounds.max.x > kLevelMaxXZ || bounds.min.z < kLevelMinXZ ||
      bounds.max.z > kLevelMaxXZ)
    return ARX_LEVEL_VERTEX_OUT_OF_BOUNDS;
  rc = level_validation::geometryError(geometry::validateTextures(geometry.textures));
  if (rc != ARX_OK) return rc;
  ArxAabb referenced_bounds;
  rc = level_validation::geometryError(
      geometry::validateFaces(geometry.faces, geometry.vertices, geometry.textures.size(), &referenced_bounds));
  if (rc != ARX_OK) return rc;
  rc = level_validation::faceTypes(geometry.faces);
  if (rc != ARX_OK) return rc;
  rc = level_validation::roomsError(rooms::validateFaceRooms(face_rooms, geometry.faces.size(), room_count));
  if (rc != ARX_OK) return rc;
  rc = level_validation::lightingError(lights::validateCornerColors(corner_colors, geometry.faces.size()));
  if (rc != ARX_OK) return rc;

  state = {};
  level_validation::markValid(state,
                              LevelValidation::kVertices | LevelValidation::kTextures | LevelValidation::kFaces |
                                  LevelValidation::kFaceRooms | LevelValidation::kCornerColors);
  state.derived.bounds = bounds;
  state.derived.referenced_bounds = referenced_bounds;
  return ARX_OK;
}

bool strictlyContains(const ArxAabb& bounds, const ArxVector3& point) noexcept {
  return point.x > bounds.min.x && point.x < bounds.max.x && point.y > bounds.min.y && point.y < bounds.max.y &&
         point.z > bounds.min.z && point.z < bounds.max.z;
}

bool validLevelPosition(const ArxVector3& position) noexcept {
  return math::finite(position) && position.x >= kLevelMinXZ && position.x <= kLevelMaxXZ &&
         position.z >= kLevelMinXZ && position.z <= kLevelMaxXZ;
}

bool geometryWeldOptions(const Level::VertexWeldOptions& options, geometry::VertexWeldOptions& out) noexcept {
  out.radius = options.radius;
  switch (options.metric) {
    case Level::PositionWeldMetric::kEuclidean:
      out.metric = geometry::PositionWeldMetric::kEuclidean;
      break;
    case Level::PositionWeldMetric::kAxisAligned:
      out.metric = geometry::PositionWeldMetric::kAxisAligned;
      break;
    default:
      return false;
  }
  switch (options.degenerate_faces) {
    case Level::DegenerateFacePolicy::kPreserve:
      out.degenerate_faces = geometry::DegenerateFacePolicy::kPreserve;
      break;
    case Level::DegenerateFacePolicy::kReject:
      out.degenerate_faces = geometry::DegenerateFacePolicy::kReject;
      break;
    case Level::DegenerateFacePolicy::kDiscard:
      out.degenerate_faces = geometry::DegenerateFacePolicy::kDiscard;
      break;
    default:
      return false;
  }
  return true;
}

bool faceReferencesRoom(const RoomsData& rooms, RoomIndex room) noexcept {
  return std::any_of(
      rooms.face_rooms.begin(), rooms.face_rooms.end(), [room](RoomIndex face_room) { return face_room == room; });
}

bool asciiInsensitiveEqual(std::string_view lhs, std::string_view rhs) noexcept {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    const char left = lhs[i] >= 'A' && lhs[i] <= 'Z' ? static_cast<char>(lhs[i] - 'A' + 'a') : lhs[i];
    const char right = rhs[i] >= 'A' && rhs[i] <= 'Z' ? static_cast<char>(rhs[i] - 'A' + 'a') : rhs[i];
    if (left != right) return false;
  }
  return true;
}

template <class Value>
bool nameAvailable(std::span<const Value> values, std::string_view name, std::size_t ignored,
                   bool ascii_insensitive = false) noexcept {
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i == ignored) continue;
    const std::string_view current = values[i].name;
    if (ascii_insensitive ? asciiInsensitiveEqual(current, name) : current == name) return false;
  }
  return true;
}

bool roomNameAvailable(const RoomsData& rooms, std::string_view name, RoomIndex ignored = kInvalidRoomIndex) noexcept {
  return nameAvailable<Room>(rooms.definitions, name, static_cast<std::size_t>(ignored));
}

bool portalReferencesRoom(const RoomsData& rooms, PortalIndex portal, RoomIndex room) noexcept {
  if (!validIndex(portal, rooms.portals.size())) return false;
  const Portal& value = rooms.portals[portal];
  return value.room_1 == room || value.room_2 == room;
}

ArxReturnCode internalRoomDistance(const ArxLevelRoomDistance& value, const RoomsData& rooms, RoomIndex& low_room,
                                   RoomIndex& high_room, RoomDistance& out) noexcept {
  if (!validIndex(value.room_a, rooms.definitions.size()) || !validIndex(value.room_b, rooms.definitions.size()))
    return ARX_LEVEL_BAD_ROOM_DISTANCE;
  if (value.room_a == value.room_b || !math::finite(value.distance)) return ARX_LEVEL_BAD_ROOM_DISTANCE;

  PortalIndex portal_a = value.portal_a;
  PortalIndex portal_b = value.portal_b;
  if (value.distance > 0.0f) {
    const bool supplied =
        portalReferencesRoom(rooms, portal_a, value.room_a) && portalReferencesRoom(rooms, portal_b, value.room_b);
    const bool swapped =
        portalReferencesRoom(rooms, portal_a, value.room_b) && portalReferencesRoom(rooms, portal_b, value.room_a);
    if (!supplied && !swapped) return ARX_LEVEL_BAD_ROOM_DISTANCE;
    if (!supplied) std::swap(portal_a, portal_b);
  } else {
    const bool invalid = portal_a == kInvalidPortalIndex && portal_b == kInvalidPortalIndex;
    if (!invalid && (portal_a != portal_b || !validIndex(portal_a, rooms.portals.size()) ||
                     !rooms::connectsRooms(rooms.portals[portal_a], value.room_a, value.room_b)))
      return ARX_LEVEL_BAD_ROOM_DISTANCE;
  }

  low_room = value.room_a;
  high_room = value.room_b;
  if (low_room > high_room) {
    std::swap(low_room, high_room);
    std::swap(portal_a, portal_b);
  }
  out = {.distance = value.distance, .low_room_portal = portal_a, .high_room_portal = portal_b};
  return ARX_OK;
}

bool pathNameAvailable(const SceneData& scene, std::string_view name, PathIndex ignored = kInvalidPathIndex) noexcept {
  return nameAvailable<Path>(scene.paths, name, static_cast<std::size_t>(ignored), true);
}

bool anchorNameAvailable(const NavigationData& navigation, std::string_view name,
                         AnchorIndex ignored = kInvalidAnchorIndex) noexcept {
  return name.empty() || nameAvailable<Anchor>(navigation.anchors, name, static_cast<std::size_t>(ignored));
}

bool fogNameAvailable(const SceneData& scene, std::string_view name, FogIndex ignored = kInvalidFogIndex) noexcept {
  return name.empty() || nameAvailable<Fog>(scene.fogs, name, static_cast<std::size_t>(ignored));
}

void remapRoomsAfterRemoval(RoomsData& rooms, RoomIndex removed) {
  rooms.portals.erase(
      std::remove_if(rooms.portals.begin(),
                     rooms.portals.end(),
                     [removed](const Portal& portal) { return portal.room_1 == removed || portal.room_2 == removed; }),
      rooms.portals.end());
  for (Portal& portal : rooms.portals) {
    if (portal.room_1 > removed) --portal.room_1;
    if (portal.room_2 > removed) --portal.room_2;
  }
  for (RoomIndex& room : rooms.face_rooms) {
    if (room > removed) --room;
  }
  rooms.distances.clear();
}

bool anchorConnectionLess(const AnchorConnection& lhs, const AnchorConnection& rhs) noexcept {
  if (lhs.first != rhs.first) return lhs.first < rhs.first;
  return lhs.second < rhs.second;
}

bool samePortalTopology(const Portal& lhs, const Portal& rhs) noexcept {
  if (lhs.room_1 != rhs.room_1 || lhs.room_2 != rhs.room_2 || lhs.shape != rhs.shape) return false;
  const std::size_t count = rooms::portalVertexCount(lhs.shape);
  for (std::size_t i = 0; i < count; ++i) {
    if (lhs.vertices[i].x != rhs.vertices[i].x || lhs.vertices[i].y != rhs.vertices[i].y ||
        lhs.vertices[i].z != rhs.vertices[i].z)
      return false;
  }
  return true;
}

bool sameAnchorConnection(const AnchorConnection& lhs, const AnchorConnection& rhs) noexcept {
  return lhs.first == rhs.first && lhs.second == rhs.second;
}

std::size_t sortedAnchorConnectionIndex(const std::vector<AnchorConnection>& connections,
                                        const AnchorConnection& connection) {
  return static_cast<std::size_t>(
      std::lower_bound(connections.begin(), connections.end(), connection, anchorConnectionLess) - connections.begin());
}

void remapAnchorsAfterRemoval(NavigationData& navigation, AnchorIndex removed) {
  navigation.connections.erase(
      std::remove_if(navigation.connections.begin(),
                     navigation.connections.end(),
                     [removed](const AnchorConnection& c) { return c.first == removed || c.second == removed; }),
      navigation.connections.end());
  for (AnchorConnection& connection : navigation.connections) {
    if (connection.first > removed) --connection.first;
    if (connection.second > removed) --connection.second;
  }
}

}  // namespace

Level::Level() : data_(std::make_unique<Data>()) {}

Level::~Level() = default;

Level::Level(const Level& other) : data_(std::make_unique<Data>(*other.data_)) {}

Level& Level::operator=(const Level& other) {
  if (this != &other) {
    Level copy(other);
    swap(copy);
  }
  return *this;
}

void Level::swap(Level& other) noexcept { data_.swap(other.data_); }

void Level::reset() {
  Level empty;
  swap(empty);
}

ArxReturnCode Level::validateMesh() const { return level_validation::mesh(*data_, data_->validation); }

ArxReturnCode Level::validate() const { return level_validation::all(*data_, data_->validation); }

ArxReturnCode Level::validateVertices() const { return level_validation::vertices(*data_, data_->validation); }

ArxReturnCode Level::validateTextures() const { return level_validation::textures(*data_, data_->validation); }

ArxReturnCode Level::validateFaces() const { return level_validation::faces(*data_, data_->validation); }

ArxReturnCode Level::validateFaceRooms() const { return level_validation::faceRooms(*data_, data_->validation); }

ArxReturnCode Level::validateCornerColors() const { return level_validation::cornerColors(*data_, data_->validation); }

ArxReturnCode Level::validateRooms() const { return level_validation::rooms(*data_, data_->validation); }

ArxReturnCode Level::validatePortals() const { return level_validation::portals(*data_, data_->validation); }

ArxReturnCode Level::validateRoomDistances() const {
  return level_validation::roomDistances(*data_, data_->validation);
}

ArxReturnCode Level::validateNavSurface() const { return level_validation::navSurface(*data_, data_->validation); }

ArxReturnCode Level::validateAnchors() const { return level_validation::anchors(*data_, data_->validation); }

ArxReturnCode Level::validateAnchorConnections() const {
  return level_validation::anchorConnections(*data_, data_->validation);
}

ArxReturnCode Level::validateLights() const { return level_validation::lightSources(*data_, data_->validation); }

ArxReturnCode Level::validatePlayerSpawn() const { return level_validation::playerSpawn(*data_, data_->validation); }

ArxReturnCode Level::validateEntities() const { return level_validation::entities(*data_, data_->validation); }

ArxReturnCode Level::validateFogs() const { return level_validation::fogs(*data_, data_->validation); }

ArxReturnCode Level::validateZones() const { return level_validation::zones(*data_, data_->validation); }

ArxReturnCode Level::validatePaths() const { return level_validation::paths(*data_, data_->validation); }

std::optional<ArxAabb> Level::bounds() const {
  if (validateVertices() != ARX_OK) return std::nullopt;
  return data_->validation.derived.bounds;
}

std::optional<ArxAabb> Level::referencedBounds() const {
  if (validateFaces() != ARX_OK) return std::nullopt;
  return data_->validation.derived.referenced_bounds;
}

std::size_t Level::vertexCount() const noexcept { return data_->geometry.vertices.size(); }

std::size_t Level::faceCount() const noexcept { return data_->geometry.faces.size(); }

std::size_t Level::textureCount() const noexcept { return data_->geometry.textures.size(); }

std::size_t Level::roomCount() const noexcept { return data_->rooms.definitions.size(); }

std::size_t Level::portalCount() const noexcept { return data_->rooms.portals.size(); }

std::size_t Level::roomDistanceCount() const noexcept { return data_->rooms.distances.size(); }

std::size_t Level::anchorCount() const noexcept { return data_->navigation.anchors.size(); }

std::size_t Level::anchorConnectionCount() const noexcept { return data_->navigation.connections.size(); }

std::size_t Level::lightCount() const noexcept { return data_->lighting.lights.size(); }

std::size_t Level::entityCount() const noexcept { return data_->scene.entities.size(); }

std::size_t Level::fogCount() const noexcept { return data_->scene.fogs.size(); }

std::size_t Level::zoneCount() const noexcept { return data_->scene.zones.size(); }

std::size_t Level::pathCount() const noexcept { return data_->scene.paths.size(); }

ArxReturnCode Level::copyVertices(std::size_t offset, std::size_t count, ArxLevelVertex* out_vertices) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->geometry.vertices.size(), offset, count, out_vertices);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) out_vertices[i].position = data_->geometry.vertices[offset + i].position;
  return ARX_OK;
}

ArxReturnCode Level::copyFaces(std::size_t offset, std::size_t count, ArxLevelFace* out_faces) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->geometry.faces.size(), offset, count, out_faces);
  if (rc != ARX_OK) return rc;
  if (data_->rooms.face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
  const bool has_colors = !data_->lighting.corner_colors.empty();
  if (has_colors && data_->lighting.corner_colors.size() != lights::expectedCornerColorCount(data_->geometry))
    return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t face_index = offset + i;
    const Face& source = data_->geometry.faces[face_index];
    ArxLevelFace& target = out_faces[i];
    target = {};
    for (std::size_t corner = 0; corner < source.corners.size(); ++corner) {
      target.corners[corner].vertex = source.corners[corner].vertex;
      target.corners[corner].normal = source.corners[corner].normal;
      target.corners[corner].u = source.corners[corner].u;
      target.corners[corner].v = source.corners[corner].v;
      target.corners[corner].color =
          has_colors
              ? data_->lighting.corner_colors[lights::cornerColorIndex(static_cast<FaceIndex>(face_index), corner)]
              : lights::kDefaultCornerColor;
    }
    target.texture = source.texture;
    target.room = data_->rooms.face_rooms[face_index];
    target.flags = source.flags;
    target.transval = source.transval;
    target.has_corner_colors = has_colors ? 1U : 0U;
  }
  return ARX_OK;
}

ArxReturnCode Level::copyTextureViews(std::size_t offset, std::size_t count,
                                      ArxLevelTextureView* out_views) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->geometry.textures.size(), offset, count, out_views);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Texture& texture = data_->geometry.textures[offset + i];
    out_views[i].path = borrowedString(texture.path);
    out_views[i].encoded_image = borrowedImage(texture.encoded_image);
  }
  return ARX_OK;
}

ArxReturnCode Level::copyRooms(std::size_t offset, std::size_t count, ArxLevelRoom* out_rooms) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->rooms.definitions.size(), offset, count, out_rooms);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) out_rooms[i].name = borrowedString(data_->rooms.definitions[offset + i].name);
  return ARX_OK;
}

ArxReturnCode Level::copyPortals(std::size_t offset, std::size_t count, ArxLevelPortal* out_portals) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->rooms.portals.size(), offset, count, out_portals);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Portal& source = data_->rooms.portals[offset + i];
    ArxLevelPortal& target = out_portals[i];
    target.name = borrowedString(source.name);
    target.room_1 = source.room_1;
    target.room_2 = source.room_2;
    target.shape = static_cast<ArxPortalShape>(source.shape);
    std::copy(source.vertices.begin(), source.vertices.end(), target.vertices);
  }
  return ARX_OK;
}

ArxReturnCode Level::copyRoomDistances(std::size_t offset, std::size_t count,
                                       ArxLevelRoomDistance* out_distances) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->rooms.distances.size(), offset, count, out_distances);
  if (rc != ARX_OK) return rc;
  std::size_t second = 1;
  std::size_t first = offset;
  while (second < data_->rooms.definitions.size() && first >= second) {
    first -= second;
    ++second;
  }
  for (std::size_t i = 0; i < count; ++i) {
    const RoomDistance& source = data_->rooms.distances[offset + i];
    ArxLevelRoomDistance& target = out_distances[i];
    target.room_a = static_cast<RoomIndex>(first);
    target.room_b = static_cast<RoomIndex>(second);
    target.distance = source.distance;
    target.portal_a = source.low_room_portal;
    target.portal_b = source.high_room_portal;
    if (++first == second) {
      first = 0;
      ++second;
    }
  }
  return ARX_OK;
}

ArxReturnCode Level::copyAnchors(std::size_t offset, std::size_t count, ArxLevelAnchor* out_anchors) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->navigation.anchors.size(), offset, count, out_anchors);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Anchor& source = data_->navigation.anchors[offset + i];
    ArxLevelAnchor& target = out_anchors[i];
    target.position = source.position;
    target.radius = source.radius;
    target.height = source.height;
    target.flags = source.flags;
    target.name = borrowedString(source.name);
  }
  return ARX_OK;
}

ArxReturnCode Level::copyAnchorConnections(std::size_t offset, std::size_t count,
                                           ArxLevelAnchorConnection* out_connections) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->navigation.connections.size(), offset, count, out_connections);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const AnchorConnection& source = data_->navigation.connections[offset + i];
    out_connections[i] = {source.first, source.second};
  }
  return ARX_OK;
}

ArxLevelNavSurfaceInfo Level::navSurfaceInfo() const noexcept {
  return {
      .has_surface = static_cast<std::uint8_t>(data_->navigation.surface ? 1U : 0U),
      .vertex_count = data_->navigation.surface ? data_->navigation.surface->vertices.size() : 0U,
      .triangle_count = data_->navigation.surface ? data_->navigation.surface->triangles.size() : 0U,
  };
}

ArxReturnCode Level::copyNavSurfaceVertices(std::size_t offset, std::size_t count,
                                            ArxLevelVertex* out_vertices) const noexcept {
  if (!data_->navigation.surface) return validateCopyRange(0, offset, count, out_vertices);
  const NavSurface& surface = *data_->navigation.surface;
  ArxReturnCode rc = validateCopyRange(surface.vertices.size(), offset, count, out_vertices);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) out_vertices[i].position = surface.vertices[offset + i].position;
  return ARX_OK;
}

ArxReturnCode Level::copyNavSurfaceTriangles(std::size_t offset, std::size_t count,
                                             ArxLevelNavSurfaceTriangle* out_triangles) const noexcept {
  if (!data_->navigation.surface) return validateCopyRange(0, offset, count, out_triangles);
  const NavSurface& surface = *data_->navigation.surface;
  ArxReturnCode rc = validateCopyRange(surface.triangles.size(), offset, count, out_triangles);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const NavSurfaceTriangle& source = surface.triangles[offset + i];
    std::copy(source.vertices.begin(), source.vertices.end(), out_triangles[i].vertices);
  }
  return ARX_OK;
}

ArxReturnCode Level::copyLights(std::size_t offset, std::size_t count, ArxLevelLight* out_lights) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->lighting.lights.size(), offset, count, out_lights);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Light& source = data_->lighting.lights[offset + i];
    ArxLevelLight& target = out_lights[i];
    target.name = borrowedString(source.name);
    target.position = source.position;
    target.color = source.color;
    target.fallstart = source.fallstart;
    target.fallend = source.fallend;
    target.intensity = source.intensity;
    target.flicker = source.flicker;
    target.effect_radius = source.effect_radius;
    target.effect_frequency = source.effect_frequency;
    target.effect_size = source.effect_size;
    target.effect_speed = source.effect_speed;
    target.flare_size = source.flare_size;
    target.flags = source.flags;
  }
  return ARX_OK;
}

ArxLevelPlayerSpawn Level::playerSpawn() const noexcept {
  return {
      .position = data_->scene.player_spawn.position,
      .rotation = data_->scene.player_spawn.rotation,
      .is_usable = static_cast<std::uint8_t>(data_->scene.player_spawn_is_fallback ? 0U : 1U),
  };
}

ArxReturnCode Level::copyEntities(std::size_t offset, std::size_t count, ArxLevelEntity* out_entities) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->scene.entities.size(), offset, count, out_entities);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Entity& source = data_->scene.entities[offset + i];
    ArxLevelEntity& target = out_entities[i];
    target.class_path = borrowedString(source.class_path);
    target.ident = source.ident;
    target.position = source.position;
    target.rotation = source.rotation;
    target.name = borrowedString(source.name);
  }
  return ARX_OK;
}

ArxReturnCode Level::copyFogs(std::size_t offset, std::size_t count, ArxLevelFog* out_fogs) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->scene.fogs.size(), offset, count, out_fogs);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Fog& source = data_->scene.fogs[offset + i];
    ArxLevelFog& target = out_fogs[i];
    target.position = source.position;
    target.color = source.color;
    target.size = source.size;
    target.directional = source.directional ? 1U : 0U;
    target.scale = source.scale;
    target.rotation = source.rotation;
    target.speed = source.speed;
    target.rotate_speed = source.rotate_speed;
    target.lifetime_ms = source.lifetime_ms;
    target.frequency = source.frequency;
    target.name = borrowedString(source.name);
  }
  return ARX_OK;
}

ArxReturnCode Level::copyZones(std::size_t offset, std::size_t count, ArxLevelZone* out_zones) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->scene.zones.size(), offset, count, out_zones);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Zone& source = data_->scene.zones[offset + i];
    ArxLevelZone& target = out_zones[i];
    target = {};
    target.name = borrowedString(source.name);
    target.perimeter_count = source.perimeter_xz.size();
    target.reference_y = source.reference_y;
    target.height_mode =
        source.height_mode == ZoneHeightMode::kInfinite ? ARX_ZONE_HEIGHT_INFINITE : ARX_ZONE_HEIGHT_FINITE;
    target.height = source.height;
    const auto& color = source.color;
    target.has_color = color ? 1U : 0U;
    if (color) target.color = *color;
    const auto& farclip = source.farclip;
    target.has_farclip = farclip ? 1U : 0U;
    if (farclip) target.farclip = *farclip;
    const auto& ambiance = source.ambiance;
    target.has_ambiance = ambiance ? 1U : 0U;
    if (ambiance) {
      target.ambiance.name = borrowedString(ambiance->name);
      target.ambiance.volume = ambiance->volume;
    }
  }
  return ARX_OK;
}

ArxReturnCode Level::copyZonePerimeter(ZoneIndex zone, std::size_t offset, std::size_t count,
                                       ArxVector2* out_points) const noexcept {
  if (!validIndex(zone, data_->scene.zones.size())) return ARX_INDEX_OUT_OF_RANGE;
  const std::vector<ArxVector2>& perimeter = data_->scene.zones[static_cast<std::size_t>(zone)].perimeter_xz;
  ArxReturnCode rc = validateCopyRange(perimeter.size(), offset, count, out_points);
  if (rc != ARX_OK) return rc;
  std::copy_n(perimeter.begin() + static_cast<std::ptrdiff_t>(offset), count, out_points);
  return ARX_OK;
}

ArxReturnCode Level::copyPaths(std::size_t offset, std::size_t count, ArxLevelPath* out_paths) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->scene.paths.size(), offset, count, out_paths);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Path& source = data_->scene.paths[offset + i];
    ArxLevelPath& target = out_paths[i];
    target.name = borrowedString(source.name);
    target.position = source.position;
    target.node_count = source.nodes.size();
  }
  return ARX_OK;
}

ArxReturnCode Level::copyPathNodes(PathIndex path, std::size_t offset, std::size_t count,
                                   ArxLevelPathNode* out_nodes) const noexcept {
  if (!validIndex(path, data_->scene.paths.size())) return ARX_INDEX_OUT_OF_RANGE;
  const std::vector<PathNode>& nodes = data_->scene.paths[static_cast<std::size_t>(path)].nodes;
  ArxReturnCode rc = validateCopyRange(nodes.size(), offset, count, out_nodes);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const PathNode& source = nodes[offset + i];
    ArxLevelPathNode& target = out_nodes[i];
    target.relative_position = source.relative_position;
    target.type = static_cast<ArxPathNodeType>(source.type);
    target.time_ms = source.time_ms;
  }
  return ARX_OK;
}

ArxReturnCode Level::getRoomDistance(RoomIndex room_a, RoomIndex room_b, std::uint8_t& out_has_distance,
                                     ArxLevelRoomDistance& out_distance) const noexcept {
  if (!validIndex(room_a, data_->rooms.definitions.size()) || !validIndex(room_b, data_->rooms.definitions.size()))
    return ARX_INDEX_OUT_OF_RANGE;
  out_has_distance = 0;
  out_distance.room_a = std::min(room_a, room_b);
  out_distance.room_b = std::max(room_a, room_b);
  out_distance.distance = -1.0f;
  out_distance.portal_a = kInvalidPortalIndex;
  out_distance.portal_b = kInvalidPortalIndex;
  if (room_a == room_b || !rooms::hasCompleteRoomDistances(data_->rooms.distances, data_->rooms.definitions.size()))
    return ARX_OK;
  const std::size_t index = rooms::roomDistancePairIndex(out_distance.room_a, out_distance.room_b);
  const RoomDistance& source = data_->rooms.distances[index];
  out_distance.distance = source.distance;
  out_distance.portal_a = source.low_room_portal;
  out_distance.portal_b = source.high_room_portal;
  out_has_distance = 1;
  return ARX_OK;
}

ArxReturnCode Level::setVertex(VertexIndex index, ArxLevelVertex value) noexcept {
  const Vertex vertex = internalVertex(value);
  if (!validIndex(index, data_->geometry.vertices.size())) return ARX_INDEX_OUT_OF_RANGE;
  Vertex& current = data_->geometry.vertices[static_cast<std::size_t>(index)];
  if (current.position == vertex.position) return ARX_OK;

  const bool preserve_vertex_validity = level_validation::has(data_->validation, LevelValidation::kVertices) &&
                                        geometry::validateVertex(vertex) == geometry::Error::kNone &&
                                        validLevelPosition(vertex.position);
  const auto& cached_bounds = data_->validation.derived.bounds;
  const bool preserve_bounds =
      preserve_vertex_validity && cached_bounds && strictlyContains(*cached_bounds, current.position);
  std::optional<ArxAabb> bounds = preserve_bounds ? cached_bounds : std::nullopt;
  if (bounds) math::expand(*bounds, vertex.position);

  current = vertex;
  level_validation::invalidate(data_->validation, LevelValidation::kVertices);
  if (preserve_vertex_validity) level_validation::markValid(data_->validation, LevelValidation::kVertices);
  if (bounds) data_->validation.derived.bounds = *bounds;
  return ARX_OK;
}

ArxReturnCode Level::addVertex(ArxLevelVertex vertex, VertexIndex& out_index) {
  return addVertices(&vertex, 1, out_index);
}

ArxReturnCode Level::addVertices(const ArxLevelVertex* vertices, std::size_t count, VertexIndex& out_first_index) {
  out_first_index = kInvalidVertexIndex;
  if (count == 0) return ARX_INVALID_OPTIONS;
  if (vertices == nullptr) return ARX_INVALID_DATA_POINTER;
  if (count > static_cast<std::size_t>(kInvalidVertexIndex) - data_->geometry.vertices.size())
    return ARX_LEVEL_TOO_MANY_VERTICES;

  std::vector<Vertex> next;
  next.reserve(count);
  for (std::size_t i = 0; i < count; ++i) next.push_back(internalVertex(vertices[i]));
  for (const Vertex& vertex : next) {
    ArxReturnCode rc = level_validation::geometryError(geometry::validateVertex(vertex));
    if (rc != ARX_OK) return rc;
    if (!validLevelPosition(vertex.position)) return ARX_LEVEL_VERTEX_OUT_OF_BOUNDS;
  }

  const VertexIndex first = static_cast<VertexIndex>(data_->geometry.vertices.size());
  const bool had_vertices = !data_->geometry.vertices.empty();
  const bool vertices_stay_valid =
      !had_vertices || (level_validation::has(data_->validation, LevelValidation::kVertices) &&
                        data_->validation.derived.bounds.has_value());
  std::optional<ArxAabb> bounds = vertices_stay_valid && had_vertices ? data_->validation.derived.bounds : std::nullopt;
  for (const Vertex& vertex : next) {
    if (!bounds)
      bounds = ArxAabb{vertex.position, vertex.position};
    else
      math::expand(*bounds, vertex.position);
  }

  data_->geometry.vertices.insert(data_->geometry.vertices.end(), next.begin(), next.end());
  if (vertices_stay_valid) {
    level_validation::markValid(data_->validation, LevelValidation::kVertices);
    data_->validation.derived.bounds = bounds;
  }
  out_first_index = first;
  return ARX_OK;
}

ArxReturnCode Level::setFace(FaceIndex index, const ArxLevelFace& value) {
  if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
  if (data_->rooms.face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
  if (!validIndex(value.room, data_->rooms.definitions.size())) return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
  const std::size_t expected_colors = lights::expectedCornerColorCount(data_->geometry);
  if (!data_->lighting.corner_colors.empty() && data_->lighting.corner_colors.size() != expected_colors)
    return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

  const Face face = internalFace(value);
  ArxReturnCode rc = level_validation::geometryError(
      geometry::validateFaceReferences(face, data_->geometry.vertices.size(), data_->geometry.textures.size()));
  if (rc != ARX_OK) return rc;
  rc = level_validation::faceTypes(std::span<const Face>(&face, 1));
  if (rc != ARX_OK) return rc;
  if (value.has_corner_colors != 0) {
    for (const ArxLevelCorner& corner : value.corners) {
      rc = level_validation::lightingError(lights::validateCornerColor(corner.color));
      if (rc != ARX_OK) return rc;
    }
    if (data_->lighting.corner_colors.empty())
      data_->lighting.corner_colors.assign(expected_colors, lights::kDefaultCornerColor);
  }

  data_->geometry.faces[static_cast<std::size_t>(index)] = face;
  data_->rooms.face_rooms[static_cast<std::size_t>(index)] = value.room;
  if (!data_->lighting.corner_colors.empty()) {
    for (std::size_t corner = 0; corner < 3; ++corner) {
      data_->lighting.corner_colors[lights::cornerColorIndex(index, corner)] =
          value.has_corner_colors != 0 ? value.corners[corner].color : lights::kDefaultCornerColor;
    }
  }
  level_validation::invalidate(data_->validation, LevelValidation::kFaces);
  return ARX_OK;
}

ArxReturnCode Level::addFace(const ArxLevelFace& value, FaceIndex& out_index) {
  out_index = kInvalidFaceIndex;
  if (!validIndex(value.room, data_->rooms.definitions.size())) return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
  if (data_->geometry.faces.size() >= static_cast<std::size_t>(kInvalidFaceIndex)) return ARX_LEVEL_TOO_MANY_FACES;
  if (data_->rooms.face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
  if (!data_->lighting.corner_colors.empty() &&
      data_->lighting.corner_colors.size() != lights::expectedCornerColorCount(data_->geometry))
    return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

  const Face face = internalFace(value);
  ArxAabb face_bounds;
  const geometry::Error error = geometry::validateFaces(
      std::span<const Face>(&face, 1), data_->geometry.vertices, data_->geometry.textures.size(), &face_bounds);
  ArxReturnCode rc = level_validation::geometryError(error);
  if (rc != ARX_OK) return rc;
  rc = level_validation::faceTypes(std::span<const Face>(&face, 1));
  if (rc != ARX_OK) return rc;
  if (value.has_corner_colors != 0) {
    for (const ArxLevelCorner& corner : value.corners) {
      rc = level_validation::lightingError(lights::validateCornerColor(corner.color));
      if (rc != ARX_OK) return rc;
    }
  }

  const bool materialize_colors = value.has_corner_colors != 0 && data_->lighting.corner_colors.empty();
  std::vector<ArxColor3> materialized_colors;
  if (materialize_colors) {
    materialized_colors.assign(lights::expectedCornerColorCount(data_->geometry), lights::kDefaultCornerColor);
    materialized_colors.reserve(materialized_colors.size() + 3U);
  }
  data_->geometry.faces.reserve(data_->geometry.faces.size() + 1U);
  data_->rooms.face_rooms.reserve(data_->rooms.face_rooms.size() + 1U);
  if (!data_->lighting.corner_colors.empty())
    data_->lighting.corner_colors.reserve(data_->lighting.corner_colors.size() + 3U);
  else if (materialize_colors)
    data_->lighting.corner_colors = std::move(materialized_colors);

  const FaceIndex index = static_cast<FaceIndex>(data_->geometry.faces.size());
  std::optional<ArxAabb> referenced_bounds = level_validation::has(data_->validation, LevelValidation::kFaces)
                                                 ? data_->validation.derived.referenced_bounds
                                                 : std::nullopt;
  const bool faces_stay_valid = referenced_bounds.has_value();
  const bool face_rooms_stay_valid = level_validation::has(data_->validation, LevelValidation::kFaceRooms);
  const bool colors_stay_valid = level_validation::has(data_->validation, LevelValidation::kCornerColors);

  data_->geometry.faces.push_back(face);
  data_->rooms.face_rooms.push_back(value.room);
  if (!data_->lighting.corner_colors.empty()) {
    for (std::size_t corner = 0; corner < 3; ++corner) {
      data_->lighting.corner_colors.push_back(value.has_corner_colors != 0 ? value.corners[corner].color
                                                                           : lights::kDefaultCornerColor);
    }
  }
  level_validation::invalidate(data_->validation, LevelValidation::kFaces);
  if (faces_stay_valid) {
    level_validation::markValid(data_->validation, LevelValidation::kFaces);
    ArxAabb bounds = *referenced_bounds;
    math::expand(bounds, face_bounds.min);
    math::expand(bounds, face_bounds.max);
    data_->validation.derived.referenced_bounds = bounds;
  }
  if (faces_stay_valid && face_rooms_stay_valid)
    level_validation::markValid(data_->validation, LevelValidation::kFaceRooms);
  if (faces_stay_valid && colors_stay_valid)
    level_validation::markValid(data_->validation, LevelValidation::kCornerColors);
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::removeFace(FaceIndex index) {
  if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
  if (data_->rooms.face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
  if (!data_->lighting.corner_colors.empty() &&
      data_->lighting.corner_colors.size() != lights::expectedCornerColorCount(data_->geometry))
    return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

  const std::ptrdiff_t offset = static_cast<std::ptrdiff_t>(index);
  data_->geometry.faces.erase(data_->geometry.faces.begin() + offset);
  data_->rooms.face_rooms.erase(data_->rooms.face_rooms.begin() + offset);
  if (!data_->lighting.corner_colors.empty()) {
    const std::ptrdiff_t color = static_cast<std::ptrdiff_t>(lights::cornerColorIndex(index, 0));
    data_->lighting.corner_colors.erase(data_->lighting.corner_colors.begin() + color,
                                        data_->lighting.corner_colors.begin() + color + 3);
  }
  level_validation::invalidate(data_->validation, LevelValidation::kFaces);
  return ARX_OK;
}

ArxReturnCode Level::compactVertices(std::size_t* removed) {
  ArxReturnCode rc = validateFaces();
  if (rc != ARX_OK) return rc;
  const std::size_t count = geometry::compactVertices(data_->geometry);
  data_->validation.derived.bounds = data_->validation.derived.referenced_bounds;
  if (removed) *removed = count;
  return ARX_OK;
}

ArxReturnCode Level::compactTextures(std::size_t* removed) {
  const ArxReturnCode rc = validateFaces();
  if (rc != ARX_OK) return rc;
  const std::size_t count = geometry::compactTextures(data_->geometry);
  if (removed) *removed = count;
  return ARX_OK;
}

ArxReturnCode Level::weldVertices() { return weldVertices(VertexWeldOptions{}); }

ArxReturnCode Level::weldVertices(const VertexWeldOptions& options) {
  ArxReturnCode rc = validateMesh();
  if (rc != ARX_OK) return rc;

  geometry::VertexWeldOptions module_options;
  if (!geometryWeldOptions(options, module_options)) return ARX_INVALID_OPTIONS;

  std::vector<std::vector<VertexIndex>> segment_vertices;
  std::vector<VertexIndex> protected_vertices;
  rc = level_validation::roomsError(rooms::collectVertexWeldSegments(
      data_->geometry, data_->rooms, module_options.radius, segment_vertices, protected_vertices));
  if (rc != ARX_OK) return rc;

  std::vector<geometry::VertexWeldSegment> segments;
  segments.reserve(segment_vertices.size());
  for (const std::vector<VertexIndex>& vertices : segment_vertices) segments.push_back({vertices});

  geometry::GeometryRemap remap;
  const geometry::Error error = geometry::weldVerticesSegmented(
      data_->geometry, {.segments = segments, .protected_vertices = protected_vertices}, module_options, &remap);
  if (error != geometry::Error::kNone) return level_validation::geometryError(error);

  rooms::remapFaceRooms(data_->rooms, remap.faces);
  lights::remapCornerColors(data_->lighting, remap.faces);
  level_validation::invalidate(data_->validation, LevelValidation::kVertices);
  return ARX_OK;
}

ArxReturnCode Level::setTexture(TextureIndex index, const ArxLevelTextureView& value) {
  if (!validIndex(index, data_->geometry.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
  Texture texture;
  if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
  ArxReturnCode rc = level_validation::geometryError(geometry::validateTexture(texture));
  if (rc != ARX_OK) return rc;
  data_->geometry.textures[static_cast<std::size_t>(index)] = std::move(texture);
  return ARX_OK;
}

ArxReturnCode Level::addTexture(const ArxLevelTextureView& value, TextureIndex& out_index) {
  out_index = kNoTexture;
  if (!canAppendIndex<TextureIndex>(data_->geometry.textures.size())) return ARX_LEVEL_TOO_MANY_TEXTURES;

  Texture texture;
  if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
  ArxReturnCode rc = level_validation::geometryError(geometry::validateTexture(texture));
  if (rc != ARX_OK) return rc;

  const bool textures_stay_valid =
      data_->geometry.textures.empty() || level_validation::has(data_->validation, LevelValidation::kTextures);
  const TextureIndex index = geometry::addTexture(data_->geometry, std::move(texture));
  if (textures_stay_valid) level_validation::markValid(data_->validation, LevelValidation::kTextures);
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) {
  if (!validIndex(index, data_->geometry.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
  if (encoded_image.size == 0) return ARX_LEVEL_BAD_TEXTURE_IMAGE;
  std::vector<std::uint8_t> next_image;
  if (!copyImage(encoded_image, next_image)) return ARX_INVALID_DATA_POINTER;
  ArxReturnCode rc = level_validation::imageError(geometry::inspectImage(next_image));
  if (rc != ARX_OK) return rc;
  data_->geometry.textures[static_cast<std::size_t>(index)].encoded_image = std::move(next_image);
  return ARX_OK;
}

ArxReturnCode Level::clearTextureImage(TextureIndex index) noexcept {
  if (!validIndex(index, data_->geometry.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
  data_->geometry.textures[static_cast<std::size_t>(index)].encoded_image.clear();
  return ARX_OK;
}

ArxReturnCode Level::setFaceRoom(FaceIndex face, RoomIndex room) noexcept {
  if (!validIndex(face, data_->rooms.face_rooms.size())) return ARX_INDEX_OUT_OF_RANGE;
  if (!validIndex(room, data_->rooms.definitions.size())) return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
  data_->rooms.face_rooms[static_cast<std::size_t>(face)] = room;
  return ARX_OK;
}

ArxReturnCode Level::setCornerColor(FaceIndex face, std::uint8_t corner, ArxColor3 color) {
  if (!validIndex(face, data_->geometry.faces.size()) || corner >= 3) return ARX_INDEX_OUT_OF_RANGE;
  ArxReturnCode rc = level_validation::lightingError(lights::validateCornerColor(color));
  if (rc != ARX_OK) return rc;
  const std::size_t expected = lights::expectedCornerColorCount(data_->geometry);
  const std::size_t index = lights::cornerColorIndex(face, corner);
  if (data_->lighting.corner_colors.empty()) {
    data_->lighting.corner_colors.assign(expected, lights::kDefaultCornerColor);
  } else if (data_->lighting.corner_colors.size() != expected) {
    return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;
  }
  data_->lighting.corner_colors[index] = color;
  return ARX_OK;
}

void Level::clearCornerColors() noexcept { data_->lighting.corner_colors.clear(); }

ArxReturnCode Level::replaceMesh(const ArxLevelMeshInput& mesh) {
  if ((mesh.vertex_count != 0 && mesh.vertices == nullptr) || (mesh.face_count != 0 && mesh.faces == nullptr) ||
      (mesh.texture_count != 0 && mesh.textures == nullptr))
    return ARX_INVALID_DATA_POINTER;

  GeometryData geometry;
  geometry.vertices.reserve(mesh.vertex_count);
  for (std::size_t i = 0; i < mesh.vertex_count; ++i) geometry.vertices.push_back(internalVertex(mesh.vertices[i]));
  geometry.faces.reserve(mesh.face_count);
  for (std::size_t i = 0; i < mesh.face_count; ++i) geometry.faces.push_back(internalFace(mesh.faces[i]));
  geometry.textures.resize(mesh.texture_count);
  for (std::size_t i = 0; i < mesh.texture_count; ++i) {
    if (!internalTexture(mesh.textures[i], geometry.textures[i])) return ARX_INVALID_DATA_POINTER;
  }

  std::vector<RoomIndex> face_rooms;
  face_rooms.reserve(mesh.face_count);
  bool has_corner_colors = false;
  for (std::size_t i = 0; i < mesh.face_count; ++i) {
    face_rooms.push_back(mesh.faces[i].room);
    has_corner_colors = has_corner_colors || mesh.faces[i].has_corner_colors != 0;
  }
  std::vector<ArxColor3> corner_colors;
  if (has_corner_colors) {
    corner_colors.assign(mesh.face_count * 3U, lights::kDefaultCornerColor);
    for (std::size_t face = 0; face < mesh.face_count; ++face) {
      if (mesh.faces[face].has_corner_colors == 0) continue;
      for (std::size_t corner = 0; corner < 3; ++corner)
        corner_colors[lights::cornerColorIndex(static_cast<FaceIndex>(face), corner)] =
            mesh.faces[face].corners[corner].color;
    }
  }

  LevelValidationState next_validation;
  ArxReturnCode rc =
      validateMeshCoherence(geometry, face_rooms, corner_colors, data_->rooms.definitions.size(), next_validation);
  if (rc != ARX_OK) return rc;

  LevelValidationState final_validation = data_->validation;
  level_validation::invalidate(final_validation,
                               LevelValidation::kVertices | LevelValidation::kTextures | LevelValidation::kFaces |
                                   LevelValidation::kFaceRooms | LevelValidation::kCornerColors |
                                   LevelValidation::kNavSurface | LevelValidation::kAnchors);
  final_validation.derived = next_validation.derived;
  level_validation::markValid(final_validation,
                              next_validation.valid | LevelValidation::kNavSurface | LevelValidation::kAnchors |
                                  LevelValidation::kAnchorConnections);

  data_->geometry = std::move(geometry);
  data_->rooms.face_rooms = std::move(face_rooms);
  data_->lighting.corner_colors = std::move(corner_colors);
  data_->navigation.surface.reset();
  data_->navigation.anchors.clear();
  data_->navigation.connections.clear();
  data_->validation = final_validation;
  return ARX_OK;
}

void Level::clearMesh() noexcept {
  level_validation::invalidate(data_->validation,
                               LevelValidation::kVertices | LevelValidation::kTextures | LevelValidation::kFaces |
                                   LevelValidation::kFaceRooms | LevelValidation::kCornerColors |
                                   LevelValidation::kNavSurface | LevelValidation::kAnchors);
  data_->geometry.vertices.clear();
  data_->geometry.faces.clear();
  data_->geometry.textures.clear();
  data_->rooms.face_rooms.clear();
  data_->lighting.corner_colors.clear();
  data_->navigation.surface.reset();
  data_->navigation.anchors.clear();
  data_->navigation.connections.clear();
}

ArxReturnCode Level::setRoom(RoomIndex index, const ArxLevelRoom& value) {
  if (!validIndex(index, data_->rooms.definitions.size())) return ARX_INDEX_OUT_OF_RANGE;
  Room room;
  if (!internalRoom(value, room)) return ARX_INVALID_DATA_POINTER;
  ArxReturnCode rc = level_validation::roomsError(rooms::validateRoom(room));
  if (rc != ARX_OK) return rc;
  if (!roomNameAvailable(data_->rooms, room.name, index)) return ARX_LEVEL_DUPLICATE_ROOM_NAME;
  if (data_->rooms.definitions[static_cast<std::size_t>(index)].name == room.name) return ARX_OK;
  data_->rooms.definitions[static_cast<std::size_t>(index)] = std::move(room);
  return ARX_OK;
}

ArxReturnCode Level::addRoom(const ArxLevelRoom& value, RoomIndex& out_index) {
  out_index = kInvalidRoomIndex;
  Room room;
  if (!internalRoom(value, room)) return ARX_INVALID_DATA_POINTER;
  ArxReturnCode rc = level_validation::roomsError(rooms::validateRoom(room));
  if (rc != ARX_OK) return rc;
  if (!roomNameAvailable(data_->rooms, room.name)) return ARX_LEVEL_DUPLICATE_ROOM_NAME;
  if (data_->rooms.definitions.size() >= level_validation::kMaxRooms) return ARX_LEVEL_TOO_MANY_ROOMS;
  if (!canAppendIndex<RoomIndex>(data_->rooms.definitions.size())) return ARX_LEVEL_TOO_MANY_ROOMS;
  const RoomIndex index = static_cast<RoomIndex>(data_->rooms.definitions.size());
  data_->rooms.definitions.push_back(std::move(room));
  data_->rooms.distances.clear();
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::removeRoom(RoomIndex index) {
  if (!validIndex(index, data_->rooms.definitions.size())) return ARX_INDEX_OUT_OF_RANGE;
  if (faceReferencesRoom(data_->rooms, index)) return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
  const bool rooms_stay_valid =
      level_validation::has(data_->validation, LevelValidation::kRooms) && data_->rooms.definitions.size() > 1;
  data_->rooms.definitions.erase(data_->rooms.definitions.begin() + static_cast<std::ptrdiff_t>(index));
  remapRoomsAfterRemoval(data_->rooms, index);
  level_validation::invalidate(data_->validation, LevelValidation::kRooms);
  if (rooms_stay_valid) level_validation::markValid(data_->validation, LevelValidation::kRooms);
  return ARX_OK;
}

ArxReturnCode Level::setPortal(PortalIndex index, const ArxLevelPortal& value) {
  if (!validIndex(index, data_->rooms.portals.size())) return ARX_INDEX_OUT_OF_RANGE;
  Portal portal;
  if (!internalPortal(value, portal)) return ARX_INVALID_DATA_POINTER;
  if (!validPortalShape(value.shape)) return ARX_LEVEL_BAD_PORTAL_SHAPE;
  ArxReturnCode rc = level_validation::roomsError(rooms::validatePortal(portal, data_->rooms.definitions.size()));
  if (rc != ARX_OK) return rc;
  if (!level_validation::validPortalBounds(portal)) return ARX_LEVEL_PORTAL_OUT_OF_BOUNDS;
  if (!nameAvailable<Portal>(data_->rooms.portals, portal.name, static_cast<std::size_t>(index)))
    return ARX_LEVEL_DUPLICATE_PORTAL_NAME;
  const bool topology_changed = !samePortalTopology(data_->rooms.portals[static_cast<std::size_t>(index)], portal);
  data_->rooms.portals[static_cast<std::size_t>(index)] = std::move(portal);
  if (topology_changed) data_->rooms.distances.clear();
  return ARX_OK;
}

ArxReturnCode Level::addPortal(const ArxLevelPortal& value, PortalIndex& out_index) {
  out_index = kInvalidPortalIndex;
  Portal portal;
  if (!internalPortal(value, portal)) return ARX_INVALID_DATA_POINTER;
  if (!validPortalShape(value.shape)) return ARX_LEVEL_BAD_PORTAL_SHAPE;
  ArxReturnCode rc = level_validation::roomsError(rooms::validatePortal(portal, data_->rooms.definitions.size()));
  if (rc != ARX_OK) return rc;
  if (!level_validation::validPortalBounds(portal)) return ARX_LEVEL_PORTAL_OUT_OF_BOUNDS;
  if (!nameAvailable<Portal>(data_->rooms.portals, portal.name, data_->rooms.portals.size()))
    return ARX_LEVEL_DUPLICATE_PORTAL_NAME;
  if (!canAppendIndex<PortalIndex>(data_->rooms.portals.size())) return ARX_LEVEL_TOO_MANY_PORTALS;
  const PortalIndex index = static_cast<PortalIndex>(data_->rooms.portals.size());
  data_->rooms.portals.push_back(std::move(portal));
  data_->rooms.distances.clear();
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::removePortal(PortalIndex index) {
  if (!validIndex(index, data_->rooms.portals.size())) return ARX_INDEX_OUT_OF_RANGE;
  data_->rooms.portals.erase(data_->rooms.portals.begin() + static_cast<std::ptrdiff_t>(index));
  data_->rooms.distances.clear();
  return ARX_OK;
}

ArxReturnCode Level::setRoomDistance(const ArxLevelRoomDistance& value) {
  RoomIndex low_room = 0;
  RoomIndex high_room = 0;
  RoomDistance distance;
  ArxReturnCode rc = internalRoomDistance(value, data_->rooms, low_room, high_room, distance);
  if (rc != ARX_OK) return rc;

  const std::size_t expected = rooms::roomDistancePairCount(data_->rooms.definitions.size());
  if (!data_->rooms.distances.empty() && data_->rooms.distances.size() != expected)
    return ARX_LEVEL_BAD_ROOM_DISTANCE_COUNT;
  if (data_->rooms.distances.empty())
    rooms::initializeRoomDistances(data_->rooms.distances, data_->rooms.definitions.size());
  data_->rooms.distances[rooms::roomDistancePairIndex(low_room, high_room)] = distance;
  level_validation::markValid(data_->validation, LevelValidation::kRoomDistances);
  return ARX_OK;
}

ArxReturnCode Level::replaceRoomDistances(const ArxLevelRoomDistance* distances, std::size_t count) {
  const std::size_t expected = rooms::roomDistancePairCount(data_->rooms.definitions.size());
  if (count != 0 && distances == nullptr) return ARX_INVALID_DATA_POINTER;
  if (count != 0 && count != expected) return ARX_LEVEL_BAD_ROOM_DISTANCE_COUNT;
  std::vector<RoomDistance> next;
  if (count != 0) {
    next.resize(expected);
    std::vector<std::uint8_t> seen(expected, 0);
    for (std::size_t i = 0; i < count; ++i) {
      RoomIndex low_room = 0;
      RoomIndex high_room = 0;
      RoomDistance distance;
      ArxReturnCode rc = internalRoomDistance(distances[i], data_->rooms, low_room, high_room, distance);
      if (rc != ARX_OK) return rc;
      const std::size_t index = rooms::roomDistancePairIndex(low_room, high_room);
      if (seen[index] != 0) return ARX_LEVEL_BAD_ROOM_DISTANCE_COUNT;
      seen[index] = 1;
      next[index] = distance;
    }
  }
  ArxReturnCode rc = level_validation::roomsError(rooms::validateRoomDistances(next, data_->rooms));
  if (rc != ARX_OK) return rc;
  data_->rooms.distances = std::move(next);
  level_validation::markValid(data_->validation, LevelValidation::kRoomDistances);
  return ARX_OK;
}

void Level::clearRoomDistances() noexcept { data_->rooms.distances.clear(); }

ArxReturnCode Level::setAnchor(AnchorIndex index, const ArxLevelAnchor& value) {
  if (!validIndex(index, data_->navigation.anchors.size())) return ARX_INDEX_OUT_OF_RANGE;
  Anchor anchor;
  if (!internalAnchor(value, anchor)) return ARX_INVALID_DATA_POINTER;
  ArxReturnCode rc = level_validation::navigationError(navigation::validateAnchor(anchor));
  if (rc != ARX_OK) return rc;
  if (!anchorNameAvailable(data_->navigation, anchor.name, index)) return ARX_LEVEL_DUPLICATE_ANCHOR_NAME;
  data_->navigation.anchors[static_cast<std::size_t>(index)] = std::move(anchor);
  level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
  return ARX_OK;
}

ArxReturnCode Level::addAnchor(const ArxLevelAnchor& value, AnchorIndex& out_index) {
  out_index = kInvalidAnchorIndex;
  Anchor anchor;
  if (!internalAnchor(value, anchor)) return ARX_INVALID_DATA_POINTER;
  ArxReturnCode rc = level_validation::navigationError(navigation::validateAnchor(anchor));
  if (rc != ARX_OK) return rc;
  if (!anchorNameAvailable(data_->navigation, anchor.name)) return ARX_LEVEL_DUPLICATE_ANCHOR_NAME;
  if (!canAppendIndex<AnchorIndex>(data_->navigation.anchors.size())) return ARX_LEVEL_TOO_MANY_ANCHORS;
  const AnchorIndex index = static_cast<AnchorIndex>(data_->navigation.anchors.size());
  data_->navigation.anchors.push_back(std::move(anchor));
  level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::removeAnchor(AnchorIndex index) {
  if (!validIndex(index, data_->navigation.anchors.size())) return ARX_INDEX_OUT_OF_RANGE;
  const bool anchors_stay_valid = level_validation::has(data_->validation, LevelValidation::kAnchors);
  data_->navigation.anchors.erase(data_->navigation.anchors.begin() + static_cast<std::ptrdiff_t>(index));
  remapAnchorsAfterRemoval(data_->navigation, index);
  level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
  if (anchors_stay_valid) level_validation::markValid(data_->validation, LevelValidation::kAnchors);
  return ARX_OK;
}

ArxReturnCode Level::setAnchorConnection(AnchorConnectionIndex index, ArxLevelAnchorConnection value) {
  if (!validIndex(index, data_->navigation.connections.size())) return ARX_INDEX_OUT_OF_RANGE;
  const AnchorConnection connection = internalConnection(value);
  std::vector<AnchorConnection> next = data_->navigation.connections;
  next[static_cast<std::size_t>(index)] = connection;
  ArxReturnCode rc =
      level_validation::navigationError(navigation::validateConnections(data_->navigation.anchors, next));
  if (rc != ARX_OK) return rc;
  data_->navigation.connections = std::move(next);
  level_validation::markValid(data_->validation, LevelValidation::kAnchorConnections);
  return ARX_OK;
}

ArxReturnCode Level::addAnchorConnection(ArxLevelAnchorConnection value, AnchorConnectionIndex& out_index) {
  out_index = kInvalidAnchorConnectionIndex;
  const AnchorConnection connection = internalConnection(value);
  ArxReturnCode rc =
      level_validation::navigationError(navigation::validateConnection(connection, data_->navigation.anchors.size()));
  if (rc != ARX_OK) return rc;
  if (!canAppendIndex<AnchorConnectionIndex>(data_->navigation.connections.size()))
    return ARX_LEVEL_TOO_MANY_ANCHOR_CONNECTIONS;
  const std::size_t index = sortedAnchorConnectionIndex(data_->navigation.connections, connection);
  if (index != data_->navigation.connections.size() &&
      sameAnchorConnection(data_->navigation.connections[index], connection)) {
    out_index = static_cast<AnchorConnectionIndex>(index);
    return ARX_OK;
  }
  data_->navigation.connections.insert(data_->navigation.connections.begin() + static_cast<std::ptrdiff_t>(index),
                                       connection);
  out_index = static_cast<AnchorConnectionIndex>(index);
  return ARX_OK;
}

ArxReturnCode Level::removeAnchorConnection(AnchorConnectionIndex index) {
  if (!validIndex(index, data_->navigation.connections.size())) return ARX_INDEX_OUT_OF_RANGE;
  data_->navigation.connections.erase(data_->navigation.connections.begin() + static_cast<std::ptrdiff_t>(index));
  return ARX_OK;
}

ArxReturnCode Level::replaceAnchors(const ArxLevelAnchorsInput& input) {
  if ((input.anchor_count != 0 && input.anchors == nullptr) ||
      (input.connection_count != 0 && input.connections == nullptr))
    return ARX_INVALID_DATA_POINTER;
  if (input.anchor_count > static_cast<std::size_t>(kInvalidAnchorIndex)) return ARX_LEVEL_TOO_MANY_ANCHORS;
  std::vector<Anchor> anchors(input.anchor_count);
  for (std::size_t i = 0; i < input.anchor_count; ++i) {
    if (!internalAnchor(input.anchors[i], anchors[i])) return ARX_INVALID_DATA_POINTER;
  }
  std::vector<AnchorConnection> connections;
  connections.reserve(input.connection_count);
  for (std::size_t i = 0; i < input.connection_count; ++i)
    connections.push_back(internalConnection(input.connections[i]));
  ArxReturnCode rc = level_validation::navigationError(navigation::validateAnchorDefinitions(anchors));
  if (rc != ARX_OK) return rc;
  rc = level_validation::navigationError(navigation::validateConnections(anchors, connections));
  if (rc != ARX_OK) return rc;
  data_->navigation.anchors = std::move(anchors);
  data_->navigation.connections = std::move(connections);
  level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
  level_validation::markValid(data_->validation, LevelValidation::kAnchorConnections);
  return ARX_OK;
}

void Level::clearAnchors() noexcept {
  data_->navigation.anchors.clear();
  data_->navigation.connections.clear();
}

ArxReturnCode Level::setNavSurface(const ArxLevelNavSurfaceInput& input) {
  if ((input.vertex_count != 0 && input.vertices == nullptr) ||
      (input.triangle_count != 0 && input.triangles == nullptr))
    return ARX_INVALID_DATA_POINTER;
  NavSurface surface;
  surface.vertices.reserve(input.vertex_count);
  for (std::size_t i = 0; i < input.vertex_count; ++i) surface.vertices.push_back(internalVertex(input.vertices[i]));
  surface.triangles.resize(input.triangle_count);
  for (std::size_t i = 0; i < input.triangle_count; ++i) {
    std::copy(std::begin(input.triangles[i].vertices),
              std::end(input.triangles[i].vertices),
              surface.triangles[i].vertices.begin());
  }
  ArxReturnCode rc = level_validation::navigationError(navigation::validateSurface(surface));
  if (rc != ARX_OK) return rc;
  data_->navigation.surface = std::move(surface);
  level_validation::markValid(data_->validation, LevelValidation::kNavSurface);
  return ARX_OK;
}

void Level::clearNavSurface() noexcept { data_->navigation.surface.reset(); }

ArxReturnCode Level::setLight(LightIndex index, const ArxLevelLight& value) {
  if (!validIndex(index, data_->lighting.lights.size())) return ARX_INDEX_OUT_OF_RANGE;
  Light light;
  if (!internalLight(value, light)) return ARX_INVALID_DATA_POINTER;
  ArxReturnCode rc = level_validation::lightingError(lights::validateLightSource(light));
  if (rc != ARX_OK) return rc;
  if (!nameAvailable<Light>(data_->lighting.lights, light.name, static_cast<std::size_t>(index)))
    return ARX_LEVEL_DUPLICATE_LIGHT_NAME;
  data_->lighting.lights[static_cast<std::size_t>(index)] = std::move(light);
  return ARX_OK;
}

ArxReturnCode Level::addLight(const ArxLevelLight& value, LightIndex& out_index) {
  out_index = kInvalidLightIndex;
  Light light;
  if (!internalLight(value, light)) return ARX_INVALID_DATA_POINTER;
  ArxReturnCode rc = level_validation::lightingError(lights::validateLightSource(light));
  if (rc != ARX_OK) return rc;
  if (!nameAvailable<Light>(data_->lighting.lights, light.name, data_->lighting.lights.size()))
    return ARX_LEVEL_DUPLICATE_LIGHT_NAME;
  if (!canAppendIndex<LightIndex>(data_->lighting.lights.size())) return ARX_LEVEL_TOO_MANY_LIGHTS;
  const LightIndex index = static_cast<LightIndex>(data_->lighting.lights.size());
  data_->lighting.lights.push_back(std::move(light));
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::removeLight(LightIndex index) {
  if (!validIndex(index, data_->lighting.lights.size())) return ARX_INDEX_OUT_OF_RANGE;
  data_->lighting.lights.erase(data_->lighting.lights.begin() + static_cast<std::ptrdiff_t>(index));
  return ARX_OK;
}

ArxReturnCode Level::setPlayerSpawn(const ArxLevelPlayerSpawn& value) {
  if (value.is_usable == 0) {
    clearPlayerSpawn();
    return ARX_OK;
  }
  const PlayerSpawn spawn = {.position = value.position, .rotation = value.rotation};
  ArxReturnCode rc = level_validation::sceneError(scene::setPlayerSpawn(data_->scene, spawn));
  if (rc == ARX_OK) level_validation::markValid(data_->validation, LevelValidation::kPlayerSpawn);
  return rc;
}

void Level::clearPlayerSpawn() noexcept {
  scene::clearPlayerSpawn(data_->scene);
  level_validation::markValid(data_->validation, LevelValidation::kPlayerSpawn);
}

ArxReturnCode Level::setEntity(EntityIndex index, const ArxLevelEntity& value) {
  if (!validIndex(index, data_->scene.entities.size())) return ARX_INDEX_OUT_OF_RANGE;
  Entity entity;
  if (!internalEntity(value, entity)) return ARX_INVALID_DATA_POINTER;
  if (!scene::normalizeRotation(entity.rotation)) return ARX_LEVEL_BAD_ENTITY_ROTATION;
  ArxReturnCode rc = level_validation::sceneError(scene::validateEntity(entity));
  if (rc != ARX_OK) return rc;
  scene::makeEntityNameUnique(entity, data_->scene.entities, static_cast<std::size_t>(index));
  data_->scene.entities[static_cast<std::size_t>(index)] = std::move(entity);
  return ARX_OK;
}

ArxReturnCode Level::addEntity(const ArxLevelEntity& value, EntityIndex& out_index) {
  out_index = kInvalidEntityIndex;
  Entity entity;
  if (!internalEntity(value, entity)) return ARX_INVALID_DATA_POINTER;
  if (!scene::normalizeRotation(entity.rotation)) return ARX_LEVEL_BAD_ENTITY_ROTATION;
  ArxReturnCode rc = level_validation::sceneError(scene::validateEntity(entity));
  if (rc != ARX_OK) return rc;
  if (!canAppendIndex<EntityIndex>(data_->scene.entities.size())) return ARX_LEVEL_TOO_MANY_ENTITIES;
  scene::makeEntityNameUnique(entity, data_->scene.entities);
  const EntityIndex index = static_cast<EntityIndex>(data_->scene.entities.size());
  data_->scene.entities.push_back(std::move(entity));
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::removeEntity(EntityIndex index) {
  if (!validIndex(index, data_->scene.entities.size())) return ARX_INDEX_OUT_OF_RANGE;
  data_->scene.entities.erase(data_->scene.entities.begin() + static_cast<std::ptrdiff_t>(index));
  return ARX_OK;
}

ArxReturnCode Level::setFog(FogIndex index, const ArxLevelFog& value) {
  if (!validIndex(index, data_->scene.fogs.size())) return ARX_INDEX_OUT_OF_RANGE;
  Fog fog;
  if (!internalFog(value, fog)) return ARX_INVALID_DATA_POINTER;
  if (!scene::normalizeRotation(fog.rotation)) return ARX_LEVEL_BAD_FOG_ROTATION;
  ArxReturnCode rc = level_validation::sceneError(scene::validateFog(fog));
  if (rc != ARX_OK) return rc;
  if (!fogNameAvailable(data_->scene, fog.name, index)) return ARX_LEVEL_DUPLICATE_FOG_NAME;
  data_->scene.fogs[static_cast<std::size_t>(index)] = std::move(fog);
  return ARX_OK;
}

ArxReturnCode Level::addFog(const ArxLevelFog& value, FogIndex& out_index) {
  out_index = kInvalidFogIndex;
  Fog fog;
  if (!internalFog(value, fog)) return ARX_INVALID_DATA_POINTER;
  if (!scene::normalizeRotation(fog.rotation)) return ARX_LEVEL_BAD_FOG_ROTATION;
  ArxReturnCode rc = level_validation::sceneError(scene::validateFog(fog));
  if (rc != ARX_OK) return rc;
  if (!fogNameAvailable(data_->scene, fog.name)) return ARX_LEVEL_DUPLICATE_FOG_NAME;
  if (!canAppendIndex<FogIndex>(data_->scene.fogs.size())) return ARX_LEVEL_TOO_MANY_FOGS;
  const FogIndex index = static_cast<FogIndex>(data_->scene.fogs.size());
  data_->scene.fogs.push_back(std::move(fog));
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::removeFog(FogIndex index) {
  if (!validIndex(index, data_->scene.fogs.size())) return ARX_INDEX_OUT_OF_RANGE;
  data_->scene.fogs.erase(data_->scene.fogs.begin() + static_cast<std::ptrdiff_t>(index));
  return ARX_OK;
}

ArxReturnCode Level::setZone(ZoneIndex index, const ArxLevelZoneInput& value) {
  if (!validIndex(index, data_->scene.zones.size())) return ARX_INDEX_OUT_OF_RANGE;
  Zone zone;
  if (!internalZone(value, zone)) return ARX_INVALID_DATA_POINTER;
  if (!validZoneHeightMode(value.value.height_mode)) return ARX_LEVEL_BAD_ZONE_HEIGHT_MODE;
  ArxReturnCode rc = level_validation::sceneError(scene::validateZone(zone));
  if (rc != ARX_OK) return rc;
  if (!nameAvailable<Zone>(data_->scene.zones, zone.name, static_cast<std::size_t>(index), true))
    return ARX_LEVEL_DUPLICATE_ZONE_NAME;
  data_->scene.zones[static_cast<std::size_t>(index)] = std::move(zone);
  return ARX_OK;
}

ArxReturnCode Level::addZone(const ArxLevelZoneInput& value, ZoneIndex& out_index) {
  out_index = kInvalidZoneIndex;
  Zone zone;
  if (!internalZone(value, zone)) return ARX_INVALID_DATA_POINTER;
  if (!validZoneHeightMode(value.value.height_mode)) return ARX_LEVEL_BAD_ZONE_HEIGHT_MODE;
  ArxReturnCode rc = level_validation::sceneError(scene::validateZone(zone));
  if (rc != ARX_OK) return rc;
  if (!nameAvailable<Zone>(data_->scene.zones, zone.name, data_->scene.zones.size(), true))
    return ARX_LEVEL_DUPLICATE_ZONE_NAME;
  if (!canAppendIndex<ZoneIndex>(data_->scene.zones.size())) return ARX_LEVEL_TOO_MANY_ZONES;
  const ZoneIndex index = static_cast<ZoneIndex>(data_->scene.zones.size());
  data_->scene.zones.push_back(std::move(zone));
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::removeZone(ZoneIndex index) {
  if (!validIndex(index, data_->scene.zones.size())) return ARX_INDEX_OUT_OF_RANGE;
  data_->scene.zones.erase(data_->scene.zones.begin() + static_cast<std::ptrdiff_t>(index));
  return ARX_OK;
}

ArxReturnCode Level::setPath(PathIndex index, const ArxLevelPathInput& value) {
  if (!validIndex(index, data_->scene.paths.size())) return ARX_INDEX_OUT_OF_RANGE;
  Path path;
  if (!internalPath(value, path)) return ARX_INVALID_DATA_POINTER;
  if (!validPathNodeTypes(value)) return ARX_LEVEL_BAD_PATH_NODE_TYPE;
  ArxReturnCode rc = level_validation::sceneError(scene::validatePath(path));
  if (rc != ARX_OK) return rc;
  if (!pathNameAvailable(data_->scene, path.name, index)) return ARX_LEVEL_DUPLICATE_PATH_NAME;
  data_->scene.paths[static_cast<std::size_t>(index)] = std::move(path);
  return ARX_OK;
}

ArxReturnCode Level::addPath(const ArxLevelPathInput& value, PathIndex& out_index) {
  out_index = kInvalidPathIndex;
  Path path;
  if (!internalPath(value, path)) return ARX_INVALID_DATA_POINTER;
  if (!validPathNodeTypes(value)) return ARX_LEVEL_BAD_PATH_NODE_TYPE;
  ArxReturnCode rc = level_validation::sceneError(scene::validatePath(path));
  if (rc != ARX_OK) return rc;
  if (!pathNameAvailable(data_->scene, path.name)) return ARX_LEVEL_DUPLICATE_PATH_NAME;
  if (!canAppendIndex<PathIndex>(data_->scene.paths.size())) return ARX_LEVEL_TOO_MANY_PATHS;
  const PathIndex index = static_cast<PathIndex>(data_->scene.paths.size());
  data_->scene.paths.push_back(std::move(path));
  out_index = index;
  return ARX_OK;
}

ArxReturnCode Level::removePath(PathIndex index) {
  if (!validIndex(index, data_->scene.paths.size())) return ARX_INDEX_OUT_OF_RANGE;
  data_->scene.paths.erase(data_->scene.paths.begin() + static_cast<std::ptrdiff_t>(index));
  return ARX_OK;
}

ArxReturnCode validateLevelModules(const LevelModules& modules, ArxAabb* out_bounds, ArxAabb* out_referenced_bounds) {
  LevelValidationState state;
  ArxReturnCode rc = validateLevelModules(modules, state);
  if (rc != ARX_OK) return rc;
  const auto& bounds = state.derived.bounds;
  if (out_bounds) {
    if (!bounds.has_value()) return ARX_INTERNAL_ERROR;
    *out_bounds = *bounds;
  }
  const auto& referenced_bounds = state.derived.referenced_bounds;
  if (out_referenced_bounds) {
    if (!referenced_bounds.has_value()) return ARX_INTERNAL_ERROR;
    *out_referenced_bounds = *referenced_bounds;
  }
  return ARX_OK;
}

ArxReturnCode validateLevelModules(const LevelModules& modules, LevelValidationState& out_state) {
  LevelValidationState state;
  ArxReturnCode rc = level_validation::all(modules, state);
  if (rc == ARX_OK) out_state = state;
  return rc;
}

}  // namespace pistoris
