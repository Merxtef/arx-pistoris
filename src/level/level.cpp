// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level.hpp"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/texture.h"

#include "api/status_boundary.h"
#include "level/data.h"
#include "level/validation.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/loading_screen.h"
#include "modules/minimap.h"
#include "modules/navigation.h"
#include "modules/resource.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "modules/textures.h"
#include "utils/log.h"
#include "utils/math/bounds.h"
#include "utils/math/finite.h"
#include "utils/math/rotation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
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
  if (value.empty()) return {};
  return {value.data(), value.size()};
}

ArxReturnCode levelResourceError(resource::Error error) noexcept {
  switch (error) {
    case resource::Error::kNone:
      return ARX_OK;
    case resource::Error::kBadPath:
      return ARX_LEVEL_BAD_RESOURCE_PATH;
    case resource::Error::kBadKind:
      return ARX_INTERNAL_ERROR;
  }
  return ARX_INTERNAL_ERROR;
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
    if (type != ARX_PATH_NODE_STANDARD && type != ARX_PATH_NODE_BEZIER) return false;
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
  result.flags = face.flags & ~kFaceBitQuad;
  result.transval = face.transval;
  return result;
}

AnchorConnection internalConnection(ArxLevelAnchorConnection connection) noexcept {
  return {connection.first, connection.second};
}

bool internalTexture(const ArxTextureView& texture, Texture& out) {
  return copyString(texture.path, out.path) && copyImage(texture.encoded_image, out.encoded_image) &&
         copyString(texture.external_image_extension, out.external_image_extension);
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

ArxReturnCode validateMeshCoherence(const GeometryData& geometry, const TexturesData& textures,
                                    std::span<const RoomIndex> face_rooms, std::span<const ArxColor3> corner_colors,
                                    std::size_t room_count, LevelValidationState& state) noexcept {
  ArxAabb bounds;
  ArxReturnCode rc = level_validation::geometryError(geometry::validateVertices(geometry.vertices, &bounds));
  if (rc != ARX_OK) return rc;
  if (bounds.min.x < kLevelMinXZ || bounds.max.x > kLevelMaxXZ || bounds.min.z < kLevelMinXZ ||
      bounds.max.z > kLevelMaxXZ)
    return ARX_LEVEL_VERTEX_OUT_OF_BOUNDS;
  rc = level_validation::textureError(pistoris::textures::validate(textures.textures));
  if (rc != ARX_OK) return rc;
  ArxAabb referenced_bounds;
  rc = level_validation::geometryError(
      geometry::validateFaces(geometry.faces, geometry.vertices, textures.textures.size(), &referenced_bounds));
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

ArxReturnCode Level::validateMesh() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::mesh(*data_, data_->validation); });
}

ArxReturnCode Level::validate() const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const ArxReturnCode rc = levelResourceError(resource::validate(data_->resource, ARX_RESOURCE_KIND_LEVEL));
    if (rc != ARX_OK) return rc;
    return level_validation::all(*data_, data_->validation);
  });
}

ArxReturnCode Level::validateVertices() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::vertices(*data_, data_->validation); });
}

ArxReturnCode Level::validateTextures() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::textures(*data_, data_->validation); });
}

ArxReturnCode Level::validateFaces() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::faces(*data_, data_->validation); });
}

ArxReturnCode Level::validateFaceRooms() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::faceRooms(*data_, data_->validation); });
}

ArxReturnCode Level::validateCornerColors() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::cornerColors(*data_, data_->validation); });
}

ArxReturnCode Level::validateRooms() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::rooms(*data_, data_->validation); });
}

ArxReturnCode Level::validatePortals() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::portals(*data_, data_->validation); });
}

ArxReturnCode Level::validateRoomDistances() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::roomDistances(*data_, data_->validation); });
}

ArxReturnCode Level::validateNavSurface() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::navSurface(*data_, data_->validation); });
}

ArxReturnCode Level::validateAnchors() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::anchors(*data_, data_->validation); });
}

ArxReturnCode Level::validateAnchorConnections() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::anchorConnections(*data_, data_->validation); });
}

ArxReturnCode Level::validateLights() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::lightSources(*data_, data_->validation); });
}

ArxReturnCode Level::validatePlayerSpawn() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::playerSpawn(*data_, data_->validation); });
}

ArxReturnCode Level::validateEntities() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::entities(*data_, data_->validation); });
}

ArxReturnCode Level::validateFogs() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::fogs(*data_, data_->validation); });
}

ArxReturnCode Level::validateZones() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::zones(*data_, data_->validation); });
}

ArxReturnCode Level::validatePaths() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::paths(*data_, data_->validation); });
}

ArxReturnCode Level::validateMinimap() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::minimap(*data_, data_->validation); });
}

ArxReturnCode Level::validateLoadingScreen() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return level_validation::loadingScreen(*data_, data_->validation); });
}

std::string_view Level::resourcePath() const noexcept { return data_->resource.path; }

ArxReturnCode Level::setResourcePath(std::string_view resource_path) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::string path;
    const ArxReturnCode rc = levelResourceError(resource::repairPath(ARX_RESOURCE_KIND_LEVEL, resource_path, path));
    if (rc != ARX_OK) return rc;
    resource::setPath(data_->resource, std::move(path));
    return ARX_OK;
  });
}

std::optional<ArxAabb> Level::bounds() const {
  if (validateVertices() != ARX_OK) return std::nullopt;
  return data_->validation.derived.bounds;
}

std::optional<ArxAabb> Level::referencedBounds() const {
  if (validateFaces() != ARX_OK) return std::nullopt;
  return data_->validation.derived.referenced_bounds;
}

Level::MinimapView Level::minimap() const noexcept {
  return {
      .encoded_image = borrowedImage(data_->minimap.encoded_image),
      .world_xz_bounds = data_->minimap.world_xz_bounds,
  };
}

ArxEncodedImageView Level::loadingScreen() const noexcept { return borrowedImage(data_->loading_screen.encoded_image); }

ArxReturnCode Level::setMinimap(ArxEncodedImageView encoded_image, ArxRect world_xz_bounds) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (encoded_image.size == 0) return ARX_LEVEL_BAD_MINIMAP_IMAGE;
    std::vector<std::uint8_t> copy;
    if (!copyImage(encoded_image, copy)) return ARX_INVALID_DATA_POINTER;
    MinimapData value{.encoded_image = std::move(copy), .world_xz_bounds = world_xz_bounds};
    const ArxReturnCode rc = level_validation::minimapError(minimap::validate(value));
    if (rc != ARX_OK) return rc;
    minimap::setImage(data_->minimap, std::move(value.encoded_image), value.world_xz_bounds);
    level_validation::markValid(data_->validation, LevelValidation::kMinimap);
    return ARX_OK;
  });
}

