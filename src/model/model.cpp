// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/model.hpp"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/texture.h"

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
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/name_tokens.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
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

template <class T>
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
    case inventory_icon::Error::kBadImage:
      return ARX_MODEL_BAD_INVENTORY_ICON;
    case inventory_icon::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
  }
  return ARX_INTERNAL_ERROR;
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
    case geometry::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
    case geometry::Error::kInvalidOptions:
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

Model::Model(const Model& other) : data_(std::make_unique<Data>(*other.data_)) {}

Model& Model::operator=(const Model& other) {
  if (this != &other) {
    Model copy(other);
    swap(copy);
  }
  return *this;
}

void Model::swap(Model& other) noexcept { data_.swap(other.data_); }

void Model::reset() { data_ = std::make_unique<Data>(); }

ArxReturnCode Model::validateMesh() const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = model_detail::textureError(textures::validate(data_->textures.textures));
    if (rc != ARX_OK) return rc;
    return model_detail::geometryError(geometry::validate(data_->geometry, data_->textures.textures.size()));
  });
}

ArxReturnCode Model::validateSkeleton() const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    return model_detail::skeletonError(skeleton::validate(data_->skeleton, data_->geometry.vertices.size()));
  });
}

ArxReturnCode Model::validateActionPoints() const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    return model_detail::actionPointError(action_points::validate(data_->action_points, data_->skeleton.bones.size()));
  });
}

ArxReturnCode Model::validateSelections() const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    return model_detail::selectionError(selections::validate(data_->selections,
                                                             data_->geometry.vertices.size(),
                                                             data_->skeleton.bones.size(),
                                                             data_->action_points.points.size()));
  });
}

ArxReturnCode Model::validate() const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return model_detail::validateStructure(static_cast<const ModelModules&>(*data_)); });
}

std::string_view Model::resourcePath() const noexcept { return data_->resource.path; }

ArxReturnCode Model::setResourcePath(std::string_view resource_path) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::string path;
    const ArxReturnCode rc = modelResourceError(resource::repairPath(ARX_RESOURCE_KIND_MODEL, resource_path, path));
    if (rc != ARX_OK) return rc;
    resource::setPath(data_->resource, std::move(path));
    return ARX_OK;
  });
}

Model::InventoryIconView Model::inventoryIcon() const noexcept {
  return {
      .encoded_image = borrowedImage(data_->inventory_icon.encoded_image),
      .width_slots = data_->inventory_icon.width_slots,
      .height_slots = data_->inventory_icon.height_slots,
  };
}

ArxReturnCode Model::setInventoryIcon(ArxEncodedImageView encoded_image) noexcept {
  return setInventoryIcon(encoded_image, InventoryIconSetOptions{});
}

ArxReturnCode Model::setInventoryIcon(ArxEncodedImageView encoded_image,
                                      const InventoryIconSetOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
  });
}

void Model::clearInventoryIcon() noexcept { inventory_icon::clear(data_->inventory_icon); }

ArxReturnCode Model::renderIconPng(const InventoryIconRenderOptions& options,
                                   std::vector<std::uint8_t>& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    inventory_icon::Layout layout = inventory_icon::Layout::kCenter;
    switch (options.layout) {
      case InventoryIconLayout::kCenter:
        layout = inventory_icon::Layout::kCenter;
        break;
      case InventoryIconLayout::kTopLeft:
        layout = inventory_icon::Layout::kTopLeft;
        break;
      case InventoryIconLayout::kTopRight:
        layout = inventory_icon::Layout::kTopRight;
        break;
      case InventoryIconLayout::kBottomLeft:
        layout = inventory_icon::Layout::kBottomLeft;
        break;
      case InventoryIconLayout::kBottomRight:
        layout = inventory_icon::Layout::kBottomRight;
        break;
      case InventoryIconLayout::kStretch:
        layout = inventory_icon::Layout::kStretch;
        break;
      default:
        return ARX_INVALID_OPTIONS;
    }
    return inventoryIconError(inventory_icon::renderPng(data_->inventory_icon,
                                                        {
                                                            .width_slots = options.width_slots,
                                                            .height_slots = options.height_slots,
                                                            .layout = layout,
                                                        },
                                                        out));
  });
}

