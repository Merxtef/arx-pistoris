// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "animation/data.h"
#include "animation/internal.h"
#include "api/status_boundary.h"
#include "modules/animation.h"
#include "modules/sounds.h"
#include "native/tea.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/quat.h"
#include "utils/native_text.h"
#include "utils/resource_path.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

template <class Present, class Value, class Interpolate>
void fillRootGaps(const tea::Data& source, Present present, Value value, Interpolate interpolate) {
  std::size_t previous = 0;
  while (previous < source.keyframes.size() && !present(source.keyframes[previous])) ++previous;
  while (previous < source.keyframes.size()) {
    std::size_t next = previous + 1U;
    while (next < source.keyframes.size() && !present(source.keyframes[next])) ++next;
    if (next == source.keyframes.size()) break;
    double total = 0.0;
    for (std::size_t index = previous + 1U; index <= next; ++index)
      total += static_cast<double>(source.keyframes[index].num_frame);
    if (total > 0.0) {
      double before = 0.0;
      for (std::size_t index = previous + 1U; index < next; ++index) {
        before += static_cast<double>(source.keyframes[index].num_frame);
        interpolate(index,
                    value(source.keyframes[previous]),
                    value(source.keyframes[next]),
                    static_cast<float>(before / total),
                    static_cast<float>((total - before) / total));
      }
    }
    previous = next;
  }
}

template <std::size_t N>
std::string_view fixedString(const char (&value)[N]) noexcept {
  const char* end = static_cast<const char*>(std::memchr(value, '\0', N));
  return {value, end ? static_cast<std::size_t>(end - value) : N};
}

std::string normalizedName(std::string_view source) {
  constexpr std::string_view kProducerPrefix = "arx-pistoris/";
  if (source.starts_with(kProducerPrefix)) source.remove_prefix(kProducerPrefix.size());
  std::string result(source);
  if (animation::repairName(result) != IdentifierRepair::kNone)
    log(ARX_LOG_WARN, "TEA -> Animation: animation name '{}' normalized to '{}'", source, result);
  return result;
}