ArxReturnCode Level::setMinimapFromProjection(ArxEncodedImageView encoded_image,
                                              ArxVector2 projection_offset) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (encoded_image.size == 0) return ARX_LEVEL_BAD_MINIMAP_IMAGE;
    ArxReturnCode rc = validateFaces();
    if (rc != ARX_OK) return rc;
    if (!data_->validation.derived.referenced_bounds) return ARX_LEVEL_NO_GEOMETRY;
    std::vector<std::uint8_t> copy;
    if (!copyImage(encoded_image, copy)) return ARX_INVALID_DATA_POINTER;
    ArxRect bounds;
    rc = level_validation::minimapError(
        minimap::projectedBounds(copy, *data_->validation.derived.referenced_bounds, projection_offset, bounds));
    if (rc != ARX_OK) return rc;
    minimap::setImage(data_->minimap, std::move(copy), bounds);
    level_validation::markValid(data_->validation, LevelValidation::kMinimap);
    return ARX_OK;
  });
}

void Level::clearMinimap() noexcept {
  minimap::clear(data_->minimap);
  level_validation::markValid(data_->validation, LevelValidation::kMinimap);
}

ArxReturnCode Level::renderMinimapPng(const MinimapRenderOptions& options,
                                      std::vector<std::uint8_t>& out) const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return renderMinimapProjectionPng(options, std::nullopt, out); });
}

ArxReturnCode Level::renderGameMinimapPng(const GameMinimapRenderOptions& options,
                                          std::vector<std::uint8_t>& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    return renderMinimapProjectionPng(
        {.projection_offset = options.projection_offset, .fill_color = options.fill_color}, options.border_color, out);
  });
}

ArxReturnCode Level::renderMinimapProjectionPng(const MinimapRenderOptions& options,
                                                std::optional<ArxColor3> border_color,
                                                std::vector<std::uint8_t>& out) const {
  const minimap::RenderOptions render_options{
      .projection_offset = options.projection_offset,
      .fill_color = options.fill_color,
      .border_color = border_color,
  };
  ArxReturnCode rc = level_validation::minimapError(minimap::validateRenderOptions(render_options));
  if (rc != ARX_OK) return rc;
  if (data_->minimap.encoded_image.empty()) {
    out.clear();
    return ARX_OK;
  }
  rc = validateMinimap();
  if (rc != ARX_OK) return rc;
  rc = validateFaces();
  if (rc != ARX_OK) return rc;
  if (!data_->validation.derived.referenced_bounds) return ARX_LEVEL_NO_GEOMETRY;
  minimap::RenderInfo info;
  rc = level_validation::minimapError(
      minimap::renderPng(data_->minimap, *data_->validation.derived.referenced_bounds, render_options, out, &info));
  if (rc != ARX_OK) return rc;
  if (info.invisible) {
    log(ARX_LOG_WARN, "Level minimap is outside the requested projection; output omitted");
  } else if (info.cropped) {
    log(ARX_LOG_WARN, "Level minimap was cropped by the requested projection");
  } else if (info.padded) {
    log(ARX_LOG_DEBUG, "Level minimap was padded for the requested projection");
  }
  return ARX_OK;
}

ArxReturnCode Level::renderCompactMinimapPng(ArxVector2& out_projection_offset,
                                             std::vector<std::uint8_t>& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (data_->minimap.encoded_image.empty()) {
      out_projection_offset = {};
      out.clear();
      return ARX_OK;
    }
    ArxReturnCode rc = validateMinimap();
    if (rc != ARX_OK) return rc;
    rc = validateFaces();
    if (rc != ARX_OK) return rc;
    if (!data_->validation.derived.referenced_bounds) return ARX_LEVEL_NO_GEOMETRY;
    ArxVector2 projection_offset{};
    rc = level_validation::minimapError(minimap::compactProjectionOffset(
        data_->minimap, *data_->validation.derived.referenced_bounds, projection_offset));
    if (rc != ARX_OK) return rc;
    rc = renderMinimapPng({.projection_offset = projection_offset}, out);
    if (rc == ARX_OK) out_projection_offset = projection_offset;
    return rc;
  });
}

ArxReturnCode Level::setLoadingScreen(ArxEncodedImageView encoded_image) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (encoded_image.size == 0) return ARX_LEVEL_BAD_LOADING_SCREEN_IMAGE;
    std::vector<std::uint8_t> copy;
    if (!copyImage(encoded_image, copy)) return ARX_INVALID_DATA_POINTER;
    const ArxReturnCode rc = level_validation::loadingScreenError(loading_screen::validateImage(copy));
    if (rc != ARX_OK) return rc;
    loading_screen::setImage(data_->loading_screen, std::move(copy));
    level_validation::markValid(data_->validation, LevelValidation::kLoadingScreen);
    return ARX_OK;
  });
}

void Level::clearLoadingScreen() noexcept {
  loading_screen::clear(data_->loading_screen);
  level_validation::markValid(data_->validation, LevelValidation::kLoadingScreen);
}

ArxReturnCode Level::renderLoadingScreenPng(std::vector<std::uint8_t>& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const ArxReturnCode rc = validateLoadingScreen();
    if (rc != ARX_OK) return rc;
    return level_validation::loadingScreenError(loading_screen::renderPng(data_->loading_screen, false, out));
  });
}

ArxReturnCode Level::renderFullscreenLoadingScreenPng(std::vector<std::uint8_t>& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const ArxReturnCode rc = validateLoadingScreen();
    if (rc != ARX_OK) return rc;
    return level_validation::loadingScreenError(loading_screen::renderPng(data_->loading_screen, true, out));
  });
}

ArxReturnCode Level::transcodeLoadingScreenPng(std::vector<std::uint8_t>& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const ArxReturnCode rc = validateLoadingScreen();
    if (rc != ARX_OK) return rc;
    return level_validation::loadingScreenError(loading_screen::transcodePng(data_->loading_screen, out));
  });
}

std::size_t Level::vertexCount() const noexcept { return data_->geometry.vertices.size(); }

std::size_t Level::faceCount() const noexcept { return data_->geometry.faces.size(); }

std::size_t Level::textureCount() const noexcept { return data_->textures.textures.size(); }

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

