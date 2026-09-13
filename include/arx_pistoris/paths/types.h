// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_PATHS_TYPES_H
#define ARX_PISTORIS_PATHS_TYPES_H

#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef uint8_t ArxResourceKind;
enum {
  ARX_RESOURCE_KIND_NONE = 0,
  ARX_RESOURCE_KIND_LEVEL = 1,
  ARX_RESOURCE_KIND_MODEL = 2,
  ARX_RESOURCE_KIND_ANIMATION = 3,
  ARX_RESOURCE_KIND_CINEMATIC = 4,
  ARX_RESOURCE_KIND_AMBIANCE = 5,
};

typedef uint8_t ArxEntityClassKind;
enum {
  ARX_ENTITY_CLASS_KIND_UNKNOWN = 0,
  ARX_ENTITY_CLASS_KIND_ITEM,
  ARX_ENTITY_CLASS_KIND_NPC,
  ARX_ENTITY_CLASS_KIND_FIX,
  ARX_ENTITY_CLASS_KIND_CAMERA,
  ARX_ENTITY_CLASS_KIND_MARKER,
};

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_PATHS_TYPES_H */
