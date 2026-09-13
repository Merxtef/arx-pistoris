// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

struct SelectionLeadingVertex {
  ArxVector3 position = {};
  BoneIndex bone = kInvalidBoneIndex;
};

struct Selection {
  std::string name;
  std::optional<SelectionLeadingVertex> leading_vertex;
};

using SelectionMask = std::uint64_t;

struct SelectionsData {
  std::array<Selection, 64> slots;
  SelectionMask occupied = 0;
  std::vector<SelectionMask> vertex_masks;
  std::vector<SelectionMask> bone_masks;
  std::vector<SelectionMask> action_point_masks;
  SelectionMask origin_mask = 0;
};

namespace selections {

constexpr std::size_t kMaxNameLength = 63;

enum class Error : std::uint8_t {
  kNone,
  kTooManySelections,
  kBadId,
  kBadCount,
  kBadMask,
  kBadVertexMember,
  kBadBoneMember,
  kBadActionPointMember,
  kBadName,
  kDuplicateName,
  kBadLeadingPosition,
  kBadLeadingBone,
};

struct BoneMaskInferenceResult {
  std::vector<SelectionMask> masks;
  std::size_t membership_count = 0;
  std::size_t bones_without_vertices = 0;
};

constexpr SelectionMask bit(SelectionId id) noexcept {
  return id < 64U ? static_cast<SelectionMask>(1) << id : SelectionMask{0};
}

// --- Validation ---

bool validName(std::string_view name) noexcept;
Error validateSelectionAppend(const SelectionsData& selections) noexcept;
Error validateSelection(const Selection& selection, std::size_t bone_count) noexcept;
Error validateMemberUpdate(const SelectionsData& selections, SelectionId id,
                           std::optional<std::span<const VertexIndex>> vertices,
                           std::optional<std::span<const BoneIndex>> bones,
                           std::optional<std::span<const ActionPointIndex>> action_points) noexcept;
Error validateBoneReferences(const SelectionsData& selections, std::size_t bone_count) noexcept;
Error validate(const SelectionsData& selections, std::size_t vertex_count, std::size_t bone_count,
               std::size_t action_point_count);
Error validateScale(const SelectionsData& selections, float factor) noexcept;
Error validateRotation(const SelectionsData& selections, const ArxMat3& rotation) noexcept;
Error validateTranslation(const SelectionsData& selections, const ArxVector3& offset) noexcept;

// --- Queries ---

bool occupied(const SelectionsData& selections, SelectionId id) noexcept;
bool leadingVerticesReferenceBone(const SelectionsData& selections, BoneIndex bone) noexcept;
std::size_t vertexMemberCount(const SelectionsData& selections, SelectionId id) noexcept;
std::size_t boneMemberCount(const SelectionsData& selections, SelectionId id) noexcept;
std::size_t actionPointMemberCount(const SelectionsData& selections, SelectionId id) noexcept;
void copyVertexMembers(const SelectionsData& selections, SelectionId id, std::size_t offset, std::size_t count,
                       VertexIndex* out) noexcept;
void copyBoneMembers(const SelectionsData& selections, SelectionId id, std::size_t offset, std::size_t count,
                     BoneIndex* out) noexcept;
void copyActionPointMembers(const SelectionsData& selections, SelectionId id, std::size_t offset, std::size_t count,
                            ActionPointIndex* out) noexcept;
bool includesOrigin(const SelectionsData& selections, SelectionId id) noexcept;

// --- Mutation ---

SelectionId addSelection(SelectionsData& selections, Selection selection);
void setSelection(SelectionsData& selections, SelectionId id, Selection selection) noexcept;
void reserveVertexCapacity(SelectionsData& selections, std::size_t capacity);
void reserveBoneCapacity(SelectionsData& selections, std::size_t capacity);
void reserveActionPointCapacity(SelectionsData& selections, std::size_t capacity);
void appendEmptyVertexMask(SelectionsData& selections);
void appendEmptyBoneMask(SelectionsData& selections);
void appendEmptyActionPointMask(SelectionsData& selections);
void truncateVertexMasks(SelectionsData& selections, std::size_t size) noexcept;
void removeBoneMask(SelectionsData& selections, BoneIndex index) noexcept;
void removeActionPointMask(SelectionsData& selections, ActionPointIndex index) noexcept;
void replaceVertexMasks(SelectionsData& selections, std::vector<SelectionMask>&& masks) noexcept;
void replaceBoneMasks(SelectionsData& selections, std::vector<SelectionMask>&& masks) noexcept;
void replaceActionPointMasks(SelectionsData& selections, std::vector<SelectionMask>&& masks) noexcept;
void clearVertexMasks(SelectionsData& selections) noexcept;
void clearBoneMasks(SelectionsData& selections) noexcept;
void clearActionPointMasks(SelectionsData& selections) noexcept;
void clearOriginMask(SelectionsData& selections) noexcept;
void clearLeadingBoneReferences(SelectionsData& selections) noexcept;
void updateMembers(SelectionsData& selections, SelectionId id, std::optional<std::span<const VertexIndex>> vertices,
                   std::optional<std::span<const BoneIndex>> bones,
                   std::optional<std::span<const ActionPointIndex>> action_points) noexcept;
void clearVertexMembers(SelectionsData& selections, SelectionId id) noexcept;
void clearBoneMembers(SelectionsData& selections, SelectionId id) noexcept;
void clearActionPointMembers(SelectionsData& selections, SelectionId id) noexcept;
void setIncludesOrigin(SelectionsData& selections, SelectionId id, bool includes) noexcept;
void removeSelection(SelectionsData& selections, SelectionId id) noexcept;
void clearSelections(SelectionsData& selections) noexcept;

// --- Repair ---

void repairNames(const SelectionsData& selections, std::span<Selection> candidates,
                 SelectionId ignored = kInvalidSelectionId);

// --- Transformation ---

void remapLeadingBoneIndicesAfterRemoval(SelectionsData& selections, BoneIndex removed) noexcept;
void remapVertexMasks(SelectionsData& selections, std::span<const VertexIndex> vertex_remap) noexcept;
void applyScale(SelectionsData& selections, float factor) noexcept;
void applyRotation(SelectionsData& selections, const ArxMat3& rotation) noexcept;
void applyTranslation(SelectionsData& selections, const ArxVector3& offset) noexcept;

// --- Generation ---

BoneMaskInferenceResult inferBoneMasksFromVertices(const SelectionsData& selections,
                                                   std::span<const BoneIndex> vertex_bones, std::size_t bone_count);

}  // namespace selections
}  // namespace pistoris