ArxReturnCode Level::copyTextureViews(std::size_t offset, std::size_t count, ArxTextureView* out_views) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->textures.textures.size(), offset, count, out_views);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Texture& texture = data_->textures.textures[offset + i];
    out_views[i].path = borrowedString(texture.path);
    out_views[i].encoded_image = borrowedImage(texture.encoded_image);
    out_views[i].external_image_extension = borrowedString(texture.external_image_extension);
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
  const PlayerSpawn spawn = data_->scene.player_spawn.value_or(PlayerSpawn{});
  return {
      .position = spawn.position,
      .rotation = spawn.rotation,
      .is_usable = static_cast<std::uint8_t>(data_->scene.player_spawn.has_value() ? 1U : 0U),
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

ArxReturnCode Level::roomDistance(RoomIndex room_a, RoomIndex room_b, std::uint8_t& out_has_distance,
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
  ArxReturnCode rc = level_validation::geometryError(geometry::validateVertex(vertex));
  if (rc != ARX_OK) return rc;
  const Vertex current = data_->geometry.vertices[static_cast<std::size_t>(index)];
  if (current.position == vertex.position) return ARX_OK;

  const bool preserve_vertex_validity = level_validation::has(data_->validation, LevelValidation::kVertices) &&
                                        geometry::validateVertex(vertex) == geometry::Error::kNone &&
                                        validLevelPosition(vertex.position);
  const auto& cached_bounds = data_->validation.derived.bounds;
  const bool preserve_bounds =
      preserve_vertex_validity && cached_bounds && strictlyContains(*cached_bounds, current.position);
  std::optional<ArxAabb> bounds = preserve_bounds ? cached_bounds : std::nullopt;
  if (bounds) math::expand(*bounds, vertex.position);

  geometry::setVertex(data_->geometry, index, vertex);
  level_validation::invalidate(data_->validation, LevelValidation::kVertices);
  if (preserve_vertex_validity) level_validation::markValid(data_->validation, LevelValidation::kVertices);
  if (bounds) data_->validation.derived.bounds = *bounds;
  return ARX_OK;
}

ArxReturnCode Level::addVertex(ArxLevelVertex vertex, VertexIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidVertexIndex;
    if (data_->geometry.vertices.size() >= static_cast<std::size_t>(kInvalidVertexIndex))
      return ARX_LEVEL_TOO_MANY_VERTICES;

    const Vertex internal = internalVertex(vertex);
    ArxReturnCode rc = level_validation::geometryError(geometry::validateVertex(internal));
    if (rc != ARX_OK) return rc;
    if (!validLevelPosition(internal.position)) return ARX_LEVEL_VERTEX_OUT_OF_BOUNDS;

    const bool had_vertices = !data_->geometry.vertices.empty();
    const bool vertices_stay_valid =
        !had_vertices || (level_validation::has(data_->validation, LevelValidation::kVertices) &&
                          data_->validation.derived.bounds.has_value());
    std::optional<ArxAabb> bounds =
        vertices_stay_valid && had_vertices ? data_->validation.derived.bounds : std::nullopt;
    if (!bounds)
      bounds = ArxAabb{internal.position, internal.position};
    else
      math::expand(*bounds, internal.position);

    out_index = geometry::addVertex(data_->geometry, internal);
    if (vertices_stay_valid) {
      level_validation::markValid(data_->validation, LevelValidation::kVertices);
      data_->validation.derived.bounds = bounds;
    }
    return ARX_OK;
  });
}

ArxReturnCode Level::addVertices(const ArxLevelVertex* vertices, std::size_t count,
                                 VertexIndex& out_first_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_first_index = kInvalidVertexIndex;
    if (count == 0) return ARX_INVALID_OPTIONS;
    if (vertices == nullptr) return ARX_INVALID_DATA_POINTER;
    if (count > static_cast<std::size_t>(kInvalidVertexIndex) - data_->geometry.vertices.size())
      return ARX_LEVEL_TOO_MANY_VERTICES;

    const bool had_vertices = !data_->geometry.vertices.empty();
    const bool vertices_stay_valid =
        !had_vertices || (level_validation::has(data_->validation, LevelValidation::kVertices) &&
                          data_->validation.derived.bounds.has_value());
    std::optional<ArxAabb> bounds =
        vertices_stay_valid && had_vertices ? data_->validation.derived.bounds : std::nullopt;

    std::vector<Vertex> internal_vertices;
    internal_vertices.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      const Vertex vertex = internalVertex(vertices[i]);
      ArxReturnCode rc = level_validation::geometryError(geometry::validateVertex(vertex));
      if (rc != ARX_OK) return rc;
      if (!validLevelPosition(vertex.position)) return ARX_LEVEL_VERTEX_OUT_OF_BOUNDS;
      if (!bounds)
        bounds = ArxAabb{vertex.position, vertex.position};
      else
        math::expand(*bounds, vertex.position);
      internal_vertices.push_back(vertex);
    }

    geometry::reserveVertexCapacity(
        data_->geometry,
        geometry::vertexCapacityForAppend(data_->geometry, count, static_cast<std::size_t>(kInvalidVertexIndex)));
    const VertexIndex appended_first = geometry::appendVertices(data_->geometry, internal_vertices);
    if (vertices_stay_valid) {
      level_validation::markValid(data_->validation, LevelValidation::kVertices);
      data_->validation.derived.bounds = bounds;
    }
    out_first_index = appended_first;
    return ARX_OK;
  });
}

ArxReturnCode Level::setFace(FaceIndex index, const ArxLevelFace& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
    if (data_->rooms.face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
    if (!validIndex(value.room, data_->rooms.definitions.size())) return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
    const std::size_t expected_colors = lights::expectedCornerColorCount(data_->geometry);
    if (!data_->lighting.corner_colors.empty() && data_->lighting.corner_colors.size() != expected_colors)
      return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

    Face face = internalFace(value);
    ArxReturnCode rc = level_validation::geometryError(geometry::validateFaces(
        std::span<const Face>(&face, 1), data_->geometry.vertices, data_->textures.textures.size()));
    if (rc != ARX_OK) return rc;
    face.normal = geometry::faceNormalOr(data_->geometry, face, {});
    rc = level_validation::faceTypes(std::span<const Face>(&face, 1));
    if (rc != ARX_OK) return rc;
    if (value.has_corner_colors != 0) {
      for (const ArxLevelCorner& corner : value.corners) {
        rc = level_validation::lightingError(lights::validateCornerColor(corner.color));
        if (rc != ARX_OK) return rc;
      }
    }

    std::optional<std::array<ArxColor3, 3>> colors;
    if (value.has_corner_colors != 0) colors = {value.corners[0].color, value.corners[1].color, value.corners[2].color};
    lights::setFaceCornerColors(data_->lighting, index, colors, data_->geometry.faces.size());
    geometry::setFace(data_->geometry, index, face);
    rooms::setFaceRoom(data_->rooms, index, value.room);
    if ((value.flags & kFaceBitQuad) != 0)
      log(ARX_LOG_WARN, "Level face edit: stripped QUAD flag from triangular face input");
    level_validation::invalidate(data_->validation, LevelValidation::kFaces);
    return ARX_OK;
  });
}

