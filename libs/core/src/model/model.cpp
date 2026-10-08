// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/model.hpp"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/texture.h"

#include "api/bulk_copy.h"
#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "model/data.h"
#include "model/internal.h"
#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/inventory_icon.h"
#include "modules/resource.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "modules/textures.h"
#include "utils/encoded_image.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/name_tokens.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

template <class Index>
bool validIndex(Index index, std::size_t size) noexcept {
  return static_cast<std::size_t>(index) < size;
}

ArxStringView borrowedString(const std::string& value) noexcept { return {value.data(), value.size()}; }

ArxEncodedImageView borrowedImage(const std::vector<std::uint8_t>& value) noexcept {
  if (value.empty()) return {};
  return {value.data(), value.size()};
}

ArxReturnCode modelResourceError(resource::Error error) noexcept {
  switch (error) {
    case resource::Error::kNone:
      return ARX_OK;
    case resource::Error::kBadPath:
      return ARX_MODEL_BAD_RESOURCE_PATH;
    case resource::Error::kBadKind:
      return ARX_INTERNAL_ERROR;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode inventoryIconError(inventory_icon::Error error) noexcept {
  switch (error) {
    case inventory_icon::Error::kNone:
      return ARX_OK;
    case inventory_icon::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case inventory_icon::Error::kUnsupportedFormat:
      return ARX_MODEL_UNSUPPORTED_INVENTORY_ICON_FORMAT;
    case inventory_icon::Error::kBadImage:
      return ARX_MODEL_BAD_INVENTORY_ICON;
    case inventory_icon::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
  }
  return ARX_INTERNAL_ERROR;
}

bool inventoryIconRenderOptions(const Model::InventoryIconRenderOptions& options, inventory_icon::RenderOptions& out,
                                image::Format& format) noexcept {
  inventory_icon::Layout layout = inventory_icon::Layout::kCenter;
  switch (options.layout) {
    case Model::InventoryIconLayout::kCenter:
      layout = inventory_icon::Layout::kCenter;
      break;
    case Model::InventoryIconLayout::kTopLeft:
      layout = inventory_icon::Layout::kTopLeft;
      break;
    case Model::InventoryIconLayout::kTopRight:
      layout = inventory_icon::Layout::kTopRight;
      break;
    case Model::InventoryIconLayout::kBottomLeft:
      layout = inventory_icon::Layout::kBottomLeft;
      break;
    case Model::InventoryIconLayout::kBottomRight:
      layout = inventory_icon::Layout::kBottomRight;
      break;
    case Model::InventoryIconLayout::kStretch:
      layout = inventory_icon::Layout::kStretch;
      break;
    default:
      return false;
  }
  out = {
      .width_slots = options.width_slots,
      .height_slots = options.height_slots,
      .layout = layout,
  };
  switch (options.format) {
    case ImageFormat::kUnknown:
      format = image::Format::kUnknown;
      break;
    case ImageFormat::kJpeg:
      format = image::Format::kJpeg;
      break;
    case ImageFormat::kPng:
      format = image::Format::kPng;
      break;
    case ImageFormat::kBmp:
      format = image::Format::kBmp;
      break;
    case ImageFormat::kTga:
      format = image::Format::kTga;
      break;
    default:
      return false;
  }
  return true;
}

bool copyString(ArxStringView value, std::string& out) {
  if (value.size != 0 && value.data == nullptr) return false;
  out.assign(value.data ? value.data : "", value.size);
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

Vertex internalVertex(const ArxModelVertex& vertex) noexcept { return {vertex.position}; }

Face internalFace(const ArxModelFace& source) noexcept {
  Face result;
  for (std::size_t corner = 0; corner < result.corners.size(); ++corner) {
    result.corners[corner] = {
        .vertex = source.corners[corner].vertex,
        .normal = source.corners[corner].normal,
        .u = source.corners[corner].u,
        .v = source.corners[corner].v,
    };
  }
  result.normal = source.normal;
  result.texture = source.texture;
  result.flags = source.flags & ~kFaceBitQuad;
  result.transval = source.transval;
  return result;
}

bool internalTexture(const ArxTextureView& texture, Texture& out) {
  return copyString(texture.path, out.path) && copyImage(texture.encoded_image, out.encoded_image) &&
         copyString(texture.external_image_extension, out.external_image_extension);
}

ArxReturnCode internalBone(const ArxModelBone& source, Bone& out) {
  std::string name;
  if (!copyString(source.name, name)) return ARX_INVALID_DATA_POINTER;
  if (!model_detail::normalizedName(name, out.name)) return ARX_MODEL_BAD_BONE_NAME;
  out.position = source.position;
  out.parent = source.parent;
  out.blob_shadow_size = source.blob_shadow_size;
  return ARX_OK;
}

ArxReturnCode internalActionPoint(const ArxModelActionPoint& source, ActionPoint& out) {
  std::string name;
  if (!copyString(source.name, name)) return ARX_INVALID_DATA_POINTER;
  if (!model_detail::normalizedName(name, out.name)) return ARX_MODEL_BAD_ACTION_POINT_NAME;
  out.position = source.position;
  out.bone = source.bone;
  return ARX_OK;
}

ArxReturnCode internalSelection(const ArxModelSelection& source, std::size_t bone_count, Selection& out) {
  std::string name;
  if (!copyString(source.name, name)) return ARX_INVALID_DATA_POINTER;
  if (!model_detail::normalizedSelectionName(name, out.name)) return ARX_MODEL_BAD_SELECTION_NAME;
  if (source.has_leading_vertex == 0U) {
    out.leading_vertex.reset();
    return ARX_OK;
  }
  if (!math::finite(source.leading_position)) return ARX_MODEL_BAD_SELECTION_LEADING_POSITION;
  if (!skeleton::validBoneIndex(source.leading_bone, bone_count)) return ARX_MODEL_BAD_SELECTION_LEADING_BONE;
  out.leading_vertex = SelectionLeadingVertex{source.leading_position, source.leading_bone};
  return ARX_OK;
}

ArxReturnCode boneReferenceError(BoneIndex bone, std::size_t bone_count, ArxReturnCode error) noexcept {
  return skeleton::validBoneIndex(bone, bone_count) ? ARX_OK : error;
}

ArxReturnCode validateFace(const ArxModelFace& source, const GeometryData& geometry, std::size_t texture_count,
                           Face& face) {
  face = internalFace(source);
  return model_detail::geometryError(
      geometry::validateFaces(std::span<const Face>(&face, 1), geometry.vertices, texture_count));
}

bool geometryWeldOptions(const Model::VertexWeldOptions& source, geometry::VertexWeldOptions& out) noexcept {
  out.radius = source.radius;
  switch (source.metric) {
    case Model::PositionWeldMetric::kEuclidean:
      out.metric = geometry::PositionWeldMetric::kEuclidean;
      break;
    case Model::PositionWeldMetric::kAxisAligned:
      out.metric = geometry::PositionWeldMetric::kAxisAligned;
      break;
    default:
      return false;
  }
  switch (source.degenerate_faces) {
    case Model::DegenerateFacePolicy::kPreserve:
      out.degenerate_faces = geometry::DegenerateFacePolicy::kPreserve;
      break;
    case Model::DegenerateFacePolicy::kReject:
      out.degenerate_faces = geometry::DegenerateFacePolicy::kReject;
      break;
    case Model::DegenerateFacePolicy::kDiscard:
      out.degenerate_faces = geometry::DegenerateFacePolicy::kDiscard;
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace

namespace model_detail {

ArxReturnCode geometryError(geometry::Error error) noexcept {
  switch (error) {
    case geometry::Error::kNone:
      return ARX_OK;
    case geometry::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
    case geometry::Error::kNoGeometry:
      return ARX_MODEL_NO_GEOMETRY;
    case geometry::Error::kTooManyVertices:
      return ARX_MODEL_TOO_MANY_VERTICES;
    case geometry::Error::kTooManyFaces:
      return ARX_MODEL_TOO_MANY_FACES;
    case geometry::Error::kBadVertex:
      return ARX_MODEL_BAD_VERTEX_POSITION;
    case geometry::Error::kBadFaceTexture:
      return ARX_MODEL_BAD_FACE_TEXTURE;
    case geometry::Error::kBadFaceVertex:
      return ARX_MODEL_BAD_FACE_VERTEX;
    case geometry::Error::kBadCornerNormal:
      return ARX_MODEL_BAD_CORNER_NORMAL;
    case geometry::Error::kBadFaceType:
      return ARX_MODEL_BAD_FACE_TYPE;
    case geometry::Error::kBadFaceTransval:
      return ARX_MODEL_BAD_FACE_TRANSVAL;
    case geometry::Error::kBadFaceNormal:
      return ARX_MODEL_BAD_FACE_NORMAL;
    case geometry::Error::kBadFaceUv:
      return ARX_MODEL_BAD_FACE_UV;
    case geometry::Error::kDegenerateFace:
      return ARX_MODEL_DEGENERATE_FACE;
    case geometry::Error::kBadVertexCount:
      return ARX_MODEL_BAD_VERTEX_COUNT;
    case geometry::Error::kBadFaceCount:
      return ARX_MODEL_BAD_FACE_COUNT;
    case geometry::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case geometry::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
    case geometry::Error::kBadVertexWeldSegment:
    case geometry::Error::kOverlappingVertexWeldSegments:
      return ARX_INTERNAL_ERROR;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode textureError(textures::Error error) noexcept {
  switch (error) {
    case textures::Error::kNone:
      return ARX_OK;
    case textures::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
    case textures::Error::kTooManyTextures:
      return ARX_MODEL_TOO_MANY_TEXTURES;
    case textures::Error::kBadTexture:
    case textures::Error::kDuplicateTexture:
      return ARX_MODEL_BAD_TEXTURE_PATH;
    case textures::Error::kBadImage:
      return ARX_MODEL_BAD_TEXTURE_IMAGE;
    case textures::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
    case textures::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode skeletonError(skeleton::Error error) noexcept {
  switch (error) {
    case skeleton::Error::kNone:
      return ARX_OK;
    case skeleton::Error::kTooManyBones:
      return ARX_MODEL_TOO_MANY_BONES;
    case skeleton::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
    case skeleton::Error::kBadName:
      return ARX_MODEL_BAD_BONE_NAME;
    case skeleton::Error::kDuplicateName:
      return ARX_MODEL_DUPLICATE_BONE_NAME;
    case skeleton::Error::kBadPosition:
      return ARX_MODEL_BAD_BONE_POSITION;
    case skeleton::Error::kBadParent:
      return ARX_MODEL_BAD_BONE_PARENT;
    case skeleton::Error::kBadBlobShadowSize:
      return ARX_MODEL_BAD_BONE_BLOB_SHADOW_SIZE;
    case skeleton::Error::kBadVertexBoneCount:
      return ARX_MODEL_BAD_VERTEX_BONE_COUNT;
    case skeleton::Error::kBadVertexBone:
      return ARX_MODEL_BAD_VERTEX_BONE;
    case skeleton::Error::kBadOriginBone:
      return ARX_MODEL_BAD_ORIGIN_BONE;
    case skeleton::Error::kReferenceBoneCountMismatch:
      return ARX_MODEL_REFERENCE_BONE_COUNT_MISMATCH;
    case skeleton::Error::kReferenceBoneTopologyMismatch:
      return ARX_MODEL_REFERENCE_BONE_TOPOLOGY_MISMATCH;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode actionPointError(action_points::Error error) noexcept {
  switch (error) {
    case action_points::Error::kNone:
      return ARX_OK;
    case action_points::Error::kTooManyActionPoints:
      return ARX_MODEL_TOO_MANY_ACTION_POINTS;
    case action_points::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
    case action_points::Error::kBadName:
      return ARX_MODEL_BAD_ACTION_POINT_NAME;
    case action_points::Error::kBadPosition:
      return ARX_MODEL_BAD_ACTION_POINT_POSITION;
    case action_points::Error::kBadBone:
      return ARX_MODEL_BAD_ACTION_POINT_BONE;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode selectionError(selections::Error error) noexcept {
  switch (error) {
    case selections::Error::kNone:
      return ARX_OK;
    case selections::Error::kTooManySelections:
      return ARX_MODEL_TOO_MANY_SELECTIONS;
    case selections::Error::kBadId:
      return ARX_INDEX_OUT_OF_RANGE;
    case selections::Error::kBadCount:
    case selections::Error::kBadMask:
      return ARX_INTERNAL_ERROR;
    case selections::Error::kBadVertexMember:
      return ARX_MODEL_BAD_SELECTION_VERTEX;
    case selections::Error::kBadBoneMember:
      return ARX_MODEL_BAD_SELECTION_BONE;
    case selections::Error::kBadActionPointMember:
      return ARX_MODEL_BAD_SELECTION_ACTION_POINT;
    case selections::Error::kBadName:
      return ARX_MODEL_BAD_SELECTION_NAME;
    case selections::Error::kDuplicateName:
      return ARX_MODEL_DUPLICATE_SELECTION_NAME;
    case selections::Error::kBadLeadingPosition:
      return ARX_MODEL_BAD_SELECTION_LEADING_POSITION;
    case selections::Error::kBadLeadingBone:
      return ARX_MODEL_BAD_SELECTION_LEADING_BONE;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode validateStructure(const ModelModules& modules) noexcept {
  ArxReturnCode rc = modelResourceError(resource::validate(modules.resource, ARX_RESOURCE_KIND_MODEL));
  if (rc != ARX_OK) return rc;
  rc = inventoryIconError(inventory_icon::validate(modules.inventory_icon));
  if (rc != ARX_OK) return rc;
  rc = textureError(textures::validate(modules.textures.textures));
  if (rc != ARX_OK) return rc;
  rc = geometryError(geometry::validate(modules.geometry, modules.textures.textures.size()));
  if (rc != ARX_OK) return rc;
  rc = skeletonError(skeleton::validate(modules.skeleton, modules.geometry.vertices.size()));
  if (rc != ARX_OK) return rc;
  rc = actionPointError(action_points::validate(modules.action_points, modules.skeleton.bones.size()));
  if (rc != ARX_OK) return rc;
  return selectionError(selections::validate(modules.selections,
                                             modules.geometry.vertices.size(),
                                             modules.skeleton.bones.size(),
                                             modules.action_points.points.size()));
}

bool normalizedName(std::string_view name, std::string& out) {
  if (name.empty() || name.size() > skeleton::kMaxNameLength || hasEmbeddedNull(name)) return false;
  out.clear();
  out.reserve(name.size());
  for (unsigned char value : name) {
    out.push_back(value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : static_cast<char>(value));
  }
  return true;
}

bool normalizedSelectionName(std::string_view name, std::string& out) {
  if (name.empty() || name.size() > selections::kMaxNameLength) return false;
  out.clear();
  out.reserve(name.size());
  for (unsigned char value : name) {
    out.push_back(value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : static_cast<char>(value));
  }
  return selections::validName(out);
}

}  // namespace model_detail

Model::Model() : data_(std::make_unique<Data>()) {}

Model::~Model() = default;

Model::Model(const Model& other) : data_(other.data_ ? std::make_unique<Data>(*other.data_) : nullptr) {}

Model::Model(Model&& other) noexcept = default;

Model& Model::operator=(const Model& other) {
  if (this != &other) {
    Model copy(other);
    swap(copy);
  }
  return *this;
}

Model& Model::operator=(Model&& other) noexcept = default;

void Model::swap(Model& other) noexcept { data_.swap(other.data_); }

ModelResult<void> Model::reset() noexcept {
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<void> {
    data_ = std::make_unique<Data>();
    return ModelResult<void>::success();
  });
}

ModelResult<void> Model::validateGeometry() const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::resourceValidationBoundary<ModelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        ArxReturnCode rc = model_detail::textureError(textures::validate(data_->textures.textures));
        if (rc != ARX_OK) return rc;
        return model_detail::geometryError(geometry::validate(data_->geometry, data_->textures.textures.size()));
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::validateSkeleton() const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::resourceValidationBoundary<ModelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        return model_detail::skeletonError(skeleton::validate(data_->skeleton, data_->geometry.vertices.size()));
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kBone));
}

ModelResult<void> Model::validateActionPoints() const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::resourceValidationBoundary<ModelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        return model_detail::actionPointError(
            action_points::validate(data_->action_points, data_->skeleton.bones.size()));
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kActionPoint));
}

ModelResult<void> Model::validateSelections() const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::resourceValidationBoundary<ModelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        return model_detail::selectionError(selections::validate(data_->selections,
                                                                 data_->geometry.vertices.size(),
                                                                 data_->skeleton.bones.size(),
                                                                 data_->action_points.points.size()));
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kSelection));
}

ModelResult<void> Model::validate() const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::resourceValidationBoundary<ModelResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode { return model_detail::validateStructure(static_cast<const ModelModules&>(*data_)); },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

std::string_view Model::resourcePath() const noexcept {
  return data_ ? std::string_view(data_->resource.path) : std::string_view{};
}

ModelResult<void> Model::setResourcePath(std::string_view resource_path) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        std::string path;
        const ArxReturnCode rc = modelResourceError(resource::repairPath(ARX_RESOURCE_KIND_MODEL, resource_path, path));
        if (rc != ARX_OK) return rc;
        resource::setPath(data_->resource, std::move(path));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

Model::InventoryIconView Model::inventoryIcon() const noexcept {
  if (!data_) return {};
  return {
      .encoded_image = borrowedImage(data_->inventory_icon.encoded_image),
      .width_slots = data_->inventory_icon.width_slots,
      .height_slots = data_->inventory_icon.height_slots,
  };
}

ModelResult<void> Model::setInventoryIcon(ArxEncodedImageView encoded_image) noexcept {
  return setInventoryIcon(encoded_image, InventoryIconSetOptions{});
}

ModelResult<void> Model::setInventoryIcon(ArxEncodedImageView encoded_image,
                                          const InventoryIconSetOptions& options) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const inventory_icon::SetOptions set_options{
            .width_slots = options.width_slots,
            .height_slots = options.height_slots,
        };
        ArxReturnCode rc = inventoryIconError(inventory_icon::validateSetOptions(set_options));
        if (rc != ARX_OK) return rc;
        std::vector<std::uint8_t> copy;
        if (!copyImage(encoded_image, copy)) return ARX_INVALID_DATA_POINTER;
        std::uint8_t width_slots = 0;
        std::uint8_t height_slots = 0;
        rc = inventoryIconError(inventory_icon::resolveImageFootprint(copy, set_options, width_slots, height_slots));
        if (rc != ARX_OK) return rc;
        inventory_icon::setImage(data_->inventory_icon, std::move(copy), width_slots, height_slots);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kInventoryIcon));
}

void Model::clearInventoryIcon() noexcept {
  if (data_) inventory_icon::clear(data_->inventory_icon);
}

ModelResult<std::vector<std::uint8_t>> Model::renderIcon(const InventoryIconRenderOptions& options) const noexcept {
  if (!data_)
    return api_detail::modelFailure<std::vector<std::uint8_t>>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<std::vector<std::uint8_t>> {
    inventory_icon::RenderOptions internal;
    image::Format format = image::Format::kUnknown;
    if (!inventoryIconRenderOptions(options, internal, format))
      return api_detail::modelFailure<std::vector<std::uint8_t>>(
          ARX_INVALID_OPTIONS, api_detail::resourceLocation(resourcePath(), ModelElement::kInventoryIcon));
    std::vector<std::uint8_t> out;
    const ArxReturnCode rc = inventoryIconError(inventory_icon::render(data_->inventory_icon, internal, format, out));
    if (rc != ARX_OK)
      return api_detail::modelFailure<std::vector<std::uint8_t>>(
          rc, api_detail::resourceLocation(resourcePath(), ModelElement::kInventoryIcon));
    return ModelResult<std::vector<std::uint8_t>>::success(std::move(out));
  });
}

std::size_t Model::vertexCount() const noexcept { return data_ ? data_->geometry.vertices.size() : 0; }

std::size_t Model::faceCount() const noexcept { return data_ ? data_->geometry.faces.size() : 0; }

std::size_t Model::textureCount() const noexcept { return data_ ? data_->textures.textures.size() : 0; }

std::size_t Model::boneCount() const noexcept { return data_ ? data_->skeleton.bones.size() : 0; }

std::size_t Model::actionPointCount() const noexcept { return data_ ? data_->action_points.points.size() : 0; }

std::size_t Model::selectionCount() const noexcept {
  return data_ ? static_cast<std::size_t>(std::popcount(data_->selections.occupied)) : 0;
}

ArxModelVertex Model::vertexAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& model = *static_cast<const Model*>(owner);
  assert(model.data_ && index < model.data_->geometry.vertices.size());
  return {
      .position = model.data_->geometry.vertices[index].position,
      .bone = model.data_->skeleton.vertex_bones[index],
  };
}

