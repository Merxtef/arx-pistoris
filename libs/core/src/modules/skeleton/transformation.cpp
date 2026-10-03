// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/skeleton.h"
#include "utils/math/finite.h"
#include "utils/math/mat3.h"

#include <cassert>
#include <cmath>
#include <cstddef>

namespace pistoris::skeleton {

Error validateScale(const SkeletonData& skeleton, float factor) noexcept {
  for (const Bone& bone : skeleton.bones) {
    if (!math::finite(bone.position * factor)) return Error::kBadPosition;
    if (!std::isfinite(bone.blob_shadow_size * factor)) return Error::kBadBlobShadowSize;
  }
  return Error::kNone;
}

void applyScale(SkeletonData& skeleton, float factor) noexcept {
  for (Bone& bone : skeleton.bones) {
    bone.position = bone.position * factor;
    bone.blob_shadow_size *= factor;
  }
}

Error validateRotation(const SkeletonData& skeleton, const ArxMat3& rotation) noexcept {
  for (const Bone& bone : skeleton.bones)
    if (!math::finite(rotation * bone.position)) return Error::kBadPosition;
  return Error::kNone;
}

void applyRotation(SkeletonData& skeleton, const ArxMat3& rotation) noexcept {
  for (Bone& bone : skeleton.bones) bone.position = rotation * bone.position;
}

Error validateTranslation(const SkeletonData& skeleton, const ArxVector3& offset) noexcept {
  for (const Bone& bone : skeleton.bones)
    if (!math::finite(bone.position + offset)) return Error::kBadPosition;
  return Error::kNone;
}

void applyTranslation(SkeletonData& skeleton, const ArxVector3& offset) noexcept {
  for (Bone& bone : skeleton.bones) bone.position = bone.position + offset;
}

Error validateReferenceCompatibility(const SkeletonData& skeleton, const SkeletonData& reference) noexcept {
  if (skeleton.bones.size() != reference.bones.size()) return Error::kReferenceBoneCountMismatch;
  for (std::size_t index = 0; index < skeleton.bones.size(); ++index) {
    const Bone& target = skeleton.bones[index];
    const Bone& source = reference.bones[index];
    if (target.parent != kInvalidBoneIndex && static_cast<std::size_t>(target.parent) >= index)
      return Error::kBadParent;
    if (source.parent != kInvalidBoneIndex && static_cast<std::size_t>(source.parent) >= index)
      return Error::kBadParent;
    if (target.parent != source.parent) return Error::kReferenceBoneTopologyMismatch;
  }
  return Error::kNone;
}

void copyBonePositions(SkeletonData& skeleton, const SkeletonData& reference) noexcept {
  assert(skeleton.bones.size() == reference.bones.size());
  for (std::size_t index = 0; index < skeleton.bones.size(); ++index) {
    skeleton.bones[index].position = reference.bones[index].position;
  }
}

}  // namespace pistoris::skeleton