ArxReturnCode Level::addFace(const ArxLevelFace& value, FaceIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidFaceIndex;
    if (!validIndex(value.room, data_->rooms.definitions.size())) return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
    if (data_->geometry.faces.size() >= static_cast<std::size_t>(kInvalidFaceIndex)) return ARX_LEVEL_TOO_MANY_FACES;
    if (data_->rooms.face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
    if (!data_->lighting.corner_colors.empty() &&
        data_->lighting.corner_colors.size() != lights::expectedCornerColorCount(data_->geometry))
      return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

    Face face = internalFace(value);
    ArxAabb face_bounds;
    const geometry::Error error = geometry::validateFaces(
        std::span<const Face>(&face, 1), data_->geometry.vertices, data_->textures.textures.size(), &face_bounds);
    ArxReturnCode rc = level_validation::geometryError(error);
    if (rc != ARX_OK) return rc;
    face.normal = geometry::faceNormalOr(data_->geometry, face, {});
    rc = level_validation::faceTypes(std::span<const Face>(&face, 1));
    if (rc != ARX_OK) return rc;
    if (value.has_corner_colors != 0) {
      for (const ArxLevelCorner& corner : value.corners) {
        rc = level_validation::lightingError(lights::validateCornerColor(corner.color));
        if (rc != ARX_OK) return rc;
      }
    }

    const std::size_t face_count = data_->geometry.faces.size();
    const FaceIndex index = static_cast<FaceIndex>(face_count);
    std::optional<ArxAabb> referenced_bounds = level_validation::has(data_->validation, LevelValidation::kFaces)
                                                   ? data_->validation.derived.referenced_bounds
                                                   : std::nullopt;
    const bool faces_stay_valid = referenced_bounds.has_value();
    const bool face_rooms_stay_valid = level_validation::has(data_->validation, LevelValidation::kFaceRooms);
    const bool colors_stay_valid = level_validation::has(data_->validation, LevelValidation::kCornerColors);
    std::optional<std::array<ArxColor3, 3>> colors;
    if (value.has_corner_colors != 0) colors = {value.corners[0].color, value.corners[1].color, value.corners[2].color};

    try {
      out_index = geometry::addFace(data_->geometry, face);
      rooms::appendFaceRooms(data_->rooms, std::span<const RoomIndex>(&value.room, 1));
      lights::appendFaceCornerColors(data_->lighting, colors, face_count);
    } catch (...) {
      rooms::truncateFaceRooms(data_->rooms, face_count);
      if (data_->geometry.faces.size() > face_count) geometry::removeFace(data_->geometry, index);
      out_index = kInvalidFaceIndex;
      throw;
    }
    if ((value.flags & kFaceBitQuad) != 0)
      log(ARX_LOG_WARN, "Level face edit: stripped QUAD flag from triangular face input");
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
  });
}

ArxReturnCode Level::removeFace(FaceIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
    if (data_->rooms.face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
    if (!data_->lighting.corner_colors.empty() &&
        data_->lighting.corner_colors.size() != lights::expectedCornerColorCount(data_->geometry))
      return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

    lights::removeFaceCornerColors(data_->lighting, index, data_->geometry.faces.size());
    rooms::removeFaceRoom(data_->rooms, index);
    geometry::removeFace(data_->geometry, index);
    level_validation::invalidate(data_->validation, LevelValidation::kFaces);
    return ARX_OK;
  });
}

ArxReturnCode Level::compactVertices(std::size_t* removed) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validateFaces();
    if (rc != ARX_OK) return rc;
    const std::size_t count = geometry::compactVertices(data_->geometry);
    data_->validation.derived.bounds = data_->validation.derived.referenced_bounds;
    if (removed) *removed = count;
    return ARX_OK;
  });
}

ArxReturnCode Level::compactTextures(std::size_t* removed) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validateFaces();
    if (rc != ARX_OK) return rc;
    std::vector<std::uint8_t> used;
    rc = level_validation::geometryError(
        geometry::collectTextureUsage(data_->geometry, data_->textures.textures.size(), used));
    if (rc != ARX_OK) return rc;
    std::vector<TextureIndex> remap;
    std::size_t count = 0;
    rc = level_validation::textureError(textures::compact(data_->textures, used, remap, count));
    if (rc != ARX_OK) return rc;
    geometry::remapTextureReferences(data_->geometry, remap);
    if (removed) *removed = count;
    return ARX_OK;
  });
}

ArxReturnCode Level::weldVertices() noexcept { return weldVertices(VertexWeldOptions{}); }

ArxReturnCode Level::weldVertices(const VertexWeldOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validateMesh();
    if (rc != ARX_OK) return rc;

    geometry::VertexWeldOptions module_options;
    if (!geometryWeldOptions(options, module_options)) return ARX_INVALID_OPTIONS;

    rooms::VertexWeldSegments collected;
    rc = level_validation::roomsError(
        rooms::collectVertexWeldSegments(data_->geometry, data_->rooms, module_options.radius, collected));
    if (rc != ARX_OK) return rc;

    std::vector<geometry::VertexWeldSegment> segments;
    segments.reserve(data_->rooms.definitions.size());
    for (std::size_t room = 0; room < data_->rooms.definitions.size(); ++room) {
      segments.push_back(
          {std::span<const VertexIndex>(collected.vertices)
               .subspan(collected.offsets[room], collected.offsets[room + 1U] - collected.offsets[room])});
    }

    geometry::GeometryRemap remap;
    const geometry::Error error =
        geometry::weldVerticesSegmented(data_->geometry,
                                        {.segments = segments, .protected_vertices = collected.protected_vertices},
                                        module_options,
                                        &remap);
    if (error != geometry::Error::kNone) return level_validation::geometryError(error);

    rooms::remapFaceRooms(data_->rooms, remap.faces);
    lights::remapCornerColors(data_->lighting, remap.faces);
    level_validation::invalidate(data_->validation, LevelValidation::kVertices);
    return ARX_OK;
  });
}

ArxReturnCode Level::snapGeometryToPortals() noexcept { return snapGeometryToPortals(PortalSnapOptions{}); }

ArxReturnCode Level::snapGeometryToPortals(const PortalSnapOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validateMesh();
    if (rc != ARX_OK) return rc;
    rc = validatePortals();
    if (rc != ARX_OK) return rc;

    GeometryData next = data_->geometry;
    rooms::PortalSnapStatistics statistics;
    rc = level_validation::roomsError(
        rooms::snapGeometryToPortals(next, data_->rooms, {.radius = options.radius}, &statistics));
    if (rc != ARX_OK) return rc;

    LevelValidationState next_validation;
    rc = validateMeshCoherence(next,
                               data_->textures,
                               data_->rooms.face_rooms,
                               data_->lighting.corner_colors,
                               data_->rooms.definitions.size(),
                               next_validation);
    if (rc != ARX_OK) return rc;

    LevelValidationState final_validation = data_->validation;
    level_validation::invalidate(final_validation, LevelValidation::kVertices);
    final_validation.derived = next_validation.derived;
    level_validation::markValid(final_validation, next_validation.valid);
    geometry::replace(data_->geometry, std::move(next));
    data_->validation = final_validation;

    log(ARX_LOG_INFO,
        "Level portal snapping: {} candidate vertices, {} snapped, {} already aligned",
        statistics.candidates,
        statistics.snapped,
        statistics.already_aligned);
    const std::size_t skipped =
        statistics.skipped_room_conflict + statistics.skipped_ambiguous + statistics.skipped_face_safety;
    if (skipped != 0) {
      log(ARX_LOG_WARN,
          "Level portal snapping skipped {} vertices: {} shared with unrelated rooms, {} ambiguous, {} rejected "
          "to preserve incident faces",
          skipped,
          statistics.skipped_room_conflict,
          statistics.skipped_ambiguous,
          statistics.skipped_face_safety);
    }
    return ARX_OK;
  });
}

