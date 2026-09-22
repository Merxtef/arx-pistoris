// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_pistoris.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iterator>
#include <utility>
#include <vector>

namespace arx_fuzz {

inline void discardLog(ArxLogLevel, const char*, void*) {}

inline void silenceLogs() {
  static const bool done = [] {
    arx_pistoris_set_log_callback(discardLog, nullptr);
    return true;
  }();
  (void)done;
}

struct ByteBuffer {
  std::uint8_t* value = nullptr;
  std::size_t byte_count = 0;

  ByteBuffer() = default;
  ByteBuffer(const ByteBuffer&) = delete;
  ByteBuffer& operator=(const ByteBuffer&) = delete;
  ByteBuffer(ByteBuffer&& other) noexcept
      : value(std::exchange(other.value, nullptr)), byte_count(std::exchange(other.byte_count, 0)) {}
  ByteBuffer& operator=(ByteBuffer&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value = std::exchange(other.value, nullptr);
    byte_count = std::exchange(other.byte_count, 0);
    return *this;
  }
  ~ByteBuffer() { reset(); }

  void reset() noexcept {
    arx_pistoris_free_bytes(value);
    value = nullptr;
    byte_count = 0;
  }

  [[nodiscard]] std::uint8_t* get() const noexcept { return value; }
  [[nodiscard]] std::size_t size() const noexcept { return byte_count; }
};

template <typename Handle, auto Destroy>
class OwnedHandle {
 public:
  OwnedHandle() = default;
  explicit OwnedHandle(Handle handle) : value_(handle) {}
  OwnedHandle(const OwnedHandle&) = delete;
  OwnedHandle& operator=(const OwnedHandle&) = delete;
  OwnedHandle(OwnedHandle&& other) noexcept : value_(std::exchange(other.value_, nullptr)) {}
  OwnedHandle& operator=(OwnedHandle&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value_ = std::exchange(other.value_, nullptr);
    return *this;
  }
  ~OwnedHandle() { reset(); }

  void reset() noexcept {
    Destroy(value_);
    value_ = nullptr;
  }

  [[nodiscard]] Handle get() const noexcept { return value_; }

 private:
  Handle value_ = nullptr;
};

using FtlHandle = OwnedHandle<ArxFtl*, arx_pistoris_ftl_destroy>;
using TeaHandle = OwnedHandle<ArxTea*, arx_pistoris_tea_destroy>;
using AmbHandle = OwnedHandle<ArxAmb*, arx_pistoris_amb_destroy>;
using CinHandle = OwnedHandle<ArxCin*, arx_pistoris_cin_destroy>;
using ModelHandle = OwnedHandle<ArxModel*, arx_pistoris_model_destroy>;
using AnimationHandle = OwnedHandle<ArxAnimation*, arx_pistoris_animation_destroy>;
using AnimationListHandle = OwnedHandle<ArxAnimationList*, arx_pistoris_animation_list_destroy>;
using AmbianceHandle = OwnedHandle<ArxAmbiance*, arx_pistoris_ambiance_destroy>;
using CinematicHandle = OwnedHandle<ArxCinematic*, arx_pistoris_cinematic_destroy>;

using TextureSourcePathsHandle = OwnedHandle<ArxTextureSourcePaths*, arx_pistoris_texture_source_paths_destroy>;
using SoundSourceReferencesHandle =
    OwnedHandle<ArxSoundSourceReferences*, arx_pistoris_sound_source_references_destroy>;
using CinematicSoundSourceReferencesHandle =
    OwnedHandle<ArxCinematicSoundSourceReferences*, arx_pistoris_cinematic_sound_source_references_destroy>;
using AnimationSoundSourceReferencesHandle =
    OwnedHandle<ArxAnimationSoundSourceReferences*, arx_pistoris_animation_sound_source_references_destroy>;
using ObjMaterialLibraryPathsHandle =
    OwnedHandle<ArxObjMaterialLibraryPaths*, arx_pistoris_obj_material_library_paths_destroy>;
using FtsHandle = OwnedHandle<ArxFts*, arx_pistoris_fts_destroy>;
using LlfHandle = OwnedHandle<ArxLlf*, arx_pistoris_llf_destroy>;
using DlfHandle = OwnedHandle<ArxDlf*, arx_pistoris_dlf_destroy>;

struct DlfBundle {
  DlfHandle dlf;
  LlfHandle embedded_lighting;

