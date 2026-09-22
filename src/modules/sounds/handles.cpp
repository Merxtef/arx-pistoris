// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/sound.hpp"

#include <cstdint>
#include <limits>

namespace pistoris {
namespace {

constexpr std::uint64_t kKindShift = 32;
constexpr std::uint64_t kIndexMask = std::numeric_limits<std::uint32_t>::max();

bool validKind(SoundKind kind) noexcept { return kind == SoundKind::kEffect || kind == SoundKind::kSpeech; }

}  // namespace

ArxReturnCode soundHandle(SoundKind kind, SoundIndex index, SoundHandle& out) noexcept {
  out = kNoSoundHandle;
  if (!validKind(kind) || index == kNoSound) return ARX_INVALID_OPTIONS;
  out = (static_cast<SoundHandle>(kind) << kKindShift) | index;
  return ARX_OK;
}

ArxReturnCode soundHandleKind(SoundHandle handle, SoundKind& out) noexcept {
  const auto raw = static_cast<std::uint32_t>(handle >> kKindShift);
  if (handle == kNoSoundHandle || raw > static_cast<std::uint32_t>(SoundKind::kSpeech)) return ARX_INVALID_OPTIONS;
  out = static_cast<SoundKind>(raw);
  return ARX_OK;
}

ArxReturnCode soundHandleIndex(SoundHandle handle, SoundIndex& out) noexcept {
  out = kNoSound;
  SoundKind kind = SoundKind::kEffect;
  if (soundHandleKind(handle, kind) != ARX_OK) return ARX_INVALID_OPTIONS;
  out = static_cast<SoundIndex>(handle & kIndexMask);
  if (out == kNoSound) return ARX_INVALID_OPTIONS;
  return ARX_OK;
}

}  // namespace pistoris
