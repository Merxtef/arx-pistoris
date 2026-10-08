// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level.hpp"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/level/images.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/texture.h"

#include "api/bulk_copy.h"
#include "api/result_failure.h"
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
#include "utils/encoded_image.h"
#include "utils/log.h"
#include "utils/math/bounds.h"
#include "utils/math/finite.h"
#include "utils/math/rotation.h"

#include <algorithm>
#include <array>
#include <cassert>
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

bool checkedScalarCount(std::size_t count, std::size_t width, std::size_t& out) noexcept {
  if (count > std::numeric_limits<std::size_t>::max() / width) return false;
  out = count * width;
  return true;
}

ArxStringView borrowedString(const std::string& value) noexcept { return {value.data(), value.size()}; }

ArxEncodedImageView borrowedImage(const std::vector<std::uint8_t>& value) noexcept {
  if (value.empty()) return {};
  return {value.data(), value.size()};
}

std::optional<image::Format> internalImageFormat(ImageFormat format) noexcept {
  switch (format) {
    case ImageFormat::kPng:
      return image::Format::kPng;
    case ImageFormat::kBmp:
      return image::Format::kBmp;
    case ImageFormat::kTga:
      return image::Format::kTga;
    case ImageFormat::kUnknown:
    case ImageFormat::kJpeg:
      return std::nullopt;
  }
  return std::nullopt;
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
  result.normal = face.normal;
  return result;
}

std::array<ArxColor3, 3> faceCornerColors(const ArxLevelFace& face) noexcept {
  return {face.corners[0].color, face.corners[1].color, face.corners[2].color};
}

bool hasNonDefaultCornerColor(const std::array<ArxColor3, 3>& colors) noexcept {
  for (const ArxColor3& color : colors) {
    if (color.r != lights::kDefaultCornerColor.r || color.g != lights::kDefaultCornerColor.g ||
        color.b != lights::kDefaultCornerColor.b)
      return true;
  }
  return false;
}

std::optional<std::array<ArxColor3, 3>> storedFaceCornerColors(const ArxLevelFace& face) noexcept {
  auto colors = faceCornerColors(face);
  if (!hasNonDefaultCornerColor(colors)) return std::nullopt;
  return colors;
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
  out.room_1 = portal.room_front;
  out.room_2 = portal.room_back;
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

Level::Level(const Level& other) : data_(other.data_ ? std::make_unique<Data>(*other.data_) : nullptr) {}

Level::Level(Level&& other) noexcept = default;

Level& Level::operator=(const Level& other) {
  if (this != &other) {
    Level copy(other);
    swap(copy);
  }
  return *this;
}

Level& Level::operator=(Level&& other) noexcept = default;

void Level::swap(Level& other) noexcept { data_.swap(other.data_); }

LevelResult<void> Level::reset() noexcept {
  return api_detail::levelBoundary(resourcePath(), [&]() -> LevelResult<void> {
    data_ = std::make_unique<Data>();
    return {};
  });
}

LevelResult<void> Level::validateGeometry() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::mesh(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
}

LevelResult<void> Level::validate() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const ArxReturnCode rc = levelResourceError(resource::validate(data_->resource, ARX_RESOURCE_KIND_LEVEL));
        if (rc != ARX_OK) return rc;
        return level_validation::all(*data_, data_->validation);
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
}

LevelResult<void> Level::validateVertices() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::vertices(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kVertex));
}

LevelResult<void> Level::validateTextures() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::textures(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kTexture));
}

LevelResult<void> Level::validateFaces() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::faces(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
}

LevelResult<void> Level::validateFaceRooms() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::faceRooms(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
}

LevelResult<void> Level::validateCornerColors() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::cornerColors(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
}

LevelResult<void> Level::validateRooms() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::rooms(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kRoom));
}

LevelResult<void> Level::validatePortals() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::portals(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kPortal));
}

LevelResult<void> Level::validateRoomDistances() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::roomDistances(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kRoomDistance));
}

LevelResult<void> Level::validateNavSurface() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::navSurface(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kNavSurfaceTriangle));
}

LevelResult<void> Level::validateAnchors() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::anchors(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kAnchor));
}

LevelResult<void> Level::validateAnchorConnections() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::anchorConnections(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kAnchorConnection));
}

LevelResult<void> Level::validateLights() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::lightSources(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kLight));
}

LevelResult<void> Level::validatePlayerSpawn() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::playerSpawn(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kPlayerSpawn));
}

LevelResult<void> Level::validateEntities() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::entities(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kEntity));
}

LevelResult<void> Level::validateFogs() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::fogs(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFog));
}

LevelResult<void> Level::validateZones() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::zones(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kZone));
}

LevelResult<void> Level::validatePaths() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::paths(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kPath));
}

LevelResult<void> Level::validateMinimap() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::minimap(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kMinimap));
}

LevelResult<void> Level::validateLoadingScreen() const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::resourceValidationBoundary<LevelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return level_validation::loadingScreen(*data_, data_->validation); },
      api_detail::resourceLocation(resourcePath(), LevelElement::kLoadingScreen));
}

std::string_view Level::resourcePath() const noexcept {
  return data_ ? std::string_view(data_->resource.path) : std::string_view{};
}

LevelResult<void> Level::setResourcePath(std::string_view resource_path) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        std::string path;
        const ArxReturnCode rc = levelResourceError(resource::repairPath(ARX_RESOURCE_KIND_LEVEL, resource_path, path));
        if (rc != ARX_OK) return rc;
        resource::setPath(data_->resource, std::move(path));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
}

std::optional<ArxAabb> Level::bounds() const noexcept {
  if (!data_ || !validateVertices()) return std::nullopt;
  return data_->validation.derived.bounds;
}

std::optional<ArxAabb> Level::referencedBounds() const noexcept {
  if (!data_ || !validateFaces()) return std::nullopt;
  return data_->validation.derived.referenced_bounds;
}

Level::MinimapView Level::minimap() const noexcept {
  if (!data_) return {};
  return {
      .encoded_image = borrowedImage(data_->minimap.encoded_image),
      .world_xz_bounds = data_->minimap.world_xz_bounds,
  };
}

ArxEncodedImageView Level::loadingScreen() const noexcept {
  return data_ ? borrowedImage(data_->loading_screen.encoded_image) : ArxEncodedImageView{};
}

LevelResult<void> Level::setMinimap(ArxEncodedImageView encoded_image, ArxRect world_xz_bounds) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (encoded_image.size == 0) return ARX_LEVEL_BAD_MINIMAP_IMAGE;
        std::vector<std::uint8_t> copy;
        if (!copyImage(encoded_image, copy)) return ARX_INVALID_DATA_POINTER;
        MinimapData value{.encoded_image = std::move(copy), .world_xz_bounds = world_xz_bounds};
        const ArxReturnCode rc = level_validation::minimapError(minimap::validate(value));
        if (rc != ARX_OK) return rc;
        minimap::setImage(data_->minimap, std::move(value.encoded_image), value.world_xz_bounds);
        level_validation::markValid(data_->validation, LevelValidation::kMinimap);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kMinimap));
}

LevelResult<void> Level::setMinimapFromProjection(ArxEncodedImageView encoded_image,
                                                  ArxVector2 projection_offset) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelBoundary(resourcePath(), [&]() -> LevelResult<void> {
    const LevelLocation minimap_location = api_detail::resourceLocation(resourcePath(), LevelElement::kMinimap);
    if (encoded_image.size == 0) return api_detail::levelFailure<void>(ARX_LEVEL_BAD_MINIMAP_IMAGE, minimap_location);
    LevelResult<void> faces = validateFaceRooms();
    if (!faces)
      return api_detail::levelFailure<void>(api_detail::withResourceIdentity(std::move(faces), resourcePath()));
    if (!data_->validation.derived.effective_bounds)
      return api_detail::levelFailure<void>(ARX_LEVEL_NO_GEOMETRY, minimap_location);
    std::vector<std::uint8_t> copy;
    if (!copyImage(encoded_image, copy))
      return api_detail::levelFailure<void>(ARX_INVALID_DATA_POINTER, minimap_location);
    ArxRect bounds;
    const ArxReturnCode rc = level_validation::minimapError(
        minimap::projectedBounds(copy, *data_->validation.derived.effective_bounds, projection_offset, bounds));
    if (rc != ARX_OK) return api_detail::levelFailure<void>(rc, minimap_location);
    minimap::setImage(data_->minimap, std::move(copy), bounds);
    level_validation::markValid(data_->validation, LevelValidation::kMinimap);
    return LevelResult<void>::success();
  });
}

void Level::clearMinimap() noexcept {
  if (!data_) return;
  minimap::clear(data_->minimap);
  level_validation::markValid(data_->validation, LevelValidation::kMinimap);
}

LevelResult<Level::RenderedMinimap> Level::renderMinimap() const noexcept {
  return renderMinimap(MinimapRenderOptions{});
}