  DlfBundle() = default;
  DlfBundle(ArxDlf* dlf_handle, ArxLlf* embedded_lighting_handle)
      : dlf(dlf_handle), embedded_lighting(embedded_lighting_handle) {}
};

using LevelHandle = OwnedHandle<ArxLevel*, arx_pistoris_level_destroy>;

inline void validateTextureSourcePaths(const ArxTextureSourcePaths* paths, std::size_t texture_count) {
  if (!paths) std::abort();
  std::size_t source_count = 0;
  if (arx_pistoris_texture_source_paths_count(paths, &source_count) != ARX_OK) std::abort();
  if (source_count != texture_count) std::abort();
  for (std::size_t texture = 0; texture < source_count; ++texture) {
    ArxStringView path{};
    if (arx_pistoris_texture_source_paths_get(paths, texture, &path) != ARX_OK) std::abort();
    if (path.size != 0 && !path.data) std::abort();
  }
}

inline void validateSoundSourceReferences(const ArxSoundSourceReferences* references, std::size_t sound_count) {
  if (!references) std::abort();
  std::size_t reference_count = 0;
  if (arx_pistoris_sound_source_references_count(references, &reference_count) != ARX_OK) std::abort();
  for (std::size_t index = 0; index < reference_count; ++index) {
    ArxSoundSourceReference reference{};
    if (arx_pistoris_sound_source_references_get(references, index, &reference) != ARX_OK) std::abort();
    if (reference.sound >= sound_count) std::abort();
    if (reference.path.size != 0 && !reference.path.data) std::abort();
  }
}

inline void validateCinematicSoundSourceReferences(const ArxCinematicSoundSourceReferences* references,
                                                   const ArxCinematic* cinematic) {
  if (!references || !cinematic) std::abort();
  std::size_t effect_count = 0;
  std::size_t speech_count = 0;
  if (arx_pistoris_cinematic_sound_count(cinematic, ARX_SOUND_EFFECT, &effect_count) != ARX_OK) std::abort();
  if (arx_pistoris_cinematic_sound_count(cinematic, ARX_SOUND_SPEECH, &speech_count) != ARX_OK) std::abort();

  std::size_t reference_count = 0;
  if (arx_pistoris_cinematic_sound_source_references_count(references, &reference_count) != ARX_OK) std::abort();
  for (std::size_t index = 0; index < reference_count; ++index) {
    ArxCinematicSoundSourceReference reference{};
    if (arx_pistoris_cinematic_sound_source_references_get(references, index, &reference) != ARX_OK) std::abort();
    ArxSoundKind kind = ARX_SOUND_EFFECT;
    ArxSoundIndex sound = ARX_NO_SOUND;
    if (arx_pistoris_sound_handle_kind(reference.sound, &kind) != ARX_OK) std::abort();
    if (arx_pistoris_sound_handle_index(reference.sound, &sound) != ARX_OK) std::abort();
    std::size_t sound_count = 0;
    switch (kind) {
      case ARX_SOUND_EFFECT:
        sound_count = effect_count;
        break;
      case ARX_SOUND_SPEECH:
        sound_count = speech_count;
        break;
      default:
        std::abort();
    }
    if (sound >= sound_count) std::abort();
    if (reference.path.size != 0 && !reference.path.data) std::abort();
  }
}

inline void validateAnimationSoundSourceReferences(const ArxAnimationSoundSourceReferences* references,
                                                   ArxAnimationList* animations) {
  if (!references || !animations) std::abort();
  std::size_t animation_count = 0;
  std::size_t reference_count = 0;
  if (arx_pistoris_animation_list_count(animations, &animation_count) != ARX_OK) std::abort();
  if (arx_pistoris_animation_sound_source_references_count(references, &reference_count) != ARX_OK) std::abort();
  for (std::size_t index = 0; index < reference_count; ++index) {
    ArxAnimationSoundSourceReference reference{};
    if (arx_pistoris_animation_sound_source_references_get(references, index, &reference) != ARX_OK) std::abort();
    if (reference.animation_index >= animation_count) std::abort();
    ArxAnimation* animation = nullptr;
    if (arx_pistoris_animation_list_get(animations, reference.animation_index, &animation) != ARX_OK || !animation)
      std::abort();
    std::size_t sound_count = 0;
    if (arx_pistoris_animation_sound_count(animation, &sound_count) != ARX_OK) std::abort();
    if (reference.reference.sound >= sound_count) std::abort();
    if (reference.reference.path.size != 0 && !reference.reference.path.data) std::abort();
  }
}

inline std::vector<std::uint8_t> readBytes(const char* path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return {};
  return {std::istreambuf_iterator<char>(file), {}};
}

inline FtsHandle parseFtsFile(const char* path) {
  const std::vector<std::uint8_t> bytes = readBytes(path);
  if (bytes.empty()) return {};
  ArxFts* raw_fts = nullptr;
  const ArxReturnCode rc = arx_pistoris_fts_read(bytes.data(), bytes.size(), &raw_fts);
  FtsHandle fts(raw_fts);
  if (rc != ARX_OK) {
    if (fts.get()) std::abort();
    return {};
  }
  return fts;
}

inline LlfHandle parseLlfFile(const char* path) {
  const std::vector<std::uint8_t> bytes = readBytes(path);
  if (bytes.empty()) return {};
  ArxLlf* raw_llf = nullptr;
  const ArxReturnCode rc = arx_pistoris_llf_read(bytes.data(), bytes.size(), &raw_llf);
  LlfHandle llf(raw_llf);
  if (rc != ARX_OK) {
    if (llf.get()) std::abort();
    return {};
  }
  return llf;
}

inline DlfBundle parseDlfFile(const char* path) {
  const std::vector<std::uint8_t> bytes = readBytes(path);
  if (bytes.empty()) return {};
  ArxDlf* dlf = nullptr;
  ArxLlf* embedded_lighting = nullptr;
  const ArxReturnCode read_rc = arx_pistoris_dlf_read(bytes.data(), bytes.size(), &dlf, &embedded_lighting);
  DlfBundle result(dlf, embedded_lighting);
  if (read_rc != ARX_OK) {
    if (result.dlf.get() || result.embedded_lighting.get()) std::abort();
    return {};
  }
  return result;
}

inline const ArxFts* fixtureLevelFts() {
  static const FtsHandle fixture = parseFtsFile("data/fixtures/mount/game/graph/levels/level29/fast.fts");
  if (!fixture.get()) std::abort();
  return fixture.get();
}

inline const ArxLlf* fixtureLevelLlf() {
  static const LlfHandle fixture = parseLlfFile("data/fixtures/mount/graph/levels/level29/level29.llf");
  if (!fixture.get()) std::abort();
  return fixture.get();
}

inline const ArxDlf* fixtureLevelDlf() {
  static const DlfBundle fixture = parseDlfFile("data/fixtures/mount/graph/levels/level29/level29.dlf");
  if (!fixture.dlf.get()) std::abort();
  return fixture.dlf.get();
}

inline void exerciseLevelFromNative(const ArxFts* fts, const ArxLlf* llf, const ArxDlf* dlf) {
  if (!fts) return;
  ArxLevel* raw_level = nullptr;
  ArxTextureSourcePaths* raw_sources = nullptr;
  const ArxReturnCode rc =
      arx_pistoris_level_import_native(fts, llf, dlf, &raw_level, &raw_sources, ARX_NATIVE_TEXT_AUTO);
  LevelHandle level(raw_level);
  TextureSourcePathsHandle sources(raw_sources);
  if (rc != ARX_OK) {
    if (level.get() || sources.get()) std::abort();
    return;
  }
  if (!level.get() || !sources.get()) std::abort();
  if (arx_pistoris_level_validate(level.get()) != ARX_OK) std::abort();
  std::size_t texture_count = 0;
  if (arx_pistoris_level_texture_count(level.get(), &texture_count) != ARX_OK) std::abort();
  validateTextureSourcePaths(sources.get(), texture_count);
}

inline void pushLe32(std::vector<std::uint8_t>& buffer, std::uint32_t value) {
  buffer.push_back(static_cast<std::uint8_t>(value & 0xffu));
  buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
  buffer.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
  buffer.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

inline std::vector<std::uint8_t> buildGlbFromFuzzInput(const std::uint8_t* data, std::size_t size) {
  constexpr std::uint32_t kGlbMagic = 0x46546C67u;       // 'glTF'
  constexpr std::uint32_t kChunkTypeJson = 0x4E4F534Au;  // 'JSON'
  constexpr std::uint32_t kChunkTypeBin = 0x004E4942u;   // 'BIN\0'

  if (size < sizeof(std::uint32_t)) return {};

  std::uint32_t bin_len_request = 0;
  std::memcpy(&bin_len_request, data, sizeof(bin_len_request));
  data += sizeof(bin_len_request);
  size -= sizeof(bin_len_request);

  const std::size_t bin_len = (size > 0) ? (bin_len_request % size) : 0;
  const std::size_t json_len = size - bin_len;

  const std::uint8_t* json_data = data;
  const std::uint8_t* bin_data = data + json_len;

  const std::size_t json_pad = (4 - (json_len % 4)) % 4;
  const std::size_t bin_pad = (4 - (bin_len % 4)) % 4;

  std::uint32_t total_len = 12U + 8U + static_cast<std::uint32_t>(json_len + json_pad);
  if (bin_len > 0) total_len += 8U + static_cast<std::uint32_t>(bin_len + bin_pad);

  std::vector<std::uint8_t> glb;
  glb.reserve(total_len);
  pushLe32(glb, kGlbMagic);
  pushLe32(glb, 2U);
  pushLe32(glb, total_len);

  pushLe32(glb, static_cast<std::uint32_t>(json_len + json_pad));
  pushLe32(glb, kChunkTypeJson);
  glb.insert(glb.end(), json_data, json_data + json_len);
  for (std::size_t i = 0; i < json_pad; ++i) glb.push_back(' ');

  if (bin_len > 0) {
    pushLe32(glb, static_cast<std::uint32_t>(bin_len + bin_pad));
    pushLe32(glb, kChunkTypeBin);
    glb.insert(glb.end(), bin_data, bin_data + bin_len);
    for (std::size_t i = 0; i < bin_pad; ++i) glb.push_back(0);
  }

  return glb;
}

}  // namespace arx_fuzz