std::size_t Model::vertexCount() const noexcept { return data_->geometry.vertices.size(); }

std::size_t Model::faceCount() const noexcept { return data_->geometry.faces.size(); }

std::size_t Model::textureCount() const noexcept { return data_->textures.textures.size(); }

std::size_t Model::boneCount() const noexcept { return data_->skeleton.bones.size(); }

std::size_t Model::actionPointCount() const noexcept { return data_->action_points.points.size(); }

std::size_t Model::selectionCount() const noexcept {
  return static_cast<std::size_t>(std::popcount(data_->selections.occupied));
}

ArxReturnCode Model::copyVertices(std::size_t offset, std::size_t count, ArxModelVertex* out_vertices) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->geometry.vertices.size(), offset, count, out_vertices);
  if (rc != ARX_OK) return rc;
  if (data_->skeleton.vertex_bones.size() != data_->geometry.vertices.size()) return ARX_MODEL_BAD_VERTEX_BONE_COUNT;
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t index = offset + i;
    out_vertices[i] = {
        .position = data_->geometry.vertices[index].position,
        .bone = data_->skeleton.vertex_bones[index],
    };
  }
  return ARX_OK;
}

ArxReturnCode Model::copyFaces(std::size_t offset, std::size_t count, ArxModelFace* out_faces) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->geometry.faces.size(), offset, count, out_faces);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Face& source = data_->geometry.faces[offset + i];
    ArxModelFace& target = out_faces[i];
    target = {};
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
  }
  return ARX_OK;
}

ArxReturnCode Model::copyTextureViews(std::size_t offset, std::size_t count, ArxTextureView* out_views) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->textures.textures.size(), offset, count, out_views);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const Texture& texture = data_->textures.textures[offset + i];
    out_views[i] = {borrowedString(texture.path),
                    borrowedImage(texture.encoded_image),
                    borrowedString(texture.external_image_extension)};
  }
  return ARX_OK;
}

ArxReturnCode Model::copyBones(std::size_t offset, std::size_t count, ArxModelBone* out_bones) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->skeleton.bones.size(), offset, count, out_bones);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t index = offset + i;
    const Bone& source = data_->skeleton.bones[index];
    out_bones[i] = {
        .name = borrowedString(source.name),
        .position = source.position,
        .parent = source.parent,
        .blob_shadow_size = source.blob_shadow_size,
    };
  }
  return ARX_OK;
}

ArxReturnCode Model::copyActionPoints(std::size_t offset, std::size_t count,
                                      ArxModelActionPoint* out_points) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->action_points.points.size(), offset, count, out_points);
  if (rc != ARX_OK) return rc;
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t index = offset + i;
    const ActionPoint& source = data_->action_points.points[index];
    out_points[i] = {
        .name = borrowedString(source.name),
        .position = source.position,
        .bone = source.bone,
    };
  }
  return ARX_OK;
}

ArxModelOrigin Model::origin() const noexcept { return {.bone = data_->skeleton.origin_bone}; }

ArxReturnCode Model::copySelectionIds(std::size_t offset, std::size_t count, SelectionId* out_ids) const noexcept {
  ArxReturnCode rc = validateCopyRange(selectionCount(), offset, count, out_ids);
  if (rc != ARX_OK) return rc;
  std::size_t visible = 0;
  std::size_t copied = 0;
  for (SelectionId id = 0; id < 64U && copied < count; ++id) {
    if (!selections::occupied(data_->selections, id)) continue;
    if (visible++ < offset) continue;
    out_ids[copied++] = id;
  }
  return ARX_OK;
}

