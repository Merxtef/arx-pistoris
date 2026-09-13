// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

struct Bone {
  std::string name;
  ArxVector3 position = {};
  BoneIndex parent = kInvalidBoneIndex;
  float blob_shadow_size = 0.0f;
};

struct SkeletonData {
  std::vector<Bone> bones;
  std::vector<BoneIndex> vertex_bones;
  BoneIndex origin_bone = kInvalidBoneIndex;
};

namespace skeleton {

constexpr std::size_t kMaxBones = 1024;
constexpr std::size_t kMaxNameLength = 255;

enum class Error : std::uint8_t {
  kNone,
  kTooManyBones,
  kBadIndex,
  kBadName,
  kDuplicateName,
  kBadPosition,
  kBadParent,
  kBadBlobShadowSize,
  kBadVertexBoneCount,
  kBadVertexBone,
  kBadOriginBone,
  kReferenceBoneCountMismatch,
  kReferenceBoneTopologyMismatch,
};

// --- Validation ---

bool validBoneIndex(BoneIndex bone, std::size_t bone_count) noexcept;
Error validateBoneCount(std::size_t bone_count) noexcept;
Error validateBone(const Bone& bone, std::size_t index) noexcept;
Error validateBones(const SkeletonData& skeleton);
Error validateBoneReferences(std::span<const BoneIndex> vertex_bones, BoneIndex origin_bone, std::size_t vertex_count,
                             std::size_t bone_count) noexcept;
Error validateVertexBones(const SkeletonData& skeleton, std::size_t vertex_count) noexcept;
Error validate(const SkeletonData& skeleton, std::size_t vertex_count);
Error validateScale(const SkeletonData& skeleton, float factor) noexcept;
Error validateRotation(const SkeletonData& skeleton, const ArxMat3& rotation) noexcept;
Error validateTranslation(const SkeletonData& skeleton, const ArxVector3& offset) noexcept;
Error validateReferenceCompatibility(const SkeletonData& skeleton, const SkeletonData& reference) noexcept;

// --- Queries ---

bool referencesBone(const SkeletonData& skeleton, BoneIndex index) noexcept;

// --- Mutation ---

BoneIndex addBone(SkeletonData& skeleton, Bone bone);
void setBone(SkeletonData& skeleton, BoneIndex index, Bone bone) noexcept;
void removeBone(SkeletonData& skeleton, BoneIndex index) noexcept;
void setVertexBone(SkeletonData& skeleton, VertexIndex index, BoneIndex bone) noexcept;
void appendVertexBone(SkeletonData& skeleton, BoneIndex bone);
void appendVertexBones(SkeletonData& skeleton, std::span<const BoneIndex> bones);
void truncateVertexBones(SkeletonData& skeleton, std::size_t size) noexcept;
void setOriginBone(SkeletonData& skeleton, BoneIndex bone) noexcept;
void replaceBones(SkeletonData& skeleton, std::vector<Bone>&& bones, BoneIndex origin_bone) noexcept;
void replaceVertexBones(SkeletonData& skeleton, std::vector<BoneIndex>&& vertex_bones) noexcept;
void clearBones(SkeletonData& skeleton) noexcept;
void clearVertexBones(SkeletonData& skeleton) noexcept;
void reserveVertexCapacity(SkeletonData& skeleton, std::size_t capacity);
void reserveBoneCapacity(SkeletonData& skeleton, std::size_t capacity);

// --- Repair ---

void repairNames(const SkeletonData& skeleton, std::span<Bone> candidates, BoneIndex ignored = kInvalidBoneIndex);
void repairNames(std::span<Bone> bones);

// --- Transformation ---

void applyScale(SkeletonData& skeleton, float factor) noexcept;
void applyRotation(SkeletonData& skeleton, const ArxMat3& rotation) noexcept;
void applyTranslation(SkeletonData& skeleton, const ArxVector3& offset) noexcept;
void copyBonePositions(SkeletonData& skeleton, const SkeletonData& reference) noexcept;
// Empty means identity; otherwise the map must describe an order-preserving compaction
void remapVertexBones(SkeletonData& skeleton, std::span<const VertexIndex> vertex_remap) noexcept;

}  // namespace skeleton
}  // namespace pistoris