ArxReturnCode Level::setTexture(TextureIndex index, const ArxTextureView& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
    Texture texture;
    if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
    textures::PathRepairInfo repair;
    ArxReturnCode rc = level_validation::textureError(textures::repairPath(data_->textures, texture, index, &repair));
    if (rc != ARX_OK) return rc;
    rc = level_validation::textureError(textures::validateTexture(texture));
    if (rc != ARX_OK) return rc;
    textures::setTexture(data_->textures, index, std::move(texture));
    for (const textures::PathRepairInfo::Repair& item : repair.repairs)
      log(ARX_LOG_WARN, "Level texture: '{}' normalized to '{}'", item.original, item.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Level::addTexture(const ArxTextureView& value, TextureIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kNoTexture;
    Texture texture;
    if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
    ArxReturnCode rc =
        level_validation::textureError(textures::validateTextureCount(data_->textures.textures.size() + 1U));
    if (rc != ARX_OK) return rc;
    textures::PathRepairInfo repair;
    rc = level_validation::textureError(textures::repairPath(data_->textures, texture, kNoTexture, &repair));
    if (rc != ARX_OK) return rc;
    rc = level_validation::textureError(textures::validateTexture(texture));
    if (rc != ARX_OK) return rc;
    const bool textures_stay_valid =
        data_->textures.textures.empty() || level_validation::has(data_->validation, LevelValidation::kTextures);
    const TextureIndex index = textures::addTexture(data_->textures, std::move(texture));
    if (textures_stay_valid) level_validation::markValid(data_->validation, LevelValidation::kTextures);
    out_index = index;
    for (const textures::PathRepairInfo::Repair& item : repair.repairs)
      log(ARX_LOG_WARN, "Level texture: '{}' normalized to '{}'", item.original, item.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Level::rebaseTexturePaths(std::string_view directory) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    textures::PathRebaseInfo info;
    const ArxReturnCode rc = level_validation::textureError(textures::rebasePaths(data_->textures, directory, &info));
    if (rc != ARX_OK) return rc;
    for (const textures::PathRebaseInfo::Repair& repair : info.repairs)
      log(ARX_LOG_WARN, "Level texture rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Level::setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
    if (encoded_image.size == 0) return ARX_LEVEL_BAD_TEXTURE_IMAGE;
    std::vector<std::uint8_t> next_image;
    if (!copyImage(encoded_image, next_image)) return ARX_INVALID_DATA_POINTER;
    const ArxReturnCode rc = level_validation::textureError(textures::validateEncodedImage(next_image));
    if (rc != ARX_OK) return rc;
    textures::setEncodedImage(data_->textures, index, std::move(next_image));
    return ARX_OK;
  });
}

ArxReturnCode Level::clearTextureImage(TextureIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
    textures::clearEncodedImage(data_->textures, index);
    return ARX_OK;
  });
}

ArxReturnCode Level::setFaceRoom(FaceIndex face, RoomIndex room) noexcept {
  if (!validIndex(face, data_->rooms.face_rooms.size())) return ARX_INDEX_OUT_OF_RANGE;
  if (!validIndex(room, data_->rooms.definitions.size())) return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
  rooms::setFaceRoom(data_->rooms, face, room);
  return ARX_OK;
}

ArxReturnCode Level::setCornerColor(FaceIndex face, std::uint8_t corner, ArxColor3 color) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(face, data_->geometry.faces.size()) || corner >= 3) return ARX_INDEX_OUT_OF_RANGE;
    ArxReturnCode rc = level_validation::lightingError(lights::validateCornerColors(data_->lighting, data_->geometry));
    if (rc != ARX_OK) return rc;
    rc = level_validation::lightingError(lights::validateCornerColor(color));
    if (rc != ARX_OK) return rc;
    lights::setCornerColor(data_->lighting, face, corner, color, data_->geometry.faces.size());
    return ARX_OK;
  });
}

void Level::clearCornerColors() noexcept { lights::clearCornerColors(data_->lighting); }

ArxReturnCode Level::replaceMesh(const ArxLevelMeshInput& mesh) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (mesh.vertex_count > static_cast<std::size_t>(kInvalidVertexIndex)) return ARX_LEVEL_TOO_MANY_VERTICES;
    if (mesh.face_count > static_cast<std::size_t>(kInvalidFaceIndex)) return ARX_LEVEL_TOO_MANY_FACES;
    if (mesh.texture_count > static_cast<std::size_t>(kNoTexture)) return ARX_LEVEL_TOO_MANY_TEXTURES;
    if ((mesh.vertex_count != 0 && mesh.vertices == nullptr) || (mesh.face_count != 0 && mesh.faces == nullptr) ||
        (mesh.texture_count != 0 && mesh.textures == nullptr))
      return ARX_INVALID_DATA_POINTER;

    GeometryData geometry;
    TexturesData texture_data;
    geometry.vertices.reserve(mesh.vertex_count);
    for (std::size_t i = 0; i < mesh.vertex_count; ++i) geometry.vertices.push_back(internalVertex(mesh.vertices[i]));
    geometry.faces.reserve(mesh.face_count);
    std::size_t stripped_quad_flags = 0;
    for (std::size_t i = 0; i < mesh.face_count; ++i) {
      stripped_quad_flags += static_cast<std::size_t>((mesh.faces[i].flags & kFaceBitQuad) != 0);
      geometry.faces.push_back(internalFace(mesh.faces[i]));
    }
    texture_data.textures.resize(mesh.texture_count);
    for (std::size_t i = 0; i < mesh.texture_count; ++i) {
      if (!internalTexture(mesh.textures[i], texture_data.textures[i])) return ARX_INVALID_DATA_POINTER;
    }
    textures::PathRepairInfo texture_repairs;
    ArxReturnCode rc = level_validation::textureError(textures::repairPaths(texture_data.textures, &texture_repairs));
    if (rc != ARX_OK) return rc;

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
    rc = validateMeshCoherence(
        geometry, texture_data, face_rooms, corner_colors, data_->rooms.definitions.size(), next_validation);
    if (rc != ARX_OK) return rc;
    for (Face& face : geometry.faces) face.normal = geometry::faceNormalOr(geometry, face, {});

    LevelValidationState final_validation = data_->validation;
    level_validation::invalidate(final_validation,
                                 LevelValidation::kVertices | LevelValidation::kTextures | LevelValidation::kFaces |
                                     LevelValidation::kFaceRooms | LevelValidation::kCornerColors |
                                     LevelValidation::kNavSurface | LevelValidation::kAnchors);
    final_validation.derived = next_validation.derived;
    level_validation::markValid(final_validation,
                                next_validation.valid | LevelValidation::kNavSurface | LevelValidation::kAnchors |
                                    LevelValidation::kAnchorConnections);

    geometry::replace(data_->geometry, std::move(geometry));
    textures::replaceTextures(data_->textures, std::move(texture_data.textures));
    rooms::replaceFaceRooms(data_->rooms, std::move(face_rooms));
    lights::replaceCornerColors(data_->lighting, std::move(corner_colors));
    navigation::clearSurface(data_->navigation);
    navigation::clearAnchors(data_->navigation);
    data_->validation = final_validation;
    for (const textures::PathRepairInfo::Repair& repair : texture_repairs.repairs)
      log(ARX_LOG_WARN,
          "Level mesh replacement: texture path '{}' normalized to '{}'",
          repair.original,
          repair.repaired);
    if (stripped_quad_flags != 0)
      log(ARX_LOG_WARN, "Level mesh replacement: stripped QUAD flag from {} triangular face(s)", stripped_quad_flags);
    return ARX_OK;
  });
}