std::string wavPath(std::string_view source) {
  std::string result(source);
  normalizeResourcePathIdentity(result);
  const std::size_t slash = result.find_last_of('/');
  const std::size_t dot = result.find_last_of('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash + 1U)) result.resize(dot);
  result += ".wav";
  return result;
}

std::string nativeSoundPath(std::string_view source) {
  std::string result(source);
  normalizeResourcePathIdentity(result);
  const std::string_view sound_directory = paths::soundDirectory();
  if (!result.starts_with(sound_directory) ||
      (result.size() != sound_directory.size() && result[sound_directory.size()] != '/')) {
    result.insert(0, "/");
    result.insert(0, sound_directory);
  }
  return wavPath(result);
}

std::string importedNativeSoundPath(std::string_view source) {
  std::string result(paths::soundDirectory());
  result += '/';
  result += source;
  return wavPath(result);
}

std::string nativeSampleName(std::string_view path) {
  const std::string_view sound_directory = paths::soundDirectory();
  if (path.starts_with(sound_directory) && path.size() > sound_directory.size() && path[sound_directory.size()] == '/')
    path.remove_prefix(sound_directory.size() + 1U);
  if (path.ends_with(".wav")) path.remove_suffix(4U);
  return std::string(path);
}

std::size_t nativeGroupCount(const AnimationData& animation) noexcept {
  std::size_t count = animation.group_count;
  while (count != 0) {
    const std::size_t group = count - 1U;
    if (animation.claimed_groups.test(group) || !animation::isIdentityGroup(animation, group)) break;
    --count;
  }
  return count;
}

ArxReturnCode projectNativeSounds(const AnimationModules& modules, bool include_files,
                                  std::vector<std::string>& projected_paths, std::vector<SoundFile>& files) {
  const std::size_t sound_count = sounds::count(modules.sounds, SoundKind::kEffect);
  std::vector<std::uint8_t> used(sound_count, 0);
  for (const AnimationKeyframe& keyframe : modules.animation.keyframes) {
    if (keyframe.sound == kNoSoundHandle) continue;
    SoundIndex sound = kNoSound;
    if (!sounds::effectIndex(keyframe.sound, sound) || sound >= used.size()) return ARX_ANIMATION_BAD_KEYFRAME_SOUND;
    used[sound] = 1;
  }

  projected_paths.resize(sound_count);
  ResourcePathUniquifier uniquifier;
  uniquifier.reserve(sound_count);
  for (std::size_t index = 0; index < sound_count; ++index) {
    if (used[index] == 0) continue;
    const std::string_view path = sounds::path(modules.sounds, sounds::effectHandle(static_cast<SoundIndex>(index)));
    if (!path.ends_with(".wav")) continue;
    projected_paths[index] = nativeSoundPath(path);
    uniquifier.add(projected_paths[index]);
  }
  for (std::size_t index = 0; index < sound_count; ++index) {
    if (used[index] == 0) continue;
    if (!projected_paths[index].empty()) continue;
    projected_paths[index] =
        nativeSoundPath(sounds::path(modules.sounds, sounds::effectHandle(static_cast<SoundIndex>(index))));
    uniquifier.add(projected_paths[index]);
  }
  if (uniquifier.apply() != ResourcePathError::kNone) return ARX_ANIMATION_BAD_SOUND_PATH;

  std::vector<sounds::AudioPreparationRequest> requests;
  std::vector<std::size_t> preparation(sound_count, std::numeric_limits<std::size_t>::max());
  requests.reserve(sound_count);
  for (std::size_t index = 0; index < sound_count; ++index) {
    const SoundHandle handle = sounds::effectHandle(static_cast<SoundIndex>(index));
    if (used[index] == 0 || sounds::encodedAudio(modules.sounds, handle).empty()) continue;
    preparation[index] = requests.size();
    requests.push_back({handle,
                        kSoundEffects,
                        {sounds::audioFormatFlag(sounds::AudioFormat::kWav),
                         sounds::AudioFormat::kWav,
                         sounds::ChannelMode::kPreserve,
                         include_files}});
  }
  std::vector<sounds::PreparedAudio> prepared;
  ArxReturnCode rc = animation_detail::soundErrorCode(sounds::prepareAudio(modules.sounds, requests, prepared));
  if (rc != ARX_OK) return rc;
  if (include_files) files.reserve(requests.size());

  for (std::size_t index = 0; index < sound_count; ++index) {
    if (used[index] == 0) continue;
    const std::size_t prepared_index = preparation[index];
    if (!include_files || prepared_index == std::numeric_limits<std::size_t>::max()) continue;
    sounds::PreparedAudio& sound = prepared[prepared_index];
    SoundFile file{static_cast<SoundIndex>(index), projected_paths[index], {}};
    if (sound.bytes.converted.empty()) {
      file.encoded_audio.assign(sound.bytes.borrowed.begin(), sound.bytes.borrowed.end());
    } else {
      file.encoded_audio = std::move(sound.bytes.converted);
    }
    files.push_back(std::move(file));
  }
  return ARX_OK;
}

}  // namespace