ArxReturnCode Model::selection(SelectionId id, ArxModelSelection& out) const noexcept {
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  const Selection& source = data_->selections.slots[id];
  out = {};
  out.name = borrowedString(source.name);
  if (source.leading_vertex) {
    out.has_leading_vertex = 1;
    out.leading_position = source.leading_vertex->position;
    out.leading_bone = source.leading_vertex->bone;
  }
  return ARX_OK;
}

ArxReturnCode Model::selectionVertexCount(SelectionId id, std::size_t& out_count) const noexcept {
  out_count = 0;
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  out_count = selections::vertexMemberCount(data_->selections, id);
  return ARX_OK;
}

ArxReturnCode Model::selectionBoneCount(SelectionId id, std::size_t& out_count) const noexcept {
  out_count = 0;
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  out_count = selections::boneMemberCount(data_->selections, id);
  return ARX_OK;
}

ArxReturnCode Model::selectionActionPointCount(SelectionId id, std::size_t& out_count) const noexcept {
  out_count = 0;
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  out_count = selections::actionPointMemberCount(data_->selections, id);
  return ARX_OK;
}

ArxReturnCode Model::copySelectionVertices(SelectionId id, std::size_t offset, std::size_t count,
                                           VertexIndex* out_vertices) const noexcept {
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  const ArxReturnCode rc =
      validateCopyRange(selections::vertexMemberCount(data_->selections, id), offset, count, out_vertices);
  if (rc != ARX_OK) return rc;
  selections::copyVertexMembers(data_->selections, id, offset, count, out_vertices);
  return ARX_OK;
}

ArxReturnCode Model::copySelectionBones(SelectionId id, std::size_t offset, std::size_t count,
                                        BoneIndex* out_bones) const noexcept {
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  const ArxReturnCode rc =
      validateCopyRange(selections::boneMemberCount(data_->selections, id), offset, count, out_bones);
  if (rc != ARX_OK) return rc;
  selections::copyBoneMembers(data_->selections, id, offset, count, out_bones);
  return ARX_OK;
}

ArxReturnCode Model::copySelectionActionPoints(SelectionId id, std::size_t offset, std::size_t count,
                                               ActionPointIndex* out_points) const noexcept {
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  const ArxReturnCode rc =
      validateCopyRange(selections::actionPointMemberCount(data_->selections, id), offset, count, out_points);
  if (rc != ARX_OK) return rc;
  selections::copyActionPointMembers(data_->selections, id, offset, count, out_points);
  return ARX_OK;
}

ArxReturnCode Model::selectionIncludesOrigin(SelectionId id, bool& out_includes) const noexcept {
  out_includes = false;
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  out_includes = selections::includesOrigin(data_->selections, id);
  return ARX_OK;
}

ArxReturnCode Model::setVertex(VertexIndex index, const ArxModelVertex& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->geometry.vertices.size())) return ARX_INDEX_OUT_OF_RANGE;
    const Vertex vertex = internalVertex(value);
    ArxReturnCode rc = model_detail::geometryError(geometry::validateVertex(vertex));
    if (rc != ARX_OK) return rc;
    rc = boneReferenceError(value.bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_VERTEX_BONE);
    if (rc != ARX_OK) return rc;
    geometry::setVertex(data_->geometry, index, vertex);
    skeleton::setVertexBone(data_->skeleton, index, value.bone);
    return ARX_OK;
  });
}

ArxReturnCode Model::addVertex(const ArxModelVertex& value, VertexIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidVertexIndex;
    ArxReturnCode rc = model_detail::geometryError(geometry::validateVertex(internalVertex(value)));
    if (rc != ARX_OK) return rc;
    rc = model_detail::geometryError(geometry::validateVertexAppend(data_->geometry, 1));
    if (rc != ARX_OK) return rc;
    rc = boneReferenceError(value.bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_VERTEX_BONE);
    if (rc != ARX_OK) return rc;

    const std::size_t vertex_count = data_->geometry.vertices.size();
    const std::size_t vertex_bone_count = data_->skeleton.vertex_bones.size();
    const std::size_t vertex_mask_count = data_->selections.vertex_masks.size();
    try {
      const VertexIndex index = geometry::addVertex(data_->geometry, internalVertex(value));
      skeleton::appendVertexBone(data_->skeleton, value.bone);
      selections::appendEmptyVertexMask(data_->selections);
      out_index = index;
      return ARX_OK;
    } catch (...) {
      selections::truncateVertexMasks(data_->selections, vertex_mask_count);
      skeleton::truncateVertexBones(data_->skeleton, vertex_bone_count);
      geometry::truncateVertices(data_->geometry, vertex_count);
      throw;
    }
  });
}

