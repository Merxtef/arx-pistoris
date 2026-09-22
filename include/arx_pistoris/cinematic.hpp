// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
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
  // --- Lifetime ---

  Cinematic();
  ~Cinematic();

  Cinematic(const Cinematic& other);
  Cinematic(Cinematic&& other) = delete;
  Cinematic& operator=(const Cinematic& other);
  Cinematic& operator=(Cinematic&& other) = delete;

  void swap(Cinematic& other) noexcept;
  friend void swap(Cinematic& first, Cinematic& second) noexcept { first.swap(second); }
  void reset();

  // --- Conversion ---

  [[nodiscard]] static ArxReturnCode importNative(Cinematic& out, const cin::Data& native,
                                                  std::vector<std::string>* illustration_source_paths = nullptr,
                                                  std::vector<CinematicSoundSourceReference>* sound_sources = nullptr,
                                                  NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
  [[nodiscard]] static ArxReturnCode importGlb(
      Cinematic& out, std::span<const std::uint8_t> data,
      std::vector<CinematicSoundSourceReference>* sound_sources = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode bakeNative(cin::Data& out) const noexcept;
  [[nodiscard]] ArxReturnCode bakeNativeBundle(const NativeCinematicBakeOptions& options,
                                               NativeCinematicBundle& out) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlbBundle(CinematicGlbBundle& out) const noexcept;

  // --- Validation ---

  [[nodiscard]] ArxReturnCode validate() const noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] ArxReturnCode setResourcePath(std::string_view resource_path) noexcept;

  // --- Inspection ---

  [[nodiscard]] std::int32_t endFrame() const noexcept;
  [[nodiscard]] float fps() const noexcept;
  [[nodiscard]] std::size_t illustrationCount() const noexcept;
  [[nodiscard]] std::size_t keyframeCount() const noexcept;
  [[nodiscard]] std::size_t textureCount() const noexcept;
  [[nodiscard]] std::size_t soundCount(SoundKind kind) const noexcept;
  [[nodiscard]] std::size_t languageCount() const noexcept;
  [[nodiscard]] std::size_t soundEncodingCount() const noexcept;
  [[nodiscard]] ArxReturnCode copyIllustrations(std::size_t offset, std::size_t count,
                                                ArxCinematicIllustration* out_illustrations) const noexcept;
  [[nodiscard]] ArxReturnCode copyKeyframes(std::size_t offset, std::size_t count,
                                            ArxCinematicKeyframe* out_keyframes) const noexcept;
  [[nodiscard]] ArxReturnCode copyTextureViews(std::size_t offset, std::size_t count,
                                               ArxTextureView* out_textures) const noexcept;
  [[nodiscard]] ArxReturnCode copySoundViews(SoundKind kind, std::size_t offset, std::size_t count,
                                             ArxCinematicSoundView* out_sounds) const noexcept;
  [[nodiscard]] ArxReturnCode copyLanguages(std::size_t offset, std::size_t count,
                                            ArxCinematicLanguageView* out_languages) const noexcept;
  [[nodiscard]] ArxReturnCode copySoundEncodings(std::size_t offset, std::size_t count,
                                                 ArxCinematicSoundEncodingView* out_encodings) const noexcept;

  // --- Timeline ---

  [[nodiscard]] ArxReturnCode setTimeline(std::int32_t end_frame, float fps) noexcept;
  [[nodiscard]] ArxReturnCode setKeyframe(std::size_t index, const ArxCinematicKeyframe& keyframe) noexcept;
  [[nodiscard]] ArxReturnCode addKeyframe(const ArxCinematicKeyframe& keyframe, std::size_t& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeKeyframe(std::size_t index) noexcept;
  void clearKeyframes() noexcept;

  // --- Illustrations ---

  [[nodiscard]] ArxReturnCode setIllustration(CinematicIllustrationIndex index,
                                              ArxCinematicIllustration illustration) noexcept;
  [[nodiscard]] ArxReturnCode addIllustration(ArxCinematicIllustration illustration,
                                              CinematicIllustrationIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeIllustration(CinematicIllustrationIndex index) noexcept;
  void clearIllustrations() noexcept;

  // --- Textures ---

  [[nodiscard]] ArxReturnCode compactTextures(std::size_t* removed = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode rebaseTexturePaths(std::string_view directory) noexcept;
  [[nodiscard]] ArxReturnCode setTexture(TextureIndex index, const ArxTextureView& texture) noexcept;
  [[nodiscard]] ArxReturnCode addTexture(const ArxTextureView& texture, TextureIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept;
  [[nodiscard]] ArxReturnCode clearTextureImage(TextureIndex index) noexcept;

  // --- Sounds ---

  [[nodiscard]] ArxReturnCode compactSounds(SoundKind kind, std::size_t* removed = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode rebaseSoundPaths(SoundKind kind, std::string_view directory) noexcept;
  [[nodiscard]] ArxReturnCode setSoundPath(SoundHandle sound, std::string_view path) noexcept;
  [[nodiscard]] ArxReturnCode addSound(SoundKind kind, std::string_view path, SoundHandle& out_sound) noexcept;
  [[nodiscard]] ArxReturnCode removeSound(SoundHandle sound) noexcept;
  [[nodiscard]] ArxReturnCode setSoundData(SoundHandle sound, LanguageId language,
                                           ArxEncodedAudioView encoded_audio) noexcept;
  [[nodiscard]] ArxReturnCode clearSoundData(SoundHandle sound, LanguageId language) noexcept;

  // --- Languages ---

  [[nodiscard]] ArxReturnCode setLanguage(LanguageId language, std::string_view name) noexcept;
  [[nodiscard]] ArxReturnCode addLanguage(std::string_view name, LanguageId& out_language) noexcept;
  [[nodiscard]] ArxReturnCode removeLanguage(LanguageId language) noexcept;

 private:
  struct Data;
  std::unique_ptr<Data> data_;
};

}  // namespace pistoris
