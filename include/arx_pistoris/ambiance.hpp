// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/status.h"

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
struct NativeSoundBakeOptions;
struct SoundSourceReference;

// Collection indices are current zero-based positions, not persistent identities
// Non-const calls invalidate collection indices and borrowed views
class Ambiance {
 public:
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
  Ambiance(Ambiance&& other) = delete;
  Ambiance& operator=(const Ambiance& other);
  Ambiance& operator=(Ambiance&& other) = delete;

  void swap(Ambiance& other) noexcept;
  friend void swap(Ambiance& first, Ambiance& second) noexcept { first.swap(second); }
  void reset();

  // --- Conversion ---

  [[nodiscard]] static ArxReturnCode importNative(Ambiance& out, const amb::Data& native,
                                                  std::vector<SoundSourceReference>* sound_sources = nullptr) noexcept;
  [[nodiscard]] static ArxReturnCode importGlb(Ambiance& out, std::span<const std::uint8_t> data) noexcept;
  [[nodiscard]] static ArxReturnCode importGlb(Ambiance& out, std::span<const std::uint8_t> data,
                                               const GlbImportOptions& options,
                                               std::vector<SoundSourceReference>* sound_sources = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode bakeNative(amb::Data& out) const noexcept;
  [[nodiscard]] ArxReturnCode bakeNativeBundle(const NativeSoundBakeOptions& options,
                                               NativeAmbianceBundle& out) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options,
                                        const Model* reference_model) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlbBundle(const GlbExportOptions& options, const Model* reference_model,
                                              AmbianceGlbBundle& out) const noexcept;

  // --- Validation ---

  [[nodiscard]] ArxReturnCode validate() const noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] ArxReturnCode setResourcePath(std::string_view resource_path) noexcept;

  // --- Inspection ---

  [[nodiscard]] std::size_t trackCount() const noexcept;
  [[nodiscard]] std::size_t soundCount() const noexcept;
  [[nodiscard]] AmbianceTrackIndex masterTrack() const noexcept;
  [[nodiscard]] ArxReturnCode copyTracks(std::size_t offset, std::size_t count,
                                         ArxAmbianceTrack* out_tracks) const noexcept;
  [[nodiscard]] ArxReturnCode copySoundViews(std::size_t offset, std::size_t count,
                                             ArxSoundView* out_sounds) const noexcept;
  [[nodiscard]] ArxReturnCode copyPannedKeys(AmbianceTrackIndex track, std::size_t offset, std::size_t count,
                                             ArxAmbiancePannedKey* out_keys) const noexcept;
  [[nodiscard]] ArxReturnCode copyPositionedKeys(AmbianceTrackIndex track, std::size_t offset, std::size_t count,
                                                 ArxAmbiancePositionedKey* out_keys) const noexcept;

  // --- Tracks ---

  [[nodiscard]] ArxReturnCode setPannedTrack(AmbianceTrackIndex index,
                                             const ArxAmbiancePannedTrackInput& track) noexcept;
  [[nodiscard]] ArxReturnCode addPannedTrack(const ArxAmbiancePannedTrackInput& track,
                                             AmbianceTrackIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode setPositionedTrack(AmbianceTrackIndex index,
                                                 const ArxAmbiancePositionedTrackInput& track) noexcept;
  [[nodiscard]] ArxReturnCode addPositionedTrack(const ArxAmbiancePositionedTrackInput& track,
                                                 AmbianceTrackIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeTrack(AmbianceTrackIndex index) noexcept;
  void clearTracks() noexcept;
  [[nodiscard]] ArxReturnCode setMasterTrack(AmbianceTrackIndex index) noexcept;
  [[nodiscard]] ArxReturnCode trimTracksToMaster(std::size_t* trimmed_tracks = nullptr) noexcept;

  // --- Sounds ---

  [[nodiscard]] ArxReturnCode compactSounds(std::size_t* removed = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode rebaseSoundPaths(std::string_view directory) noexcept;
  [[nodiscard]] ArxReturnCode setSound(SoundIndex index, const ArxSoundView& sound) noexcept;
  [[nodiscard]] ArxReturnCode addSound(const ArxSoundView& sound, SoundIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode setSoundData(SoundIndex index, ArxEncodedAudioView encoded_audio) noexcept;
  [[nodiscard]] ArxReturnCode clearSoundData(SoundIndex index) noexcept;
  [[nodiscard]] ArxReturnCode removeSound(SoundIndex index) noexcept;

 private:
  struct Data;
  std::unique_ptr<Data> data_;
};

}  // namespace pistoris