ArxReturnCode Model::addVertices(const ArxModelVertex* vertices, std::size_t count,
                                 VertexIndex& out_first_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_first_index = kInvalidVertexIndex;
    if (count == 0) return ARX_INVALID_OPTIONS;
    if (vertices == nullptr) return ARX_INVALID_DATA_POINTER;
    ArxReturnCode rc = model_detail::geometryError(geometry::validateVertexAppend(data_->geometry, count));
    if (rc != ARX_OK) return rc;

    std::vector<Vertex> internal_vertices;
    std::vector<BoneIndex> vertex_bones;
    internal_vertices.reserve(count);
    vertex_bones.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      const Vertex vertex = internalVertex(vertices[i]);
      rc = model_detail::geometryError(geometry::validateVertex(vertex));
      if (rc != ARX_OK) return rc;
      rc = boneReferenceError(vertices[i].bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_VERTEX_BONE);
      if (rc != ARX_OK) return rc;
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

    out_first_index = static_cast<VertexIndex>(vertex_count);
    return ARX_OK;
  });
}

ArxReturnCode Model::setFace(FaceIndex index, const ArxModelFace& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
    Face face;
    ArxReturnCode rc = validateFace(value, data_->geometry, data_->textures.textures.size(), face);
    if (rc != ARX_OK) return rc;
    geometry::setFace(data_->geometry, index, face);
    if ((value.flags & kFaceBitQuad) != 0)
      log(ARX_LOG_WARN, "Model face edit: stripped QUAD flag from triangular face input");
    return ARX_OK;
  });
}

ArxReturnCode Model::addFace(const ArxModelFace& value, FaceIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidFaceIndex;
    Face face;
    ArxReturnCode rc = validateFace(value, data_->geometry, data_->textures.textures.size(), face);
    if (rc != ARX_OK) return rc;
    if (data_->geometry.faces.size() >= static_cast<std::size_t>(kInvalidFaceIndex)) return ARX_MODEL_TOO_MANY_FACES;
    out_index = geometry::addFace(data_->geometry, face);
    if ((value.flags & kFaceBitQuad) != 0)
      log(ARX_LOG_WARN, "Model face edit: stripped QUAD flag from triangular face input");
    return ARX_OK;
  });
}

ArxReturnCode Model::removeFace(FaceIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->geometry.faces.size())) return ARX_INDEX_OUT_OF_RANGE;
    geometry::removeFace(data_->geometry, index);
    return ARX_OK;
  });
}

ArxReturnCode Model::compactVertices(std::size_t* removed) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validateMesh();
    if (rc != ARX_OK) return rc;
    geometry::VertexIndexRemap remap;
    const std::size_t count = geometry::compactVertices(data_->geometry, &remap);
    if (!remap.empty()) {
      skeleton::remapVertexBones(data_->skeleton, remap);
      selections::remapVertexMasks(data_->selections, remap);
    }
    if (removed) *removed = count;
    return ARX_OK;
  });
}

ArxReturnCode Model::compactTextures(std::size_t* removed) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validateMesh();
    if (rc != ARX_OK) return rc;
    std::vector<std::uint8_t> used;
    rc = model_detail::geometryError(
        geometry::collectTextureUsage(data_->geometry, data_->textures.textures.size(), used));
    if (rc != ARX_OK) return rc;
    std::vector<TextureIndex> remap;
    std::size_t count = 0;
    rc = model_detail::textureError(textures::compact(data_->textures, used, remap, count));
    if (rc != ARX_OK) return rc;
    geometry::remapTextureReferences(data_->geometry, remap);
    if (removed) *removed = count;
    return ARX_OK;
  });
}

