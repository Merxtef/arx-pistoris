// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indexed_view.hpp"
#include "arx_pistoris/cinematic/location.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

struct ArxTextureView;

namespace pistoris {

namespace cin {
struct Data;
}

struct CinematicSoundSourceReference;
struct CinematicGlbBundle;
struct NativeCinematicBakeOptions;
struct NativeCinematicBundle;

// Collection indices are current zero-based positions, not persistent identities
// Non-const calls invalidate collection indices, SoundHandles, and borrowed views
class Cinematic {
 public:
  struct IllustrationsViewTag;
  struct KeyframesViewTag;
  struct TexturesViewTag;
  struct SoundsViewTag;
  struct LanguagesViewTag;
  struct SoundEncodingsViewTag;

  using IllustrationsView = IndexedView<ArxCinematicIllustration, IllustrationsViewTag>;
  using KeyframesView = IndexedView<ArxCinematicKeyframe, KeyframesViewTag>;
  using TexturesView = IndexedView<ArxTextureView, TexturesViewTag>;
  using SoundsView = IndexedView<ArxCinematicSoundView, SoundsViewTag>;
  using LanguagesView = IndexedView<ArxCinematicLanguageView, LanguagesViewTag>;
  using SoundEncodingsView = IndexedView<ArxCinematicSoundEncodingView, SoundEncodingsViewTag>;

  // --- Lifetime ---

  Cinematic();
  ~Cinematic();

  Cinematic(const Cinematic& other);
  Cinematic(Cinematic&& other) noexcept;
  Cinematic& operator=(const Cinematic& other);
  Cinematic& operator=(Cinematic&& other) noexcept;

  void swap(Cinematic& other) noexcept;
  friend void swap(Cinematic& first, Cinematic& second) noexcept { first.swap(second); }
  [[nodiscard]] CinematicResult<void> reset() noexcept;

  // --- Conversion ---

  [[nodiscard]] static CinResult<Cinematic> importNative(
      const cin::Data& native, std::vector<std::string>* illustration_source_paths = nullptr,
      std::vector<CinematicSoundSourceReference>* sound_sources = nullptr,
      NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
  [[nodiscard]] static GlbResult<Cinematic> importGlb(
      std::span<const std::uint8_t> data, std::vector<CinematicSoundSourceReference>* sound_sources = nullptr) noexcept;
  [[nodiscard]] CinematicResult<cin::Data> bakeNative() const noexcept;
  [[nodiscard]] CinematicResult<NativeCinematicBundle> bakeNativeBundle(
      const NativeCinematicBakeOptions& options) const noexcept;
  [[nodiscard]] CinematicResult<std::vector<std::uint8_t>> exportGlb() const noexcept;
  [[nodiscard]] CinematicResult<CinematicGlbBundle> exportGlbBundle() const noexcept;

  // --- Validation ---

  [[nodiscard]] CinematicResult<void> validate() const noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] CinematicResult<void> setResourcePath(std::string_view resource_path) noexcept;

  // --- Inspection ---

  [[nodiscard]] std::int32_t endFrame() const noexcept;
  [[nodiscard]] float fps() const noexcept;
  [[nodiscard]] std::size_t illustrationCount() const noexcept;
  [[nodiscard]] std::size_t keyframeCount() const noexcept;
  [[nodiscard]] std::size_t textureCount() const noexcept;
  [[nodiscard]] std::size_t soundCount(SoundKind kind) const noexcept;
  [[nodiscard]] std::size_t languageCount() const noexcept;
  [[nodiscard]] std::size_t soundEncodingCount() const noexcept;
  [[nodiscard]] IllustrationsView illustrations() const noexcept;
  [[nodiscard]] KeyframesView keyframes() const noexcept;
  [[nodiscard]] TexturesView textures() const noexcept;
  [[nodiscard]] SoundsView sounds(SoundKind kind) const noexcept;
  [[nodiscard]] LanguagesView languages() const noexcept;
  [[nodiscard]] std::optional<LanguageId> findLanguage(std::string_view name) const noexcept;
  [[nodiscard]] SoundEncodingsView soundEncodings() const noexcept;

