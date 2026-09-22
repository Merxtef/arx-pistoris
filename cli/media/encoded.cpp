// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "media/encoded.h"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace cli::media {
namespace {

constexpr std::array<std::string_view, 5> kImageLookupExtensions = {
    ".png",
    ".jpg",
    ".jpeg",
    ".bmp",
    ".tga",
};

constexpr std::array<std::string_view, 3> kAudioLookupExtensions = {
    ".wav",
    ".mp3",
    ".ogg",
};

}  // namespace

ArxReturnCode prepareImage(std::vector<std::uint8_t> encoded, PreparedImage& out) noexcept {
  PreparedImage prepared;
  const ArxReturnCode rc = pistoris::binary::inspectEncodedImage(encoded, prepared.info);
  if (rc != ARX_OK) return rc;
  prepared.encoded = std::move(encoded);
  out = std::move(prepared);
  return ARX_OK;
}

ArxReturnCode prepareAudio(std::vector<std::uint8_t> encoded, PreparedAudio& out) noexcept {
  PreparedAudio prepared;
  const ArxReturnCode rc = pistoris::binary::inspectEncodedAudio(encoded, prepared.info);
  if (rc != ARX_OK) return rc;
  prepared.encoded = std::move(encoded);
  out = std::move(prepared);
  return ARX_OK;
}

std::span<const std::string_view> imageLookupExtensions() noexcept { return kImageLookupExtensions; }

std::span<const std::string_view> audioLookupExtensions() noexcept { return kAudioLookupExtensions; }

std::string_view imageExtension(ArxImageFormat format) noexcept {
  switch (format) {
    case ARX_IMAGE_FORMAT_JPEG:
      return ".jpg";
    case ARX_IMAGE_FORMAT_PNG:
      return ".png";
    case ARX_IMAGE_FORMAT_BMP:
      return ".bmp";
    case ARX_IMAGE_FORMAT_TGA:
      return ".tga";
    default:
      return {};
  }
}

std::string_view audioExtension(ArxAudioFormat format) noexcept {
  switch (format) {
    case ARX_AUDIO_FORMAT_WAV:
      return ".wav";
    case ARX_AUDIO_FORMAT_MP3:
      return ".mp3";
    case ARX_AUDIO_FORMAT_OGG_VORBIS:
      return ".ogg";
    default:
      return {};
  }
}

}  // namespace cli::media