ArxReturnCode Model::setTexture(TextureIndex index, const ArxTextureView& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
  });
}

ArxReturnCode Model::addTexture(const ArxTextureView& value, TextureIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kNoTexture;
    Texture texture;
    if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
    ArxReturnCode rc = model_detail::textureError(textures::validateTextureCount(data_->textures.textures.size() + 1U));
    if (rc != ARX_OK) return rc;
    textures::PathRepairInfo repair;
    rc = model_detail::textureError(textures::repairPath(data_->textures, texture, kNoTexture, &repair));
    if (rc != ARX_OK) return rc;
    rc = model_detail::textureError(textures::validateTexture(texture));
    if (rc != ARX_OK) return rc;
    out_index = textures::addTexture(data_->textures, std::move(texture));
    for (const textures::PathRepairInfo::Repair& item : repair.repairs)
      log(ARX_LOG_WARN, "Model texture: '{}' normalized to '{}'", item.original, item.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Model::rebaseTexturePaths(std::string_view directory) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    textures::PathRebaseInfo info;
    const ArxReturnCode rc = model_detail::textureError(textures::rebasePaths(data_->textures, directory, &info));
    if (rc != ARX_OK) return rc;
    for (const textures::PathRebaseInfo::Repair& repair : info.repairs)
      log(ARX_LOG_WARN, "Model texture rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Model::setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
    if (encoded_image.size == 0) return ARX_MODEL_BAD_TEXTURE_IMAGE;
    std::vector<std::uint8_t> image;
    if (!copyImage(encoded_image, image)) return ARX_INVALID_DATA_POINTER;
    const ArxReturnCode rc = model_detail::textureError(textures::validateEncodedImage(image));
    if (rc != ARX_OK) return rc;
    textures::setEncodedImage(data_->textures, index, std::move(image));
    return ARX_OK;
  });
}

ArxReturnCode Model::clearTextureImage(TextureIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->textures.textures.size())) return ARX_INDEX_OUT_OF_RANGE;
    textures::clearEncodedImage(data_->textures, index);
    return ARX_OK;
  });
}

ArxReturnCode Model::replaceMesh(const ArxModelMeshInput& mesh) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = model_detail::geometryError(geometry::validateCounts(mesh.vertex_count, mesh.face_count));
    if (rc != ARX_OK) return rc;
    if (mesh.texture_count > static_cast<std::size_t>(kNoTexture)) return ARX_MODEL_TOO_MANY_TEXTURES;
    if ((mesh.vertex_count != 0 && mesh.vertices == nullptr) || (mesh.face_count != 0 && mesh.faces == nullptr) ||
        (mesh.texture_count != 0 && mesh.textures == nullptr))
      return ARX_INVALID_DATA_POINTER;

    GeometryData geometry;
    TexturesData texture_data;
    std::vector<BoneIndex> vertex_bones;
    std::vector<SelectionMask> vertex_masks;

    geometry.vertices.reserve(mesh.vertex_count);
    vertex_bones.reserve(mesh.vertex_count);
    vertex_masks.reserve(mesh.vertex_count);
    for (std::size_t i = 0; i < mesh.vertex_count; ++i) {
      const ArxModelVertex& source = mesh.vertices[i];
      ArxReturnCode rc = boneReferenceError(source.bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_VERTEX_BONE);
      if (rc != ARX_OK) return rc;
      geometry.vertices.push_back(internalVertex(source));
      vertex_bones.push_back(source.bone);
      vertex_masks.push_back(0);
    }

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
    rc = model_detail::textureError(textures::repairPaths(texture_data.textures, &texture_repairs));
    if (rc != ARX_OK) return rc;

    rc = model_detail::textureError(textures::validate(texture_data.textures));
    if (rc != ARX_OK) return rc;
    rc = model_detail::geometryError(geometry::validate(geometry, texture_data.textures.size()));
    if (rc != ARX_OK) return rc;

    geometry::replace(data_->geometry, std::move(geometry));
    textures::replaceTextures(data_->textures, std::move(texture_data.textures));
    skeleton::replaceVertexBones(data_->skeleton, std::move(vertex_bones));
    selections::replaceVertexMasks(data_->selections, std::move(vertex_masks));
    for (const textures::PathRepairInfo::Repair& repair : texture_repairs.repairs)
      log(ARX_LOG_WARN,
          "Model mesh replacement: texture path '{}' normalized to '{}'",
          repair.original,
          repair.repaired);
    if (stripped_quad_flags != 0)
      log(ARX_LOG_WARN, "Model mesh replacement: stripped QUAD flag from {} triangular face(s)", stripped_quad_flags);
    return ARX_OK;
  });
}

