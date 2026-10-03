// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance/location.hpp"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/indexed_view.hpp"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/text.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

struct ArxSoundView;

namespace pistoris {

namespace amb {
struct Data;
}

class Model;
struct AmbianceGlbBundle;
struct NativeAmbianceBundle;
struct NativeAmbianceBakeOptions;
struct SoundSourceReference;

// Collection indices are current zero-based positions, not persistent identities
// Non-const calls invalidate collection indices and borrowed views
class Ambiance {
 public:
  struct TracksViewTag;
  struct SoundsViewTag;
  struct PannedKeysViewTag;
  struct PositionedKeysViewTag;

  using TracksView = IndexedView<ArxAmbianceTrack, TracksViewTag>;
  using SoundsView = IndexedView<ArxSoundView, SoundsViewTag>;
  using PannedKeysView = IndexedView<ArxAmbiancePannedKey, PannedKeysViewTag>;
  using PositionedKeysView = IndexedView<ArxAmbiancePositionedKey, PositionedKeysViewTag>;

  struct GlbImportOptions {
    // Range [1, 1000]
    float arx_units_per_glb_unit = 10.0f;
  };

  struct GlbExportOptions {
    // Range [1, 1000]
    float arx_units_per_glb_unit = 10.0f;
  };

  // --- Lifetime ---

  Ambiance();
  ~Ambiance();

  Ambiance(const Ambiance& other);
  Ambiance(Ambiance&& other) noexcept;
  Ambiance& operator=(const Ambiance& other);
  Ambiance& operator=(Ambiance&& other) noexcept;

  void swap(Ambiance& other) noexcept;
  friend void swap(Ambiance& first, Ambiance& second) noexcept { first.swap(second); }
  [[nodiscard]] AmbianceResult<void> reset() noexcept;

  // --- Conversion ---

  [[nodiscard]] static AmbResult<Ambiance> importNative(const amb::Data& native,
                                                        std::vector<SoundSourceReference>* sound_sources = nullptr,
                                                        NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
  [[nodiscard]] static GlbResult<Ambiance> importGlb(std::span<const std::uint8_t> data) noexcept;
  [[nodiscard]] static GlbResult<Ambiance> importGlb(
      std::span<const std::uint8_t> data, const GlbImportOptions& options,
      std::vector<SoundSourceReference>* sound_sources = nullptr) noexcept;
  [[nodiscard]] AmbianceResult<amb::Data> bakeNative() const noexcept;
  [[nodiscard]] AmbianceResult<NativeAmbianceBundle> bakeNativeBundle(
      const NativeAmbianceBakeOptions& options) const noexcept;
  [[nodiscard]] AmbianceGlbExportResult<std::vector<std::uint8_t>> exportGlb() const noexcept;
  [[nodiscard]] AmbianceGlbExportResult<std::vector<std::uint8_t>> exportGlb(
      const GlbExportOptions& options) const noexcept;
  [[nodiscard]] AmbianceGlbExportResult<std::vector<std::uint8_t>> exportGlb(
      const GlbExportOptions& options, const Model* reference_model) const noexcept;
  [[nodiscard]] AmbianceGlbExportResult<AmbianceGlbBundle> exportGlbBundle(const GlbExportOptions& options,
                                                                           const Model* reference_model) const noexcept;

  // --- Validation ---

  [[nodiscard]] AmbianceResult<void> validate() const noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] AmbianceResult<void> setResourcePath(std::string_view resource_path) noexcept;

  // --- Inspection ---

  [[nodiscard]] std::size_t trackCount() const noexcept;
  [[nodiscard]] std::size_t soundCount() const noexcept;
  [[nodiscard]] AmbianceTrackIndex masterTrack() const noexcept;
  [[nodiscard]] TracksView tracks() const noexcept;
  [[nodiscard]] SoundsView sounds() const noexcept;
  [[nodiscard]] AmbianceResult<PannedKeysView> pannedKeys(AmbianceTrackIndex track) const noexcept;
  [[nodiscard]] AmbianceResult<PositionedKeysView> positionedKeys(AmbianceTrackIndex track) const noexcept;

  // --- Tracks ---

  [[nodiscard]] AmbianceResult<void> setPannedTrack(AmbianceTrackIndex index,
                                                    const ArxAmbiancePannedTrackInput& track) noexcept;
  [[nodiscard]] AmbianceResult<AmbianceTrackIndex> addPannedTrack(const ArxAmbiancePannedTrackInput& track) noexcept;
  [[nodiscard]] AmbianceResult<void> setPositionedTrack(AmbianceTrackIndex index,
                                                        const ArxAmbiancePositionedTrackInput& track) noexcept;
  [[nodiscard]] AmbianceResult<AmbianceTrackIndex> addPositionedTrack(
      const ArxAmbiancePositionedTrackInput& track) noexcept;
  [[nodiscard]] AmbianceResult<void> removeTrack(AmbianceTrackIndex index) noexcept;
  void clearTracks() noexcept;
  [[nodiscard]] AmbianceResult<void> setMasterTrack(AmbianceTrackIndex index) noexcept;
  [[nodiscard]] AmbianceResult<std::size_t> trimTracksToMaster() noexcept;

  // --- Sounds ---

  [[nodiscard]] AmbianceResult<std::size_t> compactSounds() noexcept;
  [[nodiscard]] AmbianceResult<void> rebaseSoundPaths(std::string_view directory) noexcept;
  [[nodiscard]] AmbianceResult<void> setSound(SoundIndex index, const ArxSoundView& sound) noexcept;
  [[nodiscard]] AmbianceResult<SoundIndex> addSound(const ArxSoundView& sound) noexcept;
  [[nodiscard]] AmbianceResult<void> setSoundPath(SoundIndex index, std::string_view path) noexcept;
  [[nodiscard]] AmbianceResult<void> setSoundData(SoundIndex index, ArxEncodedAudioView encoded_audio) noexcept;
  [[nodiscard]] AmbianceResult<void> clearSoundData(SoundIndex index) noexcept;
  [[nodiscard]] AmbianceResult<void> removeSound(SoundIndex index) noexcept;

 private:
  [[nodiscard]] static ArxAmbianceTrack trackAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxSoundView soundAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxAmbiancePannedKey pannedKeyAt(const void* owner, std::size_t track,
                                                        std::size_t index) noexcept;
  [[nodiscard]] static ArxAmbiancePositionedKey positionedKeyAt(const void* owner, std::size_t track,
                                                                std::size_t index) noexcept;
  struct Data;
  std::unique_ptr<Data> data_;
};

}  // namespace pistoris
