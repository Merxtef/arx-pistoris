// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/skeleton.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <unordered_set>

namespace pistoris::skeleton {

Error validateBone(const Bone& bone, std::size_t index) noexcept {
  if (!isIdentifier(bone.name, {.letter_case = IdentifierCase::kLower, .max_length = kMaxNameLength}))
    return Error::kBadName;
  if (!math::finite(bone.position)) return Error::kBadPosition;
  if (!math::finite(bone.blob_shadow_size) || bone.blob_shadow_size < 0.0f) return Error::kBadBlobShadowSize;
  if (bone.parent != kInvalidBoneIndex && static_cast<std::size_t>(bone.parent) >= index) return Error::kBadParent;
  return Error::kNone;
}

bool validBoneIndex(BoneIndex bone, std::size_t bone_count) noexcept {
  return bone == kInvalidBoneIndex || static_cast<std::size_t>(bone) < bone_count;
}

Error validateBoneCount(std::size_t bone_count) noexcept {
  return bone_count > kMaxBones ? Error::kTooManyBones : Error::kNone;
}

Error validateBones(const SkeletonData& skeleton) {
  const Error count_error = validateBoneCount(skeleton.bones.size());
  if (count_error != Error::kNone) {
    log(ARX_LOG_DEBUG, "Skeleton validation: bone count {} exceeds limit {}", skeleton.bones.size(), kMaxBones);
    return count_error;
  }

  std::unordered_set<std::string_view> names;
  names.reserve(skeleton.bones.size());
  for (std::size_t index = 0; index < skeleton.bones.size(); ++index) {
    const Bone& bone = skeleton.bones[index];
    Error error = validateBone(bone, index);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Skeleton validation: bone {} '{}' is invalid: parent {}, error {}",
          index,
          bone.name,
          bone.parent,
          static_cast<int>(error));
      return error;
    }
    if (!names.insert(bone.name).second) {
      log(ARX_LOG_DEBUG, "Skeleton validation: bone {} duplicates name '{}'", index, bone.name);
      return Error::kDuplicateName;
    }
  }
  return Error::kNone;
}

Error validateBoneReferences(std::span<const BoneIndex> vertex_bones, BoneIndex origin_bone, std::size_t vertex_count,
                             std::size_t bone_count) noexcept {
  if (vertex_bones.size() != vertex_count) {
    log(ARX_LOG_DEBUG,
        "Skeleton validation: vertex-bone count {} does not match vertex count {}",
        vertex_bones.size(),
        vertex_count);
    return Error::kBadVertexBoneCount;
  }
  for (std::size_t vertex = 0; vertex < vertex_bones.size(); ++vertex) {
    const BoneIndex bone = vertex_bones[vertex];
    if (!validBoneIndex(bone, bone_count)) {
      log(ARX_LOG_DEBUG,
          "Skeleton validation: vertex {} references bone {} with bone count {}",
          vertex,
          bone,
          bone_count);
      return Error::kBadVertexBone;
    }
  }
  if (!validBoneIndex(origin_bone, bone_count)) {
    log(ARX_LOG_DEBUG, "Skeleton validation: origin references bone {} with bone count {}", origin_bone, bone_count);
    return Error::kBadOriginBone;
  }
  return Error::kNone;
}

Error validateVertexBones(const SkeletonData& skeleton, std::size_t vertex_count) noexcept {
  return validateBoneReferences(skeleton.vertex_bones, skeleton.origin_bone, vertex_count, skeleton.bones.size());
}

Error validate(const SkeletonData& skeleton, std::size_t vertex_count) {
  Error error = validateBones(skeleton);
  if (error != Error::kNone) return error;
  return validateVertexBones(skeleton, vertex_count);
}

}  // namespace pistoris::skeleton