void Model::clearMesh() noexcept {
  geometry::clear(data_->geometry);
  textures::clear(data_->textures);
  skeleton::clearVertexBones(data_->skeleton);
  selections::clearVertexMasks(data_->selections);
}

ArxReturnCode Model::setBone(BoneIndex index, const ArxModelBone& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->skeleton.bones.size())) return ARX_INDEX_OUT_OF_RANGE;
    Bone bone;
    ArxReturnCode rc = internalBone(value, bone);
    if (rc != ARX_OK) return rc;
    skeleton::repairNames(data_->skeleton, std::span<Bone>(&bone, 1), index);
    rc = model_detail::skeletonError(skeleton::validateBone(bone, index));
    if (rc != ARX_OK) return rc;
    skeleton::setBone(data_->skeleton, index, std::move(bone));
    return ARX_OK;
  });
}

ArxReturnCode Model::addBone(const ArxModelBone& value, BoneIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidBoneIndex;
    BoneIndex index = kInvalidBoneIndex;
    try {
      Bone bone;
      ArxReturnCode rc = internalBone(value, bone);
      if (rc != ARX_OK) return rc;
      rc = model_detail::skeletonError(skeleton::validateBoneCount(data_->skeleton.bones.size() + 1U));
      if (rc != ARX_OK) return rc;
      skeleton::repairNames(data_->skeleton, std::span<Bone>(&bone, 1));
      rc = model_detail::skeletonError(skeleton::validateBone(bone, data_->skeleton.bones.size()));
      if (rc != ARX_OK) return rc;
      index = skeleton::addBone(data_->skeleton, std::move(bone));

      selections::appendEmptyBoneMask(data_->selections);
      out_index = index;
      return ARX_OK;
    } catch (...) {
      if (index != kInvalidBoneIndex) skeleton::removeBone(data_->skeleton, index);
      throw;
    }
  });
}

ArxReturnCode Model::removeBone(BoneIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->skeleton.bones.size())) return ARX_INDEX_OUT_OF_RANGE;
    if (skeleton::referencesBone(data_->skeleton, index) ||
        action_points::referencesBone(data_->action_points, index) ||
        selections::leadingVerticesReferenceBone(data_->selections, index))
      return ARX_MODEL_BONE_IN_USE;
    skeleton::removeBone(data_->skeleton, index);
    selections::removeBoneMask(data_->selections, index);
    action_points::remapBoneIndicesAfterRemoval(data_->action_points, index);
    selections::remapLeadingBoneIndicesAfterRemoval(data_->selections, index);
    return ARX_OK;
  });
}