LevelResult<Level::RenderedMinimap> Level::renderMinimap(const MinimapRenderOptions& options) const noexcept {
  if (!data_)
    return api_detail::levelFailure<RenderedMinimap>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelBoundary(resourcePath(), [&]() -> LevelResult<RenderedMinimap> {
    const LevelLocation minimap_location = api_detail::resourceLocation(resourcePath(), LevelElement::kMinimap);
    const std::optional<image::Format> format = internalImageFormat(options.format);
    if (!format) return api_detail::levelFailure<RenderedMinimap>(ARX_INVALID_OPTIONS, minimap_location);
    std::optional<ArxColor3> border_color;
    switch (options.mode) {
      case level_images::MinimapRenderMode::kPlain:
        if (options.border_color)
          return api_detail::levelFailure<RenderedMinimap>(ARX_INVALID_OPTIONS, minimap_location);
        break;
      case level_images::MinimapRenderMode::kGame:
        if (!options.projection_offset)
          return api_detail::levelFailure<RenderedMinimap>(ARX_INVALID_OPTIONS, minimap_location);
        border_color = options.border_color.value_or(ArxColor3{1.0f, 1.0f, 1.0f});
        break;
      default:
        return api_detail::levelFailure<RenderedMinimap>(ARX_INVALID_OPTIONS, minimap_location);
    }
    RenderedMinimap result;
    result.projection_offset = options.projection_offset.value_or(ArxVector2{});
    const minimap::RenderOptions render_options{
        .projection_offset = result.projection_offset,
        .fill_color = options.fill_color,
        .border_color = border_color,
        .format = *format,
    };
    ArxReturnCode rc = level_validation::minimapError(minimap::validateRenderOptions(render_options));
    if (rc != ARX_OK) return api_detail::levelFailure<RenderedMinimap>(rc, minimap_location);
    if (data_->minimap.encoded_image.empty()) return result;
    LevelResult<void> validation = validateMinimap();
    if (!validation) return api_detail::levelFailure<RenderedMinimap>(std::move(validation));
    LevelResult<void> faces = validateFaceRooms();
    if (!faces) return api_detail::levelFailure<RenderedMinimap>(std::move(faces));
    if (!data_->validation.derived.effective_bounds)
      return api_detail::levelFailure<RenderedMinimap>(
          ARX_LEVEL_NO_GEOMETRY, api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
    if (!options.projection_offset) {
      rc = level_validation::minimapError(minimap::compactProjectionOffset(
          data_->minimap, *data_->validation.derived.effective_bounds, result.projection_offset));
      if (rc != ARX_OK) return api_detail::levelFailure<RenderedMinimap>(rc, minimap_location);
    }
    minimap::RenderInfo info;
    auto effective_options = render_options;
    effective_options.projection_offset = result.projection_offset;
    rc = level_validation::minimapError(minimap::render(
        data_->minimap, *data_->validation.derived.effective_bounds, effective_options, result.encoded_image, &info));
    if (rc != ARX_OK) return api_detail::levelFailure<RenderedMinimap>(rc, minimap_location);
    if (info.invisible) {
      log(ARX_LOG_WARN, "Level minimap is outside the requested projection; output omitted");
    } else if (info.cropped) {
      log(ARX_LOG_WARN, "Level minimap was cropped by the requested projection");
    } else if (info.padded) {
      log(ARX_LOG_DEBUG, "Level minimap was padded for the requested projection");
    }
    return result;
  });
}

LevelResult<void> Level::setLoadingScreen(ArxEncodedImageView encoded_image) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (encoded_image.size == 0) return ARX_LEVEL_BAD_LOADING_SCREEN_IMAGE;
        std::vector<std::uint8_t> copy;
        if (!copyImage(encoded_image, copy)) return ARX_INVALID_DATA_POINTER;
        const ArxReturnCode rc = level_validation::loadingScreenError(loading_screen::validateImage(copy));
        if (rc != ARX_OK) return rc;
        loading_screen::setImage(data_->loading_screen, std::move(copy));
        level_validation::markValid(data_->validation, LevelValidation::kLoadingScreen);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kLoadingScreen));
}

void Level::clearLoadingScreen() noexcept {
  if (!data_) return;
  loading_screen::clear(data_->loading_screen);
  level_validation::markValid(data_->validation, LevelValidation::kLoadingScreen);
}

LevelResult<std::vector<std::uint8_t>> Level::renderLoadingScreen() const noexcept {
  return renderLoadingScreen(LoadingScreenRenderOptions{});
}

LevelResult<std::vector<std::uint8_t>> Level::renderLoadingScreen(
    const LoadingScreenRenderOptions& options) const noexcept {
  if (!data_)
    return api_detail::levelFailure<std::vector<std::uint8_t>>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelBoundary(resourcePath(), [&]() -> LevelResult<std::vector<std::uint8_t>> {
    LevelResult<void> validation = validateLoadingScreen();
    if (!validation) return api_detail::levelFailure<std::vector<std::uint8_t>>(std::move(validation));
    std::vector<std::uint8_t> out;
    const ArxReturnCode render_rc = level_images::renderLoadingScreen(
        data_->loading_screen.encoded_image, {.layout = options.layout, .format = options.format}, out);
    if (render_rc != ARX_OK)
      return api_detail::levelFailure<std::vector<std::uint8_t>>(
          render_rc, api_detail::resourceLocation(resourcePath(), LevelElement::kLoadingScreen));
    return out;
  });
}

std::size_t Level::vertexCount() const noexcept { return data_ ? data_->geometry.vertices.size() : 0; }
std::size_t Level::faceCount() const noexcept { return data_ ? data_->geometry.faces.size() : 0; }
std::size_t Level::textureCount() const noexcept { return data_ ? data_->textures.textures.size() : 0; }
std::size_t Level::roomCount() const noexcept { return data_ ? data_->rooms.definitions.size() : 0; }
std::size_t Level::portalCount() const noexcept { return data_ ? data_->rooms.portals.size() : 0; }
std::size_t Level::roomDistanceCount() const noexcept {
  return data_ ? rooms::roomDistancePairCount(data_->rooms.definitions.size()) : 0;
}
std::size_t Level::anchorCount() const noexcept { return data_ ? data_->navigation.anchors.size() : 0; }
std::size_t Level::anchorConnectionCount() const noexcept { return data_ ? data_->navigation.connections.size() : 0; }
std::size_t Level::lightCount() const noexcept { return data_ ? data_->lighting.lights.size() : 0; }
std::size_t Level::entityCount() const noexcept { return data_ ? data_->scene.entities.size() : 0; }
std::size_t Level::fogCount() const noexcept { return data_ ? data_->scene.fogs.size() : 0; }
std::size_t Level::zoneCount() const noexcept { return data_ ? data_->scene.zones.size() : 0; }
std::size_t Level::pathCount() const noexcept { return data_ ? data_->scene.paths.size() : 0; }

ArxLevelVertex Level::vertexAt(const void* owner, std::size_t, std::size_t index) noexcept {
  return {.position = static_cast<const Data*>(owner)->geometry.vertices[index].position};
}

ArxLevelFace Level::faceAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Data& data = *static_cast<const Data*>(owner);
  const Face& source = data.geometry.faces[index];
  ArxLevelFace target{};
  const bool has_room = index < data.rooms.face_rooms.size();
  const bool has_colors = data.lighting.corner_colors.size() == lights::expectedCornerColorCount(data.geometry);
  for (std::size_t corner = 0; corner < source.corners.size(); ++corner) {
    target.corners[corner].vertex = source.corners[corner].vertex;
    target.corners[corner].normal = source.corners[corner].normal;
    target.corners[corner].u = source.corners[corner].u;
    target.corners[corner].v = source.corners[corner].v;
    target.corners[corner].color =
        has_colors ? data.lighting.corner_colors[lights::cornerColorIndex(static_cast<FaceIndex>(index), corner)]
                   : lights::kDefaultCornerColor;
  }
  target.texture = source.texture;
  target.room = has_room ? data.rooms.face_rooms[index] : kNoRoom;
  target.flags = source.flags;
  target.transval = source.transval;
  target.normal = source.normal;
  return target;
}

ArxTextureView Level::textureAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Texture& texture = static_cast<const Data*>(owner)->textures.textures[index];
  return {borrowedString(texture.path),
          borrowedImage(texture.encoded_image),
          borrowedString(texture.external_image_extension)};
}

ArxLevelRoom Level::roomAt(const void* owner, std::size_t, std::size_t index) noexcept {
  return {.name = borrowedString(static_cast<const Data*>(owner)->rooms.definitions[index].name)};
}

ArxLevelPortal Level::portalAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Portal& source = static_cast<const Data*>(owner)->rooms.portals[index];
  ArxLevelPortal target{};
  target.name = borrowedString(source.name);
  target.room_front = source.room_1;
  target.room_back = source.room_2;
  target.shape = static_cast<ArxPortalShape>(source.shape);
  std::copy(source.vertices.begin(), source.vertices.end(), target.vertices);
  return target;
}

ArxLevelRoomDistance Level::roomDistanceAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Data& data = *static_cast<const Data*>(owner);
  std::size_t second = 1;
  std::size_t first = index;
  while (second < data.rooms.definitions.size() && first >= second) {
    first -= second;
    ++second;
  }
  const RoomDistance source =
      rooms::roomDistance(data.rooms, static_cast<RoomIndex>(first), static_cast<RoomIndex>(second));
  return {.room_a = static_cast<RoomIndex>(first),
          .room_b = static_cast<RoomIndex>(second),
          .distance = source.distance,
          .portal_a = source.low_room_portal,
          .portal_b = source.high_room_portal};
}

ArxLevelAnchor Level::anchorAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Anchor& source = static_cast<const Data*>(owner)->navigation.anchors[index];
  return {.position = source.position,
          .radius = source.radius,
          .height = source.height,
          .flags = source.flags,
          .name = borrowedString(source.name)};
}

ArxLevelAnchorConnection Level::anchorConnectionAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const AnchorConnection& source = static_cast<const Data*>(owner)->navigation.connections[index];
  return {source.first, source.second};
}

ArxLevelVertex Level::navSurfaceVertexAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& surface = static_cast<const Data*>(owner)->navigation.surface;
  if (!surface) {
    assert(false);
    return {};
  }
  return {.position = surface->vertices[index].position};
}

ArxLevelNavSurfaceTriangle Level::navSurfaceTriangleAt(const void* owner, std::size_t, std::size_t index) noexcept {
  ArxLevelNavSurfaceTriangle target{};
  const auto& surface = static_cast<const Data*>(owner)->navigation.surface;
  if (!surface) {
    assert(false);
    return target;
  }
  const NavSurfaceTriangle& source = surface->triangles[index];
  std::copy(source.vertices.begin(), source.vertices.end(), target.vertices);
  return target;
}

ArxLevelLight Level::lightAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Light& source = static_cast<const Data*>(owner)->lighting.lights[index];
  return {.name = borrowedString(source.name),
          .position = source.position,
          .color = source.color,
          .fallstart = source.fallstart,
          .fallend = source.fallend,
          .intensity = source.intensity,
          .flicker = source.flicker,
          .effect_radius = source.effect_radius,
          .effect_frequency = source.effect_frequency,
          .effect_size = source.effect_size,
          .effect_speed = source.effect_speed,
          .flare_size = source.flare_size,
          .flags = source.flags};
}

ArxLevelEntity Level::entityAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Entity& source = static_cast<const Data*>(owner)->scene.entities[index];
  return {.class_path = borrowedString(source.class_path),
          .ident = source.ident,
          .position = source.position,
          .rotation = source.rotation,
          .name = borrowedString(source.name)};
}

ArxLevelFog Level::fogAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Fog& source = static_cast<const Data*>(owner)->scene.fogs[index];
  return {.position = source.position,
          .color = source.color,
          .size = source.size,
          .directional = static_cast<std::uint8_t>(source.directional ? 1U : 0U),
          .scale = source.scale,
          .rotation = source.rotation,
          .speed = source.speed,
          .rotate_speed = source.rotate_speed,
          .lifetime_ms = source.lifetime_ms,
          .frequency = source.frequency,
          .name = borrowedString(source.name)};
}

