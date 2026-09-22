// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_BINARY_H
#define ARX_PISTORIS_BINARY_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/buffer.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"

#include <stddef.h>

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef enum ArxTextEncoding {
  ARX_TEXT_ENCODING_ASCII = 0,
  ARX_TEXT_ENCODING_UTF8 = 1,
  ARX_TEXT_ENCODING_LATIN1 = 2,
} ArxTextEncoding;

ARX_EXTERN_C_BEGIN

// --- Text encoding ---

ARX_API ArxReturnCode arx_pistoris_binary_classify_text_encoding(ArxStringView text,
                                                                 ArxTextEncoding* out_encoding) ARX_NOEXCEPT;
/* Successful output is NUL-terminated, sized without the terminator, and freed
 * with arx_pistoris_free_string. */
ARX_API ArxReturnCode arx_pistoris_binary_latin1_to_utf8(ArxStringView input, char** out_text,
                                                         size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_binary_utf8_to_latin1(ArxStringView input, char** out_text,
                                                         size_t* out_size) ARX_NOEXCEPT;

// --- Encoded media ---

ARX_API ArxReturnCode arx_pistoris_binary_validate_encoded_audio(ArxEncodedAudioView encoded_audio) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_binary_inspect_encoded_audio(ArxEncodedAudioView encoded_audio,
                                                                ArxAudioInfo* out_info) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_binary_validate_encoded_image(ArxEncodedImageView encoded_image) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_binary_inspect_encoded_image(ArxEncodedImageView encoded_image,
                                                                ArxImageInfo* out_info) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_BINARY_H */