ArxReturnCode Model::replaceSkeleton(const ArxModelSkeletonInput& input) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = model_detail::skeletonError(skeleton::validateBoneCount(input.bone_count));
    if (rc != ARX_OK) return rc;
    if (input.bone_count != 0 && input.bones == nullptr) return ARX_INVALID_DATA_POINTER;

    SkeletonData skeleton_data;
    skeleton_data.origin_bone = input.origin.bone;
    skeleton_data.bones.resize(input.bone_count);
    std::vector<SelectionMask> bone_masks(input.bone_count, 0);
    for (std::size_t i = 0; i < input.bone_count; ++i) {
      const ArxReturnCode rc = internalBone(input.bones[i], skeleton_data.bones[i]);
      if (rc != ARX_OK) return rc;
    }

    skeleton::repairNames(skeleton_data.bones);
    rc = model_detail::skeletonError(skeleton::validateBones(skeleton_data));
    if (rc != ARX_OK) return rc;
    rc = model_detail::skeletonError(skeleton::validateBoneReferences(data_->skeleton.vertex_bones,
                                                                      skeleton_data.origin_bone,
                                                                      data_->geometry.vertices.size(),
                                                                      skeleton_data.bones.size()));
    if (rc != ARX_OK) return rc;
    rc = model_detail::actionPointError(
        action_points::validateBoneReferences(data_->action_points, skeleton_data.bones.size()));
    if (rc != ARX_OK) return rc;
    rc =
        model_detail::selectionError(selections::validateBoneReferences(data_->selections, skeleton_data.bones.size()));
    if (rc != ARX_OK) return rc;

    skeleton::replaceBones(data_->skeleton, std::move(skeleton_data.bones), skeleton_data.origin_bone);
    selections::replaceBoneMasks(data_->selections, std::move(bone_masks));
    selections::clearOriginMask(data_->selections);
    return ARX_OK;
  });
}

ArxReturnCode Model::setOrigin(ArxModelOrigin value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const ArxReturnCode rc = boneReferenceError(value.bone, data_->skeleton.bones.size(), ARX_MODEL_BAD_ORIGIN_BONE);
    if (rc != ARX_OK) return rc;
    skeleton::setOriginBone(data_->skeleton, value.bone);
    return ARX_OK;
  });
}

void Model::clearSkeleton() noexcept {
  skeleton::clearBones(data_->skeleton);
  selections::clearBoneMasks(data_->selections);
  selections::clearOriginMask(data_->selections);
  action_points::clearBoneReferences(data_->action_points);
  selections::clearLeadingBoneReferences(data_->selections);
}

ArxReturnCode Model::setActionPoint(ActionPointIndex index, const ArxModelActionPoint& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->action_points.points.size())) return ARX_INDEX_OUT_OF_RANGE;
    ActionPoint point;
    ArxReturnCode rc = internalActionPoint(value, point);
    if (rc != ARX_OK) return rc;
    rc = model_detail::actionPointError(action_points::validatePoint(point, data_->skeleton.bones.size()));
    if (rc != ARX_OK) return rc;
    action_points::setActionPoint(data_->action_points, index, std::move(point));
    return ARX_OK;
  });
}

ArxReturnCode Model::addActionPoint(const ArxModelActionPoint& value, ActionPointIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidActionPointIndex;
    ActionPointIndex index = kInvalidActionPointIndex;
    try {
      ActionPoint point;
      ArxReturnCode rc = internalActionPoint(value, point);
      if (rc != ARX_OK) return rc;
      rc = model_detail::actionPointError(action_points::validateCount(data_->action_points.points.size() + 1U));
      if (rc != ARX_OK) return rc;
      rc = model_detail::actionPointError(action_points::validatePoint(point, data_->skeleton.bones.size()));
      if (rc != ARX_OK) return rc;
      index = action_points::addActionPoint(data_->action_points, std::move(point));

      selections::appendEmptyActionPointMask(data_->selections);
      out_index = index;
      return ARX_OK;
    } catch (...) {
      if (index != kInvalidActionPointIndex) action_points::removeActionPoint(data_->action_points, index);
      throw;
    }
  });
}