void Level::clearMesh() noexcept {
  level_validation::invalidate(data_->validation,
                               LevelValidation::kVertices | LevelValidation::kTextures | LevelValidation::kFaces |
                                   LevelValidation::kFaceRooms | LevelValidation::kCornerColors |
                                   LevelValidation::kNavSurface | LevelValidation::kAnchors);
  geometry::clear(data_->geometry);
  textures::clear(data_->textures);
  rooms::clearFaceRooms(data_->rooms);
  lights::clearCornerColors(data_->lighting);
  navigation::clearSurface(data_->navigation);
  navigation::clearAnchors(data_->navigation);
}

ArxReturnCode Level::setRoom(RoomIndex index, const ArxLevelRoom& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->rooms.definitions.size())) return ARX_INDEX_OUT_OF_RANGE;
    Room room;
    if (!internalRoom(value, room)) return ARX_INVALID_DATA_POINTER;
    if (data_->rooms.definitions[static_cast<std::size_t>(index)].name == room.name) return ARX_OK;
    rooms::repairRoomName(data_->rooms, room, index);
    ArxReturnCode rc = level_validation::roomsError(rooms::validateRoom(room));
    if (rc != ARX_OK) return rc;
    rooms::setRoom(data_->rooms, index, std::move(room));
    return ARX_OK;
  });
}

ArxReturnCode Level::addRoom(const ArxLevelRoom& value, RoomIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidRoomIndex;
    Room room;
    if (!internalRoom(value, room)) return ARX_INVALID_DATA_POINTER;
    if (data_->rooms.definitions.size() >= level_validation::kMaxRooms) return ARX_LEVEL_TOO_MANY_ROOMS;
    rooms::repairRoomName(data_->rooms, room);
    ArxReturnCode rc = level_validation::roomsError(rooms::validateRoom(room));
    if (rc != ARX_OK) return rc;
    out_index = rooms::addRoom(data_->rooms, std::move(room));
    return ARX_OK;
  });
}

ArxReturnCode Level::removeRoom(RoomIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->rooms.definitions.size())) return ARX_INDEX_OUT_OF_RANGE;
    const bool rooms_stay_valid =
        level_validation::has(data_->validation, LevelValidation::kRooms) && data_->rooms.definitions.size() > 1;
    ArxReturnCode rc = level_validation::roomsError(rooms::validateRoomRemoval(data_->rooms, index));
    if (rc != ARX_OK) return rc;
    rooms::removeRoom(data_->rooms, index);
    level_validation::invalidate(data_->validation, LevelValidation::kRooms);
    if (rooms_stay_valid) level_validation::markValid(data_->validation, LevelValidation::kRooms);
    return ARX_OK;
  });
}

ArxReturnCode Level::setPortal(PortalIndex index, const ArxLevelPortal& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->rooms.portals.size())) return ARX_INDEX_OUT_OF_RANGE;
    Portal portal;
    if (!internalPortal(value, portal)) return ARX_INVALID_DATA_POINTER;
    if (!validPortalShape(value.shape)) return ARX_LEVEL_BAD_PORTAL_SHAPE;
    rooms::repairPortalName(data_->rooms, portal, index);
    ArxReturnCode rc = level_validation::roomsError(rooms::validatePortal(portal, data_->rooms.definitions.size()));
    if (rc != ARX_OK) return rc;
    if (!level_validation::validPortalBounds(portal)) return ARX_LEVEL_PORTAL_OUT_OF_BOUNDS;
    rooms::setPortal(data_->rooms, index, std::move(portal));
    return ARX_OK;
  });
}

ArxReturnCode Level::addPortal(const ArxLevelPortal& value, PortalIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidPortalIndex;
    Portal portal;
    if (!internalPortal(value, portal)) return ARX_INVALID_DATA_POINTER;
    if (!validPortalShape(value.shape)) return ARX_LEVEL_BAD_PORTAL_SHAPE;
    ArxReturnCode rc = level_validation::roomsError(rooms::validatePortalCount(data_->rooms.portals.size() + 1));
    if (rc != ARX_OK) return rc;
    rooms::repairPortalName(data_->rooms, portal);
    rc = level_validation::roomsError(rooms::validatePortal(portal, data_->rooms.definitions.size()));
    if (rc != ARX_OK) return rc;
    if (!level_validation::validPortalBounds(portal)) return ARX_LEVEL_PORTAL_OUT_OF_BOUNDS;
    out_index = rooms::addPortal(data_->rooms, std::move(portal));
    return ARX_OK;
  });
}

ArxReturnCode Level::removePortal(PortalIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->rooms.portals.size())) return ARX_INDEX_OUT_OF_RANGE;
    rooms::removePortal(data_->rooms, index);
    return ARX_OK;
  });
}

ArxReturnCode Level::flattenPortals() noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validatePortals();
    if (rc != ARX_OK) return rc;

    const bool had_room_distances = !data_->rooms.distances.empty();
    RoomsData next = data_->rooms;
    rooms::PortalFlattenStatistics statistics;
    rc = level_validation::roomsError(rooms::flattenPortals(next, &statistics));
    if (rc != ARX_OK) return rc;
    if (statistics.flattened_quads != 0) {
      for (const Portal& portal : next.portals)
        if (!level_validation::validPortalBounds(portal)) return ARX_LEVEL_PORTAL_OUT_OF_BOUNDS;
      data_->rooms = std::move(next);
    }

    log(ARX_LOG_INFO,
        "Level portal flattening: {} quads flattened, {} already planar, {} triangles unchanged",
        statistics.flattened_quads,
        statistics.already_planar_quads,
        statistics.triangles);
    if (had_room_distances && statistics.flattened_quads != 0) {
      log(ARX_LOG_INFO, "Level portal flattening discarded room distances because portal geometry changed");
    }
    return ARX_OK;
  });
}

ArxReturnCode Level::setRoomDistance(const ArxLevelRoomDistance& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    RoomIndex low_room = 0;
    RoomIndex high_room = 0;
    RoomDistance distance;
    ArxReturnCode rc = internalRoomDistance(value, data_->rooms, low_room, high_room, distance);
    if (rc != ARX_OK) return rc;

    rc = level_validation::roomsError(rooms::validateRoomDistances(data_->rooms));
    if (rc != ARX_OK) return rc;
    rc = level_validation::roomsError(rooms::validateRoomDistance(distance, data_->rooms, low_room, high_room));
    if (rc != ARX_OK) return rc;
    rooms::setRoomDistance(data_->rooms, low_room, high_room, distance);
    if (rc != ARX_OK) return rc;
    level_validation::markValid(data_->validation, LevelValidation::kRoomDistances);
    return ARX_OK;
  });
}

