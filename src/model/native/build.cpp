// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "model/data.h"
#include "model/internal.h"
#include "model/native/internal.h"
#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "modules/textures.h"
#include "native/ftl.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/native_text.h"
#include "utils/resource_path.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris {

using namespace model_native;
namespace {

struct FtlRig {
  std::vector<BoneIndex> vertex_bones;
  std::vector<BoneIndex> parents;
};

FtlRig deriveFtlRig(const ftl::Data& native) {
  FtlRig rig;
  rig.vertex_bones.assign(native.vertices.size(), kInvalidBoneIndex);
  for (std::size_t bone = 0; bone < native.groups.size(); ++bone) {
    for (std::int32_t vertex : native.groups[bone].indices)
      rig.vertex_bones[static_cast<std::size_t>(vertex)] = static_cast<BoneIndex>(bone);
  }

  rig.parents.assign(native.groups.size(), kInvalidBoneIndex);
  for (std::size_t child = 1; child < native.groups.size(); ++child) {
    const std::uint32_t origin = native.groups[child].origin;
    const BoneIndex owner = rig.vertex_bones[origin];
    if (owner != kInvalidBoneIndex && owner < child) {
      rig.parents[child] = owner;
      continue;
    }
    for (std::size_t candidate = child; candidate-- > 0;) {
      const std::vector<std::int32_t>& members = native.groups[candidate].indices;
      if (std::find(members.begin(), members.end(), static_cast<std::int32_t>(origin)) != members.end()) {
        rig.parents[child] = static_cast<BoneIndex>(candidate);
        break;
      }
    }
  }
  return rig;
}

}  // namespace

