// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_BINARY_H
#define ARX_PISTORIS_BINARY_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"

// NOLINTBEGIN(readability-identifier-naming)

ARX_EXTERN_C_BEGIN

ARX_API ArxReturnCode arx_pistoris_binary_validate_encoded_audio(ArxEncodedAudioView encoded_audio) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_binary_inspect_encoded_audio(ArxEncodedAudioView encoded_audio,
                                                                ArxAudioInfo* out_info) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_binary_validate_encoded_image(ArxEncodedImageView encoded_image) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_binary_inspect_encoded_image(ArxEncodedImageView encoded_image,
                                                                ArxImageInfo* out_info) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_BINARY_H */