ArxModelFace Model::faceAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& model = *static_cast<const Model*>(owner);
  assert(model.data_ && index < model.data_->geometry.faces.size());
  const Face& source = model.data_->geometry.faces[index];
  ArxModelFace target{};
  for (std::size_t corner = 0; corner < source.corners.size(); ++corner) {
    target.corners[corner] = {
        .vertex = source.corners[corner].vertex,
        .normal = source.corners[corner].normal,
        .u = source.corners[corner].u,
        .v = source.corners[corner].v,
    };
  }
  target.normal = source.normal;
  target.texture = source.texture;
  target.flags = source.flags;
  target.transval = source.transval;
  return target;
}

ArxTextureView Model::textureAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& model = *static_cast<const Model*>(owner);
  assert(model.data_ && index < model.data_->textures.textures.size());
  const Texture& texture = model.data_->textures.textures[index];
  return {borrowedString(texture.path),
          borrowedImage(texture.encoded_image),
          borrowedString(texture.external_image_extension)};
}

ArxModelBone Model::boneAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& model = *static_cast<const Model*>(owner);
  assert(model.data_ && index < model.data_->skeleton.bones.size());
  const Bone& source = model.data_->skeleton.bones[index];
  return {
      .name = borrowedString(source.name),
      .position = source.position,
      .parent = source.parent,
      .blob_shadow_size = source.blob_shadow_size,
  };
}