ArxLevelZone Level::zoneAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Zone& source = static_cast<const Data*>(owner)->scene.zones[index];
  ArxLevelZone target{};
  target.name = borrowedString(source.name);
  target.perimeter_count = source.perimeter_xz.size();
  target.reference_y = source.reference_y;
  target.height_mode =
      source.height_mode == ZoneHeightMode::kInfinite ? ARX_ZONE_HEIGHT_INFINITE : ARX_ZONE_HEIGHT_FINITE;
  target.height = source.height;
  target.has_color = source.color ? 1U : 0U;
  if (source.color) target.color = *source.color;
  target.has_farclip = source.farclip ? 1U : 0U;
  if (source.farclip) target.farclip = *source.farclip;
  target.has_ambiance = source.ambiance ? 1U : 0U;
  if (source.ambiance) {
    target.ambiance.name = borrowedString(source.ambiance->name);
    target.ambiance.volume = source.ambiance->volume;
  }
  return target;
}

ArxVector2 Level::zonePerimeterAt(const void* owner, std::size_t zone, std::size_t index) noexcept {
  return static_cast<const Data*>(owner)->scene.zones[zone].perimeter_xz[index];
}

ArxLevelPath Level::pathAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const Path& source = static_cast<const Data*>(owner)->scene.paths[index];
  return {.name = borrowedString(source.name), .position = source.position, .node_count = source.nodes.size()};
}

ArxLevelPathNode Level::pathNodeAt(const void* owner, std::size_t path, std::size_t index) noexcept {
  const PathNode& source = static_cast<const Data*>(owner)->scene.paths[path].nodes[index];
  return {.relative_position = source.relative_position,
          .type = static_cast<ArxPathNodeType>(source.type),
          .time_ms = source.time_ms};
}

Level::VerticesView Level::vertices() const noexcept {
  return data_ ? VerticesView(data_.get(), 0, vertexCount(), &vertexAt) : VerticesView{};
}
Level::FacesView Level::faces() const noexcept {
  return data_ ? FacesView(data_.get(), 0, faceCount(), &faceAt) : FacesView{};
}
Level::TexturesView Level::textures() const noexcept {
  return data_ ? TexturesView(data_.get(), 0, textureCount(), &textureAt) : TexturesView{};
}
Level::RoomsView Level::rooms() const noexcept {
  return data_ ? RoomsView(data_.get(), 0, roomCount(), &roomAt) : RoomsView{};
}
Level::PortalsView Level::portals() const noexcept {
  return data_ ? PortalsView(data_.get(), 0, portalCount(), &portalAt) : PortalsView{};
}
Level::RoomDistancesView Level::roomDistances() const noexcept {
  return data_ ? RoomDistancesView(data_.get(), 0, roomDistanceCount(), &roomDistanceAt) : RoomDistancesView{};
}
Level::AnchorsView Level::anchors() const noexcept {
  return data_ ? AnchorsView(data_.get(), 0, anchorCount(), &anchorAt) : AnchorsView{};
}
Level::AnchorConnectionsView Level::anchorConnections() const noexcept {
  return data_ ? AnchorConnectionsView(data_.get(), 0, anchorConnectionCount(), &anchorConnectionAt)
               : AnchorConnectionsView{};
}

ArxLevelNavSurfaceInfo Level::navSurfaceInfo() const noexcept {
  if (!data_ || !data_->navigation.surface) return {};
  return {.has_surface = 1U,
          .vertex_count = data_->navigation.surface->vertices.size(),
          .triangle_count = data_->navigation.surface->triangles.size()};
}

Level::NavSurfaceVerticesView Level::navSurfaceVertices() const noexcept {
  return data_ && data_->navigation.surface
             ? NavSurfaceVerticesView(data_.get(), 0, data_->navigation.surface->vertices.size(), &navSurfaceVertexAt)
             : NavSurfaceVerticesView{};
}
Level::NavSurfaceTrianglesView Level::navSurfaceTriangles() const noexcept {
  return data_ && data_->navigation.surface
             ? NavSurfaceTrianglesView(
                   data_.get(), 0, data_->navigation.surface->triangles.size(), &navSurfaceTriangleAt)
             : NavSurfaceTrianglesView{};
}
Level::LightsView Level::lights() const noexcept {
  return data_ ? LightsView(data_.get(), 0, lightCount(), &lightAt) : LightsView{};
}

ArxLevelPlayerSpawn Level::playerSpawn() const noexcept {
  if (!data_) return {};
  const PlayerSpawn spawn = data_->scene.player_spawn.value_or(PlayerSpawn{});
  return {.position = spawn.position,
          .rotation = spawn.rotation,
          .is_usable = static_cast<std::uint8_t>(data_->scene.player_spawn.has_value() ? 1U : 0U)};
}

Level::EntitiesView Level::entities() const noexcept {
  return data_ ? EntitiesView(data_.get(), 0, entityCount(), &entityAt) : EntitiesView{};
}
Level::FogsView Level::fogs() const noexcept {
  return data_ ? FogsView(data_.get(), 0, fogCount(), &fogAt) : FogsView{};
}
Level::ZonesView Level::zones() const noexcept {
  return data_ ? ZonesView(data_.get(), 0, zoneCount(), &zoneAt) : ZonesView{};
}
LevelResult<Level::ZonePerimeterView> Level::zonePerimeter(ZoneIndex zone) const noexcept {
  if (!data_)
    return api_detail::levelFailure<ZonePerimeterView>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  if (!validIndex(zone, data_->scene.zones.size()))
    return api_detail::levelFailure<ZonePerimeterView>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), LevelElement::kZone, zone));
  return ZonePerimeterView(data_.get(),
                           static_cast<std::size_t>(zone),
                           data_->scene.zones[static_cast<std::size_t>(zone)].perimeter_xz.size(),
                           &zonePerimeterAt);
}
Level::PathsView Level::paths() const noexcept {
  return data_ ? PathsView(data_.get(), 0, pathCount(), &pathAt) : PathsView{};
}
LevelResult<Level::PathNodesView> Level::pathNodes(PathIndex path) const noexcept {
  if (!data_)
    return api_detail::levelFailure<PathNodesView>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  if (!validIndex(path, data_->scene.paths.size()))
    return api_detail::levelFailure<PathNodesView>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), LevelElement::kPath, path));
  return PathNodesView(data_.get(),
                       static_cast<std::size_t>(path),
                       data_->scene.paths[static_cast<std::size_t>(path)].nodes.size(),
                       &pathNodeAt);
}

LevelResult<ArxLevelRoomDistance> Level::roomDistance(RoomIndex room_a, RoomIndex room_b) const noexcept {
  if (!data_)
    return api_detail::levelFailure<ArxLevelRoomDistance>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  if (!validIndex(room_a, data_->rooms.definitions.size()))
    return api_detail::levelFailure<ArxLevelRoomDistance>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), LevelElement::kRoom, room_a));
  if (!validIndex(room_b, data_->rooms.definitions.size()))
    return api_detail::levelFailure<ArxLevelRoomDistance>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), LevelElement::kRoom, room_b));
  if (room_a == room_b)
    return api_detail::levelFailure<ArxLevelRoomDistance>(
        ARX_LEVEL_BAD_ROOM_DISTANCE, api_detail::resourceLocation(resourcePath(), LevelElement::kRoomDistance));
  const RoomIndex low = std::min(room_a, room_b);
  const RoomIndex high = std::max(room_a, room_b);
  const RoomDistance source = rooms::roomDistance(data_->rooms, low, high);
  return ArxLevelRoomDistance{.room_a = low,
                              .room_b = high,
                              .distance = source.distance,
                              .portal_a = source.low_room_portal,
                              .portal_b = source.high_room_portal};
}

LevelResult<void> Level::copyVertexPositions(std::span<float> positions) const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  std::size_t expected = 0;
  if (!checkedScalarCount(data_->geometry.vertices.size(), 3U, expected))
    return api_detail::levelFailure<void>(ARX_INVALID_OPTIONS,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kVertex));
  api_detail::BulkCopyOutputs outputs;
  const ArxReturnCode rc = outputs.add(positions, expected);
  if (rc != ARX_OK)
    return api_detail::levelFailure<void>(rc, api_detail::resourceLocation(resourcePath(), LevelElement::kVertex));
  for (std::size_t i = 0; i < data_->geometry.vertices.size(); ++i) {
    const ArxVector3 position = data_->geometry.vertices[i].position;
    positions[i * 3U] = position.x;
    positions[i * 3U + 1U] = position.y;
    positions[i * 3U + 2U] = position.z;
  }
  return {};
}

