// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace cli::media {

struct PreparedImage {
  std::vector<std::uint8_t> encoded;
  ArxImageInfo info{};
};

struct PreparedAudio {
  std::vector<std::uint8_t> encoded;
  ArxAudioInfo info{};
};

ArxReturnCode prepareImage(std::vector<std::uint8_t> encoded, PreparedImage& out) noexcept;
ArxReturnCode prepareAudio(std::vector<std::uint8_t> encoded, PreparedAudio& out) noexcept;

std::span<const std::string_view> imageLookupExtensions() noexcept;
std::span<const std::string_view> audioLookupExtensions() noexcept;
std::string_view imageExtension(ArxImageFormat format) noexcept;
std::string_view audioExtension(ArxAudioFormat format) noexcept;

}  // namespace cli::media