ArxModelActionPoint Model::actionPointAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& model = *static_cast<const Model*>(owner);
  assert(model.data_ && index < model.data_->action_points.points.size());
  const ActionPoint& source = model.data_->action_points.points[index];
  return {
      .name = borrowedString(source.name),
      .position = source.position,
      .bone = source.bone,
  };
}

Model::VerticesView Model::vertices() const noexcept { return {this, 0, vertexCount(), &Model::vertexAt}; }

Model::FacesView Model::faces() const noexcept { return {this, 0, faceCount(), &Model::faceAt}; }

Model::TexturesView Model::textures() const noexcept { return {this, 0, textureCount(), &Model::textureAt}; }

Model::BonesView Model::bones() const noexcept { return {this, 0, boneCount(), &Model::boneAt}; }

Model::ActionPointsView Model::actionPoints() const noexcept {
  return {this, 0, actionPointCount(), &Model::actionPointAt};
}

ArxModelOrigin Model::origin() const noexcept {
  return {.bone = data_ ? data_->skeleton.origin_bone : kInvalidBoneIndex};
}

SelectionId Model::selectionIdAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& model = *static_cast<const Model*>(owner);
  assert(model.data_ && index < model.selectionCount());
  std::size_t visible = 0;
  for (SelectionId id = 0; id < 64U; ++id) {
    if (!selections::occupied(model.data_->selections, id)) continue;
    if (visible++ == index) return id;
  }
  assert(false);
  return kInvalidSelectionId;
}

Model::SelectionIdsView Model::selectionIds() const noexcept {
  return {this, 0, selectionCount(), &Model::selectionIdAt};
}