ArxReturnCode Level::replaceRoomDistances(const ArxLevelRoomDistance* distances, std::size_t count) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
    rooms::replaceRoomDistances(data_->rooms, std::move(next));
    level_validation::markValid(data_->validation, LevelValidation::kRoomDistances);
    return ARX_OK;
  });
}

void Level::clearRoomDistances() noexcept { rooms::clearRoomDistances(data_->rooms); }

ArxReturnCode Level::setAnchor(AnchorIndex index, const ArxLevelAnchor& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->navigation.anchors.size())) return ARX_INDEX_OUT_OF_RANGE;
    Anchor anchor;
    if (!internalAnchor(value, anchor)) return ARX_INVALID_DATA_POINTER;
    navigation::repairAnchorName(data_->navigation, anchor, index);
    ArxReturnCode rc = level_validation::navigationError(navigation::validateAnchor(anchor));
    if (rc != ARX_OK) return rc;
    navigation::setAnchor(data_->navigation, index, std::move(anchor));
    level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
    return ARX_OK;
  });
}

ArxReturnCode Level::addAnchor(const ArxLevelAnchor& value, AnchorIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidAnchorIndex;
    Anchor anchor;
    if (!internalAnchor(value, anchor)) return ARX_INVALID_DATA_POINTER;
    ArxReturnCode rc =
        level_validation::navigationError(navigation::validateAnchorCount(data_->navigation.anchors.size() + 1));
    if (rc != ARX_OK) return rc;
    navigation::repairAnchorName(data_->navigation, anchor);
    rc = level_validation::navigationError(navigation::validateAnchor(anchor));
    if (rc != ARX_OK) return rc;
    out_index = navigation::addAnchor(data_->navigation, std::move(anchor));
    level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
    return ARX_OK;
  });
}

ArxReturnCode Level::removeAnchor(AnchorIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->navigation.anchors.size())) return ARX_INDEX_OUT_OF_RANGE;
    const bool anchors_stay_valid = level_validation::has(data_->validation, LevelValidation::kAnchors);
    navigation::removeAnchor(data_->navigation, index);
    level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
    if (anchors_stay_valid) level_validation::markValid(data_->validation, LevelValidation::kAnchors);
    return ARX_OK;
  });
}

ArxReturnCode Level::setAnchorConnection(AnchorConnectionIndex index, ArxLevelAnchorConnection value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->navigation.connections.size())) return ARX_INDEX_OUT_OF_RANGE;
    const AnchorConnection connection = internalConnection(value);
    ArxReturnCode rc = level_validation::navigationError(
        navigation::validateConnectionPlacement(data_->navigation, index, connection));
    if (rc != ARX_OK) return rc;
    navigation::setConnection(data_->navigation, index, connection);
    level_validation::markValid(data_->validation, LevelValidation::kAnchorConnections);
    return ARX_OK;
  });
}

ArxReturnCode Level::addAnchorConnection(ArxLevelAnchorConnection value, AnchorConnectionIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidAnchorConnectionIndex;
    const AnchorConnection connection = internalConnection(value);
    ArxReturnCode rc = level_validation::navigationError(
        navigation::validateConnectionInsertion(data_->navigation, connection, out_index));
    if (rc != ARX_OK) return rc;
    navigation::insertConnection(data_->navigation, out_index, connection);
    return ARX_OK;
  });
}

ArxReturnCode Level::removeAnchorConnection(AnchorConnectionIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->navigation.connections.size())) return ARX_INDEX_OUT_OF_RANGE;
    navigation::removeConnection(data_->navigation, index);
    return ARX_OK;
  });
}

ArxReturnCode Level::replaceAnchors(const ArxLevelAnchorsInput& input) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
    navigation::repairAnchorNames(anchors);
    ArxReturnCode rc = level_validation::navigationError(navigation::validateAnchorDefinitions(anchors));
    if (rc != ARX_OK) return rc;
    rc = level_validation::navigationError(navigation::validateConnections(anchors, connections));
    if (rc != ARX_OK) return rc;
    navigation::replaceAnchors(data_->navigation, std::move(anchors), std::move(connections));
    level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
    level_validation::markValid(data_->validation, LevelValidation::kAnchorConnections);
    return ARX_OK;
  });
}

void Level::clearAnchors() noexcept { navigation::clearAnchors(data_->navigation); }

ArxReturnCode Level::setNavSurface(const ArxLevelNavSurfaceInput& input) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
    navigation::setSurface(data_->navigation, std::move(surface));
    level_validation::markValid(data_->validation, LevelValidation::kNavSurface);
    return ARX_OK;
  });
}

void Level::clearNavSurface() noexcept { navigation::clearSurface(data_->navigation); }

ArxReturnCode Level::setLight(LightIndex index, const ArxLevelLight& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->lighting.lights.size())) return ARX_INDEX_OUT_OF_RANGE;
    Light light;
    if (!internalLight(value, light)) return ARX_INVALID_DATA_POINTER;
    lights::repairLightName(data_->lighting, light, index);
    ArxReturnCode rc = level_validation::lightingError(lights::validateLight(light));
    if (rc != ARX_OK) return rc;
    lights::setLight(data_->lighting, index, std::move(light));
    return ARX_OK;
  });
}

ArxReturnCode Level::addLight(const ArxLevelLight& value, LightIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidLightIndex;
    Light light;
    if (!internalLight(value, light)) return ARX_INVALID_DATA_POINTER;
    ArxReturnCode rc = level_validation::lightingError(lights::validateLightCount(data_->lighting.lights.size() + 1));
    if (rc != ARX_OK) return rc;
    lights::repairLightName(data_->lighting, light);
    rc = level_validation::lightingError(lights::validateLight(light));
    if (rc != ARX_OK) return rc;
    out_index = lights::addLight(data_->lighting, std::move(light));
    return ARX_OK;
  });
}

ArxReturnCode Level::removeLight(LightIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->lighting.lights.size())) return ARX_INDEX_OUT_OF_RANGE;
    lights::removeLight(data_->lighting, index);
    return ARX_OK;
  });
}

ArxReturnCode Level::setPlayerSpawn(const ArxLevelPlayerSpawn& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (value.is_usable == 0) {
      clearPlayerSpawn();
      return ARX_OK;
    }
    PlayerSpawn spawn = {.position = value.position, .rotation = value.rotation};
    if (!math::normalizeRotation(spawn.rotation)) return ARX_LEVEL_BAD_PLAYER_SPAWN;
    ArxReturnCode rc = level_validation::sceneError(scene::validatePlayerSpawn(spawn));
    if (rc != ARX_OK) return rc;
    scene::setPlayerSpawn(data_->scene, spawn);
    level_validation::markValid(data_->validation, LevelValidation::kPlayerSpawn);
    return ARX_OK;
  });
}