ArxReturnCode Model::importNative(Model& out, const ftl::Data& native, std::vector<std::string>* texture_source_paths,
                                  NativeTextMode text_mode) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    ArxReturnCode rc = validateFtl(&native);
    if (rc != ARX_OK) return rc;
    if (native.groups.size() > skeleton::kMaxBones) return ARX_MODEL_TOO_MANY_BONES;
    if (native.selections.size() > 64U) return ARX_MODEL_TOO_MANY_SELECTIONS;

    Model result;
    const ArxVector3 native_origin = native.vertices[native.header.origin].position;
    const FtlRig rig = deriveFtlRig(native);

    std::vector<SelectionMask> native_selection_masks(native.vertices.size(), 0);
    IdentifierUniquifier selection_names(
        {.letter_case = IdentifierCase::kLower, .max_length = selections::kMaxNameLength});
    selection_names.reserve(native.selections.size());
    std::array<IdentifierRepair, 64> selection_repairs{};
    std::array<IdentifierRepair, 64> selection_uniqueness_repairs{};
    for (std::size_t index = 0; index < native.selections.size(); ++index) {
      const SelectionId id = static_cast<SelectionId>(index);
      const ftl::Selection& source = native.selections[index];
      Selection& selection = result.data_->selections.slots[id];
      std::string decoded_name;
      if (!native_text::decode(fixedString(source.name), text_mode, decoded_name)) return ARX_FTL_BAD_SELECTION_NAME;
      IdentifierNormalization normalized = normalizeIdentifier(
          decoded_name, {.letter_case = IdentifierCase::kLower, .max_length = selections::kMaxNameLength});
      if (hasIdentifierRepair(normalized.repair, IdentifierRepair::kEmpty)) normalized.value = "selection";
      selection.name = std::move(normalized.value);
      selection_repairs[id] = normalized.repair;
      selection_names.add(selection.name);
      result.data_->selections.occupied |= selections::bit(id);
      for (std::int32_t member : source.selected)
        native_selection_masks[static_cast<std::size_t>(member)] |= selections::bit(id);
    }
    const IdentifierRepairSummary selection_name_summary = selection_names.apply(
        std::span<IdentifierRepair>(selection_uniqueness_repairs.data(), native.selections.size()));
    if (selection_name_summary.exhausted) return ARX_MODEL_DUPLICATE_SELECTION_NAME;
    for (std::size_t index = 0; index < native.selections.size(); ++index) {
      const SelectionId id = static_cast<SelectionId>(index);
      Selection& selection = result.data_->selections.slots[id];
      selection_repairs[id] |= selection_uniqueness_repairs[id];
      if (reportableIdentifierRepair(selection_repairs[id])) {
        std::string decoded_name;
        if (!native_text::decode(fixedString(native.selections[index].name), text_mode, decoded_name))
          return ARX_FTL_BAD_SELECTION_NAME;
        log(ARX_LOG_INFO, "FTL -> Model: selection '{}' normalized to '{}'", decoded_name, selection.name);
      }
      if (cutSelection(selection.name)) {
        const std::size_t leading = static_cast<std::size_t>(native.selections[index].selected.front());
        selection.leading_vertex =
            SelectionLeadingVertex{native.vertices[leading].position - native_origin, rig.vertex_bones[leading]};
      }
    }

    geometry::reserveVertexCapacity(result.data_->geometry, native.vertices.size());
    geometry::reserveFaceCapacity(result.data_->geometry, native.faces.size());
    skeleton::reserveVertexCapacity(result.data_->skeleton, native.vertices.size());
    selections::reserveVertexCapacity(result.data_->selections, native.vertices.size());
    skeleton::reserveBoneCapacity(result.data_->skeleton, native.groups.size());
    selections::reserveBoneCapacity(result.data_->selections, native.groups.size());
    action_points::reserveActionPointCapacity(result.data_->action_points, native.actions.size());
    selections::reserveActionPointCapacity(result.data_->selections, native.actions.size());
    result.data_->textures.textures.reserve(native.texture_containers.size());
    std::vector<std::string> source_paths;
    if (texture_source_paths) source_paths.reserve(native.texture_containers.size());

    std::vector<TextureIndex> texture_remap(native.texture_containers.size(), kNoTexture);
    std::unordered_map<std::string, TextureIndex, ResourcePathIdentityHash, ResourcePathIdentityEqual> textures_by_path;
    std::unordered_map<std::string, std::string, ResourcePathIdentityHash, ResourcePathIdentityEqual> decoded_sources;
    textures_by_path.reserve(native.texture_containers.size());
    for (std::size_t index = 0; index < native.texture_containers.size(); ++index) {
      const std::string_view source_path = fixedString(native.texture_containers[index].filename);
      if (source_path.empty()) continue;
      std::string decoded_path;
      if (!native_text::decode(source_path, text_mode, decoded_path)) return ARX_FTL_BAD_TEXTURE_PATH;
      const auto [source, inserted_source] = decoded_sources.try_emplace(decoded_path, source_path);
      if (!inserted_source && !ResourcePathIdentityEqual{}(source->second, source_path)) {
        log(ARX_LOG_ERROR, "FTL -> Model: distinct native texture paths decode to '{}'", decoded_path);
        return ARX_FTL_BAD_TEXTURE_PATH;
      }
      Texture parsed(decoded_path);
      auto existing = textures_by_path.find(std::string_view(parsed.path));
      if (existing != textures_by_path.end()) {
        texture_remap[index] = existing->second;
        continue;
      }
      const TextureIndex texture = static_cast<TextureIndex>(result.data_->textures.textures.size());
      result.data_->textures.textures.push_back(std::move(parsed));
      textures_by_path.emplace(result.data_->textures.textures.back().path, texture);
      texture_remap[index] = texture;
      if (texture_source_paths) source_paths.push_back(std::move(decoded_path));
    }
    textures::PathRepairInfo texture_repairs;
    rc = model_detail::textureError(textures::repairPaths(result.data_->textures.textures, &texture_repairs));
    if (rc != ARX_OK) return rc;
    for (const textures::PathRepairInfo::Repair& repair : texture_repairs.repairs)
      log(ARX_LOG_WARN, "FTL -> Model: texture path '{}' normalized to '{}'", repair.original, repair.repaired);

    std::vector<VertexIndex> vertex_remap(native.vertices.size(), kInvalidVertexIndex);
    std::size_t clamped_blob_shadow_sizes = 0;
    std::size_t discarded_degenerate_faces = 0;
    std::size_t regenerated_corner_normals = 0;
    std::size_t normalized_corner_normals = 0;
    std::size_t stripped_quad_flags = 0;
    for (const ftl::Face& source : native.faces) {
      std::array<ArxVector3, 3> positions{};
      for (std::size_t corner = 0; corner < positions.size(); ++corner)
        positions[corner] = native.vertices[vertexComponent(source.vertex_idx, corner)].position - native_origin;
      if (geometry::degenerateTriangle(positions[0], positions[1], positions[2])) {
        ++discarded_degenerate_faces;
        continue;
      }
      const ArxVector3 generated_face_normal =
          math::normalizeFiniteOr(math::cross(positions[1] - positions[0], positions[2] - positions[0]), {});

      Face face;
      face.normal = source.norm;
      stripped_quad_flags += static_cast<std::size_t>((source.type & kFaceBitQuad) != 0);
      face.flags = source.type & ~kFaceBitQuad;
      face.texture = source.texture_id == kFtlTextureNone ? kNoTexture
                                                          : texture_remap[static_cast<std::size_t>(source.texture_id)];
      face.transval = source.transval;
      for (std::size_t corner = 0; corner < face.corners.size(); ++corner) {
        const std::uint16_t native_vertex = vertexComponent(source.vertex_idx, corner);
        VertexIndex vertex = vertex_remap[native_vertex];
        if (vertex == kInvalidVertexIndex) {
          vertex = static_cast<VertexIndex>(result.data_->geometry.vertices.size());
          result.data_->geometry.vertices.push_back({positions[corner]});
          result.data_->skeleton.vertex_bones.push_back(rig.vertex_bones[native_vertex]);
          result.data_->selections.vertex_masks.push_back(native_selection_masks[native_vertex]);
          vertex_remap[native_vertex] = vertex;
        }
        ArxVector3 normal = native.vertices[native_vertex].normal;
        const double normal_length = math::length(normal);
        if (!std::isfinite(normal_length) || normal_length <= kNormalRepairEpsilon) {
          normal = generated_face_normal;
          ++regenerated_corner_normals;
        } else {
          if (std::abs(normal_length - 1.0) > kNormalRepairEpsilon) ++normalized_corner_normals;
          const double inverse_length = 1.0 / normal_length;
          normal = {
              static_cast<float>(static_cast<double>(normal.x) * inverse_length),
              static_cast<float>(static_cast<double>(normal.y) * inverse_length),
              static_cast<float>(static_cast<double>(normal.z) * inverse_length),
          };
        }
        face.corners[corner] = {
            .vertex = vertex,
            .normal = normal,
            .u = vectorComponent(source.u, corner),
            .v = vectorComponent(source.v, corner),
        };
      }
      result.data_->geometry.faces.push_back(face);
    }
    if (discarded_degenerate_faces != 0)
      log(ARX_LOG_WARN, "FTL -> Model: discarded {} degenerate face(s)", discarded_degenerate_faces);
    for (std::size_t index = 0; index < native.groups.size(); ++index) {
      const ftl::Group& source = native.groups[index];
      Bone bone;
      if (!native_text::decode(fixedString(source.name), text_mode, bone.name)) return ARX_FTL_BAD_GROUP_NAME;
      bone.position = native.vertices[source.origin].position - native_origin;
      bone.parent = rig.parents[index];
      if (!std::isfinite(source.blob_shadow_size)) return ARX_MODEL_BAD_BONE_BLOB_SHADOW_SIZE;
      if (source.blob_shadow_size < 0.0f) {
        bone.blob_shadow_size = 0.0f;
        ++clamped_blob_shadow_sizes;
      } else {
        bone.blob_shadow_size = source.blob_shadow_size;
      }
      result.data_->skeleton.bones.push_back(std::move(bone));
      result.data_->selections.bone_masks.push_back(native_selection_masks[source.origin]);
    }
    IdentifierUniquifier bone_names({.letter_case = IdentifierCase::kLower, .max_length = skeleton::kMaxNameLength});
    bone_names.reserve(result.data_->skeleton.bones.size());
    for (Bone& bone : result.data_->skeleton.bones) bone_names.add(bone.name);
    std::vector<IdentifierRepair> bone_repairs(result.data_->skeleton.bones.size());
    if (bone_names.apply(bone_repairs).exhausted) return ARX_MODEL_DUPLICATE_BONE_NAME;
    for (std::size_t index = 0; index < bone_repairs.size(); ++index)
      if (reportableIdentifierRepair(bone_repairs[index])) {
        std::string decoded_name;
        if (!native_text::decode(fixedString(native.groups[index].name), text_mode, decoded_name))
          return ARX_FTL_BAD_GROUP_NAME;
        log(ARX_LOG_INFO,
            "FTL -> Model: bone '{}' normalized to '{}'",
            decoded_name,
            result.data_->skeleton.bones[index].name);
      }
    result.data_->skeleton.origin_bone = rig.vertex_bones[native.header.origin];
    result.data_->selections.origin_mask = native_selection_masks[native.header.origin];

    for (std::size_t index = 0; index < native.actions.size(); ++index) {
      const ftl::Action& source = native.actions[index];
      const std::size_t vertex = static_cast<std::size_t>(source.vertex_idx);
      const std::string_view source_name = fixedString(source.name);
      ActionPoint point;
      if (!native_text::decode(source_name, text_mode, point.name)) return ARX_FTL_BAD_ACTION_NAME;
      const std::string decoded_name = point.name;
      const IdentifierRepair name_repair = action_points::repairName(point.name);
      point.position = native.vertices[vertex].position - native_origin;
      point.bone = rig.vertex_bones[vertex];
      result.data_->action_points.points.push_back(std::move(point));
      result.data_->selections.action_point_masks.push_back(native_selection_masks[vertex]);
      if (reportableIdentifierRepair(name_repair))
        log(ARX_LOG_INFO,
            "FTL -> Model: action point '{}' normalized to '{}'",
            decoded_name,
            result.data_->action_points.points[index].name);
    }

    rc = result.validate();
    if (rc != ARX_OK) return rc;
    out.swap(result);
    if (texture_source_paths) *texture_source_paths = std::move(source_paths);
    if (regenerated_corner_normals != 0 || normalized_corner_normals != 0 || clamped_blob_shadow_sizes != 0)
      logLazy(ARX_LOG_WARN, [&] {
        std::string warning = "FTL -> Model repairs:";
        if (regenerated_corner_normals != 0)
          warning += std::format(" {} corner normal(s) regenerated;", regenerated_corner_normals);
        if (normalized_corner_normals != 0)
          warning += std::format(" {} corner normal(s) normalized;", normalized_corner_normals);
        if (clamped_blob_shadow_sizes != 0)
          warning += std::format(" {} negative bone blob-shadow size(s) clamped to zero;", clamped_blob_shadow_sizes);
        warning.pop_back();
        return warning;
      });
    if (stripped_quad_flags != 0)
      log(ARX_LOG_WARN, "FTL -> Model: stripped QUAD flag from {} triangular face(s)", stripped_quad_flags);
    return ARX_OK;
  });
}

}  // namespace pistoris
