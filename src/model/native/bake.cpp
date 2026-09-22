// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/model/bake.hpp"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "model/data.h"
#include "model/native/internal.h"
#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "modules/textures.h"
#include "native/ftl.h"
#include "utils/encoded_image.h"
#include "utils/log.h"
#include "utils/native_text.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris {

using namespace model_native;
namespace {

struct NativeTextureProjection {
  TextureIndex source_texture = kNoTexture;
  std::string resource_path;
  std::string image_extension;
  std::vector<std::uint8_t> encoded_image;
};

struct NativeVariantKey {
  VertexIndex vertex = kInvalidVertexIndex;
  std::array<std::uint32_t, 3> normal = {};

  bool operator==(const NativeVariantKey&) const = default;
};

struct NativeVariantKeyHash {
  std::size_t operator()(const NativeVariantKey& key) const noexcept {
    std::size_t result = std::hash<VertexIndex>{}(key.vertex);
    for (std::uint32_t component : key.normal)
      result ^= std::hash<std::uint32_t>{}(component) + 0x9e3779b9U + (result << 6U) + (result >> 2U);
    return result;
  }
};

std::uint32_t normalComponentKey(float value) noexcept {
  return value == 0.0f ? 0U : std::bit_cast<std::uint32_t>(value);
}

NativeVariantKey nativeVariantKey(VertexIndex vertex, const ArxVector3& normal) noexcept {
  return {
      vertex,
      {normalComponentKey(normal.x), normalComponentKey(normal.y), normalComponentKey(normal.z)},
  };
}

struct NativeBuildState {
  ftl::Data data;
  std::vector<BoneIndex> vertex_bones;
  std::vector<SelectionMask> vertex_selections;
  std::unordered_map<NativeVariantKey, std::uint16_t, NativeVariantKeyHash> geometry_variants;
  std::vector<std::uint16_t> first_geometry_variant;
  std::size_t geometry_begin = 0;
  std::size_t geometry_end = 0;
  std::vector<std::uint16_t> bone_vertices;
  std::vector<std::uint16_t> action_vertices;
  std::array<std::optional<std::uint16_t>, 64> leading_vertices;
};

ArxReturnCode validateNativeCounts(const ModelModules& model) noexcept {
  if (model.geometry.vertices.size() >= kFtlMaxVertices) return ARX_MODEL_TOO_MANY_NATIVE_VERTICES;
  if (model.geometry.faces.size() > kFtlMaxFaces) return ARX_MODEL_TOO_MANY_FACES;
  if (model.textures.textures.size() > kFtlMaxTextures) return ARX_MODEL_TOO_MANY_TEXTURES;
  if (model.skeleton.bones.size() > kFtlMaxGroups) return ARX_MODEL_TOO_MANY_BONES;
  if (model.action_points.points.size() > kFtlMaxActions) return ARX_MODEL_TOO_MANY_ACTION_POINTS;
  return ARX_OK;
}

void prepareNativeBuild(const ModelModules& model, NativeBuildState& state) {
  std::size_t vertex_capacity = 1U;
  const auto add_capacity = [&vertex_capacity](std::size_t count) {
    const std::size_t remaining = kFtlMaxVertices - std::min(vertex_capacity, kFtlMaxVertices);
    vertex_capacity += std::min(count, remaining);
  };
  add_capacity(model.geometry.faces.size() * 3U);
  add_capacity(model.geometry.vertices.size());
  add_capacity(model.skeleton.bones.size());
  add_capacity(model.action_points.points.size());
  for (SelectionId id = 0; id < 64U; ++id) {
    if (selections::occupied(model.selections, id) && model.selections.slots[id].leading_vertex) add_capacity(1U);
  }

  state.data.vertices.reserve(vertex_capacity);
  state.vertex_bones.reserve(vertex_capacity);
  state.vertex_selections.reserve(vertex_capacity);
  state.geometry_variants.reserve(std::min(model.geometry.faces.size() * 3U, kFtlMaxVertices));
  state.first_geometry_variant.assign(model.geometry.vertices.size(), std::numeric_limits<std::uint16_t>::max());
  state.data.faces.reserve(model.geometry.faces.size());
  state.data.texture_containers.resize(model.textures.textures.size());
  state.bone_vertices.resize(model.skeleton.bones.size());
  state.action_vertices.resize(model.action_points.points.size());
  state.data.groups.resize(model.skeleton.bones.size());
  state.data.actions.resize(model.action_points.points.size());
  state.data.selections.reserve(64U);
}

bool addNativeVertex(NativeBuildState& state, const ArxVector3& position, const ArxVector3& normal, BoneIndex bone,
                     SelectionMask selections, std::uint16_t& out_index) {
  if (state.data.vertices.size() >= kFtlMaxVertices) return false;
  out_index = static_cast<std::uint16_t>(state.data.vertices.size());
  state.data.vertices.push_back({position, normal});
  state.vertex_bones.push_back(bone);
  state.vertex_selections.push_back(selections);
  return true;
}

std::optional<std::uint16_t> nativeGeometryVariant(const ModelModules& model, NativeBuildState& state,
                                                   VertexIndex vertex, const ArxVector3& normal) {
  const NativeVariantKey key = nativeVariantKey(vertex, normal);
  const auto found = state.geometry_variants.find(key);
  if (found != state.geometry_variants.end()) return found->second;
  std::uint16_t index = 0;
  if (!addNativeVertex(state,
                       model.geometry.vertices[vertex].position,
                       normal,
                       model.skeleton.vertex_bones[vertex],
                       model.selections.vertex_masks[vertex],
                       index))
    return std::nullopt;
  state.geometry_variants.emplace(key, index);
  if (state.first_geometry_variant[vertex] == std::numeric_limits<std::uint16_t>::max())
    state.first_geometry_variant[vertex] = index;
  return index;
}

ArxReturnCode buildNativeGeometry(const ModelModules& model, NativeBuildState& state) {
  std::uint16_t origin = 0;
  if (!addNativeVertex(state, {}, kSyntheticNormal, model.skeleton.origin_bone, model.selections.origin_mask, origin))
    return ARX_MODEL_TOO_MANY_NATIVE_VERTICES;
  state.data.header.origin = origin;

  state.geometry_begin = state.data.vertices.size();
  for (const Face& source : model.geometry.faces) {
    ftl::Face face;
    face.type = source.flags;
    face.texture_id = source.texture == kNoTexture ? kFtlTextureNone : static_cast<std::int16_t>(source.texture);
    face.transval = source.transval;
    for (std::size_t corner = 0; corner < source.corners.size(); ++corner) {
      const Corner& source_corner = source.corners[corner];
      const std::optional<std::uint16_t> vertex =
          nativeGeometryVariant(model, state, source_corner.vertex, source_corner.normal);
      if (!vertex) return ARX_MODEL_TOO_MANY_NATIVE_VERTICES;
      vertexComponent(face.vertex_idx, corner) = *vertex;
      vectorComponent(face.u, corner) = source_corner.u;
      vectorComponent(face.v, corner) = source_corner.v;
    }
    face.norm = source.normal;
    state.data.faces.push_back(face);
  }

  for (VertexIndex vertex = 0; vertex < model.geometry.vertices.size(); ++vertex) {
    if (state.first_geometry_variant[vertex] != std::numeric_limits<std::uint16_t>::max()) continue;
    std::uint16_t index = 0;
    if (!addNativeVertex(state,
                         model.geometry.vertices[vertex].position,
                         kSyntheticNormal,
                         model.skeleton.vertex_bones[vertex],
                         model.selections.vertex_masks[vertex],
                         index))
      return ARX_MODEL_TOO_MANY_NATIVE_VERTICES;
    state.first_geometry_variant[vertex] = index;
  }
  state.geometry_end = state.data.vertices.size();

  return ARX_OK;
}

ArxReturnCode buildNativeTextureContainers(std::span<const NativeTextureProjection> textures,
                                           const NativeModelBakeOptions& options, NativeBuildState& state) {
  for (std::size_t index = 0; index < textures.size(); ++index) {
    if (!native_text::encodeFixed(
            textures[index].resource_path, options.text_mode, state.data.texture_containers[index].filename))
      return ARX_MODEL_BAD_TEXTURE_PATH;
  }
  return ARX_OK;
}

ArxReturnCode buildNativePoints(const ModelModules& model, NativeBuildState& state) {
  for (std::size_t index = 0; index < model.skeleton.bones.size(); ++index) {
    if (!addNativeVertex(state,
                         model.skeleton.bones[index].position,
                         kSyntheticNormal,
                         static_cast<BoneIndex>(index),
                         model.selections.bone_masks[index],
                         state.bone_vertices[index]))
      return ARX_MODEL_TOO_MANY_NATIVE_VERTICES;
  }

  for (std::size_t index = 0; index < model.action_points.points.size(); ++index) {
    if (!addNativeVertex(state,
                         model.action_points.points[index].position,
                         kSyntheticNormal,
                         model.action_points.points[index].bone,
                         model.selections.action_point_masks[index],
                         state.action_vertices[index]))
      return ARX_MODEL_TOO_MANY_NATIVE_VERTICES;
  }

  for (SelectionId id = 0; id < 64U; ++id) {
    if (!selections::occupied(model.selections, id)) continue;
    const std::optional<SelectionLeadingVertex>& leading = model.selections.slots[id].leading_vertex;
    if (!leading) continue;
    std::uint16_t vertex = 0;
    if (!addNativeVertex(state, leading->position, kSyntheticNormal, leading->bone, selections::bit(id), vertex))
      return ARX_MODEL_TOO_MANY_NATIVE_VERTICES;
    state.leading_vertices[id] = vertex;
  }
  return ARX_OK;
}

ArxReturnCode buildNativeRig(const ModelModules& model, NativeTextMode text_mode, NativeBuildState& state) {
  std::vector<std::size_t> member_counts(model.skeleton.bones.size(), 1U);
  for (std::size_t vertex = state.geometry_begin; vertex < state.geometry_end; ++vertex) {
    const BoneIndex bone = state.vertex_bones[vertex];
    if (bone != kInvalidBoneIndex) ++member_counts[bone];
  }
  if (model.skeleton.origin_bone != kInvalidBoneIndex) ++member_counts[model.skeleton.origin_bone];
  for (const ActionPoint& point : model.action_points.points) {
    if (point.bone != kInvalidBoneIndex) ++member_counts[point.bone];
  }
  for (const std::optional<std::uint16_t>& leading : state.leading_vertices) {
    if (!leading) continue;
    const BoneIndex bone = state.vertex_bones[*leading];
    if (bone != kInvalidBoneIndex) ++member_counts[bone];
  }
  for (const Bone& bone : model.skeleton.bones) {
    if (bone.parent != kInvalidBoneIndex) ++member_counts[bone.parent];
  }

  for (std::size_t index = 0; index < model.skeleton.bones.size(); ++index) {
    const Bone& source = model.skeleton.bones[index];
    ftl::Group& target = state.data.groups[index];
    if (!native_text::encodeTruncated(source.name, text_mode, target.name)) return ARX_MODEL_BAD_BONE_NAME;
    target.origin = state.bone_vertices[index];
    target.blob_shadow_size = source.blob_shadow_size;
    target.indices.reserve(member_counts[index]);
    target.indices.push_back(state.bone_vertices[index]);
  }

  for (std::size_t vertex = state.geometry_begin; vertex < state.geometry_end; ++vertex) {
    const BoneIndex bone = state.vertex_bones[vertex];
    if (bone != kInvalidBoneIndex) state.data.groups[bone].indices.push_back(static_cast<std::int32_t>(vertex));
  }
  if (model.skeleton.origin_bone != kInvalidBoneIndex)
    state.data.groups[model.skeleton.origin_bone].indices.push_back(
        static_cast<std::int32_t>(state.data.header.origin));
  for (std::size_t point = 0; point < model.action_points.points.size(); ++point) {
    const BoneIndex bone = model.action_points.points[point].bone;
    if (bone != kInvalidBoneIndex) state.data.groups[bone].indices.push_back(state.action_vertices[point]);
  }
  for (const std::optional<std::uint16_t>& leading : state.leading_vertices) {
    if (!leading) continue;
    const BoneIndex bone = state.vertex_bones[*leading];
    if (bone != kInvalidBoneIndex) state.data.groups[bone].indices.push_back(*leading);
  }
  for (std::size_t child = 0; child < model.skeleton.bones.size(); ++child) {
    const BoneIndex parent = model.skeleton.bones[child].parent;
    if (parent != kInvalidBoneIndex) state.data.groups[parent].indices.push_back(state.bone_vertices[child]);
  }

  for (std::size_t index = 0; index < model.action_points.points.size(); ++index) {
    if (!native_text::encodeTruncated(
            model.action_points.points[index].name, text_mode, state.data.actions[index].name))
      return ARX_MODEL_BAD_ACTION_POINT_NAME;
    state.data.actions[index].vertex_idx = state.action_vertices[index];
  }
  return ARX_OK;
}

std::optional<std::uint16_t> inferredCutProbe(const ModelModules& model, const NativeBuildState& state,
                                              SelectionId id) {
  const SelectionMask member = selections::bit(id);
  for (std::size_t vertex = state.geometry_begin; vertex < state.geometry_end; ++vertex) {
    if ((state.vertex_selections[vertex] & member) != 0) return static_cast<std::uint16_t>(vertex);
  }
  for (std::uint16_t vertex : state.bone_vertices) {
    if ((state.vertex_selections[vertex] & member) != 0) return vertex;
  }
  for (std::uint16_t vertex : state.action_vertices) {
    if ((state.vertex_selections[vertex] & member) != 0) return vertex;
  }
  if ((model.selections.origin_mask & member) != 0) return static_cast<std::uint16_t>(state.data.header.origin);
  return std::nullopt;
}

ArxReturnCode buildNativeSelections(const ModelModules& model, NativeTextMode text_mode, NativeBuildState& state) {
  std::size_t omitted = 0;
  for (SelectionId id = 0; id < 64U; ++id) {
    if (!selections::occupied(model.selections, id)) continue;
    const Selection& source = model.selections.slots[id];
    const SelectionMask member = selections::bit(id);
    std::vector<std::int32_t> selected;
    selected.reserve(static_cast<std::size_t>(std::count_if(
        state.vertex_selections.begin(), state.vertex_selections.end(), [member](SelectionMask selections) {
          return (selections & member) != 0;
        })));
    const std::optional<std::uint16_t>& leading = state.leading_vertices[id];
    if (leading) selected.push_back(*leading);
    for (std::size_t vertex = 0; vertex < state.vertex_selections.size(); ++vertex) {
      if ((state.vertex_selections[vertex] & member) == 0 || (leading && vertex == *leading)) continue;
      selected.push_back(static_cast<std::int32_t>(vertex));
    }

    if (!source.leading_vertex && !selected.empty() && cutSelection(source.name)) {
      const std::optional<std::uint16_t> probe = inferredCutProbe(model, state, id);
      if (probe) {
        const auto found = std::find(selected.begin(), selected.end(), static_cast<std::int32_t>(*probe));
        if (found != selected.end()) std::rotate(selected.begin(), found, found + 1);
        const ArxVector3& position = state.data.vertices[*probe].position;
        log(ARX_LOG_WARN,
            "Model -> FTL: selection '{}' cut probe inferred at {}, {}, {}",
            source.name,
            position.x,
            position.y,
            position.z);
      }
    }

    if (selected.empty()) {
      ++omitted;
      continue;
    }
    ftl::Selection selection;
    if (!native_text::encodeTruncated(source.name, text_mode, selection.name)) return ARX_MODEL_BAD_SELECTION_NAME;
    selection.selected = std::move(selected);
    state.data.selections.push_back(std::move(selection));
  }
  if (omitted != 0) log(ARX_LOG_INFO, "Model -> FTL: omitted {} empty selection(s)", omitted);
  return ARX_OK;
}

}  // namespace

