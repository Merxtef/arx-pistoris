// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/status.h"

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
struct NativeSoundBakeOptions;
struct SoundSourceReference;

class Model;

// Collection indices are current zero-based positions, not persistent identities
// Non-const calls invalidate collection indices and borrowed views
class Animation {
 public:
  // --- Lifetime ---

  Animation();
  ~Animation();

  Animation(const Animation& other);
  Animation(Animation&& other) = delete;
  Animation& operator=(const Animation& other);
  Animation& operator=(Animation&& other) = delete;

  void swap(Animation& other) noexcept;
  friend void swap(Animation& first, Animation& second) noexcept { first.swap(second); }
  void reset();

  // --- Conversion ---

  [[nodiscard]] static ArxReturnCode importNative(Animation& out, const tea::Data& native,
                                                  std::vector<SoundSourceReference>* sound_sources = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode bakeNative(tea::Data& out) const noexcept;
  [[nodiscard]] ArxReturnCode bakeNativeBundle(const NativeSoundBakeOptions& options,
                                               NativeAnimationBundle& out) const noexcept;

  // --- Validation ---

  [[nodiscard]] ArxReturnCode validate() const noexcept;

  // --- Transformation ---

  [[nodiscard]] ArxReturnCode scale(float factor) noexcept;
  [[nodiscard]] ArxReturnCode rotate(ArxQuat rotation) noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view name() const noexcept;
  [[nodiscard]] ArxReturnCode setName(std::string_view name) noexcept;
  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] ArxReturnCode setResourcePath(std::string_view resource_path) noexcept;

  // --- Inspection ---

  [[nodiscard]] std::uint32_t frameLength() const noexcept;
  [[nodiscard]] std::size_t groupCount() const noexcept;
  [[nodiscard]] ArxReturnCode isGroupVoid(std::size_t group, bool& out) const noexcept;
  [[nodiscard]] ArxReturnCode isGroupClaimed(std::size_t group, bool& out) const noexcept;
  [[nodiscard]] std::size_t keyframeCount() const noexcept;
  [[nodiscard]] std::size_t soundCount() const noexcept;
  [[nodiscard]] ArxReturnCode copySoundViews(std::size_t offset, std::size_t count,
                                             ArxSoundView* out_views) const noexcept;
  [[nodiscard]] ArxReturnCode copyKeyframes(std::size_t offset, std::size_t count,
                                            ArxAnimationKeyframe* out_keyframes) const noexcept;
  [[nodiscard]] ArxReturnCode copyGroupTransforms(std::size_t keyframe, std::size_t offset, std::size_t count,
                                                  ArxAnimationGroupTransform* out_transforms) const noexcept;

  // --- Timeline editing ---

  [[nodiscard]] ArxReturnCode setFrameLength(std::uint32_t frame_length) noexcept;
  [[nodiscard]] ArxReturnCode setKeyframe(std::size_t index, const ArxAnimationKeyframeInput& keyframe) noexcept;
  [[nodiscard]] ArxReturnCode addKeyframe(const ArxAnimationKeyframeInput& keyframe, std::size_t& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeKeyframe(std::size_t index) noexcept;
  [[nodiscard]] ArxReturnCode replaceKeyframes(std::uint32_t frame_length, const ArxAnimationKeyframeInput* keyframes,
                                               std::size_t keyframe_count) noexcept;
  void clearKeyframes() noexcept;
  [[nodiscard]] ArxReturnCode claimGroup(std::size_t group) noexcept;
  [[nodiscard]] ArxReturnCode unclaimGroup(std::size_t group) noexcept;
  [[nodiscard]] ArxReturnCode voidGroup(std::size_t group) noexcept;

  // --- Sounds ---

  [[nodiscard]] ArxReturnCode compactSounds(std::size_t* removed = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode rebaseSoundPaths(std::string_view directory) noexcept;
  [[nodiscard]] ArxReturnCode setSound(SoundIndex index, const ArxSoundView& sound) noexcept;
  [[nodiscard]] ArxReturnCode addSound(const ArxSoundView& sound, SoundIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode setSoundData(SoundIndex index, ArxEncodedAudioView encoded_audio) noexcept;
  [[nodiscard]] ArxReturnCode clearSoundData(SoundIndex index) noexcept;
  [[nodiscard]] ArxReturnCode removeSound(SoundIndex index) noexcept;

 private:
  friend class Model;

  struct Data;
  std::unique_ptr<Data> data_;
};

}  // namespace pistoris
