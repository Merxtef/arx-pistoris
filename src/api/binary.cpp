// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/binary.hpp"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"

#include "utils/audio.h"
#include "utils/encoded_image.h"

#include <cstdint>
#include <span>

namespace pistoris::binary {

namespace {

constexpr ArxAudioFormat publicAudioFormat(audio::Format format) noexcept {
  switch (format) {
    case audio::Format::kUnknown:
      return ARX_AUDIO_FORMAT_UNKNOWN;
    case audio::Format::kWav:
      return ARX_AUDIO_FORMAT_WAV;
    case audio::Format::kMp3:
      return ARX_AUDIO_FORMAT_MP3;
    case audio::Format::kOggVorbis:
      return ARX_AUDIO_FORMAT_OGG_VORBIS;
  }
  return ARX_AUDIO_FORMAT_UNKNOWN;
}

constexpr ArxImageFormat publicImageFormat(image::Format format) noexcept {
  switch (format) {
    case image::Format::kUnknown:
      return ARX_IMAGE_FORMAT_UNKNOWN;
    case image::Format::kJpeg:
      return ARX_IMAGE_FORMAT_JPEG;
    case image::Format::kPng:
      return ARX_IMAGE_FORMAT_PNG;
    case image::Format::kBmp:
      return ARX_IMAGE_FORMAT_BMP;
    case image::Format::kTga:
      return ARX_IMAGE_FORMAT_TGA;
  }
  return ARX_IMAGE_FORMAT_UNKNOWN;
}

}  // namespace

ArxReturnCode validateEncodedAudio(std::span<const std::uint8_t> encoded_audio) noexcept {
  switch (audio::validate(encoded_audio)) {
    case audio::Error::kNone:
      return ARX_OK;
    case audio::Error::kMalformed:
      return ARX_AUDIO_BAD_DATA;
    case audio::Error::kUnsupportedChannels:
      return ARX_AUDIO_UNSUPPORTED_CHANNELS;
    case audio::Error::kTooLarge:
      return ARX_AUDIO_TOO_LARGE;
    case audio::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
  }
  return ARX_AUDIO_BAD_DATA;
}

ArxReturnCode inspectEncodedAudio(std::span<const std::uint8_t> encoded_audio, ArxAudioInfo& out) noexcept {
  audio::Info info;
  const audio::Error error = audio::validate(encoded_audio, &info);
  switch (error) {
    case audio::Error::kNone:
      out = {
          .format = publicAudioFormat(info.format),
          .channels = info.channels,
          .sample_rate = info.sample_rate,
          .frame_count = info.frame_count,
      };
      return ARX_OK;
    case audio::Error::kMalformed:
      return ARX_AUDIO_BAD_DATA;
    case audio::Error::kUnsupportedChannels:
      return ARX_AUDIO_UNSUPPORTED_CHANNELS;
    case audio::Error::kTooLarge:
      return ARX_AUDIO_TOO_LARGE;
    case audio::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
  }
  return ARX_AUDIO_BAD_DATA;
}

ArxReturnCode validateEncodedImage(std::span<const std::uint8_t> encoded_image) noexcept {
  switch (image::inspect(encoded_image)) {
    case image::Error::kNone:
      return ARX_OK;
    case image::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
    case image::Error::kMalformed:
      return ARX_IMAGE_BAD_DATA;
  }
  return ARX_IMAGE_BAD_DATA;
}

ArxReturnCode inspectEncodedImage(std::span<const std::uint8_t> encoded_image, ArxImageInfo& out) noexcept {
  image::Info info;
  const image::Error error = image::inspect(encoded_image, &info);
  if (error == image::Error::kOutOfMemory) return ARX_BAD_ALLOC;
  if (error != image::Error::kNone) return ARX_IMAGE_BAD_DATA;
  out = {
      .format = publicImageFormat(info.format),
      .width = info.width,
      .height = info.height,
      .components = info.components,
  };
  return ARX_OK;
}

}  // namespace pistoris::binary
