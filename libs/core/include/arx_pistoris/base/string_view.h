// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_BASE_STRING_VIEW_H
#define ARX_PISTORIS_BASE_STRING_VIEW_H

#include <stddef.h>

// NOLINTBEGIN(readability-identifier-naming)

/* Non-owning, not NUL-terminated; {NULL, 0} empty */
typedef struct ArxStringView {
  const char* data;
  size_t size;
} ArxStringView;

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_BASE_STRING_VIEW_H */