LevelResult<void> Level::copyFaces(const FacesOutput& output) const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  const std::size_t count = data_->geometry.faces.size();
  std::size_t index_count = 0;
  std::size_t uv_count = 0;
  std::size_t normal_count = 0;
  if (!checkedScalarCount(count, 3U, index_count) || !checkedScalarCount(count, 6U, uv_count) ||
      !checkedScalarCount(count, 9U, normal_count))
    return api_detail::levelFailure<void>(ARX_INVALID_OPTIONS,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
  api_detail::BulkCopyOutputs outputs;
  ArxReturnCode rc = outputs.add(output.vertex_indices, index_count);
  rc = outputs.add(output.uvs, uv_count);
  rc = outputs.add(output.corner_normals, normal_count);
  rc = outputs.add(output.textures, count);
  rc = outputs.add(output.transvals, count);
  rc = outputs.add(output.corner_colors, normal_count);
  rc = outputs.add(output.face_normals, index_count);
  rc = outputs.add(output.flags, count);
  rc = outputs.finish();
  if (rc != ARX_OK)
    return api_detail::levelFailure<void>(rc, api_detail::resourceLocation(resourcePath(), LevelElement::kFace));

  const std::size_t stored_color_count = data_->lighting.corner_colors.size();
  if (output.corner_colors && stored_color_count != 0 && stored_color_count != count * 3U)
    return api_detail::levelFailure<void>(ARX_LEVEL_BAD_CORNER_COLOR_COUNT,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
  const bool has_colors = data_->lighting.corner_colors.size() == count * 3U;
  for (std::size_t i = 0; i < count; ++i) {
    const Face& face = data_->geometry.faces[i];
    if (output.textures) (*output.textures)[i] = face.texture;
    if (output.transvals) (*output.transvals)[i] = face.transval;
    if (output.face_normals) {
      (*output.face_normals)[i * 3U] = face.normal.x;
      (*output.face_normals)[i * 3U + 1U] = face.normal.y;
      (*output.face_normals)[i * 3U + 2U] = face.normal.z;
    }
    if (output.flags) (*output.flags)[i] = face.flags;
    for (std::size_t corner = 0; corner < 3U; ++corner) {
      const std::size_t scalar = i * 3U + corner;
      const Corner& source = face.corners[corner];
      if (output.vertex_indices) (*output.vertex_indices)[scalar] = source.vertex;
      if (output.uvs) {
        (*output.uvs)[scalar * 2U] = source.u;
        (*output.uvs)[scalar * 2U + 1U] = source.v;
      }
      if (output.corner_normals) {
        (*output.corner_normals)[scalar * 3U] = source.normal.x;
        (*output.corner_normals)[scalar * 3U + 1U] = source.normal.y;
        (*output.corner_normals)[scalar * 3U + 2U] = source.normal.z;
      }
      if (output.corner_colors) {
        const ArxColor3 color = has_colors ? data_->lighting.corner_colors[scalar] : lights::kDefaultCornerColor;
        (*output.corner_colors)[scalar * 3U] = color.r;
        (*output.corner_colors)[scalar * 3U + 1U] = color.g;
        (*output.corner_colors)[scalar * 3U + 2U] = color.b;
      }
    }
  }
  return {};
}

LevelResult<void> Level::copyFaceTextures(std::span<TextureIndex> face_textures) const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  api_detail::BulkCopyOutputs outputs;
  const ArxReturnCode rc = outputs.add(face_textures, data_->geometry.faces.size());
  if (rc != ARX_OK)
    return api_detail::levelFailure<void>(rc, api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
  for (std::size_t i = 0; i < data_->geometry.faces.size(); ++i) face_textures[i] = data_->geometry.faces[i].texture;
  return {};
}

LevelResult<void> Level::copyFaceRooms(std::span<RoomIndex> face_rooms) const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  api_detail::BulkCopyOutputs outputs;
  const ArxReturnCode rc = outputs.add(face_rooms, data_->geometry.faces.size());
  if (rc != ARX_OK)
    return api_detail::levelFailure<void>(rc, api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
  if (data_->rooms.face_rooms.size() != data_->geometry.faces.size())
    return api_detail::levelFailure<void>(ARX_LEVEL_BAD_FACE_ROOM_COUNT,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
  for (std::size_t i = 0; i < data_->geometry.faces.size(); ++i) face_rooms[i] = data_->rooms.face_rooms[i];
  return {};
}

LevelResult<void> Level::copyRoomDistances(const RoomDistancesOutput& output) const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  const std::size_t count = roomDistanceCount();
  std::size_t endpoint_count = 0;
  if (!checkedScalarCount(count, 2U, endpoint_count))
    return api_detail::levelFailure<void>(ARX_INVALID_OPTIONS,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kRoomDistance));
  api_detail::BulkCopyOutputs outputs;
  ArxReturnCode rc = outputs.add(output.distances, count);
  rc = outputs.add(output.endpoint_portals, endpoint_count);
  rc = outputs.finish();
  if (rc != ARX_OK)
    return api_detail::levelFailure<void>(rc,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kRoomDistance));

  if ((output.distances || output.endpoint_portals) && !data_->rooms.distances.empty() &&
      data_->rooms.distances.size() != count)
    return api_detail::levelFailure<void>(ARX_LEVEL_BAD_ROOM_DISTANCE_COUNT,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kRoomDistance));
  const bool has_distances = data_->rooms.distances.size() == count;
  for (std::size_t index = 0; index < count; ++index) {
    const RoomDistance value = has_distances ? data_->rooms.distances[index] : RoomDistance{};
    if (output.distances) (*output.distances)[index] = value.distance;
    if (output.endpoint_portals) {
      (*output.endpoint_portals)[index * 2U] = value.low_room_portal;
      (*output.endpoint_portals)[index * 2U + 1U] = value.high_room_portal;
    }
  }
  return {};
}

LevelResult<void> Level::copyAnchors(const AnchorsOutput& output) const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  const std::size_t count = data_->navigation.anchors.size();
  std::size_t position_count = 0;
  if (!checkedScalarCount(count, 3U, position_count))
    return api_detail::levelFailure<void>(ARX_INVALID_OPTIONS,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kAnchor));
  api_detail::BulkCopyOutputs outputs;
  ArxReturnCode rc = outputs.add(output.positions, position_count);
  rc = outputs.add(output.radii, count);
  rc = outputs.add(output.heights, count);
  rc = outputs.add(output.flags, count);
  rc = outputs.finish();
  if (rc != ARX_OK)
    return api_detail::levelFailure<void>(rc, api_detail::resourceLocation(resourcePath(), LevelElement::kAnchor));
  for (std::size_t i = 0; i < count; ++i) {
    const Anchor& anchor = data_->navigation.anchors[i];
    if (output.positions) {
      (*output.positions)[i * 3U] = anchor.position.x;
      (*output.positions)[i * 3U + 1U] = anchor.position.y;
      (*output.positions)[i * 3U + 2U] = anchor.position.z;
    }
    if (output.radii) (*output.radii)[i] = anchor.radius;
    if (output.heights) (*output.heights)[i] = anchor.height;
    if (output.flags) (*output.flags)[i] = static_cast<std::uint32_t>(anchor.flags);
  }
  return {};
}

LevelResult<void> Level::copyAnchorConnections(std::span<AnchorIndex> endpoints) const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  std::size_t endpoint_count = 0;
  if (!checkedScalarCount(data_->navigation.connections.size(), 2U, endpoint_count))
    return api_detail::levelFailure<void>(
        ARX_INVALID_OPTIONS, api_detail::resourceLocation(resourcePath(), LevelElement::kAnchorConnection));
  api_detail::BulkCopyOutputs outputs;
  const ArxReturnCode rc = outputs.add(endpoints, endpoint_count);
  if (rc != ARX_OK)
    return api_detail::levelFailure<void>(
        rc, api_detail::resourceLocation(resourcePath(), LevelElement::kAnchorConnection));
  for (std::size_t i = 0; i < data_->navigation.connections.size(); ++i) {
    endpoints[i * 2U] = data_->navigation.connections[i].first;
    endpoints[i * 2U + 1U] = data_->navigation.connections[i].second;
  }
  return {};
}

LevelResult<void> Level::copyNavSurface(const NavSurfaceOutput& output) const noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  const NavSurface* surface = data_->navigation.surface ? &*data_->navigation.surface : nullptr;
  const std::size_t vertex_count = surface ? surface->vertices.size() : 0U;
  const std::size_t triangle_count = surface ? surface->triangles.size() : 0U;
  std::size_t position_count = 0;
  std::size_t triangle_index_count = 0;
  if (!checkedScalarCount(vertex_count, 3U, position_count) ||
      !checkedScalarCount(triangle_count, 3U, triangle_index_count))
    return api_detail::levelFailure<void>(
        ARX_INVALID_OPTIONS, api_detail::resourceLocation(resourcePath(), LevelElement::kNavSurfaceTriangle));
  api_detail::BulkCopyOutputs outputs;
  ArxReturnCode rc = outputs.add(output.positions, position_count);
  rc = outputs.add(output.triangle_indices, triangle_index_count);
  rc = outputs.finish();
  if (rc != ARX_OK)
    return api_detail::levelFailure<void>(
        rc, api_detail::resourceLocation(resourcePath(), LevelElement::kNavSurfaceTriangle));
  if (!surface) return {};
  if (output.positions) {
    for (std::size_t i = 0; i < vertex_count; ++i) {
      const ArxVector3 position = surface->vertices[i].position;
      (*output.positions)[i * 3U] = position.x;
      (*output.positions)[i * 3U + 1U] = position.y;
      (*output.positions)[i * 3U + 2U] = position.z;
    }
  }
  if (output.triangle_indices) {
    for (std::size_t i = 0; i < triangle_count; ++i)
      std::copy(surface->triangles[i].vertices.begin(),
                surface->triangles[i].vertices.end(),
                output.triangle_indices->begin() + static_cast<std::ptrdiff_t>(i * 3U));
  }
  return {};
}

LevelResult<void> Level::setVertex(VertexIndex index, ArxLevelVertex value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
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
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kVertex, index));
}

LevelResult<VertexIndex> Level::addVertex(ArxLevelVertex vertex) noexcept {
  if (!data_)
    return api_detail::levelFailure<VertexIndex>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  VertexIndex out_index = kInvalidVertexIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<VertexIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kVertex, data_->geometry.vertices.size()));
  return out_index;
}

LevelResult<VertexIndex> Level::addVertices(std::span<const ArxLevelVertex> vertices) noexcept {
  if (!data_)
    return api_detail::levelFailure<VertexIndex>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  VertexIndex out_first_index = kInvalidVertexIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
    out_first_index = kInvalidVertexIndex;
    if (vertices.empty()) return ARX_INVALID_OPTIONS;
    if (vertices.size() > static_cast<std::size_t>(kInvalidVertexIndex) - data_->geometry.vertices.size())
      return ARX_LEVEL_TOO_MANY_VERTICES;

    const bool had_vertices = !data_->geometry.vertices.empty();
    const bool vertices_stay_valid =
        !had_vertices || (level_validation::has(data_->validation, LevelValidation::kVertices) &&
                          data_->validation.derived.bounds.has_value());
    std::optional<ArxAabb> bounds =
        vertices_stay_valid && had_vertices ? data_->validation.derived.bounds : std::nullopt;

    std::vector<Vertex> internal_vertices;
    internal_vertices.reserve(vertices.size());
    for (const ArxLevelVertex& input : vertices) {
      const Vertex vertex = internalVertex(input);
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
        geometry::vertexCapacityForAppend(
            data_->geometry, vertices.size(), static_cast<std::size_t>(kInvalidVertexIndex)));
    const VertexIndex appended_first = geometry::appendVertices(data_->geometry, internal_vertices);
    if (vertices_stay_valid) {
      level_validation::markValid(data_->validation, LevelValidation::kVertices);
      data_->validation.derived.bounds = bounds;
    }
    out_first_index = appended_first;
    return ARX_OK;
  });
  if (result != ARX_OK)
    return api_detail::levelFailure<VertexIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kVertex, data_->geometry.vertices.size()));
  return out_first_index;
}

