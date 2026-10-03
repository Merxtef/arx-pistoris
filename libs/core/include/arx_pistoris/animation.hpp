// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/indexed_view.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/text.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

struct ArxSoundView;

namespace pistoris {

namespace tea {
struct Data;
}

struct NativeAnimationBundle;
struct NativeAnimationBakeOptions;
struct SoundSourceReference;

class Model;

// Collection indices are current zero-based positions, not persistent identities
// Non-const calls invalidate collection indices and borrowed views
class Animation {
 public:
  struct SoundsViewTag;
  struct KeyframesViewTag;
  struct GroupTransformsViewTag;

  using SoundsView = IndexedView<ArxSoundView, SoundsViewTag>;
  using KeyframesView = IndexedView<ArxAnimationKeyframe, KeyframesViewTag>;
  using GroupTransformsView = IndexedView<ArxAnimationGroupTransform, GroupTransformsViewTag>;

  // --- Lifetime ---

  Animation();
  ~Animation();

  Animation(const Animation& other);
  Animation(Animation&& other) noexcept;
  Animation& operator=(const Animation& other);
  Animation& operator=(Animation&& other) noexcept;

  void swap(Animation& other) noexcept;
  friend void swap(Animation& first, Animation& second) noexcept { first.swap(second); }
  [[nodiscard]] AnimationResult<void> reset() noexcept;

  // --- Conversion ---

  [[nodiscard]] static TeaResult<Animation> importNative(const tea::Data& native,
                                                         std::vector<SoundSourceReference>* sound_sources = nullptr,
                                                         NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
  [[nodiscard]] AnimationResult<tea::Data> bakeNative() const noexcept;
  [[nodiscard]] AnimationResult<NativeAnimationBundle> bakeNativeBundle(
      const NativeAnimationBakeOptions& options) const noexcept;

  // --- Validation ---

  [[nodiscard]] AnimationResult<void> validate() const noexcept;

  // --- Transformation ---

  [[nodiscard]] AnimationResult<void> scale(float factor) noexcept;
  [[nodiscard]] AnimationResult<void> rotate(ArxQuat rotation) noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view name() const noexcept;
  [[nodiscard]] AnimationResult<void> setName(std::string_view name) noexcept;
  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] AnimationResult<void> setResourcePath(std::string_view resource_path) noexcept;

  // --- Inspection ---

  [[nodiscard]] std::uint32_t frameLength() const noexcept;
  [[nodiscard]] std::size_t groupCount() const noexcept;
  [[nodiscard]] AnimationResult<bool> isGroupVoid(std::size_t group) const noexcept;
  [[nodiscard]] AnimationResult<bool> isGroupClaimed(std::size_t group) const noexcept;
  [[nodiscard]] std::size_t keyframeCount() const noexcept;
  [[nodiscard]] std::size_t soundCount() const noexcept;
  [[nodiscard]] SoundsView sounds() const noexcept;
  [[nodiscard]] KeyframesView keyframes() const noexcept;
  [[nodiscard]] AnimationResult<GroupTransformsView> groupTransforms(std::size_t keyframe) const noexcept;

  // --- Timeline editing ---

  [[nodiscard]] AnimationResult<void> setFrameLength(std::uint32_t frame_length) noexcept;
  [[nodiscard]] AnimationResult<void> setKeyframe(std::size_t index,
                                                  const ArxAnimationKeyframeInput& keyframe) noexcept;
  [[nodiscard]] AnimationResult<std::size_t> addKeyframe(const ArxAnimationKeyframeInput& keyframe) noexcept;
  [[nodiscard]] AnimationResult<void> removeKeyframe(std::size_t index) noexcept;
  [[nodiscard]] AnimationResult<void> replaceKeyframes(std::uint32_t frame_length,
                                                       const ArxAnimationKeyframeInput* keyframes,
                                                       std::size_t keyframe_count) noexcept;
  void clearKeyframes() noexcept;
  [[nodiscard]] AnimationResult<void> claimGroup(std::size_t group) noexcept;
  [[nodiscard]] AnimationResult<void> unclaimGroup(std::size_t group) noexcept;
  [[nodiscard]] AnimationResult<void> voidGroup(std::size_t group) noexcept;

  // --- Sounds ---

  [[nodiscard]] AnimationResult<std::size_t> compactSounds() noexcept;
  [[nodiscard]] AnimationResult<void> rebaseSoundPaths(std::string_view directory) noexcept;
  [[nodiscard]] AnimationResult<void> setSound(SoundIndex index, const ArxSoundView& sound) noexcept;
  [[nodiscard]] AnimationResult<SoundIndex> addSound(const ArxSoundView& sound) noexcept;
  [[nodiscard]] AnimationResult<void> setSoundPath(SoundIndex index, std::string_view path) noexcept;
  [[nodiscard]] AnimationResult<void> setSoundData(SoundIndex index, ArxEncodedAudioView encoded_audio) noexcept;
  [[nodiscard]] AnimationResult<void> clearSoundData(SoundIndex index) noexcept;
  [[nodiscard]] AnimationResult<void> removeSound(SoundIndex index) noexcept;

 private:
  friend class Model;

  [[nodiscard]] static ArxSoundView soundAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxAnimationKeyframe keyframeAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxAnimationGroupTransform groupTransformAt(const void* owner, std::size_t keyframe,
                                                                   std::size_t group) noexcept;

  struct Data;
  std::unique_ptr<Data> data_;
};

}  // namespace pistoris