  // --- Timeline ---

  [[nodiscard]] CinematicResult<void> setTimeline(std::int32_t end_frame, float fps) noexcept;
  [[nodiscard]] CinematicResult<void> setKeyframe(std::size_t index, const ArxCinematicKeyframe& keyframe) noexcept;
  [[nodiscard]] CinematicResult<std::size_t> addKeyframe(const ArxCinematicKeyframe& keyframe) noexcept;
  [[nodiscard]] CinematicResult<void> removeKeyframe(std::size_t index) noexcept;
  void clearKeyframes() noexcept;

  // --- Illustrations ---

  [[nodiscard]] CinematicResult<void> setIllustration(CinematicIllustrationIndex index,
                                                      ArxCinematicIllustration illustration) noexcept;
  [[nodiscard]] CinematicResult<CinematicIllustrationIndex> addIllustration(
      ArxCinematicIllustration illustration) noexcept;
  [[nodiscard]] CinematicResult<void> removeIllustration(CinematicIllustrationIndex index) noexcept;
  void clearIllustrations() noexcept;
  [[nodiscard]] CinematicResult<std::size_t> compactIllustrations() noexcept;

  // --- Textures ---

  [[nodiscard]] CinematicResult<void> rebaseTexturePaths(std::string_view directory) noexcept;
  [[nodiscard]] CinematicResult<void> setTexture(TextureIndex index, const ArxTextureView& texture) noexcept;
  [[nodiscard]] CinematicResult<TextureIndex> addTexture(const ArxTextureView& texture) noexcept;
  [[nodiscard]] CinematicResult<void> setTexturePath(TextureIndex index, std::string_view path) noexcept;
  [[nodiscard]] CinematicResult<void> setTextureExternalImageExtension(TextureIndex index,
                                                                       std::string_view extension) noexcept;
  [[nodiscard]] CinematicResult<void> setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept;
  [[nodiscard]] CinematicResult<void> clearTextureImage(TextureIndex index) noexcept;

  // --- Sounds ---

  [[nodiscard]] CinematicResult<std::size_t> compactSounds(SoundKind kind) noexcept;
  [[nodiscard]] CinematicResult<void> rebaseSoundPaths(SoundKind kind, std::string_view directory) noexcept;
  [[nodiscard]] CinematicResult<void> setSoundPath(SoundHandle sound, std::string_view path) noexcept;
  [[nodiscard]] CinematicResult<SoundHandle> addSound(SoundKind kind, std::string_view path) noexcept;
  [[nodiscard]] CinematicResult<void> removeSound(SoundHandle sound) noexcept;
  [[nodiscard]] CinematicResult<void> setSoundData(SoundHandle sound, LanguageId language,
                                                   ArxEncodedAudioView encoded_audio) noexcept;
  [[nodiscard]] CinematicResult<void> clearSoundData(SoundHandle sound, LanguageId language) noexcept;

  // --- Languages ---

  [[nodiscard]] CinematicResult<void> setLanguage(LanguageId language, std::string_view name) noexcept;
  [[nodiscard]] CinematicResult<LanguageId> addLanguage(std::string_view name) noexcept;
  [[nodiscard]] CinematicResult<void> removeLanguage(LanguageId language) noexcept;

 private:
  [[nodiscard]] static ArxCinematicIllustration illustrationAt(const void* owner, std::size_t,
                                                               std::size_t index) noexcept;
  [[nodiscard]] static ArxCinematicKeyframe keyframeAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxTextureView textureAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxCinematicSoundView soundAt(const void* owner, std::size_t kind, std::size_t index) noexcept;
  [[nodiscard]] static ArxCinematicLanguageView languageAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxCinematicSoundEncodingView soundEncodingAt(const void* owner, std::size_t,
                                                                     std::size_t index) noexcept;
  struct Data;
  std::unique_ptr<Data> data_;
};

}  // namespace pistoris