LevelResult<void> Level::setFace(FaceIndex index, const ArxLevelFace& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
        if (data_->rooms.face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
        if (value.room != kNoRoom && !validIndex(value.room, data_->rooms.definitions.size()))
          return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
        const std::size_t expected_colors = lights::expectedCornerColorCount(data_->geometry);
        if (!data_->lighting.corner_colors.empty() && data_->lighting.corner_colors.size() != expected_colors)
          return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

        Face face = internalFace(value);
        ArxReturnCode rc = level_validation::geometryError(geometry::validateFaces(
            std::span<const Face>(&face, 1), data_->geometry.vertices, data_->textures.textures.size()));
        if (rc != ARX_OK) return rc;
        rc = level_validation::faceTypes(std::span<const Face>(&face, 1));
        if (rc != ARX_OK) return rc;
        for (const ArxLevelCorner& corner : value.corners) {
          rc = level_validation::lightingError(lights::validateCornerColor(corner.color));
          if (rc != ARX_OK) return rc;
        }

        const auto colors = storedFaceCornerColors(value);
        lights::setFaceCornerColors(data_->lighting, index, colors, data_->geometry.faces.size());
        geometry::setFace(data_->geometry, index, face);
        rooms::setFaceRoom(data_->rooms, index, value.room);
        if ((value.flags & kFaceBitQuad) != 0)
          log(ARX_LOG_WARN, "Level face edit: stripped QUAD flag from triangular face input");
        level_validation::invalidate(data_->validation, LevelValidation::kFaces);
        if (value.room == kNoRoom) level_validation::invalidate(data_->validation, LevelValidation::kFaceRooms);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace, index));
}

LevelResult<FaceIndex> Level::addFace(const ArxLevelFace& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<FaceIndex>(ARX_INVALID_STATE,
                                               api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  FaceIndex out_index = kInvalidFaceIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidFaceIndex;
    if (value.room != kNoRoom && !validIndex(value.room, data_->rooms.definitions.size()))
      return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
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
    rc = level_validation::faceTypes(std::span<const Face>(&face, 1));
    if (rc != ARX_OK) return rc;
    for (const ArxLevelCorner& corner : value.corners) {
      rc = level_validation::lightingError(lights::validateCornerColor(corner.color));
      if (rc != ARX_OK) return rc;
    }

    const std::size_t face_count = data_->geometry.faces.size();
    const FaceIndex index = static_cast<FaceIndex>(face_count);
    std::optional<ArxAabb> referenced_bounds = level_validation::has(data_->validation, LevelValidation::kFaces)
                                                   ? data_->validation.derived.referenced_bounds
                                                   : std::nullopt;
    const bool faces_stay_valid = referenced_bounds.has_value();
    const bool face_rooms_stay_valid = level_validation::has(data_->validation, LevelValidation::kFaceRooms);
    const bool colors_stay_valid = level_validation::has(data_->validation, LevelValidation::kCornerColors);
    const auto colors = storedFaceCornerColors(value);

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
    if (faces_stay_valid && face_rooms_stay_valid && value.room != kNoRoom)
      level_validation::markValid(data_->validation, LevelValidation::kFaceRooms);
    else if (value.room == kNoRoom)
      level_validation::invalidate(data_->validation, LevelValidation::kFaceRooms);
    if (faces_stay_valid && colors_stay_valid)
      level_validation::markValid(data_->validation, LevelValidation::kCornerColors);
    out_index = index;
    return ARX_OK;
  });
  if (result != ARX_OK)
    return api_detail::levelFailure<FaceIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kFace, data_->geometry.faces.size()));
  return out_index;
}

LevelResult<void> Level::removeFace(FaceIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
        if (data_->rooms.face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
        if (!data_->lighting.corner_colors.empty() &&
            data_->lighting.corner_colors.size() != lights::expectedCornerColorCount(data_->geometry))
          return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

        lights::removeFaceCornerColors(data_->lighting, index, data_->geometry.faces.size());
        rooms::removeFaceRoom(data_->rooms, index);
        geometry::removeFace(data_->geometry, index);
        navigation::clearSurface(data_->navigation);
        navigation::clearAnchors(data_->navigation);
        level_validation::invalidate(
            data_->validation, LevelValidation::kFaces | LevelValidation::kNavSurface | LevelValidation::kAnchors);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace, index));
}

LevelResult<std::size_t> Level::compactVertices() noexcept {
  if (!data_)
    return api_detail::levelFailure<std::size_t>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelBoundary(resourcePath(), [&]() -> LevelResult<std::size_t> {
    LevelResult<void> faces = validateFaces();
    if (!faces)
      return api_detail::levelFailure<std::size_t>(api_detail::withResourceIdentity(std::move(faces), resourcePath()));
    const std::size_t removed = geometry::compactVertices(data_->geometry);
    data_->validation.derived.bounds = data_->validation.derived.referenced_bounds;
    return removed;
  });
}

LevelResult<std::size_t> Level::compactTextures() noexcept {
  if (!data_)
    return api_detail::levelFailure<std::size_t>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelBoundary(resourcePath(), [&]() -> LevelResult<std::size_t> {
    LevelResult<void> faces = validateFaces();
    if (!faces)
      return api_detail::levelFailure<std::size_t>(api_detail::withResourceIdentity(std::move(faces), resourcePath()));
    std::vector<std::uint8_t> used;
    ArxReturnCode rc = level_validation::geometryError(
        geometry::collectTextureUsage(data_->geometry, data_->textures.textures.size(), used));
    if (rc != ARX_OK)
      return api_detail::levelFailure<std::size_t>(
          rc, api_detail::resourceLocation(resourcePath(), LevelElement::kTexture));
    std::vector<TextureIndex> remap;
    std::size_t removed = 0;
    rc = level_validation::textureError(textures::compact(data_->textures, used, remap, removed));
    if (rc != ARX_OK)
      return api_detail::levelFailure<std::size_t>(
          rc, api_detail::resourceLocation(resourcePath(), LevelElement::kTexture));
    geometry::remapTextureReferences(data_->geometry, remap);
    return removed;
  });
}

LevelResult<void> Level::weldVertices() noexcept { return weldVertices(VertexWeldOptions{}); }

LevelResult<void> Level::weldVertices(const VertexWeldOptions& options) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelBoundary(resourcePath(), [&]() -> LevelResult<void> {
    LevelResult<void> mesh = validateGeometry();
    if (!mesh) return api_detail::levelFailure<void>(api_detail::withResourceIdentity(std::move(mesh), resourcePath()));

    geometry::VertexWeldOptions module_options;
    if (!geometryWeldOptions(options, module_options))
      return api_detail::levelFailure<void>(ARX_INVALID_OPTIONS,
                                            api_detail::resourceLocation(resourcePath(), LevelElement::kVertex));

    rooms::VertexWeldSegments collected;
    ArxReturnCode rc = level_validation::roomsError(
        rooms::collectVertexWeldSegments(data_->geometry, data_->rooms, module_options.radius, collected));
    if (rc != ARX_OK)
      return api_detail::levelFailure<void>(rc, api_detail::resourceLocation(resourcePath(), LevelElement::kRoom));

    std::vector<geometry::VertexWeldSegment> segments;
    segments.reserve(collected.offsets.size() - 1U);
    for (std::size_t room = 0; room + 1U < collected.offsets.size(); ++room) {
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
    if (error != geometry::Error::kNone)
      return api_detail::levelFailure<void>(level_validation::geometryError(error),
                                            api_detail::resourceLocation(resourcePath(), LevelElement::kVertex));

    rooms::remapFaceRooms(data_->rooms, remap.faces);
    lights::remapCornerColors(data_->lighting, remap.faces);
    level_validation::invalidate(data_->validation, LevelValidation::kVertices);
    return LevelResult<void>::success();
  });
}

LevelResult<void> Level::snapGeometryToPortals() noexcept { return snapGeometryToPortals(PortalSnapOptions{}); }

LevelResult<void> Level::snapGeometryToPortals(const PortalSnapOptions& options) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelBoundary(resourcePath(), [&]() -> LevelResult<void> {
    LevelResult<void> mesh = validateGeometry();
    if (!mesh) return api_detail::levelFailure<void>(api_detail::withResourceIdentity(std::move(mesh), resourcePath()));
    LevelResult<void> portals = validatePortals();
    if (!portals)
      return api_detail::levelFailure<void>(api_detail::withResourceIdentity(std::move(portals), resourcePath()));

    GeometryData next = data_->geometry;
    rooms::PortalSnapStatistics statistics;
    ArxReturnCode rc = level_validation::roomsError(
        rooms::snapGeometryToPortals(next, data_->rooms, {.radius = options.radius}, &statistics));
    if (rc != ARX_OK)
      return api_detail::levelFailure<void>(rc, api_detail::resourceLocation(resourcePath(), LevelElement::kPortal));

    LevelValidationState next_validation;
    rc = validateMeshCoherence(next,
                               data_->textures,
                               data_->rooms.face_rooms,
                               data_->lighting.corner_colors,
                               data_->rooms.definitions.size(),
                               next_validation);
    if (rc != ARX_OK)
      return api_detail::levelFailure<void>(rc, api_detail::resourceLocation(resourcePath(), LevelElement::kFace));

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
    return LevelResult<void>::success();
  });
}

LevelResult<void> Level::setTexture(TextureIndex index, const ArxTextureView& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        Texture texture;
        if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
        textures::PathRepairInfo repair;
        ArxReturnCode rc =
            level_validation::textureError(textures::repairPath(data_->textures, texture, index, &repair));
        if (rc != ARX_OK) return rc;
        rc = level_validation::textureError(textures::validateTexture(texture));
        if (rc != ARX_OK) return rc;
        textures::setTexture(data_->textures, index, std::move(texture));
        for (const textures::PathRepairInfo::Repair& item : repair.repairs)
          log(ARX_LOG_WARN, "Level texture: '{}' normalized to '{}'", item.original, item.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kTexture, index));
}

LevelResult<TextureIndex> Level::addTexture(const ArxTextureView& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<TextureIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  TextureIndex out_index = kNoTexture;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<TextureIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kTexture, data_->textures.textures.size()));
  return out_index;
}

LevelResult<void> Level::setTexturePath(TextureIndex index, std::string_view requested) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        Texture candidate(requested);
        textures::PathRepairInfo repair;
        ArxReturnCode rc =
            level_validation::textureError(textures::repairPath(data_->textures, candidate, index, &repair));
        if (rc != ARX_OK) return rc;
        rc = level_validation::textureError(textures::validateTexture(candidate));
        if (rc != ARX_OK) return rc;
        textures::setPath(data_->textures, index, std::move(candidate.path));
        for (const textures::PathRepairInfo::Repair& item : repair.repairs)
          log(ARX_LOG_WARN, "Level texture: '{}' normalized to '{}'", item.original, item.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kTexture, index));
}

LevelResult<void> Level::setTextureExternalImageExtension(TextureIndex index, std::string_view requested) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        const Texture& current = data_->textures.textures[index];
        if (!current.encoded_image.empty() && !requested.empty()) return ARX_LEVEL_BAD_TEXTURE_IMAGE;
        Texture candidate(current.path);
        candidate.external_image_extension = requested;
        const ArxReturnCode rc = level_validation::textureError(textures::validateTexture(candidate));
        if (rc != ARX_OK) return rc;
        textures::setExternalImageExtension(data_->textures, index, std::move(candidate.external_image_extension));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kTexture, index));
}

LevelResult<void> Level::rebaseTexturePaths(std::string_view directory) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        textures::PathRebaseInfo info;
        const ArxReturnCode rc =
            level_validation::textureError(textures::rebasePaths(data_->textures, directory, &info));
        if (rc != ARX_OK) return rc;
        for (const textures::PathRebaseInfo::Repair& repair : info.repairs)
          log(ARX_LOG_WARN, "Level texture rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kTexture));
}

LevelResult<void> Level::setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        if (encoded_image.size == 0) return ARX_LEVEL_BAD_TEXTURE_IMAGE;
        std::vector<std::uint8_t> next_image;
        if (!copyImage(encoded_image, next_image)) return ARX_INVALID_DATA_POINTER;
        const ArxReturnCode rc = level_validation::textureError(textures::validateEncodedImage(next_image));
        if (rc != ARX_OK) return rc;
        textures::setEncodedImage(data_->textures, index, std::move(next_image));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kTexture, index));
}

LevelResult<void> Level::clearTextureImage(TextureIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        textures::clearEncodedImage(data_->textures, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kTexture, index));
}

LevelResult<void> Level::setFaceRoom(FaceIndex face, RoomIndex room) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(face, data_->rooms.face_rooms.size())) return ARX_INDEX_OUT_OF_RANGE;
        if (room != kNoRoom && !validIndex(room, data_->rooms.definitions.size())) return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
        rooms::setFaceRoom(data_->rooms, face, room);
        level_validation::invalidate(data_->validation, LevelValidation::kFaceRooms);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace, face));
}

LevelResult<void> Level::setCornerColor(FaceIndex face, std::uint8_t corner, ArxColor3 color) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(face, data_->geometry.faces.size()) || corner >= 3) return ARX_INDEX_OUT_OF_RANGE;
        ArxReturnCode rc =
            level_validation::lightingError(lights::validateCornerColors(data_->lighting, data_->geometry));
        if (rc != ARX_OK) return rc;
        rc = level_validation::lightingError(lights::validateCornerColor(color));
        if (rc != ARX_OK) return rc;
        lights::setCornerColor(data_->lighting, face, corner, color, data_->geometry.faces.size());
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace, face, corner));
}

void Level::resetCornerColors() noexcept {
  if (data_) lights::resetCornerColors(data_->lighting);
}

LevelResult<void> Level::replaceVertices(std::span<const float> positions) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        std::vector<Vertex> next;
        ArxReturnCode rc = level_validation::geometryError(geometry::buildVertices(positions, next));
        if (rc != ARX_OK) return rc;
        ArxAabb bounds;
        if (!next.empty()) {
          rc = level_validation::geometryError(geometry::validateVertices(next, &bounds));
          if (rc != ARX_OK) return rc;
          if (bounds.min.x < kLevelMinXZ || bounds.max.x > kLevelMaxXZ || bounds.min.z < kLevelMinXZ ||
              bounds.max.z > kLevelMaxXZ)
            return ARX_LEVEL_VERTEX_OUT_OF_BOUNDS;
        }

        geometry::replaceVertices(data_->geometry, std::move(next));
        rooms::clearFaceRooms(data_->rooms);
        lights::resetCornerColors(data_->lighting);
        navigation::clearSurface(data_->navigation);
        navigation::clearAnchors(data_->navigation);
        level_validation::invalidate(data_->validation,
                                     LevelValidation::kVertices | LevelValidation::kFaces |
                                         LevelValidation::kFaceRooms | LevelValidation::kCornerColors |
                                         LevelValidation::kNavSurface | LevelValidation::kAnchors);
        if (!data_->geometry.vertices.empty()) {
          level_validation::markValid(data_->validation, LevelValidation::kVertices);
          data_->validation.derived.bounds = bounds;
        }
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kVertex));
}

void Level::clearVertices() noexcept {
  if (!data_) return;
  geometry::replaceVertices(data_->geometry, {});
  rooms::clearFaceRooms(data_->rooms);
  lights::resetCornerColors(data_->lighting);
  navigation::clearSurface(data_->navigation);
  navigation::clearAnchors(data_->navigation);
  level_validation::invalidate(data_->validation,
                               LevelValidation::kVertices | LevelValidation::kFaces | LevelValidation::kFaceRooms |
                                   LevelValidation::kCornerColors | LevelValidation::kNavSurface |
                                   LevelValidation::kAnchors);
}

LevelResult<void> Level::replaceFaces(std::span<const std::uint32_t> vertex_indices, std::span<const float> uvs,
                                      std::span<const float> corner_normals,
                                      std::span<const TextureIndex> face_textures, std::span<const float> transvals,
                                      std::span<const float> corner_colors, std::span<const float> face_normals,
                                      std::span<const FaceType> flags) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        ArxReturnCode rc = ARX_OK;
        std::vector<Face> next_faces;
        rc = level_validation::geometryError(geometry::buildFaces(data_->geometry.vertices,
                                                                  data_->textures.textures.size(),
                                                                  vertex_indices,
                                                                  uvs,
                                                                  corner_normals,
                                                                  face_textures,
                                                                  transvals,
                                                                  face_normals,
                                                                  flags,
                                                                  next_faces));
        if (rc != ARX_OK) return rc;
        rc = level_validation::faceTypes(next_faces);
        if (rc != ARX_OK) return rc;
        const std::size_t face_count = next_faces.size();
        if ((face_count == 0 && !corner_colors.empty()) ||
            (face_count != 0 && corner_colors.size() != 3U && corner_colors.size() != face_count * 9U))
          return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;

        std::vector<ArxColor3> next_colors;
        if (face_count != 0) {
          const std::size_t color_count = corner_colors.size() == 3U ? 1U : face_count * 3U;
          bool all_default = true;
          for (std::size_t corner = 0; corner < color_count; ++corner) {
            const std::size_t color = corner_colors.size() == 3U ? 0U : corner * 3U;
            const ArxColor3 value{corner_colors[color], corner_colors[color + 1U], corner_colors[color + 2U]};
            rc = level_validation::lightingError(lights::validateCornerColor(value));
            if (rc != ARX_OK) return rc;
            all_default = all_default && value.r == lights::kDefaultCornerColor.r &&
                          value.g == lights::kDefaultCornerColor.g && value.b == lights::kDefaultCornerColor.b;
          }
          if (!all_default) {
            next_colors.resize(face_count * 3U);
            for (std::size_t corner = 0; corner < next_colors.size(); ++corner) {
              const std::size_t color = corner_colors.size() == 3U ? 0U : corner * 3U;
              next_colors[corner] = {corner_colors[color], corner_colors[color + 1U], corner_colors[color + 2U]};
            }
          }
        }
        std::vector<RoomIndex> next_rooms(face_count, kNoRoom);

        geometry::replaceFaces(data_->geometry, std::move(next_faces));
        rooms::replaceFaceRooms(data_->rooms, std::move(next_rooms));
        lights::replaceCornerColors(data_->lighting, std::move(next_colors));
        navigation::clearSurface(data_->navigation);
        navigation::clearAnchors(data_->navigation);
        level_validation::invalidate(data_->validation,
                                     LevelValidation::kFaces | LevelValidation::kFaceRooms |
                                         LevelValidation::kCornerColors | LevelValidation::kNavSurface |
                                         LevelValidation::kAnchors);
        level_validation::markValid(data_->validation, LevelValidation::kCornerColors);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
}

void Level::clearFaces() noexcept {
  if (!data_) return;
  geometry::clearFaces(data_->geometry);
  rooms::clearFaceRooms(data_->rooms);
  lights::resetCornerColors(data_->lighting);
  navigation::clearSurface(data_->navigation);
  navigation::clearAnchors(data_->navigation);
  level_validation::invalidate(data_->validation,
                               LevelValidation::kFaces | LevelValidation::kFaceRooms | LevelValidation::kCornerColors |
                                   LevelValidation::kNavSurface | LevelValidation::kAnchors);
}

void Level::clearTextures() noexcept {
  if (!data_) return;
  textures::clear(data_->textures);
  geometry::resetFaceTextures(data_->geometry);
  level_validation::invalidate(data_->validation, LevelValidation::kTextures | LevelValidation::kFaces);
}

LevelResult<void> Level::replaceFaceTextures(std::span<const TextureIndex> face_textures) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (face_textures.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_COUNT;
        for (TextureIndex texture : face_textures)
          if (texture != kNoTexture && !validIndex(texture, data_->textures.textures.size()))
            return ARX_LEVEL_BAD_FACE_TEXTURE;
        geometry::replaceFaceTextures(data_->geometry, face_textures);
        level_validation::invalidate(data_->validation, LevelValidation::kFaces);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
}

LevelResult<void> Level::replaceFaceRooms(std::span<const RoomIndex> face_rooms) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (face_rooms.size() != data_->geometry.faces.size()) return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
        for (RoomIndex room : face_rooms) {
          if (room != kNoRoom && !validIndex(room, data_->rooms.definitions.size())) {
            return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
          }
        }
        rooms::assignFaceRooms(data_->rooms, face_rooms);
        level_validation::invalidate(data_->validation, LevelValidation::kFaceRooms);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFace));
}