ModelResult<ArxModelSelection> Model::selection(SelectionId id) const noexcept {
  if (!data_)
    return api_detail::modelFailure<ArxModelSelection>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  if (!selections::occupied(data_->selections, id))
    return api_detail::modelFailure<ArxModelSelection>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
  const Selection& source = data_->selections.slots[id];
  ArxModelSelection out{};
  out.name = borrowedString(source.name);
  if (source.leading_vertex) {
    out.has_leading_vertex = 1;
    out.leading_position = source.leading_vertex->position;
    out.leading_bone = source.leading_vertex->bone;
  }
  return ModelResult<ArxModelSelection>::success(out);
}

VertexIndex Model::selectionVertexAt(const void* owner, std::size_t selection, std::size_t index) noexcept {
  const auto& model = *static_cast<const Model*>(owner);
  VertexIndex result = kInvalidVertexIndex;
  selections::copyVertexMembers(model.data_->selections, static_cast<SelectionId>(selection), index, 1, &result);
  return result;
}

BoneIndex Model::selectionBoneAt(const void* owner, std::size_t selection, std::size_t index) noexcept {
  const auto& model = *static_cast<const Model*>(owner);
  BoneIndex result = kInvalidBoneIndex;
  selections::copyBoneMembers(model.data_->selections, static_cast<SelectionId>(selection), index, 1, &result);
  return result;
}

ActionPointIndex Model::selectionActionPointAt(const void* owner, std::size_t selection, std::size_t index) noexcept {
  const auto& model = *static_cast<const Model*>(owner);
  ActionPointIndex result = kInvalidActionPointIndex;
  selections::copyActionPointMembers(model.data_->selections, static_cast<SelectionId>(selection), index, 1, &result);
  return result;
}

ModelResult<Model::SelectionVerticesView> Model::selectionVertices(SelectionId id) const noexcept {
  if (!data_)
    return api_detail::modelFailure<SelectionVerticesView>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  if (!selections::occupied(data_->selections, id))
    return api_detail::modelFailure<SelectionVerticesView>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
  return ModelResult<SelectionVerticesView>::success(
      SelectionVerticesView{this, id, selections::vertexMemberCount(data_->selections, id), &Model::selectionVertexAt});
}

ModelResult<Model::SelectionBonesView> Model::selectionBones(SelectionId id) const noexcept {
  if (!data_)
    return api_detail::modelFailure<SelectionBonesView>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  if (!selections::occupied(data_->selections, id))
    return api_detail::modelFailure<SelectionBonesView>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
  return ModelResult<SelectionBonesView>::success(
      SelectionBonesView{this, id, selections::boneMemberCount(data_->selections, id), &Model::selectionBoneAt});
}

ModelResult<Model::SelectionActionPointsView> Model::selectionActionPoints(SelectionId id) const noexcept {
  if (!data_)
    return api_detail::modelFailure<SelectionActionPointsView>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  if (!selections::occupied(data_->selections, id))
    return api_detail::modelFailure<SelectionActionPointsView>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
  return ModelResult<SelectionActionPointsView>::success(SelectionActionPointsView{
      this, id, selections::actionPointMemberCount(data_->selections, id), &Model::selectionActionPointAt});
}

ModelResult<bool> Model::selectionIncludesOrigin(SelectionId id) const noexcept {
  if (!data_)
    return api_detail::modelFailure<bool>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  if (!selections::occupied(data_->selections, id))
    return api_detail::modelFailure<bool>(ARX_INDEX_OUT_OF_RANGE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
  return ModelResult<bool>::success(selections::includesOrigin(data_->selections, id));
}

ModelResult<void> Model::setVertex(VertexIndex index, const ArxModelVertex& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->geometry.vertices.size())) return ARX_INDEX_OUT_OF_RANGE;
        const Vertex vertex = internalVertex(value);
        ArxReturnCode rc = model_detail::geometryError(geometry::validateVertex(vertex));
        if (rc != ARX_OK) return rc;
        rc = boneReferenceError(value.bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_VERTEX_BONE);
        if (rc != ARX_OK) return rc;
        geometry::setVertex(data_->geometry, index, vertex);
        skeleton::setVertexBone(data_->skeleton, index, value.bone);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kVertex, index));
}

ModelResult<VertexIndex> Model::addVertex(const ArxModelVertex& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<VertexIndex>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<VertexIndex> {
    const ModelLocation location =
        api_detail::resourceLocation(resourcePath(), ModelElement::kVertex, data_->geometry.vertices.size());
    ArxReturnCode rc = model_detail::geometryError(geometry::validateVertex(internalVertex(value)));
    if (rc != ARX_OK) return api_detail::modelFailure<VertexIndex>(rc, location);
    rc = model_detail::geometryError(geometry::validateVertexAppend(data_->geometry, 1));
    if (rc != ARX_OK) return api_detail::modelFailure<VertexIndex>(rc, location);
    rc = boneReferenceError(value.bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_VERTEX_BONE);
    if (rc != ARX_OK) return api_detail::modelFailure<VertexIndex>(rc, location);

    const std::size_t vertex_count = data_->geometry.vertices.size();
    const std::size_t vertex_bone_count = data_->skeleton.vertex_bones.size();
    const std::size_t vertex_mask_count = data_->selections.vertex_masks.size();
    try {
      const VertexIndex index = geometry::addVertex(data_->geometry, internalVertex(value));
      skeleton::appendVertexBone(data_->skeleton, value.bone);
      selections::appendEmptyVertexMask(data_->selections);
      return ModelResult<VertexIndex>::success(index);
    } catch (...) {
      selections::truncateVertexMasks(data_->selections, vertex_mask_count);
      skeleton::truncateVertexBones(data_->skeleton, vertex_bone_count);
      geometry::truncateVertices(data_->geometry, vertex_count);
      throw;
    }
  });
}

ModelResult<VertexIndex> Model::addVertices(std::span<const ArxModelVertex> vertices) noexcept {
  if (!data_)
    return api_detail::modelFailure<VertexIndex>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<VertexIndex> {
    const ModelLocation first_location =
        api_detail::resourceLocation(resourcePath(), ModelElement::kVertex, data_->geometry.vertices.size());
    if (vertices.empty()) return api_detail::modelFailure<VertexIndex>(ARX_INVALID_OPTIONS, first_location);
    const std::size_t count = vertices.size();
    ArxReturnCode rc = model_detail::geometryError(geometry::validateVertexAppend(data_->geometry, count));
    if (rc != ARX_OK) return api_detail::modelFailure<VertexIndex>(rc, first_location);

    std::vector<Vertex> internal_vertices;
    std::vector<BoneIndex> vertex_bones;
    internal_vertices.reserve(count);
    vertex_bones.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      const Vertex vertex = internalVertex(vertices[i]);
      rc = model_detail::geometryError(geometry::validateVertex(vertex));
      const ModelLocation location =
          api_detail::resourceLocation(resourcePath(), ModelElement::kVertex, data_->geometry.vertices.size() + i);
      if (rc != ARX_OK) return api_detail::modelFailure<VertexIndex>(rc, location);
      rc = boneReferenceError(vertices[i].bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_VERTEX_BONE);
      if (rc != ARX_OK) return api_detail::modelFailure<VertexIndex>(rc, location);
      internal_vertices.push_back(vertex);
      vertex_bones.push_back(vertices[i].bone);
    }

    const std::size_t vertex_count = data_->geometry.vertices.size();
    const std::size_t vertex_bone_count = data_->skeleton.vertex_bones.size();
    const std::size_t vertex_mask_count = data_->selections.vertex_masks.size();
    const std::size_t capacity =
        geometry::vertexCapacityForAppend(data_->geometry, count, static_cast<std::size_t>(kInvalidVertexIndex));
    try {
      geometry::reserveVertexCapacity(data_->geometry, capacity);
      skeleton::reserveVertexCapacity(data_->skeleton, capacity);
      selections::reserveVertexCapacity(data_->selections, capacity);
      (void)geometry::appendVertices(data_->geometry, internal_vertices);
      skeleton::appendVertexBones(data_->skeleton, vertex_bones);
      for (std::size_t index = 0; index < count; ++index) selections::appendEmptyVertexMask(data_->selections);
    } catch (...) {
      selections::truncateVertexMasks(data_->selections, vertex_mask_count);
      skeleton::truncateVertexBones(data_->skeleton, vertex_bone_count);
      geometry::truncateVertices(data_->geometry, vertex_count);
      throw;
    }

    return ModelResult<VertexIndex>::success(static_cast<VertexIndex>(vertex_count));
  });
}

