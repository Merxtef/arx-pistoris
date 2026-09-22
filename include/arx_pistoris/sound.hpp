// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pistoris {

enum class SoundKind : std::uint8_t {
  kEffect,
  kSpeech,
};

// SoundHandle invalidates together with SoundIndex
[[nodiscard]] ArxReturnCode soundHandle(SoundKind kind, SoundIndex index, SoundHandle& out) noexcept;
[[nodiscard]] ArxReturnCode soundHandleKind(SoundHandle handle, SoundKind& out) noexcept;
[[nodiscard]] ArxReturnCode soundHandleIndex(SoundHandle handle, SoundIndex& out) noexcept;

struct SoundFile {
  SoundIndex source_sound = kNoSound;
  std::string path;
  std::vector<std::uint8_t> encoded_audio;
};

struct SoundSourceReference {
  SoundIndex sound = kNoSound;
  std::string path;
};

struct AnimationSoundFile {
  std::size_t animation_index = 0;
  SoundFile file;
};

struct AnimationSoundSourceReference {
  std::size_t animation_index = 0;
  SoundSourceReference reference;
};

}  // namespace pistoris
