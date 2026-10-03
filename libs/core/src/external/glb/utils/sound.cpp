// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "sound.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "modules/sounds.h"
#include "utils/log.h"
#include "utils/resource_path.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb {

SoundPathImporter::SoundPathImporter(SoundsData& sounds, std::vector<ImportedSoundSource>* sources,
                                     std::string_view log_prefix)
    : sounds_(sounds), sources_(sources), log_prefix_(log_prefix) {
  kinds_[0].base_count = sounds::count(sounds_, SoundKind::kEffect);
  kinds_[1].base_count = sounds::count(sounds_, SoundKind::kSpeech);
  for (std::size_t kind_index = 0; kind_index < kinds_.size(); ++kind_index) {
    KindState& kind_state = kinds_[kind_index];
    const SoundKind kind = kind_index == 0 ? SoundKind::kEffect : SoundKind::kSpeech;
    kind_state.identities.reserve(kind_state.base_count);
    for (std::size_t index = 0; index < kind_state.base_count; ++index) {
      SoundHandle handle = kNoSoundHandle;
      const ArxReturnCode rc = soundHandle(kind, static_cast<SoundIndex>(index), handle);
      assert(rc == ARX_OK);
      (void)rc;
      kind_state.identities.emplace(sounds::path(sounds_, handle), handle);
    }
  }
}

SoundPathImporter::KindState* SoundPathImporter::state(SoundKind kind) noexcept {
  switch (kind) {
    case SoundKind::kEffect:
      return &kinds_[0];
    case SoundKind::kSpeech:
      return &kinds_[1];
  }
  return nullptr;
}

void SoundPathImporter::recordSource(KindState& state, SoundHandle sound, std::string_view source) {
  if (sources_ != nullptr && state.source_spellings.emplace(source).second)
    sources_->push_back({sound, std::string(source)});
}

SoundPathImportError SoundPathImporter::import(SoundKind kind, std::string_view source, SoundHandle& out) {
  assert(!finished_);
  KindState* kind_state = state(kind);
  if (kind_state == nullptr) return SoundPathImportError::kBadKind;
  if (const auto existing = kind_state->identities.find(source); existing != kind_state->identities.end()) {
    out = existing->second;
    recordSource(*kind_state, out, source);
    return SoundPathImportError::kNone;
  }
  const std::size_t index = kind_state->base_count + kind_state->paths.size();
  if (index >= static_cast<std::size_t>(kNoSound)) return SoundPathImportError::kTooManySounds;
  if (soundHandle(kind, static_cast<SoundIndex>(index), out) != ARX_OK) return SoundPathImportError::kBadKind;
  auto [entry, inserted] = kind_state->identities.emplace(std::string(source), out);
  assert(inserted);
  (void)inserted;
  kind_state->paths.push_back(entry->first);
  kind_state->original_paths.push_back(&entry->first);
  recordSource(*kind_state, out, source);
  return SoundPathImportError::kNone;
}

SoundPathImportError SoundPathImporter::repair(SoundKind kind, KindState& state) {
  ResourcePathUniquifier paths;
  paths.reserve(state.paths.size(), state.base_count);
  for (std::size_t index = 0; index < state.base_count; ++index) {
    SoundHandle handle = kNoSoundHandle;
    if (soundHandle(kind, static_cast<SoundIndex>(index), handle) != ARX_OK ||
        paths.occupy(sounds::path(sounds_, handle)) != ResourcePathError::kNone)
      return SoundPathImportError::kBadPath;
  }
  for (std::string& path : state.paths) paths.add(path);
  std::vector<ResourcePathRepair> repairs(state.paths.size());
  if (paths.apply(nullptr, repairs) != ResourcePathError::kNone) return SoundPathImportError::kBadPath;
  for (std::size_t index = 0; index < repairs.size(); ++index) {
    if (!hasStructuralResourcePathRepair(repairs[index])) continue;
    log(ARX_LOG_WARN,
        "{}: sound path '{}' normalized to '{}'",
        log_prefix_,
        *state.original_paths[index],
        state.paths[index]);
  }
  return SoundPathImportError::kNone;
}

SoundPathImportError SoundPathImporter::finish() {
  assert(!finished_);
  if (finished_) return SoundPathImportError::kBadPath;
  SoundPathImportError error = repair(SoundKind::kEffect, kinds_[0]);
  if (error != SoundPathImportError::kNone) return error;
  error = repair(SoundKind::kSpeech, kinds_[1]);
  if (error != SoundPathImportError::kNone) return error;
  for (std::string& path : kinds_[0].paths) (void)sounds::addPath(sounds_, SoundKind::kEffect, std::move(path));
  for (std::string& path : kinds_[1].paths) (void)sounds::addPath(sounds_, SoundKind::kSpeech, std::move(path));
  finished_ = true;
  return SoundPathImportError::kNone;
}

}  // namespace pistoris::glb