ModelResult<void> Model::setFace(FaceIndex index, const ArxModelFace& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
        Face face;
        ArxReturnCode rc = validateFace(value, data_->geometry, data_->textures.textures.size(), face);
        if (rc != ARX_OK) return rc;
        geometry::setFace(data_->geometry, index, face);
        if ((value.flags & kFaceBitQuad) != 0)
          log(ARX_LOG_WARN, "Model face edit: stripped QUAD flag from triangular face input");
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kFace, index));
}

ModelResult<FaceIndex> Model::addFace(const ArxModelFace& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<FaceIndex>(ARX_INVALID_STATE,
                                               api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<FaceIndex> {
    const ModelLocation location =
        api_detail::resourceLocation(resourcePath(), ModelElement::kFace, data_->geometry.faces.size());
    Face face;
    ArxReturnCode rc = validateFace(value, data_->geometry, data_->textures.textures.size(), face);
    if (rc != ARX_OK) return api_detail::modelFailure<FaceIndex>(rc, location);
    if (data_->geometry.faces.size() >= static_cast<std::size_t>(kInvalidFaceIndex))
      return api_detail::modelFailure<FaceIndex>(ARX_MODEL_TOO_MANY_FACES, location);
    const FaceIndex index = geometry::addFace(data_->geometry, face);
    if ((value.flags & kFaceBitQuad) != 0)
      log(ARX_LOG_WARN, "Model face edit: stripped QUAD flag from triangular face input");
    return ModelResult<FaceIndex>::success(index);
  });
}

ModelResult<void> Model::removeFace(FaceIndex index) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
        geometry::removeFace(data_->geometry, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kFace, index));
}

ModelResult<std::size_t> Model::compactVertices() noexcept {
  if (!data_)
    return api_detail::modelFailure<std::size_t>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<std::size_t> {
    ModelResult<void> validation = validateGeometry();
    if (!validation) return api_detail::modelFailure<std::size_t>(std::move(validation));
    geometry::VertexIndexRemap remap;
    const std::size_t count = geometry::compactVertices(data_->geometry, &remap);
    if (!remap.empty()) {
      skeleton::remapVertexBones(data_->skeleton, remap);
      selections::remapVertexMasks(data_->selections, remap);
    }
    return ModelResult<std::size_t>::success(count);
  });
}

ModelResult<void> Model::weldVertices() noexcept { return weldVertices(VertexWeldOptions{}); }

ModelResult<void> Model::weldVertices(const VertexWeldOptions& options) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<void> {
    auto mesh = validateGeometry();
    if (!mesh) return api_detail::modelFailure<void>(std::move(mesh));

    geometry::VertexWeldOptions module_options;
    if (!geometryWeldOptions(options, module_options))
      return api_detail::modelFailure<void>(ARX_INVALID_OPTIONS,
                                            api_detail::resourceLocation(resourcePath(), ModelElement::kVertex));

    assert(data_->skeleton.vertex_bones.size() == data_->geometry.vertices.size());
    assert(data_->selections.vertex_masks.size() == data_->geometry.vertices.size());
    std::vector<VertexIndex> members(data_->geometry.vertices.size());
    std::iota(members.begin(), members.end(), VertexIndex{0});
    std::ranges::sort(members, [&](VertexIndex left, VertexIndex right) {
      const BoneIndex left_bone = data_->skeleton.vertex_bones[left];
      const BoneIndex right_bone = data_->skeleton.vertex_bones[right];
      if (left_bone != right_bone) return left_bone < right_bone;
      const SelectionMask left_mask = data_->selections.vertex_masks[left];
      const SelectionMask right_mask = data_->selections.vertex_masks[right];
      return left_mask != right_mask ? left_mask < right_mask : left < right;
    });

    std::vector<geometry::VertexWeldSegment> segments;
    segments.reserve(members.size());
    std::vector<std::pair<BoneIndex, SelectionMask>> segment_metadata;
    segment_metadata.reserve(members.size());
    for (std::size_t begin = 0; begin < members.size();) {
      std::size_t end = begin + 1U;
      const VertexIndex first = members[begin];
      while (end < members.size()) {
        const VertexIndex next = members[end];
        if (data_->skeleton.vertex_bones[next] != data_->skeleton.vertex_bones[first] ||
            data_->selections.vertex_masks[next] != data_->selections.vertex_masks[first])
          break;
        ++end;
      }
      segments.push_back({std::span<const VertexIndex>(members).subspan(begin, end - begin)});
      segment_metadata.emplace_back(data_->skeleton.vertex_bones[first], data_->selections.vertex_masks[first]);
      begin = end;
    }

    geometry::GeometryRemap remap;
    const geometry::Error error = geometry::weldVerticesSegmented(
        data_->geometry, {.segments = segments, .protected_vertices = {}}, module_options, &remap);
    if (error != geometry::Error::kNone)
      return api_detail::modelFailure<void>(model_detail::geometryError(error),
                                            api_detail::resourceLocation(resourcePath(), ModelElement::kVertex));
    if (!remap.vertices.empty()) {
      for (std::size_t segment = 0; segment < segments.size(); ++segment) {
        const auto [bone, selection_mask] = segment_metadata[segment];
        for (VertexIndex old : segments[segment].vertices) {
          const VertexIndex mapped = remap.vertices[old];
          if (mapped == kInvalidVertexIndex) continue;
          data_->skeleton.vertex_bones[mapped] = bone;
          data_->selections.vertex_masks[mapped] = selection_mask;
        }
      }
      data_->skeleton.vertex_bones.resize(data_->geometry.vertices.size());
      data_->selections.vertex_masks.resize(data_->geometry.vertices.size());
    }
    return ModelResult<void>::success();
  });
}

ModelResult<std::size_t> Model::compactTextures() noexcept {
  if (!data_)
    return api_detail::modelFailure<std::size_t>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<std::size_t> {
    ModelResult<void> validation = validateGeometry();
    if (!validation) return api_detail::modelFailure<std::size_t>(std::move(validation));
    std::vector<std::uint8_t> used;
    ArxReturnCode rc = model_detail::geometryError(
        geometry::collectTextureUsage(data_->geometry, data_->textures.textures.size(), used));
    if (rc != ARX_OK)
      return api_detail::modelFailure<std::size_t>(rc,
                                                   api_detail::resourceLocation(resourcePath(), ModelElement::kFace));
    std::vector<TextureIndex> remap;
    std::size_t count = 0;
    rc = model_detail::textureError(textures::compact(data_->textures, used, remap, count));
    if (rc != ARX_OK)
      return api_detail::modelFailure<std::size_t>(
          rc, api_detail::resourceLocation(resourcePath(), ModelElement::kTexture));
    geometry::remapTextureReferences(data_->geometry, remap);
    return ModelResult<std::size_t>::success(count);
  });
}

ModelResult<void> Model::setTexture(TextureIndex index, const ArxTextureView& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        Texture texture;
        if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
        textures::PathRepairInfo repair;
        ArxReturnCode rc = model_detail::textureError(textures::repairPath(data_->textures, texture, index, &repair));
        if (rc != ARX_OK) return rc;
        rc = model_detail::textureError(textures::validateTexture(texture));
        if (rc != ARX_OK) return rc;
        textures::setTexture(data_->textures, index, std::move(texture));
        for (const textures::PathRepairInfo::Repair& item : repair.repairs)
          log(ARX_LOG_WARN, "Model texture: '{}' normalized to '{}'", item.original, item.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kTexture, index));
}

ModelResult<TextureIndex> Model::addTexture(const ArxTextureView& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<TextureIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<TextureIndex> {
    const ModelLocation location =
        api_detail::resourceLocation(resourcePath(), ModelElement::kTexture, data_->textures.textures.size());
    Texture texture;
    if (!internalTexture(value, texture))
      return api_detail::modelFailure<TextureIndex>(ARX_INVALID_DATA_POINTER, location);
    ArxReturnCode rc = model_detail::textureError(textures::validateTextureCount(data_->textures.textures.size() + 1U));
    if (rc != ARX_OK) return api_detail::modelFailure<TextureIndex>(rc, location);
    textures::PathRepairInfo repair;
    rc = model_detail::textureError(textures::repairPath(data_->textures, texture, kNoTexture, &repair));
    if (rc != ARX_OK) return api_detail::modelFailure<TextureIndex>(rc, location);
    rc = model_detail::textureError(textures::validateTexture(texture));
    if (rc != ARX_OK) return api_detail::modelFailure<TextureIndex>(rc, location);
    const TextureIndex index = textures::addTexture(data_->textures, std::move(texture));
    for (const textures::PathRepairInfo::Repair& item : repair.repairs)
      log(ARX_LOG_WARN, "Model texture: '{}' normalized to '{}'", item.original, item.repaired);
    return ModelResult<TextureIndex>::success(index);
  });
}

ModelResult<void> Model::setTexturePath(TextureIndex index, std::string_view requested) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        Texture candidate(requested);
        textures::PathRepairInfo repair;
        ArxReturnCode rc = model_detail::textureError(textures::repairPath(data_->textures, candidate, index, &repair));
        if (rc != ARX_OK) return rc;
        rc = model_detail::textureError(textures::validateTexture(candidate));
        if (rc != ARX_OK) return rc;
        textures::setPath(data_->textures, index, std::move(candidate.path));
        for (const textures::PathRepairInfo::Repair& item : repair.repairs)
          log(ARX_LOG_WARN, "Model texture: '{}' normalized to '{}'", item.original, item.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kTexture, index));
}

ModelResult<void> Model::setTextureExternalImageExtension(TextureIndex index, std::string_view requested) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        const Texture& current = data_->textures.textures[index];
        if (!current.encoded_image.empty() && !requested.empty()) return ARX_MODEL_BAD_TEXTURE_IMAGE;
        Texture candidate(current.path);
        candidate.external_image_extension = requested;
        const ArxReturnCode rc = model_detail::textureError(textures::validateTexture(candidate));
        if (rc != ARX_OK) return rc;
        textures::setExternalImageExtension(data_->textures, index, std::move(candidate.external_image_extension));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kTexture, index));
}

ModelResult<void> Model::rebaseTexturePaths(std::string_view directory) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        textures::PathRebaseInfo info;
        const ArxReturnCode rc = model_detail::textureError(textures::rebasePaths(data_->textures, directory, &info));
        if (rc != ARX_OK) return rc;
        for (const textures::PathRebaseInfo::Repair& repair : info.repairs)
          log(ARX_LOG_WARN, "Model texture rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kTexture));
}

ModelResult<void> Model::setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        if (encoded_image.size == 0) return ARX_MODEL_BAD_TEXTURE_IMAGE;
        std::vector<std::uint8_t> image;
        if (!copyImage(encoded_image, image)) return ARX_INVALID_DATA_POINTER;
        const ArxReturnCode rc = model_detail::textureError(textures::validateEncodedImage(image));
        if (rc != ARX_OK) return rc;
        textures::setEncodedImage(data_->textures, index, std::move(image));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kTexture, index));
}

ModelResult<void> Model::clearTextureImage(TextureIndex index) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
        textures::clearEncodedImage(data_->textures, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kTexture, index));
}

ModelResult<void> Model::replaceVertices(std::span<const float> positions) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        std::vector<Vertex> vertices;
        const ArxReturnCode rc = model_detail::geometryError(geometry::buildVertices(positions, vertices));
        if (rc != ARX_OK) return rc;
        std::vector<BoneIndex> bones(vertices.size(), kInvalidBoneIndex);
        std::vector<SelectionMask> masks(vertices.size(), 0);
        geometry::replaceVertices(data_->geometry, std::move(vertices));
        skeleton::replaceVertexBones(data_->skeleton, std::move(bones));
        selections::replaceVertexMasks(data_->selections, std::move(masks));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

void Model::clearVertices() noexcept {
  if (!data_) return;
  geometry::clear(data_->geometry);
  skeleton::clearVertexBones(data_->skeleton);
  selections::clearVertexMasks(data_->selections);
}

ModelResult<void> Model::replaceFaces(std::span<const std::uint32_t> vertex_indices, std::span<const float> uvs,
                                      std::span<const float> corner_normals, std::span<const TextureIndex> textures,
                                      std::span<const float> transvals, std::span<const float> face_normals,
                                      std::span<const FaceType> flags) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        std::vector<Face> faces;
        const ArxReturnCode rc = model_detail::geometryError(geometry::buildFaces(data_->geometry.vertices,
                                                                                  data_->textures.textures.size(),
                                                                                  vertex_indices,
                                                                                  uvs,
                                                                                  corner_normals,
                                                                                  textures,
                                                                                  transvals,
                                                                                  face_normals,
                                                                                  flags,
                                                                                  faces));
        if (rc != ARX_OK) return rc;
        geometry::replaceFaces(data_->geometry, std::move(faces));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::copyFaces(const FacesOutput& output) const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const std::size_t count = data_->geometry.faces.size();
        if (count > std::numeric_limits<std::size_t>::max() / 9U) return ARX_INVALID_OPTIONS;
        const std::size_t corner_components = count * 9U;
        if (count > std::numeric_limits<std::size_t>::max() / 6U) return ARX_INVALID_OPTIONS;
        const std::size_t uv_components = count * 6U;
        if (count > std::numeric_limits<std::size_t>::max() / 3U) return ARX_INVALID_OPTIONS;
        const std::size_t index_components = count * 3U;

        api_detail::BulkCopyOutputs outputs;
        outputs.add(output.vertex_indices, index_components);
        outputs.add(output.uvs, uv_components);
        outputs.add(output.corner_normals, corner_components);
        outputs.add(output.textures, count);
        outputs.add(output.transvals, count);
        outputs.add(output.face_normals, index_components);
        outputs.add(output.flags, count);
        const ArxReturnCode rc = outputs.finish();
        if (rc != ARX_OK) return rc;

        for (std::size_t face_index = 0; face_index < count; ++face_index) {
          const Face& face = data_->geometry.faces[face_index];
          if (output.textures) (*output.textures)[face_index] = face.texture;
          if (output.transvals) (*output.transvals)[face_index] = face.transval;
          if (output.face_normals) {
            const std::size_t offset = face_index * 3U;
            (*output.face_normals)[offset] = face.normal.x;
            (*output.face_normals)[offset + 1U] = face.normal.y;
            (*output.face_normals)[offset + 2U] = face.normal.z;
          }
          if (output.flags) (*output.flags)[face_index] = face.flags;
          for (std::size_t corner_index = 0; corner_index < face.corners.size(); ++corner_index) {
            const Corner& corner = face.corners[corner_index];
            const std::size_t index_offset = face_index * 3U + corner_index;
            if (output.vertex_indices) (*output.vertex_indices)[index_offset] = corner.vertex;
            if (output.uvs) {
              const std::size_t uv_offset = index_offset * 2U;
              (*output.uvs)[uv_offset] = corner.u;
              (*output.uvs)[uv_offset + 1U] = corner.v;
            }
            if (output.corner_normals) {
              const std::size_t normal_offset = index_offset * 3U;
              (*output.corner_normals)[normal_offset] = corner.normal.x;
              (*output.corner_normals)[normal_offset + 1U] = corner.normal.y;
              (*output.corner_normals)[normal_offset + 2U] = corner.normal.z;
            }
          }
        }
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::copyVertexPositions(std::span<float> output) const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const std::size_t count = data_->geometry.vertices.size();
        if (count > std::numeric_limits<std::size_t>::max() / 3U) return ARX_INVALID_OPTIONS;
        api_detail::BulkCopyOutputs outputs;
        outputs.add(output, count * 3U);
        const ArxReturnCode rc = outputs.finish();
        if (rc != ARX_OK) return rc;
        for (std::size_t i = 0; i < count; ++i) {
          output[i * 3U] = data_->geometry.vertices[i].position.x;
          output[i * 3U + 1U] = data_->geometry.vertices[i].position.y;
          output[i * 3U + 2U] = data_->geometry.vertices[i].position.z;
        }
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::copyFaceTextures(std::span<TextureIndex> output) const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        api_detail::BulkCopyOutputs outputs;
        outputs.add(output, data_->geometry.faces.size());
        const ArxReturnCode rc = outputs.finish();
        if (rc != ARX_OK) return rc;
        for (std::size_t i = 0; i < output.size(); ++i) output[i] = data_->geometry.faces[i].texture;
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::copyVertexBones(std::span<BoneIndex> output) const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (data_->skeleton.vertex_bones.size() != data_->geometry.vertices.size())
          return ARX_MODEL_BAD_VERTEX_BONE_COUNT;
        api_detail::BulkCopyOutputs outputs;
        outputs.add(output, data_->geometry.vertices.size());
        const ArxReturnCode rc = outputs.finish();
        if (rc != ARX_OK) return rc;
        std::copy(data_->skeleton.vertex_bones.begin(), data_->skeleton.vertex_bones.end(), output.begin());
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::copyActionPointBones(std::span<BoneIndex> output) const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        api_detail::BulkCopyOutputs outputs;
        outputs.add(output, data_->action_points.points.size());
        const ArxReturnCode rc = outputs.finish();
        if (rc != ARX_OK) return rc;
        for (std::size_t i = 0; i < output.size(); ++i) output[i] = data_->action_points.points[i].bone;
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::copyVertexSelectionMasks(std::span<std::uint64_t> output) const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (data_->selections.vertex_masks.size() != data_->geometry.vertices.size())
          return ARX_MODEL_BAD_SELECTION_VERTEX;
        api_detail::BulkCopyOutputs outputs;
        outputs.add(output, data_->geometry.vertices.size());
        const ArxReturnCode rc = outputs.finish();
        if (rc != ARX_OK) return rc;
        std::copy(data_->selections.vertex_masks.begin(), data_->selections.vertex_masks.end(), output.begin());
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::copyBoneSelectionMasks(std::span<std::uint64_t> output) const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (data_->selections.bone_masks.size() != data_->skeleton.bones.size()) return ARX_MODEL_BAD_SELECTION_BONE;
        api_detail::BulkCopyOutputs outputs;
        outputs.add(output, data_->skeleton.bones.size());
        const ArxReturnCode rc = outputs.finish();
        if (rc != ARX_OK) return rc;
        std::copy(data_->selections.bone_masks.begin(), data_->selections.bone_masks.end(), output.begin());
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::copyActionPointSelectionMasks(std::span<std::uint64_t> output) const noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (data_->selections.action_point_masks.size() != data_->action_points.points.size())
          return ARX_MODEL_BAD_SELECTION_ACTION_POINT;
        api_detail::BulkCopyOutputs outputs;
        outputs.add(output, data_->action_points.points.size());
        const ArxReturnCode rc = outputs.finish();
        if (rc != ARX_OK) return rc;
        std::copy(
            data_->selections.action_point_masks.begin(), data_->selections.action_point_masks.end(), output.begin());
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

