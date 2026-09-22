// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/binary.h"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/binary.hpp"

#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace {

using TextConversion = ArxReturnCode (*)(std::string_view, std::string&) noexcept;

constexpr ArxTextEncoding publicTextEncoding(pistoris::binary::TextEncoding encoding) noexcept {
  switch (encoding) {
    case pistoris::binary::TextEncoding::kAscii:
      return ARX_TEXT_ENCODING_ASCII;
    case pistoris::binary::TextEncoding::kUtf8:
      return ARX_TEXT_ENCODING_UTF8;
    case pistoris::binary::TextEncoding::kLatin1:
      return ARX_TEXT_ENCODING_LATIN1;
  }
  return ARX_TEXT_ENCODING_LATIN1;
}

ArxReturnCode convertText(ArxStringView input, char** out_text, std::size_t* out_size,
                          TextConversion conversion) noexcept {
  if (!pistoris::c_api::valid(input) || !out_text || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_text = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::string converted;
    const ArxReturnCode rc = conversion(pistoris::c_api::stringView(input), converted);
    if (rc != ARX_OK) return rc;
    const ArxReturnCode publish_rc = pistoris::c_api::publishString(converted, out_text);
    if (publish_rc == ARX_OK) *out_size = converted.size();
    return publish_rc;
  });
}

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_binary_classify_text_encoding(ArxStringView text, ArxTextEncoding* out_encoding) noexcept {
  if (!pistoris::c_api::valid(text) || !out_encoding) return ARX_INVALID_DATA_POINTER;
  *out_encoding = publicTextEncoding(pistoris::binary::classifyTextEncoding(pistoris::c_api::stringView(text)));
  return ARX_OK;
}

ArxReturnCode arx_pistoris_binary_latin1_to_utf8(ArxStringView input, char** out_text, std::size_t* out_size) noexcept {
  return convertText(input, out_text, out_size, pistoris::binary::latin1ToUtf8);
}

ArxReturnCode arx_pistoris_binary_utf8_to_latin1(ArxStringView input, char** out_text, std::size_t* out_size) noexcept {
  return convertText(input, out_text, out_size, pistoris::binary::utf8ToLatin1);
}

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