LevelResult<void> Level::setRoom(RoomIndex index, const ArxLevelRoom& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->rooms.definitions.size())) return ARX_INDEX_OUT_OF_RANGE;
        Room room;
        if (!internalRoom(value, room)) return ARX_INVALID_DATA_POINTER;
        if (data_->rooms.definitions[static_cast<std::size_t>(index)].name == room.name) return ARX_OK;
        rooms::repairRoomName(data_->rooms, room, index);
        ArxReturnCode rc = level_validation::roomsError(rooms::validateRoom(room));
        if (rc != ARX_OK) return rc;
        rooms::setRoom(data_->rooms, index, std::move(room));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kRoom, index));
}

LevelResult<RoomIndex> Level::addRoom(const ArxLevelRoom& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<RoomIndex>(ARX_INVALID_STATE,
                                               api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  RoomIndex out_index = kInvalidRoomIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<RoomIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kRoom, data_->rooms.definitions.size()));
  return out_index;
}

LevelResult<void> Level::removeRoom(RoomIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->rooms.definitions.size())) return ARX_INDEX_OUT_OF_RANGE;
        const bool rooms_stay_valid =
            level_validation::has(data_->validation, LevelValidation::kRooms) && data_->rooms.definitions.size() > 1;
        const bool portals_stay_valid = level_validation::has(data_->validation, LevelValidation::kPortals);
        ArxReturnCode rc = level_validation::roomsError(rooms::validateRoomRemoval(data_->rooms, index));
        if (rc != ARX_OK) return rc;
        rooms::removeRoom(data_->rooms, index);
        level_validation::invalidate(data_->validation,
                                     LevelValidation::kRooms | LevelValidation::kFaceRooms | LevelValidation::kPortals |
                                         LevelValidation::kRoomDistances);
        if (rooms_stay_valid) level_validation::markValid(data_->validation, LevelValidation::kRooms);
        if (portals_stay_valid && rooms_stay_valid)
          level_validation::markValid(data_->validation, LevelValidation::kPortals);
        level_validation::markValid(data_->validation, LevelValidation::kRoomDistances);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kRoom, index));
}

void Level::clearRooms() noexcept {
  if (!data_) return;
  rooms::clearRooms(data_->rooms);
  level_validation::invalidate(data_->validation,
                               LevelValidation::kRooms | LevelValidation::kFaceRooms | LevelValidation::kPortals |
                                   LevelValidation::kRoomDistances);
}

LevelResult<void> Level::setPortal(PortalIndex index, const ArxLevelPortal& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
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
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kPortal, index));
}

LevelResult<PortalIndex> Level::addPortal(const ArxLevelPortal& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<PortalIndex>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  PortalIndex out_index = kInvalidPortalIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<PortalIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kPortal, data_->rooms.portals.size()));
  return out_index;
}

LevelResult<void> Level::removePortal(PortalIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->rooms.portals.size())) return ARX_INDEX_OUT_OF_RANGE;
        rooms::removePortal(data_->rooms, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kPortal, index));
}

LevelResult<void> Level::flattenPortals() noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelBoundary(resourcePath(), [&]() -> LevelResult<void> {
    LevelResult<void> portals = validatePortals();
    if (!portals) return api_detail::levelFailure<void>(std::move(portals));

    const bool had_room_distances = !data_->rooms.distances.empty();
    RoomsData next = data_->rooms;
    rooms::PortalFlattenStatistics statistics;
    const ArxReturnCode rc = level_validation::roomsError(rooms::flattenPortals(next, &statistics));
    const LevelLocation location = api_detail::resourceLocation(resourcePath(), LevelElement::kPortal);
    if (rc != ARX_OK) return api_detail::levelFailure<void>(rc, location);
    if (statistics.flattened_quads != 0) {
      for (const Portal& portal : next.portals)
        if (!level_validation::validPortalBounds(portal))
          return api_detail::levelFailure<void>(ARX_LEVEL_PORTAL_OUT_OF_BOUNDS, location);
      data_->rooms = std::move(next);
    }

    log(ARX_LOG_INFO,
        "Level portal flattening: {} quads flattened, {} already planar, {} triangles unchanged",
        statistics.flattened_quads,
        statistics.already_planar_quads,
        statistics.triangles);
    if (had_room_distances && statistics.flattened_quads != 0)
      log(ARX_LOG_INFO, "Level portal flattening discarded room distances because portal geometry changed");
    return LevelResult<void>::success();
  });
}

LevelResult<void> Level::setRoomDistance(const ArxLevelRoomDistance& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
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
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kRoomDistance));
}

LevelResult<void> Level::replaceRoomDistances(std::span<const float> distances,
                                              std::span<const PortalIndex> endpoint_portals) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const std::size_t expected = rooms::roomDistancePairCount(data_->rooms.definitions.size());
        if (distances.size() != expected || endpoint_portals.size() != expected * 2U)
          return ARX_LEVEL_BAD_ROOM_DISTANCE_COUNT;
        std::vector<RoomDistance> next(expected);
        std::size_t index = 0;
        for (std::size_t high = 1; high < data_->rooms.definitions.size(); ++high) {
          for (std::size_t low = 0; low < high; ++low, ++index) {
            ArxLevelRoomDistance value{};
            value.room_a = static_cast<RoomIndex>(low);
            value.room_b = static_cast<RoomIndex>(high);
            value.distance = distances[index];
            value.portal_a = endpoint_portals[index * 2U];
            value.portal_b = endpoint_portals[index * 2U + 1U];
            RoomIndex low_room = 0;
            RoomIndex high_room = 0;
            ArxReturnCode rc = internalRoomDistance(value, data_->rooms, low_room, high_room, next[index]);
            if (rc != ARX_OK) return rc;
          }
        }
        ArxReturnCode rc = level_validation::roomsError(rooms::validateRoomDistances(next, data_->rooms));
        if (rc != ARX_OK) return rc;
        rooms::replaceRoomDistances(data_->rooms, std::move(next));
        level_validation::markValid(data_->validation, LevelValidation::kRoomDistances);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kRoomDistance));
}

void Level::clearRoomDistances() noexcept {
  if (!data_) return;
  rooms::clearRoomDistances(data_->rooms);
  level_validation::markValid(data_->validation, LevelValidation::kRoomDistances);
}

void Level::clearPortals() noexcept {
  if (!data_) return;
  rooms::clearPortals(data_->rooms);
  level_validation::invalidate(data_->validation, LevelValidation::kPortals);
  level_validation::markValid(data_->validation, LevelValidation::kRoomDistances);
}

LevelResult<void> Level::setAnchor(AnchorIndex index, const ArxLevelAnchor& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->navigation.anchors.size())) return ARX_INDEX_OUT_OF_RANGE;
        Anchor anchor;
        if (!internalAnchor(value, anchor)) return ARX_INVALID_DATA_POINTER;
        navigation::repairAnchorName(data_->navigation, anchor, index);
        ArxReturnCode rc = level_validation::navigationError(navigation::validateAnchor(anchor));
        if (rc != ARX_OK) return rc;
        navigation::setAnchor(data_->navigation, index, std::move(anchor));
        level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kAnchor, index));
}

LevelResult<AnchorIndex> Level::addAnchor(const ArxLevelAnchor& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<AnchorIndex>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  AnchorIndex out_index = kInvalidAnchorIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<AnchorIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kAnchor, data_->navigation.anchors.size()));
  return out_index;
}

LevelResult<void> Level::removeAnchor(AnchorIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->navigation.anchors.size())) return ARX_INDEX_OUT_OF_RANGE;
        const bool anchors_stay_valid = level_validation::has(data_->validation, LevelValidation::kAnchors);
        navigation::removeAnchor(data_->navigation, index);
        level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
        if (anchors_stay_valid) level_validation::markValid(data_->validation, LevelValidation::kAnchors);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kAnchor, index));
}

LevelResult<void> Level::setAnchorConnection(AnchorConnectionIndex index, ArxLevelAnchorConnection value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->navigation.connections.size())) return ARX_INDEX_OUT_OF_RANGE;
        const AnchorConnection connection = internalConnection(value);
        ArxReturnCode rc = level_validation::navigationError(
            navigation::validateConnectionPlacement(data_->navigation, index, connection));
        if (rc != ARX_OK) return rc;
        navigation::setConnection(data_->navigation, index, connection);
        level_validation::markValid(data_->validation, LevelValidation::kAnchorConnections);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kAnchorConnection, index));
}

LevelResult<AnchorConnectionIndex> Level::addAnchorConnection(ArxLevelAnchorConnection value) noexcept {
  if (!data_)
    return api_detail::levelFailure<AnchorConnectionIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  AnchorConnectionIndex out_index = kInvalidAnchorConnectionIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidAnchorConnectionIndex;
    const AnchorConnection connection = internalConnection(value);
    ArxReturnCode rc = level_validation::navigationError(
        navigation::validateConnectionInsertion(data_->navigation, connection, out_index));
    if (rc != ARX_OK) return rc;
    navigation::insertConnection(data_->navigation, out_index, connection);
    return ARX_OK;
  });
  if (result != ARX_OK)
    return api_detail::levelFailure<AnchorConnectionIndex>(
        result,
        api_detail::resourceLocation(
            resourcePath(), LevelElement::kAnchorConnection, data_->navigation.connections.size()));
  return out_index;
}

LevelResult<void> Level::removeAnchorConnection(AnchorConnectionIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->navigation.connections.size())) return ARX_INDEX_OUT_OF_RANGE;
        navigation::removeConnection(data_->navigation, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kAnchorConnection, index));
}

LevelResult<void> Level::replaceAnchors(std::span<const float> positions, std::span<const float> radii,
                                        std::span<const float> heights, std::span<const std::uint32_t> flags) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (positions.size() % 3U != 0) return ARX_LEVEL_BAD_ANCHOR_POSITION;
        const std::size_t count = positions.size() / 3U;
        if (count > static_cast<std::size_t>(kInvalidAnchorIndex)) return ARX_LEVEL_TOO_MANY_ANCHORS;
        if (radii.size() != count || heights.size() != count || flags.size() != count)
          return ARX_LEVEL_BAD_ANCHOR_COUNT;
        std::vector<Anchor> anchors(count);
        for (std::size_t i = 0; i < count; ++i) {
          if (flags[i] > static_cast<std::uint32_t>(std::numeric_limits<std::int16_t>::max()))
            return ARX_LEVEL_BAD_ANCHOR_FLAGS;
          anchors[i].position = {positions[i * 3U], positions[i * 3U + 1U], positions[i * 3U + 2U]};
          anchors[i].radius = radii[i];
          anchors[i].height = heights[i];
          anchors[i].flags = static_cast<std::int16_t>(flags[i]);
          anchors[i].name = std::format("anchor_{}", i);
        }
        ArxReturnCode rc = level_validation::navigationError(navigation::validateAnchorDefinitions(anchors));
        if (rc != ARX_OK) return rc;
        navigation::replaceAnchors(data_->navigation, std::move(anchors), {});
        level_validation::invalidate(data_->validation, LevelValidation::kAnchors);
        level_validation::markValid(data_->validation, LevelValidation::kAnchorConnections);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kAnchor));
}