void Model::clearFaces() noexcept {
  if (data_) geometry::clearFaces(data_->geometry);
}

void Model::clearTextures() noexcept {
  if (!data_) return;
  textures::clear(data_->textures);
  geometry::resetFaceTextures(data_->geometry);
}

ModelResult<void> Model::replaceFaceTextures(std::span<const TextureIndex> textures) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (textures.size() != data_->geometry.faces.size()) return ARX_MODEL_BAD_FACE_COUNT;
        for (TextureIndex texture : textures)
          if (texture != kNoTexture && texture >= data_->textures.textures.size()) return ARX_MODEL_BAD_FACE_TEXTURE;
        geometry::replaceFaceTextures(data_->geometry, textures);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::replaceVertexBones(std::span<const BoneIndex> bones) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (bones.size() != data_->geometry.vertices.size()) return ARX_MODEL_BAD_VERTEX_BONE_COUNT;
        for (BoneIndex bone : bones) {
          const ArxReturnCode rc = boneReferenceError(bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_VERTEX_BONE);
          if (rc != ARX_OK) return rc;
        }
        for (std::size_t i = 0; i < bones.size(); ++i)
          skeleton::setVertexBone(data_->skeleton, static_cast<VertexIndex>(i), bones[i]);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::replaceActionPointBones(std::span<const BoneIndex> bones) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (bones.size() != data_->action_points.points.size()) return ARX_MODEL_BAD_ACTION_POINT_BONE;
        for (BoneIndex bone : bones) {
          const ArxReturnCode rc =
              boneReferenceError(bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_ACTION_POINT_BONE);
          if (rc != ARX_OK) return rc;
        }
        action_points::replaceBoneReferences(data_->action_points, bones);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

std::uint64_t Model::activeSelectionMask() const noexcept { return data_ ? data_->selections.occupied : 0; }

ModelResult<std::uint64_t> Model::selectionMask(SelectionId id) const noexcept {
  if (!data_)
    return api_detail::modelFailure<std::uint64_t>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  if (!selections::occupied(data_->selections, id))
    return api_detail::modelFailure<std::uint64_t>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
  return ModelResult<std::uint64_t>::success(selections::bit(id));
}

ModelResult<void> Model::replaceVertexSelectionMasks(std::span<const std::uint64_t> masks) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (masks.size() != data_->selections.vertex_masks.size()) return ARX_MODEL_BAD_SELECTION_VERTEX;
        for (SelectionMask mask : masks)
          if ((mask & ~data_->selections.occupied) != 0) return ARX_MODEL_BAD_SELECTION_VERTEX;
        selections::assignVertexMasks(data_->selections, masks);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::replaceBoneSelectionMasks(std::span<const std::uint64_t> masks) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (masks.size() != data_->selections.bone_masks.size()) return ARX_MODEL_BAD_SELECTION_BONE;
        for (SelectionMask mask : masks)
          if ((mask & ~data_->selections.occupied) != 0) return ARX_MODEL_BAD_SELECTION_BONE;
        selections::assignBoneMasks(data_->selections, masks);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::replaceActionPointSelectionMasks(std::span<const std::uint64_t> masks) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (masks.size() != data_->selections.action_point_masks.size()) return ARX_MODEL_BAD_SELECTION_ACTION_POINT;
        for (SelectionMask mask : masks)
          if ((mask & ~data_->selections.occupied) != 0) return ARX_MODEL_BAD_SELECTION_ACTION_POINT;
        selections::assignActionPointMasks(data_->selections, masks);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

ModelResult<void> Model::setBone(BoneIndex index, const ArxModelBone& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->skeleton.bones.size())) return ARX_INDEX_OUT_OF_RANGE;
        Bone bone;
        ArxReturnCode rc = internalBone(value, bone);
        if (rc != ARX_OK) return rc;
        skeleton::repairNames(data_->skeleton, std::span<Bone>(&bone, 1), index);
        rc = model_detail::skeletonError(skeleton::validateBone(bone, index));
        if (rc != ARX_OK) return rc;
        skeleton::setBone(data_->skeleton, index, std::move(bone));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kBone, index));
}