ArxReturnCode Animation::importNative(Animation& out, const tea::Data& native,
                                      std::vector<SoundSourceReference>* sound_sources,
                                      NativeTextMode text_mode) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    ArxReturnCode rc = validateTea(&native);
    if (rc != ARX_OK) return rc;

    Animation result;
    std::vector<Sound> imported_sounds;
    imported_sounds.reserve(native.keyframes.size());
    AnimationData& target = result.data_->animation;
    std::string decoded_name;
    if (!native_text::decode(fixedString(native.name), text_mode, decoded_name)) return ARX_TEA_BAD_NAME;
    target.name = normalizedName(decoded_name);
    target.frame_length = static_cast<std::uint32_t>(native.num_frames);
    target.group_count = static_cast<std::size_t>(native.num_groups);
    target.keyframes.resize(native.keyframes.size());
    target.group_transforms.resize(native.keyframes.size() * target.group_count);
    std::bitset<animation::kMaxGroups> native_nonidentity;
    std::bitset<animation::kMaxGroups> stored_nonidentity;
    std::vector<SoundSourceReference> sources;
    if (sound_sources) sources.reserve(native.keyframes.size());
    std::unordered_map<std::string, SoundIndex, ResourcePathIdentityHash, ResourcePathIdentityEqual> sound_indices;
    sound_indices.reserve(native.keyframes.size());
    std::unordered_set<std::string> source_spellings;
    source_spellings.reserve(native.keyframes.size());
    std::vector<const std::string*> original_sound_paths;
    original_sound_paths.reserve(native.keyframes.size());
    std::unordered_map<std::string, std::string, ResourcePathIdentityHash, ResourcePathIdentityEqual> decoded_sources;

    for (std::size_t frame = 0; frame < native.keyframes.size(); ++frame) {
      const tea::Keyframe& source = native.keyframes[frame];
      AnimationKeyframe& keyframe = target.keyframes[frame];
      keyframe.frame = static_cast<std::uint32_t>(source.num_frame);
      keyframe.root_translation = source.translate.value_or(ArxVector3{});
      keyframe.root_rotation = source.quat.value_or(ArxQuat{});
      keyframe.footstep = source.flag_frame == kTeaFlagFrameStep;
      if (source.sample) {
        const std::string_view raw = fixedString(source.sample->name);
        std::string decoded;
        if (!native_text::decode(raw, text_mode, decoded)) return ARX_TEA_BAD_SAMPLE_PATH;
        const auto [source_entry, new_source] = decoded_sources.try_emplace(decoded, raw);
        if (!new_source && !ResourcePathIdentityEqual{}(source_entry->second, raw)) {
          log(ARX_LOG_ERROR, "TEA -> Animation: distinct native sample paths decode to '{}'", decoded);
          return ARX_TEA_BAD_SAMPLE_PATH;
        }
        const std::string logical = importedNativeSoundPath(decoded);
        auto [entry, inserted] = sound_indices.try_emplace(logical, static_cast<SoundIndex>(imported_sounds.size()));
        if (inserted) {
          imported_sounds.push_back({logical, {}});
          original_sound_paths.push_back(&entry->first);
        }
        keyframe.sound = sounds::effectHandle(entry->second);
        if (sound_sources && source_spellings.insert(decoded).second) sources.push_back({entry->second, decoded});
      }
      for (std::size_t group = 0; group < target.group_count; ++group) {
        const tea::GroupAnim& input = source.groups[group];
        const AnimationGroupTransform native_transform = {
            input.quat,
            input.translate,
            ArxVector3{1.0f + input.zoom.x, 1.0f + input.zoom.y, 1.0f + input.zoom.z},
        };
        AnimationGroupTransform& stored = target.group_transforms[frame * target.group_count + group];
        stored = native_transform;
        stored.rotation = math::canonicalizeQuaternionSign(stored.rotation);
        if (!animation::isIdentityTransform(native_transform)) native_nonidentity.set(group);
        if (!animation::isIdentityTransform(stored)) stored_nonidentity.set(group);
      }
    }

    target.claimed_groups = native_nonidentity & ~stored_nonidentity;

    ResourcePathUniquifier sound_paths;
    sound_paths.reserve(imported_sounds.size());
    for (Sound& sound : imported_sounds) sound_paths.add(sound.path);
    std::vector<ResourcePathRepair> sound_repairs(imported_sounds.size());
    if (sound_paths.apply(nullptr, sound_repairs) != ResourcePathError::kNone) return ARX_ANIMATION_BAD_SOUND_PATH;
    for (std::size_t index = 0; index < sound_repairs.size(); ++index) {
      const ResourcePathRepair repair = sound_repairs[index];
      if (!hasResourcePathRepair(repair, ResourcePathRepair::kCharacters) &&
          !hasResourcePathRepair(repair, ResourcePathRepair::kTrailing) &&
          !hasResourcePathRepair(repair, ResourcePathRepair::kReserved) &&
          !hasResourcePathRepair(repair, ResourcePathRepair::kLength) &&
          !hasResourcePathRepair(repair, ResourcePathRepair::kDuplicate))
        continue;
      log(ARX_LOG_WARN,
          "TEA -> Animation: sound path '{}' normalized to '{}'",
          *original_sound_paths[index],
          imported_sounds[index].path);
    }
    sounds::replaceSounds(result.data_->sounds, std::move(imported_sounds));

    fillRootGaps(
        native,
        [](const tea::Keyframe& keyframe) { return keyframe.translate.has_value(); },
        [](const tea::Keyframe& keyframe) { return *keyframe.translate; },
        [&](std::size_t index, const ArxVector3& previous, const ArxVector3& next, float before, float after) {
          target.keyframes[index].root_translation = next * before + previous * after;
        });
    fillRootGaps(
        native,
        [](const tea::Keyframe& keyframe) { return keyframe.quat.has_value(); },
        [](const tea::Keyframe& keyframe) { return *keyframe.quat; },
        [&](std::size_t index, const ArxQuat& previous, const ArxQuat& next, float before, float after) {
          target.keyframes[index].root_rotation = {
              next.w * before + previous.w * after,
              next.x * before + previous.x * after,
              next.y * before + previous.y * after,
              next.z * before + previous.z * after,
          };
        });

    rc = result.validate();
    if (rc != ARX_OK) return rc;
    out.swap(result);
    if (sound_sources) *sound_sources = std::move(sources);
    return ARX_OK;
  });
}

