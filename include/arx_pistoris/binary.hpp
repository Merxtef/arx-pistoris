// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pistoris::binary {

// --- Text encoding ---

enum class TextEncoding : std::uint8_t {
  kAscii,
  kUtf8,
  kLatin1,
};

[[nodiscard]] TextEncoding classifyTextEncoding(std::string_view text) noexcept;
[[nodiscard]] ArxReturnCode latin1ToUtf8(std::string_view input, std::string& out) noexcept;
[[nodiscard]] ArxReturnCode utf8ToLatin1(std::string_view input, std::string& out) noexcept;

// --- Encoded media ---

[[nodiscard]] ArxReturnCode validateEncodedAudio(std::span<const std::uint8_t> encoded_audio) noexcept;
[[nodiscard]] ArxReturnCode inspectEncodedAudio(std::span<const std::uint8_t> encoded_audio,
                                                ArxAudioInfo& out) noexcept;
[[nodiscard]] ArxReturnCode validateEncodedImage(std::span<const std::uint8_t> encoded_image) noexcept;
[[nodiscard]] ArxReturnCode inspectEncodedImage(std::span<const std::uint8_t> encoded_image,
                                                ArxImageInfo& out) noexcept;

}  // namespace pistoris::binary
