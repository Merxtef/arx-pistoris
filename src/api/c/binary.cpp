// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/binary.h"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.hpp"

#include <cstdint>
#include <span>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_binary_validate_encoded_audio(ArxEncodedAudioView encoded_audio) noexcept {
  if (!encoded_audio.data && encoded_audio.size != 0) return ARX_INVALID_DATA_POINTER;
  return pistoris::binary::validateEncodedAudio(std::span<const std::uint8_t>(encoded_audio.data, encoded_audio.size));
}

ArxReturnCode arx_pistoris_binary_inspect_encoded_audio(ArxEncodedAudioView encoded_audio,
                                                        ArxAudioInfo* out_info) noexcept {
  if (!out_info || (!encoded_audio.data && encoded_audio.size != 0)) return ARX_INVALID_DATA_POINTER;
  *out_info = {};
  return pistoris::binary::inspectEncodedAudio(std::span<const std::uint8_t>(encoded_audio.data, encoded_audio.size),
                                               *out_info);
}

ArxReturnCode arx_pistoris_binary_validate_encoded_image(ArxEncodedImageView encoded_image) noexcept {
  if (!encoded_image.data && encoded_image.size != 0) return ARX_INVALID_DATA_POINTER;
  return pistoris::binary::validateEncodedImage(std::span<const std::uint8_t>(encoded_image.data, encoded_image.size));
}

ArxReturnCode arx_pistoris_binary_inspect_encoded_image(ArxEncodedImageView encoded_image,
                                                        ArxImageInfo* out_info) noexcept {
  if (!out_info || (!encoded_image.data && encoded_image.size != 0)) return ARX_INVALID_DATA_POINTER;
  *out_info = {};
  return pistoris::binary::inspectEncodedImage(std::span<const std::uint8_t>(encoded_image.data, encoded_image.size),
                                               *out_info);
}

// NOLINTEND(readability-identifier-naming)
