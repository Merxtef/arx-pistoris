// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_NATIVE_TEXT_H
#define ARX_PISTORIS_NATIVE_TEXT_H

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

/* Controls text at native carrier boundaries; semantic and JSON text are always UTF-8.
 * AUTO decodes valid UTF-8 and falls back to Latin-1, but emits UTF-8. */
typedef enum ArxNativeTextMode {
  ARX_NATIVE_TEXT_AUTO = 0,
  ARX_NATIVE_TEXT_UTF8 = 1,
  ARX_NATIVE_TEXT_LATIN1 = 2,
} ArxNativeTextMode;

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif
