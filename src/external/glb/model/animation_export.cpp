// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "external/glb/model/animation_export.h"

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "animation/data.h"
#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/model/animation_identity.h"
#include "external/glb/object_coordinates.h"
#include "external/glb/writer.h"
#include "model/data.h"
#include "modules/animation.h"
#include "modules/skeleton.h"
#include "modules/sounds.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/quat.h"

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_model {
using glb_object::toGlbPoint;

static_assert(skeleton::kMaxBones == animation::kMaxGroups);

namespace {

struct AnimationGlbGroupProjection {
  std::bitset<animation::kMaxGroups> animated_translation;
  std::bitset<animation::kMaxGroups> animated_rotation;
  std::bitset<animation::kMaxGroups> animated_scale;
  std::bitset<animation::kMaxGroups> exact_identity;
  std::bitset<animation::kMaxGroups> tolerance_identity;
};

ArxQuat toGlbRotation(const ArxQuat& rotation) {
  constexpr ArxQuat kBasis{0.0f, 1.0f, 0.0f, 0.0f};
  return math::normalize(kBasis * rotation * math::conjugate(kBasis));
}

std::string animationResourceReference(std::string_view path) {
  if (path.empty()) return {};
  paths::AnimationPathView parsed;
  std::string selector;
  if (paths::animationFromTea(path, parsed) && paths::animationSelector(parsed, selector)) return selector;
  return std::string(path);
}

std::string settingsName(const AnimationModules& animation) {
  std::string result = "SETTINGS";
  const std::uint32_t last = animation.animation.keyframes.back().frame;
  if (animation.animation.frame_length == last + 1U) {
    result += "__EXTRA_FRAME";
  } else if (animation.animation.frame_length > last + 1U) {
    result += "__FRAME_LENGTH_" + std::to_string(animation.animation.frame_length);
  }
  bool first_step = true;
  for (const AnimationKeyframe& keyframe : animation.animation.keyframes) {
    if (!keyframe.footstep) continue;
    result += first_step ? "__STEPS_" : "_";
    result += std::to_string(keyframe.frame);
    first_step = false;
  }
  if (result == "SETTINGS") return {};
  return result + "__settings";
}

bool representableAnimationTimes(const AnimationModules& animation) noexcept {
  float previous = -std::numeric_limits<float>::infinity();
  for (const AnimationKeyframe& keyframe : animation.animation.keyframes) {
    const float time = static_cast<float>(keyframe.frame) / kTeaFps;
    if (!std::isfinite(time) || time <= previous ||
        std::round(static_cast<double>(time) * kTeaFps) != static_cast<double>(keyframe.frame))
      return false;
    previous = time;
  }
  return true;
}

bool representableAnimationRotations(const AnimationModules& animation, std::size_t group_count) noexcept {
  constexpr float kMinNormSquared = 1.0e-12f;
  const auto representable = [](const ArxQuat& rotation) {
    const float norm_squared =
        rotation.w * rotation.w + rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z;
    return std::isfinite(norm_squared) && norm_squared > kMinNormSquared;
  };
  for (const AnimationKeyframe& keyframe : animation.animation.keyframes)
    if (!representable(keyframe.root_rotation)) return false;
  for (std::size_t frame = 0; frame < animation.animation.keyframes.size(); ++frame)
    for (std::size_t group = 0; group < group_count; ++group)
      if (!representable(
              animation.animation.group_transforms[frame * animation.animation.group_count + group].rotation))
        return false;
  return true;
}

AnimationGlbGroupProjection projectAnimationGroups(const AnimationData& animation, std::size_t group_count) noexcept {
  AnimationGlbGroupProjection result;
  for (std::size_t group = 0; group < group_count; ++group) {
    bool exact_identity = true;
    bool tolerance_identity = true;
    for (std::size_t frame = 0; frame < animation.keyframes.size(); ++frame) {
      const AnimationGroupTransform& transform = animation.group_transforms[frame * animation.group_count + group];
      result.animated_translation.set(group,
                                      result.animated_translation.test(group) || transform.translation != ArxVector3{});
      result.animated_rotation.set(group,
                                   result.animated_rotation.test(group) || transform.rotation != math::kIdentityQuat);
      result.animated_scale.set(group,
                                result.animated_scale.test(group) || transform.scale != ArxVector3{1.0f, 1.0f, 1.0f});
      exact_identity = exact_identity && animation::isIdentityTransform(transform);
      tolerance_identity = tolerance_identity && isAnimationGlbIdentity(transform);
    }
    result.exact_identity.set(group, exact_identity);
    result.tolerance_identity.set(group, tolerance_identity);
  }
  return result;
}

void appendGroupSet(std::string& name, std::string_view kind, const std::bitset<animation::kMaxGroups>& groups,
                    std::size_t group_count) {
  bool first_item = true;
  for (std::size_t group = 0; group < group_count;) {
    if (!groups.test(group)) {
      ++group;
      continue;
    }
    const std::size_t first = group;
    while (group + 1U < group_count && groups.test(group + 1U)) ++group;
    const std::size_t last = group;
    if (first_item) {
      name += "__";
      name += kind;
      name += '_';
      first_item = false;
    } else {
      name += '_';
    }
    name += std::to_string(first);
    if (last != first) name += '-' + std::to_string(last);
    ++group;
  }
}

std::string groupsName(const AnimationData& animation, const AnimationGlbGroupProjection& projection,
                       std::size_t source_group_count, std::size_t model_group_count) {
  std::size_t retained_group_count = source_group_count;
  while (retained_group_count != 0 && projection.exact_identity.test(retained_group_count - 1U) &&
         !animation.claimed_groups.test(retained_group_count - 1U))
    --retained_group_count;
  std::bitset<animation::kMaxGroups> void_groups;
  std::bitset<animation::kMaxGroups> claimed_groups;
  for (std::size_t group = 0; group < retained_group_count; ++group) {
    const bool claimed = animation.claimed_groups.test(group);
    void_groups.set(group, projection.exact_identity.test(group) && !claimed);
    claimed_groups.set(
        group, claimed || (projection.tolerance_identity.test(group) && !projection.exact_identity.test(group)));
  }
  for (std::size_t group = retained_group_count; group < model_group_count; ++group) void_groups.set(group);
  if (void_groups.none() && claimed_groups.none()) return {};
  std::string result = "GROUPS";
  appendGroupSet(result, "VOID", void_groups, model_group_count);
  appendGroupSet(result, "CLAIM", claimed_groups, model_group_count);
  return result + "__groups";
}

void addAnimationHelpers(const AnimationModules& animation, std::string_view export_name, int parent,
                         const AnimationGlbGroupProjection& projection, std::size_t source_group_count,
                         std::size_t model_group_count, glb::Builder& builder) {
  const int helper = builder.addNode("arx_animation__" + std::string(export_name));
  builder.addChild(parent, helper);
  const std::string path = animationResourceReference(animation.resource.path);
  if (!path.empty()) builder.addChild(helper, builder.addNode("PATH_" + path + "__path"));
  const std::string settings = settingsName(animation);
  if (!settings.empty()) builder.addChild(helper, builder.addNode(settings));
  const std::string groups = groupsName(animation.animation, projection, source_group_count, model_group_count);
  if (!groups.empty()) builder.addChild(helper, builder.addNode(groups));

  std::map<std::string_view, std::vector<std::uint32_t>> sounds;
  for (const AnimationKeyframe& keyframe : animation.animation.keyframes) {
    if (keyframe.sound == kNoSoundHandle) continue;
    if (!sounds::validHandle(animation.sounds, keyframe.sound)) continue;
    sounds[sounds::path(animation.sounds, keyframe.sound)].push_back(keyframe.frame);
  }
  for (const auto& [sound, frames] : sounds) {
    std::string name = "SOUND__FRAMES";
    for (std::uint32_t frame : frames) name += "_" + std::to_string(frame);
    name += "__sound";
    const int sound_node = builder.addNode(std::move(name));
    builder.addChild(helper, sound_node);
    builder.addChild(sound_node, builder.addNode("PATH_" + std::string(sound) + "__path"));
  }
}

void addAnimationChannels(const AnimationModules& animation, const ModelModules& model, std::string_view export_name,
                          std::size_t group_count, const AnimationGlbGroupProjection& projection, int motion_node,
                          std::span<const int> bone_nodes, float units, glb::Builder& builder) {
  std::vector<float> times;
  times.reserve(animation.animation.keyframes.size());
  for (const AnimationKeyframe& keyframe : animation.animation.keyframes)
    times.push_back(static_cast<float>(keyframe.frame) / kTeaFps);
  const int time_accessor = builder.addTimeAccessor(times);

  std::vector<glb::AnimationChannel> channels;
  channels.reserve(group_count * 3U + 2U);
  std::vector<glb::Vec3> vectors;
  std::vector<glb::Vec4> rotations;
  vectors.reserve(animation.animation.keyframes.size());
  rotations.reserve(animation.animation.keyframes.size());

  bool animated_root_rotation = false;
  for (const AnimationKeyframe& keyframe : animation.animation.keyframes) {
    const ArxVector3 translation = toGlbPoint(keyframe.root_translation, units);
    vectors.push_back({translation.x, translation.y, translation.z});
    animated_root_rotation = animated_root_rotation || keyframe.root_rotation != math::kIdentityQuat;
  }
  channels.push_back({time_accessor, builder.addVec3Accessor(vectors), motion_node, glb::AnimationPath::kTranslation});
  if (animated_root_rotation) {
    rotations.clear();
    for (const AnimationKeyframe& keyframe : animation.animation.keyframes) {
      const ArxQuat rotation = toGlbRotation(keyframe.root_rotation);
      rotations.push_back({rotation.x, rotation.y, rotation.z, rotation.w});
    }
    channels.push_back(
        {time_accessor,
         builder.addAccessor(std::span<const glb::Vec4>(rotations), cgltf_component_type_r_32f, cgltf_type_vec4),
         motion_node,
         glb::AnimationPath::kRotation});
  }

  bool has_bone_channel = false;
  for (std::size_t group = 0; group < group_count; ++group) {
    const auto group_transform = [&](std::size_t frame) -> const AnimationGroupTransform& {
      return animation.animation.group_transforms[frame * animation.animation.group_count + group];
    };
    if (projection.animated_translation.test(group)) {
      vectors.clear();
      ArxVector3 rest = model.skeleton.bones[group].position;
      const BoneIndex parent = model.skeleton.bones[group].parent;
      if (parent != kInvalidBoneIndex) rest = rest - model.skeleton.bones[parent].position;
      for (std::size_t frame = 0; frame < animation.animation.keyframes.size(); ++frame) {
        const ArxVector3 translation = toGlbPoint(rest + group_transform(frame).translation, units);
        vectors.push_back({translation.x, translation.y, translation.z});
      }
      channels.push_back(
          {time_accessor, builder.addVec3Accessor(vectors), bone_nodes[group], glb::AnimationPath::kTranslation});
      has_bone_channel = true;
    }
    if (projection.animated_rotation.test(group)) {
      rotations.clear();
      for (std::size_t frame = 0; frame < animation.animation.keyframes.size(); ++frame) {
        const ArxQuat rotation = toGlbRotation(group_transform(frame).rotation);
        rotations.push_back({rotation.x, rotation.y, rotation.z, rotation.w});
      }
      channels.push_back(
          {time_accessor,
           builder.addAccessor(std::span<const glb::Vec4>(rotations), cgltf_component_type_r_32f, cgltf_type_vec4),
           bone_nodes[group],
           glb::AnimationPath::kRotation});
      has_bone_channel = true;
    }
    if (projection.animated_scale.test(group)) {
      vectors.clear();
      for (std::size_t frame = 0; frame < animation.animation.keyframes.size(); ++frame) {
        const ArxVector3& scale = group_transform(frame).scale;
        vectors.push_back({scale.x, scale.y, scale.z});
      }
      channels.push_back(
          {time_accessor, builder.addVec3Accessor(vectors), bone_nodes[group], glb::AnimationPath::kScale});
      has_bone_channel = true;
    }
  }
  if (!has_bone_channel && !bone_nodes.empty()) {
    rotations.assign(animation.animation.keyframes.size(), {0.0f, 0.0f, 0.0f, 1.0f});
    channels.push_back(
        {time_accessor,
         builder.addAccessor(std::span<const glb::Vec4>(rotations), cgltf_component_type_r_32f, cgltf_type_vec4),
         bone_nodes.front(),
         glb::AnimationPath::kRotation});
  }
  builder.addAnimation(std::string(export_name), std::move(channels));
}

}  // namespace

ArxReturnCode addAnimationsToGlb(const ModelModules& model, std::span<const AnimationModules* const> animations,
                                 ArxAnimationConversionReport* report, int motion_node, std::span<const int> bone_nodes,
                                 float units, glb::Builder& builder, std::vector<AnimationSoundFile>* sound_files) {
  std::vector<const AnimationModules*> compatible;
  std::vector<std::size_t> source_indices;
  std::vector<std::string> animation_names;
  compatible.reserve(animations.size());
  source_indices.reserve(animations.size());
  animation_names.reserve(animations.size());
  for (std::size_t source_index = 0; source_index < animations.size(); ++source_index) {
    const AnimationModules* animation = animations[source_index];
    const std::size_t group_count = std::min(animation->animation.group_count, model.skeleton.bones.size());
    if (!representableAnimationTimes(*animation)) {
      if (report) ++report->skipped;
      log(ARX_LOG_WARN,
          "Model -> GLB: animation '{}' frame positions cannot be represented by GLB timestamps; skipped",
          animation->animation.name);
      continue;
    }
    if (!representableAnimationRotations(*animation, group_count)) {
      if (report) ++report->skipped;
      log(ARX_LOG_WARN,
          "Model -> GLB: animation '{}' contains a rotation that GLB cannot represent; skipped",
          animation->animation.name);
      continue;
    }
    compatible.push_back(animation);
    source_indices.push_back(source_index);
    animation_names.push_back(animation->animation.name);
  }
  IdentifierUniquifier animation_name_uniquifier({.max_length = animation::kMaxNameLength});
  animation_name_uniquifier.reserve(animation_names.size());
  for (std::string& name : animation_names) animation_name_uniquifier.add(name);
  std::vector<IdentifierRepair> animation_name_repairs(animation_names.size());
  if (animation_name_uniquifier.apply(animation_name_repairs).exhausted) return ARX_GLB_BAD_ANIMATION_NAME;

  int animations_parent = -1;
  for (std::size_t index = 0; index < compatible.size(); ++index) {
    if (hasIdentifierRepair(animation_name_repairs[index], IdentifierRepair::kDuplicate))
      log(ARX_LOG_INFO,
          "Model -> GLB: animation name '{}' made unique as '{}'",
          compatible[index]->animation.name,
          animation_names[index]);
    if (animations_parent < 0) {
      animations_parent = builder.addNode("animations_parent");
      builder.addRoot(animations_parent);
    }
    const std::size_t group_count = std::min(compatible[index]->animation.group_count, model.skeleton.bones.size());
    const AnimationGlbGroupProjection projection = projectAnimationGroups(compatible[index]->animation, group_count);
    if (compatible[index]->animation.group_count > group_count)
      log(ARX_LOG_WARN,
          "Model -> GLB: animation '{}' has {} groups but model has {}; {} trailing group(s) discarded",
          compatible[index]->animation.name,
          compatible[index]->animation.group_count,
          model.skeleton.bones.size(),
          compatible[index]->animation.group_count - group_count);
    const std::size_t sound_count = sounds::count(compatible[index]->sounds, SoundKind::kEffect);
    std::vector<std::uint8_t> used(sound_count, 0);
    for (const AnimationKeyframe& keyframe : compatible[index]->animation.keyframes) {
      if (keyframe.sound == kNoSoundHandle) continue;
      SoundIndex sound = kNoSound;
      if (sounds::effectIndex(keyframe.sound, sound) && sound < used.size()) used[sound] = 1;
    }
    for (std::size_t sound_index = 0; sound_index < sound_count; ++sound_index) {
      if (used[sound_index] == 0) continue;
      const SoundHandle handle = sounds::effectHandle(static_cast<SoundIndex>(sound_index));
      const std::span<const std::uint8_t> encoded_audio = sounds::encodedAudio(compatible[index]->sounds, handle);
      if (sound_files && !encoded_audio.empty()) {
        SoundFile file{static_cast<SoundIndex>(sound_index),
                       std::string(sounds::path(compatible[index]->sounds, handle)),
                       {encoded_audio.begin(), encoded_audio.end()}};
        sound_files->push_back({source_indices[index], std::move(file)});
      }
    }
    addAnimationHelpers(*compatible[index],
                        animation_names[index],
                        animations_parent,
                        projection,
                        group_count,
                        model.skeleton.bones.size(),
                        builder);
    addAnimationChannels(*compatible[index],
                         model,
                         animation_names[index],
                         group_count,
                         projection,
                         motion_node,
                         bone_nodes,
                         units,
                         builder);
    if (report) ++report->converted;
  }
  return ARX_OK;
}

}  // namespace pistoris::glb_model