ArxReturnCode Model::bakeNativeBundle(const NativeModelBakeOptions& options, NativeModelBundle& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!native_text::validMode(options.text_mode)) return ARX_INVALID_OPTIONS;
    ArxReturnCode rc = validate();
    if (rc != ARX_OK) return rc;
    rc = validateNativeCounts(*data_);
    if (rc != ARX_OK) return rc;

    NativeBuildState state;
    prepareNativeBuild(*data_, state);
    rc = buildNativeGeometry(*data_, state);
    if (rc != ARX_OK) return rc;
    rc = buildNativePoints(*data_, state);
    if (rc != ARX_OK) return rc;
    rc = buildNativeRig(*data_, options.text_mode, state);
    if (rc != ARX_OK) return rc;
    rc = buildNativeSelections(*data_, options.text_mode, state);
    if (rc != ARX_OK) return rc;

    std::vector<textures::ImagePreparationRequest> requests;
    if (options.include_texture_files) {
      requests.reserve(data_->textures.textures.size());
      for (std::size_t index = 0; index < data_->textures.textures.size(); ++index) {
        if (data_->textures.textures[index].encoded_image.empty()) continue;
        requests.push_back({static_cast<TextureIndex>(index),
                            {.accepted_formats = image::kFormatsAll,
                             .fallback_format = image::Format::kPng,
                             .require_power_of_two = true}});
      }
    }
    std::vector<textures::PreparedImage> prepared;
    const textures::Error preparation_error = textures::prepareImages(data_->textures, requests, prepared);
    if (preparation_error == textures::Error::kInvalidOptions) return ARX_INVALID_OPTIONS;
    if (preparation_error == textures::Error::kOutOfMemory) return ARX_BAD_ALLOC;
    if (preparation_error != textures::Error::kNone) return ARX_MODEL_BAD_TEXTURE_IMAGE;

    std::vector<NativeTextureProjection> projected;
    projected.reserve(data_->textures.textures.size());
    std::size_t prepared_index = 0;
    std::size_t rescaled_images = 0;
    for (std::size_t index = 0; index < data_->textures.textures.size(); ++index) {
      const Texture& texture = data_->textures.textures[index];
      std::string resource_path = textures::normalizePath(texture.path);
      if (resource_path.empty() || !textures::validExternalPath(resource_path)) return ARX_MODEL_BAD_TEXTURE_PATH;
      NativeTextureProjection item;
      item.source_texture = static_cast<TextureIndex>(index);
      item.resource_path = std::move(resource_path);
      if (options.include_texture_files && !texture.encoded_image.empty()) {
        textures::PreparedImage& image = prepared[prepared_index++];
        item.image_extension = pistoris::image::extension(image.info.format);
        if (item.image_extension.empty()) return ARX_MODEL_BAD_TEXTURE_IMAGE;
        if (image.rescaled) ++rescaled_images;
        if (image.bytes.converted.empty()) {
          item.encoded_image.assign(image.bytes.borrowed.begin(), image.bytes.borrowed.end());
        } else {
          item.encoded_image = std::move(image.bytes.converted);
        }
      }
      projected.push_back(std::move(item));
    }
    if (rescaled_images != 0)
      log(ARX_LOG_WARN,
          "Model native bake: rescaled {} non-power-of-two texture image(s) to power-of-two dimensions for "
          "native repeat sampling",
          rescaled_images);

    rc = buildNativeTextureContainers(projected, options, state);
    if (rc != ARX_OK) return rc;
    rc = validateFtl(&state.data);
    if (rc != ARX_OK) return rc;
    NativeModelBundle bundle;
    bundle.ftl = std::move(state.data);
    if (options.include_texture_files) {
      bundle.texture_files.reserve(projected.size());
      for (NativeTextureProjection& texture : projected) {
        if (texture.encoded_image.empty()) continue;
        bundle.texture_files.push_back({texture.source_texture,
                                        texture.resource_path + texture.image_extension,
                                        std::move(texture.encoded_image)});
      }
    }
    out = std::move(bundle);
    return ARX_OK;
  });
}

}  // namespace pistoris
