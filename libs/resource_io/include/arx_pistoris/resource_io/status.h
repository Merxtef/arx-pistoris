// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_RESOURCE_IO_STATUS_H
#define ARX_PISTORIS_RESOURCE_IO_STATUS_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/status.h"

// Public C ABI naming
// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

/* Resource I/O owns return codes from -1000 through -1. */
enum {
  ARX_RESOURCE_IO_INVALID_PATH = -1000,
  ARX_RESOURCE_IO_INVALID_MOUNT,
  ARX_RESOURCE_IO_TOO_MANY_MOUNTS,
  ARX_RESOURCE_IO_NOT_FOUND,
  ARX_RESOURCE_IO_STAT_FAILED,
  ARX_RESOURCE_IO_OPEN_FAILED,
  ARX_RESOURCE_IO_READ_FAILED,
  ARX_RESOURCE_IO_WRITE_FAILED,
  ARX_RESOURCE_IO_AMBIGUOUS_PATH,
  ARX_RESOURCE_IO_INVALID_METADATA,
  ARX_RESOURCE_IO_DECISION_REQUIRED,
};

ARX_EXTERN_C_BEGIN

/* Describes Resource I/O codes and delegated non-negative core codes. */
const char* arx_pistoris_resource_io_strerror(ArxReturnCode rc) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_RESOURCE_IO_STATUS_H */
