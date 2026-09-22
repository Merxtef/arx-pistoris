// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/sound.hpp"

#include "utils/resource_path.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pistoris {

struct SoundsData;

namespace glb {

struct ImportedSoundSource {
  SoundHandle sound = kNoSoundHandle;
  std::string path;
};

enum class SoundPathImportError : std::uint8_t {
  kNone,
  kBadKind,
  kBadPath,
  kTooManySounds,
};

class SoundPathImporter {
 public:
  SoundPathImporter(SoundsData& sounds, std::vector<ImportedSoundSource>* sources, std::string_view log_prefix);
  SoundPathImporter(const SoundPathImporter&) = delete;
  SoundPathImporter& operator=(const SoundPathImporter&) = delete;
  SoundPathImporter(SoundPathImporter&&) = delete;
  SoundPathImporter& operator=(SoundPathImporter&&) = delete;

  SoundPathImportError import(SoundKind kind, std::string_view source, SoundHandle& out);
  SoundPathImportError finish();

 private:
  using PathMap = std::unordered_map<std::string, SoundHandle, ResourcePathIdentityHash, ResourcePathIdentityEqual>;

  struct KindState {
    PathMap identities;
    std::unordered_set<std::string> source_spellings;
    std::vector<std::string> paths;
    std::vector<const std::string*> original_paths;
    std::size_t base_count = 0;
  };

  KindState* state(SoundKind kind) noexcept;
  void recordSource(KindState& state, SoundHandle sound, std::string_view source);
  SoundPathImportError repair(SoundKind kind, KindState& state);

  SoundsData& sounds_;
  std::vector<ImportedSoundSource>* sources_ = nullptr;
  std::string_view log_prefix_;
  std::array<KindState, 2> kinds_;
  bool finished_ = false;
};

}  // namespace glb
}  // namespace pistoris
