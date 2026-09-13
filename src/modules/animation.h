// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "utils/identifier.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

namespace animation {

constexpr std::size_t kMaxNameLength = 255;
constexpr std::size_t kMaxGroups = 1024;
constexpr std::size_t kMaxKeyframes = 0x10000;

}  // namespace animation

struct AnimationGroupTransform {
  ArxQuat rotation{};
  ArxVector3 translation{};
  ArxVector3 scale{1.0f, 1.0f, 1.0f};
};

struct AnimationKeyframe {
  std::uint32_t frame = 0;
  ArxVector3 root_translation{};
  ArxQuat root_rotation{};
  bool footstep = false;
  SoundIndex sound = kNoSound;
};

struct AnimationData {
  std::string name;
  std::uint32_t frame_length = 0;
  std::size_t group_count = 0;
  std::vector<AnimationKeyframe> keyframes;
  std::vector<AnimationGroupTransform> group_transforms;
  std::bitset<animation::kMaxGroups> claimed_groups;
};

namespace animation {

enum class Error : std::uint8_t {
  kNone,
  kBadName,
  kNoKeyframes,
  kTooManyKeyframes,
  kTooManyGroups,
  kBadFrameLength,
  kBadFrame,
  kBadRootTransform,
  kBadGroupTransform,
  kBadSound,
  kBadTransformCount,
  kBadGroupClaim,
  kBadIndex,
};

// --- Validation ---

Error validateName(std::string_view name) noexcept;
Error validateKeyframe(const AnimationKeyframe& keyframe, std::size_t sound_count) noexcept;
Error validateTransform(const AnimationGroupTransform& transform) noexcept;
Error validateFrameLength(const AnimationData& animation, std::uint32_t frame_length) noexcept;
Error validateTimelineShape(std::uint32_t frame_length, std::size_t keyframe_count, std::size_t group_count) noexcept;
Error validateKeyframeAppend(const AnimationData& animation, std::uint32_t frame, std::size_t group_count) noexcept;
Error validateKeyframeSet(const AnimationData& animation, std::size_t index, const AnimationKeyframe& keyframe,
                          std::span<const AnimationGroupTransform> group_transforms, std::size_t sound_count) noexcept;
Error validateTimeline(std::uint32_t frame_length, std::size_t group_count,
                       std::span<const AnimationKeyframe> keyframes,
                       std::span<const AnimationGroupTransform> group_transforms, std::size_t sound_count) noexcept;
Error validate(const AnimationData& animation, std::size_t sound_count) noexcept;
Error validateScale(const AnimationData& animation, float factor) noexcept;
Error validateRotation(const AnimationData& animation, ArxQuat rotation) noexcept;

// --- Queries ---

bool isIdentityTransform(const AnimationGroupTransform& transform) noexcept;
bool isIdentityGroup(const AnimationData& animation, std::size_t group) noexcept;
bool isGroupClaimed(const AnimationData& animation, std::size_t group) noexcept;

// --- Mutation ---

void setName(AnimationData& animation, std::string_view name);
void setFrameLength(AnimationData& animation, std::uint32_t frame_length) noexcept;
void setKeyframe(AnimationData& animation, std::size_t index, AnimationKeyframe keyframe,
                 std::span<const AnimationGroupTransform> group_transforms) noexcept;
std::size_t addKeyframe(AnimationData& animation, AnimationKeyframe keyframe,
                        std::span<const AnimationGroupTransform> group_transforms);
void removeKeyframe(AnimationData& animation, std::size_t index);
void replaceKeyframes(AnimationData& animation, std::uint32_t frame_length, std::size_t group_count,
                      std::vector<AnimationKeyframe>&& keyframes,
                      std::vector<AnimationGroupTransform>&& group_transforms) noexcept;
void clearKeyframes(AnimationData& animation) noexcept;
void claimGroup(AnimationData& animation, std::size_t group) noexcept;
void unclaimGroup(AnimationData& animation, std::size_t group) noexcept;
void voidGroup(AnimationData& animation, std::size_t group) noexcept;

// --- Repair ---

IdentifierRepair repairName(std::string& name);

// --- Transformation ---

void applyScale(AnimationData& animation, float factor) noexcept;
void applyRotation(AnimationData& animation, ArxQuat rotation) noexcept;

}  // namespace animation
}  // namespace pistoris
