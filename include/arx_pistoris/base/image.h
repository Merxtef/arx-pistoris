// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_BASE_IMAGE_H
#define ARX_PISTORIS_BASE_IMAGE_H

#include <stddef.h>
#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

/* Non-owning encoded image; {NULL, 0} empty */
typedef struct ArxEncodedImageView {
  const uint8_t* data;
  size_t size;
} ArxEncodedImageView;

typedef uint8_t ArxImageFormat;
enum {
  ARX_IMAGE_FORMAT_UNKNOWN = 0,
  ARX_IMAGE_FORMAT_JPEG,
  ARX_IMAGE_FORMAT_PNG,
  ARX_IMAGE_FORMAT_BMP,
  ARX_IMAGE_FORMAT_TGA,
};

typedef struct ArxImageInfo {
  ArxImageFormat format;
  uint32_t width;
  uint32_t height;
  uint8_t components;
} ArxImageInfo;

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_BASE_IMAGE_H */