ArxReturnCode Model::removeActionPoint(ActionPointIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validIndex(index, data_->action_points.points.size())) return ARX_INDEX_OUT_OF_RANGE;
    action_points::removeActionPoint(data_->action_points, index);
    selections::removeActionPointMask(data_->selections, index);
    return ARX_OK;
  });
}

ArxReturnCode Model::replaceActionPoints(const ArxModelActionPointsInput& input) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = model_detail::actionPointError(action_points::validateCount(input.action_point_count));
    if (rc != ARX_OK) return rc;
    if (input.action_point_count != 0 && input.action_points == nullptr) return ARX_INVALID_DATA_POINTER;

    ActionPointsData points;
    points.points.resize(input.action_point_count);
    std::vector<SelectionMask> action_point_masks(input.action_point_count, 0);
    for (std::size_t i = 0; i < input.action_point_count; ++i) {
      const ArxReturnCode rc = internalActionPoint(input.action_points[i], points.points[i]);
      if (rc != ARX_OK) return rc;
    }

    rc = model_detail::actionPointError(action_points::validate(points, data_->skeleton.bones.size()));
    if (rc != ARX_OK) return rc;
    action_points::replace(data_->action_points, std::move(points));
    selections::replaceActionPointMasks(data_->selections, std::move(action_point_masks));
    return ARX_OK;
  });
}

void Model::clearActionPoints() noexcept {
  action_points::clear(data_->action_points);
  selections::clearActionPointMasks(data_->selections);
}

ArxReturnCode Model::addSelection(const ArxModelSelection& value, SelectionId& out_id) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_id = kInvalidSelectionId;
    ArxReturnCode rc = model_detail::selectionError(selections::validateSelectionAppend(data_->selections));
    if (rc != ARX_OK) return rc;
    Selection selection;
    rc = internalSelection(value, data_->skeleton.bones.size(), selection);
    if (rc != ARX_OK) return rc;
    selections::repairNames(data_->selections, std::span<Selection>(&selection, 1));
    rc = model_detail::selectionError(selections::validateSelection(selection, data_->skeleton.bones.size()));
    if (rc != ARX_OK) return rc;
    out_id = selections::addSelection(data_->selections, std::move(selection));
    return ARX_OK;
  });
}

ArxReturnCode Model::setSelection(SelectionId id, const ArxModelSelection& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
    Selection selection;
    ArxReturnCode rc = internalSelection(value, data_->skeleton.bones.size(), selection);
    if (rc != ARX_OK) return rc;
    selections::repairNames(data_->selections, std::span<Selection>(&selection, 1), id);
    rc = model_detail::selectionError(selections::validateSelection(selection, data_->skeleton.bones.size()));
    if (rc != ARX_OK) return rc;
    selections::setSelection(data_->selections, id, std::move(selection));
    return ARX_OK;
  });
}

ArxReturnCode Model::updateSelectionMembers(SelectionId id, const ArxModelSelectionMembersInput& members) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
  });
}

ArxReturnCode Model::clearSelectionVertices(SelectionId id) noexcept {
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  selections::clearVertexMembers(data_->selections, id);
  return ARX_OK;
}

ArxReturnCode Model::clearSelectionBones(SelectionId id) noexcept {
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  selections::clearBoneMembers(data_->selections, id);
  return ARX_OK;
}

ArxReturnCode Model::clearSelectionActionPoints(SelectionId id) noexcept {
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  selections::clearActionPointMembers(data_->selections, id);
  return ARX_OK;
}

ArxReturnCode Model::setSelectionIncludesOrigin(SelectionId id, bool includes) noexcept {
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  selections::setIncludesOrigin(data_->selections, id, includes);
  return ARX_OK;
}

ArxReturnCode Model::removeSelection(SelectionId id) noexcept {
  if (!selections::occupied(data_->selections, id)) return ARX_INDEX_OUT_OF_RANGE;
  selections::removeSelection(data_->selections, id);
  return ARX_OK;
}

void Model::clearSelections() noexcept { selections::clearSelections(data_->selections); }

}  // namespace pistoris