ArxReturnCode Animation::bakeNative(tea::Data& out) const noexcept {
  NativeAnimationBundle bundle;
  const ArxReturnCode rc = bakeNativeBundle({.include_sound_files = false}, bundle);
  if (rc == ARX_OK) out = std::move(bundle.tea);
  return rc;
}

ArxReturnCode Animation::bakeNativeBundle(const NativeAnimationBakeOptions& options,
                                          NativeAnimationBundle& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!native_text::validMode(options.text_mode)) return ARX_INVALID_OPTIONS;
    ArxReturnCode rc = validate();
    if (rc != ARX_OK) return rc;
    const std::size_t group_count = nativeGroupCount(data_->animation);
    if (group_count > kTeaMaxGroups) return ARX_TEA_BAD_GROUPS_N;
    if (data_->animation.keyframes.size() > kTeaMaxKeyframes) return ARX_TEA_BAD_KEYFRAMES_N;

    std::vector<std::string> sound_paths;
    std::vector<SoundFile> sound_files;
    rc = projectNativeSounds(
        static_cast<const AnimationModules&>(*data_), options.include_sound_files, sound_paths, sound_files);
    if (rc != ARX_OK) return rc;
    tea::Data result;
    result.num_frames = static_cast<std::int32_t>(data_->animation.frame_length);
    result.num_groups = static_cast<std::int32_t>(group_count);
    if (!native_text::encodeTruncated("arx-pistoris/" + data_->animation.name, options.text_mode, result.name))
      return ARX_TEA_BAD_NAME;
    result.keyframes.resize(data_->animation.keyframes.size());
    std::bitset<animation::kMaxGroups> guard_candidates = data_->animation.claimed_groups;
    for (std::size_t frame = 0; frame < data_->animation.keyframes.size(); ++frame) {
      const AnimationKeyframe& source = data_->animation.keyframes[frame];
      tea::Keyframe& keyframe = result.keyframes[frame];
      keyframe.num_frame = static_cast<std::int32_t>(source.frame);
      keyframe.flag_frame = source.footstep ? kTeaFlagFrameStep : kTeaFlagFrameNone;
      keyframe.translate = source.root_translation;
      keyframe.quat = source.root_rotation;
      keyframe.groups.resize(group_count);
      for (std::size_t group = 0; group < group_count; ++group) {
        const AnimationGroupTransform& input =
            data_->animation.group_transforms[frame * data_->animation.group_count + group];
        tea::GroupAnim& transform = keyframe.groups[group];
        transform.key_group = 0;
        transform.quat = input.rotation;
        transform.translate = input.translation;
        transform.zoom = {input.scale.x - 1.0f, input.scale.y - 1.0f, input.scale.z - 1.0f};
        if (!animation::isIdentityTransform(input)) guard_candidates.reset(group);
      }
      if (source.sound != kNoSoundHandle) {
        SoundIndex sound = kNoSound;
        if (!sounds::effectIndex(source.sound, sound) || sound >= sound_paths.size() || sound_paths[sound].empty())
          return ARX_ANIMATION_BAD_KEYFRAME_SOUND;
        tea::Sample sample;
        if (!native_text::encodeFixed(nativeSampleName(sound_paths[sound]), options.text_mode, sample.name))
          return ARX_ANIMATION_BAD_SOUND_PATH;
        keyframe.sample = sample;
      }
    }

    for (std::size_t group = 0; group < group_count; ++group)
      if (guard_candidates.test(group)) result.keyframes.front().groups[group].quat = {-1.0f, 0.0f, 0.0f, 0.0f};

    rc = validateTea(&result);
    if (rc != ARX_OK) return rc;
    out.tea = std::move(result);
    out.sound_files = std::move(sound_files);
    return ARX_OK;
  });
}

}  // namespace pistoris
