// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_IMAGE_H
#define ARX_PISTORIS_IMAGE_H

#include <stddef.h>
#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming)

/* Encoded image views never own their bytes; {NULL, 0} is an empty view */
typedef struct ArxEncodedImageView {
  const uint8_t* data;
  size_t size;
} ArxEncodedImageView;

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_IMAGE_H */