LevelResult<void> Level::replaceAnchorConnections(std::span<const AnchorIndex> endpoints) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (endpoints.size() % 2U != 0) return ARX_LEVEL_BAD_ANCHOR_CONNECTION_COUNT;
        if (endpoints.size() / 2U > static_cast<std::size_t>(kInvalidAnchorConnectionIndex))
          return ARX_LEVEL_TOO_MANY_ANCHOR_CONNECTIONS;
        std::vector<AnchorConnection> next;
        next.reserve(endpoints.size() / 2U);
        for (std::size_t i = 0; i < endpoints.size(); i += 2U) {
          AnchorIndex first = endpoints[i];
          AnchorIndex second = endpoints[i + 1U];
          if (first == second) return ARX_LEVEL_BAD_ANCHOR_CONNECTION_ORDER;
          if (first > second) std::swap(first, second);
          next.push_back({first, second});
        }
        std::sort(next.begin(), next.end(), [](const AnchorConnection& lhs, const AnchorConnection& rhs) {
          return lhs.first < rhs.first || (lhs.first == rhs.first && lhs.second < rhs.second);
        });
        ArxReturnCode rc =
            level_validation::navigationError(navigation::validateConnections(data_->navigation.anchors, next));
        if (rc != ARX_OK) return rc;
        navigation::replaceConnections(data_->navigation, std::move(next));
        level_validation::markValid(data_->validation, LevelValidation::kAnchorConnections);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kAnchorConnection));
}

void Level::clearAnchorConnections() noexcept {
  if (!data_) return;
  navigation::replaceConnections(data_->navigation, {});
  level_validation::markValid(data_->validation, LevelValidation::kAnchorConnections);
}

void Level::clearAnchors() noexcept {
  if (data_) navigation::clearAnchors(data_->navigation);
}

LevelResult<void> Level::setNavSurface(std::span<const float> positions,
                                       std::span<const NavSurfaceVertexIndex> triangle_indices) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (triangle_indices.size() % 3U != 0) return ARX_LEVEL_BAD_NAV_SURFACE_TRIANGLE;
        if (positions.empty() && triangle_indices.empty()) {
          navigation::clearSurface(data_->navigation);
          level_validation::markValid(data_->validation, LevelValidation::kNavSurface);
          return ARX_OK;
        }
        NavSurface surface;
        const geometry::Error vertex_error = geometry::buildVertices(positions, surface.vertices);
        if (vertex_error == geometry::Error::kTooManyVertices) return ARX_LEVEL_TOO_MANY_NAV_SURFACE_VERTICES;
        if (vertex_error == geometry::Error::kOutOfMemory) return ARX_BAD_ALLOC;
        if (vertex_error != geometry::Error::kNone) return ARX_LEVEL_BAD_NAV_SURFACE_VERTEX;
        surface.triangles.resize(triangle_indices.size() / 3U);
        for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
          surface.triangles[i / 3U].vertices[i % 3U] = triangle_indices[i];
        }
        ArxReturnCode rc = level_validation::navigationError(navigation::validateSurface(surface));
        if (rc != ARX_OK) return rc;
        navigation::setSurface(data_->navigation, std::move(surface));
        level_validation::markValid(data_->validation, LevelValidation::kNavSurface);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kNavSurfaceTriangle));
}

void Level::clearNavSurface() noexcept {
  if (data_) navigation::clearSurface(data_->navigation);
}

LevelResult<void> Level::setLight(LightIndex index, const ArxLevelLight& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->lighting.lights.size())) return ARX_INDEX_OUT_OF_RANGE;
        Light light;
        if (!internalLight(value, light)) return ARX_INVALID_DATA_POINTER;
        lights::repairLightName(data_->lighting, light, index);
        ArxReturnCode rc = level_validation::lightingError(lights::validateLight(light));
        if (rc != ARX_OK) return rc;
        lights::setLight(data_->lighting, index, std::move(light));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kLight, index));
}

LevelResult<LightIndex> Level::addLight(const ArxLevelLight& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<LightIndex>(ARX_INVALID_STATE,
                                                api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  LightIndex out_index = kInvalidLightIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<LightIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kLight, data_->lighting.lights.size()));
  return out_index;
}

LevelResult<void> Level::removeLight(LightIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->lighting.lights.size())) return ARX_INDEX_OUT_OF_RANGE;
        lights::removeLight(data_->lighting, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kLight, index));
}

LevelResult<void> Level::setPlayerSpawn(const ArxLevelPlayerSpawn& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
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
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kPlayerSpawn));
}

void Level::clearPlayerSpawn() noexcept {
  if (!data_) return;
  scene::clearPlayerSpawn(data_->scene);
  level_validation::markValid(data_->validation, LevelValidation::kPlayerSpawn);
}

LevelResult<void> Level::setEntity(EntityIndex index, const ArxLevelEntity& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->scene.entities.size())) return ARX_INDEX_OUT_OF_RANGE;
        Entity entity;
        if (!internalEntity(value, entity)) return ARX_INVALID_DATA_POINTER;
        if (!math::normalizeRotation(entity.rotation)) return ARX_LEVEL_BAD_ENTITY_ROTATION;
        scene::repairEntityName(data_->scene, entity, index);
        ArxReturnCode rc = level_validation::sceneError(scene::validateEntity(entity));
        if (rc != ARX_OK) return rc;
        scene::setEntity(data_->scene, index, std::move(entity));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kEntity, index));
}

LevelResult<EntityIndex> Level::addEntity(const ArxLevelEntity& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<EntityIndex>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  EntityIndex out_index = kInvalidEntityIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<EntityIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kEntity, data_->scene.entities.size()));
  return out_index;
}

LevelResult<void> Level::removeEntity(EntityIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->scene.entities.size())) return ARX_INDEX_OUT_OF_RANGE;
        scene::removeEntity(data_->scene, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kEntity, index));
}

LevelResult<void> Level::setFog(FogIndex index, const ArxLevelFog& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->scene.fogs.size())) return ARX_INDEX_OUT_OF_RANGE;
        Fog fog;
        if (!internalFog(value, fog)) return ARX_INVALID_DATA_POINTER;
        if (!math::normalizeRotation(fog.rotation)) return ARX_LEVEL_BAD_FOG_ROTATION;
        scene::repairFogName(data_->scene, fog, index);
        ArxReturnCode rc = level_validation::sceneError(scene::validateFog(fog));
        if (rc != ARX_OK) return rc;
        scene::setFog(data_->scene, index, std::move(fog));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFog, index));
}

LevelResult<FogIndex> Level::addFog(const ArxLevelFog& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<FogIndex>(ARX_INVALID_STATE,
                                              api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  FogIndex out_index = kInvalidFogIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<FogIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kFog, data_->scene.fogs.size()));
  return out_index;
}

LevelResult<void> Level::removeFog(FogIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->scene.fogs.size())) return ARX_INDEX_OUT_OF_RANGE;
        scene::removeFog(data_->scene, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kFog, index));
}

LevelResult<void> Level::setZone(ZoneIndex index, const ArxLevelZoneInput& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->scene.zones.size())) return ARX_INDEX_OUT_OF_RANGE;
        Zone zone;
        if (!internalZone(value, zone)) return ARX_INVALID_DATA_POINTER;
        if (!validZoneHeightMode(value.value.height_mode)) return ARX_LEVEL_BAD_ZONE_HEIGHT_MODE;
        scene::repairZoneName(data_->scene, zone, index);
        ArxReturnCode rc = level_validation::sceneError(scene::validateZone(zone));
        if (rc != ARX_OK) return rc;
        scene::setZone(data_->scene, index, std::move(zone));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kZone, index));
}

LevelResult<ZoneIndex> Level::addZone(const ArxLevelZoneInput& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<ZoneIndex>(ARX_INVALID_STATE,
                                               api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  ZoneIndex out_index = kInvalidZoneIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<ZoneIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kZone, data_->scene.zones.size()));
  return out_index;
}

LevelResult<void> Level::removeZone(ZoneIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->scene.zones.size())) return ARX_INDEX_OUT_OF_RANGE;
        scene::removeZone(data_->scene, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kZone, index));
}

LevelResult<void> Level::setPath(PathIndex index, const ArxLevelPathInput& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->scene.paths.size())) return ARX_INDEX_OUT_OF_RANGE;
        Path path;
        if (!internalPath(value, path)) return ARX_INVALID_DATA_POINTER;
        if (!validPathNodeTypes(value)) return ARX_LEVEL_BAD_PATH_NODE_TYPE;
        scene::repairPathName(data_->scene, path, index);
        ArxReturnCode rc = level_validation::sceneError(scene::validatePath(path));
        if (rc != ARX_OK) return rc;
        scene::setPath(data_->scene, index, std::move(path));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kPath, index));
}

LevelResult<PathIndex> Level::addPath(const ArxLevelPathInput& value) noexcept {
  if (!data_)
    return api_detail::levelFailure<PathIndex>(ARX_INVALID_STATE,
                                               api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  PathIndex out_index = kInvalidPathIndex;
  const ArxReturnCode result = api_detail::silentStatusBoundary([&]() -> ArxReturnCode {
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
  if (result != ARX_OK)
    return api_detail::levelFailure<PathIndex>(
        result, api_detail::resourceLocation(resourcePath(), LevelElement::kPath, data_->scene.paths.size()));
  return out_index;
}

LevelResult<void> Level::removePath(PathIndex index) noexcept {
  if (!data_)
    return api_detail::levelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), LevelElement::kResource));
  return api_detail::levelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->scene.paths.size())) return ARX_INDEX_OUT_OF_RANGE;
        scene::removePath(data_->scene, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), LevelElement::kPath, index));
}

}  // namespace pistoris
