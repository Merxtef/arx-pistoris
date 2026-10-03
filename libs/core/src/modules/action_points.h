// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "utils/identifier.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pistoris {

struct ActionPoint {
  std::string name;
  ArxVector3 position = {};
  BoneIndex bone = kInvalidBoneIndex;
};

struct ActionPointsData {
  std::vector<ActionPoint> points;
};

namespace action_points {

constexpr std::size_t kMaxNameLength = 255;

enum class Error : std::uint8_t {
  kNone,
  kTooManyActionPoints,
  kBadIndex,
  kBadName,
  kBadPosition,
  kBadBone,
};

// --- Validation ---

Error validateCount(std::size_t count) noexcept;
Error validatePoint(const ActionPoint& point, std::size_t bone_count) noexcept;
Error validateBoneReferences(const ActionPointsData& actions, std::size_t bone_count) noexcept;
Error validate(const ActionPointsData& actions, std::size_t bone_count) noexcept;
Error validateScale(const ActionPointsData& actions, float factor) noexcept;
Error validateRotation(const ActionPointsData& actions, const ArxMat3& rotation) noexcept;
Error validateTranslation(const ActionPointsData& actions, const ArxVector3& offset) noexcept;

// --- Queries ---

bool referencesBone(const ActionPointsData& actions, BoneIndex bone) noexcept;

// --- Mutation ---

ActionPointIndex addActionPoint(ActionPointsData& actions, ActionPoint point);
void setActionPoint(ActionPointsData& actions, ActionPointIndex index, ActionPoint point) noexcept;
void removeActionPoint(ActionPointsData& actions, ActionPointIndex index) noexcept;
void replace(ActionPointsData& actions, ActionPointsData&& replacement) noexcept;
void clear(ActionPointsData& actions) noexcept;
void clearBoneReferences(ActionPointsData& actions) noexcept;
void reserveActionPointCapacity(ActionPointsData& actions, std::size_t capacity);

// --- Repair ---

IdentifierRepair repairName(std::string& name);

// --- Transformation ---

void remapBoneIndicesAfterRemoval(ActionPointsData& actions, BoneIndex removed) noexcept;
void applyScale(ActionPointsData& actions, float factor) noexcept;
void applyRotation(ActionPointsData& actions, const ArxMat3& rotation) noexcept;
void applyTranslation(ActionPointsData& actions, const ArxVector3& offset) noexcept;
}  // namespace action_points
}  // namespace pistoris
