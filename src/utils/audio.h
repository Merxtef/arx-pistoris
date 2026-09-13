// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pistoris::audio {

inline constexpr std::uint64_t kMaxDecodedBytes = 256ULL * 1024ULL * 1024ULL;

enum class Format : std::uint8_t {
  kUnknown,
  kWav,
  kMp3,
  kOggVorbis,
};

enum class Error : std::uint8_t {
  kNone,
  kMalformed,
  kUnsupportedChannels,
  kTooLarge,
  kOutOfMemory,
};

struct Info {
  Format format = Format::kUnknown;
  std::uint32_t channels = 0;
  std::uint32_t sample_rate = 0;
  std::uint64_t frame_count = 0;
};

Error inspect(std::span<const std::uint8_t> encoded, Info* out = nullptr) noexcept;
Error validate(std::span<const std::uint8_t> encoded, Info* out = nullptr) noexcept;
Error transcodeToPcm16Wav(std::span<const std::uint8_t> encoded, bool mono, std::vector<std::uint8_t>& out,
                          Info* out_source = nullptr);
Error transcodeToPcm16WavVariants(std::span<const std::uint8_t> encoded, bool include_preserved, bool include_mono,
                                  std::vector<std::uint8_t>* out_preserved, std::vector<std::uint8_t>* out_mono,
                                  Info* out_source = nullptr);

}  // namespace pistoris::audio