void Level::clearPlayerSpawn() noexcept {
  scene::clearPlayerSpawn(data_->scene);
  level_validation::markValid(data_->validation, LevelValidation::kPlayerSpawn);
}

ArxReturnCode Level::setEntity(EntityIndex index, const ArxLevelEntity& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->scene.entities.size())) return ARX_INDEX_OUT_OF_RANGE;
    Entity entity;
    if (!internalEntity(value, entity)) return ARX_INVALID_DATA_POINTER;
    if (!math::normalizeRotation(entity.rotation)) return ARX_LEVEL_BAD_ENTITY_ROTATION;
    scene::repairEntityName(data_->scene, entity, index);
    ArxReturnCode rc = level_validation::sceneError(scene::validateEntity(entity));
    if (rc != ARX_OK) return rc;
    scene::setEntity(data_->scene, index, std::move(entity));
    return ARX_OK;
  });
}

ArxReturnCode Level::addEntity(const ArxLevelEntity& value, EntityIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidEntityIndex;
    Entity entity;
    if (!internalEntity(value, entity)) return ARX_INVALID_DATA_POINTER;
    if (!math::normalizeRotation(entity.rotation)) return ARX_LEVEL_BAD_ENTITY_ROTATION;
    ArxReturnCode rc = level_validation::sceneError(scene::validateEntityCount(data_->scene.entities.size() + 1));
    if (rc != ARX_OK) return rc;
    scene::repairEntityName(data_->scene, entity);
    rc = level_validation::sceneError(scene::validateEntity(entity));
    if (rc != ARX_OK) return rc;
    out_index = scene::addEntity(data_->scene, std::move(entity));
    return ARX_OK;
  });
}

ArxReturnCode Level::removeEntity(EntityIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->scene.entities.size())) return ARX_INDEX_OUT_OF_RANGE;
    scene::removeEntity(data_->scene, index);
    return ARX_OK;
  });
}

ArxReturnCode Level::setFog(FogIndex index, const ArxLevelFog& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->scene.fogs.size())) return ARX_INDEX_OUT_OF_RANGE;
    Fog fog;
    if (!internalFog(value, fog)) return ARX_INVALID_DATA_POINTER;
    if (!math::normalizeRotation(fog.rotation)) return ARX_LEVEL_BAD_FOG_ROTATION;
    scene::repairFogName(data_->scene, fog, index);
    ArxReturnCode rc = level_validation::sceneError(scene::validateFog(fog));
    if (rc != ARX_OK) return rc;
    scene::setFog(data_->scene, index, std::move(fog));
    return ARX_OK;
  });
}

ArxReturnCode Level::addFog(const ArxLevelFog& value, FogIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidFogIndex;
    Fog fog;
    if (!internalFog(value, fog)) return ARX_INVALID_DATA_POINTER;
    if (!math::normalizeRotation(fog.rotation)) return ARX_LEVEL_BAD_FOG_ROTATION;
    ArxReturnCode rc = level_validation::sceneError(scene::validateFogCount(data_->scene.fogs.size() + 1));
    if (rc != ARX_OK) return rc;
    scene::repairFogName(data_->scene, fog);
    rc = level_validation::sceneError(scene::validateFog(fog));
    if (rc != ARX_OK) return rc;
    out_index = scene::addFog(data_->scene, std::move(fog));
    return ARX_OK;
  });
}

ArxReturnCode Level::removeFog(FogIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->scene.fogs.size())) return ARX_INDEX_OUT_OF_RANGE;
    scene::removeFog(data_->scene, index);
    return ARX_OK;
  });
}

ArxReturnCode Level::setZone(ZoneIndex index, const ArxLevelZoneInput& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->scene.zones.size())) return ARX_INDEX_OUT_OF_RANGE;
    Zone zone;
    if (!internalZone(value, zone)) return ARX_INVALID_DATA_POINTER;
    if (!validZoneHeightMode(value.value.height_mode)) return ARX_LEVEL_BAD_ZONE_HEIGHT_MODE;
    scene::repairZoneName(data_->scene, zone, index);
    ArxReturnCode rc = level_validation::sceneError(scene::validateZone(zone));
    if (rc != ARX_OK) return rc;
    scene::setZone(data_->scene, index, std::move(zone));
    return ARX_OK;
  });
}

ArxReturnCode Level::addZone(const ArxLevelZoneInput& value, ZoneIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidZoneIndex;
    Zone zone;
    if (!internalZone(value, zone)) return ARX_INVALID_DATA_POINTER;
    if (!validZoneHeightMode(value.value.height_mode)) return ARX_LEVEL_BAD_ZONE_HEIGHT_MODE;
    ArxReturnCode rc = level_validation::sceneError(scene::validateZoneCount(data_->scene.zones.size() + 1));
    if (rc != ARX_OK) return rc;
    scene::repairZoneName(data_->scene, zone);
    rc = level_validation::sceneError(scene::validateZone(zone));
    if (rc != ARX_OK) return rc;
    out_index = scene::addZone(data_->scene, std::move(zone));
    return ARX_OK;
  });
}

ArxReturnCode Level::removeZone(ZoneIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->scene.zones.size())) return ARX_INDEX_OUT_OF_RANGE;
    scene::removeZone(data_->scene, index);
    return ARX_OK;
  });
}

ArxReturnCode Level::setPath(PathIndex index, const ArxLevelPathInput& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->scene.paths.size())) return ARX_INDEX_OUT_OF_RANGE;
    Path path;
    if (!internalPath(value, path)) return ARX_INVALID_DATA_POINTER;
    if (!validPathNodeTypes(value)) return ARX_LEVEL_BAD_PATH_NODE_TYPE;
    scene::repairPathName(data_->scene, path, index);
    ArxReturnCode rc = level_validation::sceneError(scene::validatePath(path));
    if (rc != ARX_OK) return rc;
    scene::setPath(data_->scene, index, std::move(path));
    return ARX_OK;
  });
}

ArxReturnCode Level::addPath(const ArxLevelPathInput& value, PathIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidPathIndex;
    Path path;
    if (!internalPath(value, path)) return ARX_INVALID_DATA_POINTER;
    if (!validPathNodeTypes(value)) return ARX_LEVEL_BAD_PATH_NODE_TYPE;
    ArxReturnCode rc = level_validation::sceneError(scene::validatePathCount(data_->scene.paths.size() + 1));
    if (rc != ARX_OK) return rc;
    scene::repairPathName(data_->scene, path);
    rc = level_validation::sceneError(scene::validatePath(path));
    if (rc != ARX_OK) return rc;
    out_index = scene::addPath(data_->scene, std::move(path));
    return ARX_OK;
  });
}

ArxReturnCode Level::removePath(PathIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->scene.paths.size())) return ARX_INDEX_OUT_OF_RANGE;
    scene::removePath(data_->scene, index);
    return ARX_OK;
  });
}

}  // namespace pistoris