ModelResult<BoneIndex> Model::addBone(const ArxModelBone& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<BoneIndex>(ARX_INVALID_STATE,
                                               api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<BoneIndex> {
    BoneIndex index = kInvalidBoneIndex;
    try {
      Bone bone;
      ArxReturnCode rc = internalBone(value, bone);
      const ModelLocation location =
          api_detail::resourceLocation(resourcePath(), ModelElement::kBone, data_->skeleton.bones.size());
      if (rc != ARX_OK) return api_detail::modelFailure<BoneIndex>(rc, location);
      rc = model_detail::skeletonError(skeleton::validateBoneCount(data_->skeleton.bones.size() + 1U));
      if (rc != ARX_OK) return api_detail::modelFailure<BoneIndex>(rc, location);
      skeleton::repairNames(data_->skeleton, std::span<Bone>(&bone, 1));
      rc = model_detail::skeletonError(skeleton::validateBone(bone, data_->skeleton.bones.size()));
      if (rc != ARX_OK) return api_detail::modelFailure<BoneIndex>(rc, location);
      index = skeleton::addBone(data_->skeleton, std::move(bone));

      selections::appendEmptyBoneMask(data_->selections);
      return ModelResult<BoneIndex>::success(index);
    } catch (...) {
      if (index != kInvalidBoneIndex) skeleton::removeBone(data_->skeleton, index);
      throw;
    }
  });
}

ModelResult<void> Model::removeBone(BoneIndex index) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->skeleton.bones.size())) return ARX_INDEX_OUT_OF_RANGE;
        skeleton::removeBone(data_->skeleton, index);
        selections::removeBoneMask(data_->selections, index);
        action_points::remapBoneIndicesAfterRemoval(data_->action_points, index);
        selections::remapLeadingBoneIndicesAfterRemoval(data_->selections, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kBone, index));
}

ModelResult<void> Model::setOrigin(ArxModelOrigin value) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const ArxReturnCode rc =
            boneReferenceError(value.bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_ORIGIN_BONE);
        if (rc != ARX_OK) return rc;
        skeleton::setOriginBone(data_->skeleton, value.bone);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
}

void Model::clearBones() noexcept {
  if (!data_) return;
  skeleton::clearBones(data_->skeleton);
  selections::clearBoneMasks(data_->selections);
  action_points::clearBoneReferences(data_->action_points);
  selections::clearLeadingBoneReferences(data_->selections);
}

ModelResult<void> Model::setActionPoint(ActionPointIndex index, const ArxModelActionPoint& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->action_points.points.size())) return ARX_INDEX_OUT_OF_RANGE;
        ActionPoint point;
        ArxReturnCode rc = internalActionPoint(value, point);
        if (rc != ARX_OK) return rc;
        rc = model_detail::actionPointError(action_points::validatePoint(point, data_->skeleton.bones.size()));
        if (rc != ARX_OK) return rc;
        action_points::setActionPoint(data_->action_points, index, std::move(point));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kActionPoint, index));
}

ModelResult<ActionPointIndex> Model::addActionPoint(const ArxModelActionPoint& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<ActionPointIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<ActionPointIndex> {
    ActionPointIndex index = kInvalidActionPointIndex;
    try {
      ActionPoint point;
      ArxReturnCode rc = internalActionPoint(value, point);
      const ModelLocation location =
          api_detail::resourceLocation(resourcePath(), ModelElement::kActionPoint, data_->action_points.points.size());
      if (rc != ARX_OK) return api_detail::modelFailure<ActionPointIndex>(rc, location);
      rc = model_detail::actionPointError(action_points::validateCount(data_->action_points.points.size() + 1U));
      if (rc != ARX_OK) return api_detail::modelFailure<ActionPointIndex>(rc, location);
      rc = model_detail::actionPointError(action_points::validatePoint(point, data_->skeleton.bones.size()));
      if (rc != ARX_OK) return api_detail::modelFailure<ActionPointIndex>(rc, location);
      index = action_points::addActionPoint(data_->action_points, std::move(point));

      selections::appendEmptyActionPointMask(data_->selections);
      return ModelResult<ActionPointIndex>::success(index);
    } catch (...) {
      if (index != kInvalidActionPointIndex) action_points::removeActionPoint(data_->action_points, index);
      throw;
    }
  });
}

ModelResult<void> Model::removeActionPoint(ActionPointIndex index) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validIndex(index, data_->action_points.points.size())) return ARX_INDEX_OUT_OF_RANGE;
        action_points::removeActionPoint(data_->action_points, index);
        selections::removeActionPointMask(data_->selections, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kActionPoint, index));
}

void Model::clearActionPoints() noexcept {
  if (!data_) return;
  action_points::clear(data_->action_points);
  selections::clearActionPointMasks(data_->selections);
}

ModelResult<SelectionId> Model::addSelection(const ArxModelSelection& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<SelectionId>(ARX_INVALID_STATE,
                                                 api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelBoundary(resourcePath(), [&]() -> ModelResult<SelectionId> {
    ArxReturnCode rc = model_detail::selectionError(selections::validateSelectionAppend(data_->selections));
    const ModelLocation location = api_detail::resourceLocation(resourcePath(), ModelElement::kSelection);
    if (rc != ARX_OK) return api_detail::modelFailure<SelectionId>(rc, location);
    Selection selection;
    rc = internalSelection(value, data_->skeleton.bones.size(), selection);
    if (rc != ARX_OK) return api_detail::modelFailure<SelectionId>(rc, location);
    selections::repairNames(data_->selections, std::span<Selection>(&selection, 1));
    rc = model_detail::selectionError(selections::validateSelection(selection, data_->skeleton.bones.size()));
    if (rc != ARX_OK) return api_detail::modelFailure<SelectionId>(rc, location);
    return ModelResult<SelectionId>::success(selections::addSelection(data_->selections, std::move(selection)));
  });
}

ModelResult<void> Model::setSelection(SelectionId id, const ArxModelSelection& value) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
        Selection selection;
        ArxReturnCode rc = internalSelection(value, data_->skeleton.bones.size(), selection);
        if (rc != ARX_OK) return rc;
        selections::repairNames(data_->selections, std::span<Selection>(&selection, 1), id);
        rc = model_detail::selectionError(selections::validateSelection(selection, data_->skeleton.bones.size()));
        if (rc != ARX_OK) return rc;
        selections::setSelection(data_->selections, id, std::move(selection));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
}

ModelResult<void> Model::updateSelectionMembers(SelectionId id, const ArxModelSelectionMembersInput& members) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if ((members.vertex_count != 0 && members.vertices == nullptr) ||
            (members.bone_count != 0 && members.bones == nullptr) ||
            (members.action_point_count != 0 && members.action_points == nullptr))
          return ARX_INVALID_DATA_POINTER;

        std::optional<std::span<const VertexIndex>> vertices;
        std::optional<std::span<const BoneIndex>> bones;
        std::optional<std::span<const ActionPointIndex>> action_points;
        if (members.vertices != nullptr) vertices.emplace(members.vertices, members.vertex_count);
        if (members.bones != nullptr) bones.emplace(members.bones, members.bone_count);
        if (members.action_points != nullptr) action_points.emplace(members.action_points, members.action_point_count);
        ArxReturnCode rc = model_detail::selectionError(
            selections::validateMemberUpdate(data_->selections, id, vertices, bones, action_points));
        if (rc != ARX_OK) return rc;
        selections::updateMembers(data_->selections, id, vertices, bones, action_points);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
}

ModelResult<void> Model::clearSelectionVertices(SelectionId id) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
        selections::clearVertexMembers(data_->selections, id);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
}

ModelResult<void> Model::clearSelectionBones(SelectionId id) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
        selections::clearBoneMembers(data_->selections, id);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
}

ModelResult<void> Model::clearSelectionActionPoints(SelectionId id) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
        selections::clearActionPointMembers(data_->selections, id);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
}

ModelResult<void> Model::setSelectionIncludesOrigin(SelectionId id, bool includes) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
        selections::setIncludesOrigin(data_->selections, id, includes);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
}

ModelResult<void> Model::removeSelection(SelectionId id) noexcept {
  if (!data_)
    return api_detail::modelFailure<void>(ARX_INVALID_STATE,
                                          api_detail::resourceLocation(resourcePath(), ModelElement::kResource));
  return api_detail::modelStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
        selections::removeSelection(data_->selections, id);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), ModelElement::kSelection, id));
}

void Model::clearSelections() noexcept {
  if (data_) selections::clearSelections(data_->selections);
}

}  // namespace pistoris
