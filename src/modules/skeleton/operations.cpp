// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/skeleton.h"
#include "utils/identifier.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::skeleton {

BoneIndex addBone(SkeletonData& skeleton, Bone bone) {
  assert(skeleton.bones.size() < kMaxBones);
  const std::size_t index = skeleton.bones.size();
  skeleton.bones.push_back(std::move(bone));
  return static_cast<BoneIndex>(index);
}

void setBone(SkeletonData& skeleton, BoneIndex index, Bone bone) noexcept {
  assert(static_cast<std::size_t>(index) < skeleton.bones.size());
  skeleton.bones[index] = std::move(bone);
}

void repairNames(const SkeletonData& skeleton, std::span<Bone> candidates, BoneIndex ignored) {
  IdentifierUniquifier names({.letter_case = IdentifierCase::kLower, .max_length = kMaxNameLength});
  names.reserve(candidates.size(), skeleton.bones.size());
  for (std::size_t index = 0; index < skeleton.bones.size(); ++index) {
    if (index == static_cast<std::size_t>(ignored)) continue;
    names.occupy(skeleton.bones[index].name);
  }
  for (Bone& bone : candidates) names.add(bone.name);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
}

void repairNames(std::span<Bone> bones) {
  IdentifierUniquifier names({.letter_case = IdentifierCase::kLower, .max_length = kMaxNameLength});
  names.reserve(bones.size());
  for (Bone& bone : bones) names.add(bone.name);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
}

namespace {

void shiftIndex(BoneIndex& value, BoneIndex removed) noexcept {
  if (value != kInvalidBoneIndex && value > removed) --value;
}

}  // namespace

bool referencesBone(const SkeletonData& skeleton, BoneIndex index) noexcept {
  if (skeleton.origin_bone == index) return true;
  if (std::ranges::find(skeleton.vertex_bones, index) != skeleton.vertex_bones.end()) return true;
  for (const Bone& bone : skeleton.bones)
    if (bone.parent == index) return true;
  return false;
}

void removeBone(SkeletonData& skeleton, BoneIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < skeleton.bones.size());
  assert(!referencesBone(skeleton, index));
  skeleton.bones.erase(skeleton.bones.begin() + static_cast<std::ptrdiff_t>(index));
  for (Bone& bone : skeleton.bones) shiftIndex(bone.parent, index);
  for (BoneIndex& bone : skeleton.vertex_bones) shiftIndex(bone, index);
  shiftIndex(skeleton.origin_bone, index);
}

void setVertexBone(SkeletonData& skeleton, VertexIndex index, BoneIndex bone) noexcept {
  assert(static_cast<std::size_t>(index) < skeleton.vertex_bones.size());
  skeleton.vertex_bones[index] = bone;
}

void appendVertexBone(SkeletonData& skeleton, BoneIndex bone) { skeleton.vertex_bones.push_back(bone); }

void appendVertexBones(SkeletonData& skeleton, std::span<const BoneIndex> bones) {
  skeleton.vertex_bones.insert(skeleton.vertex_bones.end(), bones.begin(), bones.end());
}

void truncateVertexBones(SkeletonData& skeleton, std::size_t size) noexcept {
  while (skeleton.vertex_bones.size() > size) skeleton.vertex_bones.pop_back();
}

void setOriginBone(SkeletonData& skeleton, BoneIndex bone) noexcept { skeleton.origin_bone = bone; }

void replaceBones(SkeletonData& skeleton, std::vector<Bone>&& bones, BoneIndex origin_bone) noexcept {
  skeleton.bones = std::move(bones);
  skeleton.origin_bone = origin_bone;
}

void replaceVertexBones(SkeletonData& skeleton, std::vector<BoneIndex>&& vertex_bones) noexcept {
  skeleton.vertex_bones = std::move(vertex_bones);
}

void clearBones(SkeletonData& skeleton) noexcept {
  skeleton.bones.clear();
  std::fill(skeleton.vertex_bones.begin(), skeleton.vertex_bones.end(), kInvalidBoneIndex);
  skeleton.origin_bone = kInvalidBoneIndex;
}

void clearVertexBones(SkeletonData& skeleton) noexcept { skeleton.vertex_bones.clear(); }

void reserveVertexCapacity(SkeletonData& skeleton, std::size_t capacity) { skeleton.vertex_bones.reserve(capacity); }

void reserveBoneCapacity(SkeletonData& skeleton, std::size_t capacity) { skeleton.bones.reserve(capacity); }

void remapVertexBones(SkeletonData& skeleton, std::span<const VertexIndex> vertex_remap) noexcept {
  if (vertex_remap.empty()) return;
  assert(skeleton.vertex_bones.size() == vertex_remap.size());
  std::size_t next = 0;
  for (std::size_t old = 0; old < vertex_remap.size(); ++old) {
    const VertexIndex mapped = vertex_remap[old];
    if (mapped == kInvalidVertexIndex) continue;
    assert(mapped == next);
    if (next != old) skeleton.vertex_bones[next] = skeleton.vertex_bones[old];
    ++next;
  }
  while (skeleton.vertex_bones.size() > next) skeleton.vertex_bones.pop_back();
}

}  // namespace pistoris::skeleton
