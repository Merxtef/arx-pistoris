// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/resource_io/status.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime.hpp"

// NOLINTBEGIN(readability-identifier-naming)

const char* arx_pistoris_resource_io_strerror(ArxReturnCode rc) noexcept {
  switch (rc) {
    case ARX_RESOURCE_IO_INVALID_PATH:
      return "Resource I/O: invalid logical path";
    case ARX_RESOURCE_IO_INVALID_MOUNT:
      return "Resource I/O: invalid mount";
    case ARX_RESOURCE_IO_TOO_MANY_MOUNTS:
      return "Resource I/O: too many read mounts";
    case ARX_RESOURCE_IO_NOT_FOUND:
      return "Resource I/O: resource not found";
    case ARX_RESOURCE_IO_STAT_FAILED:
      return "Resource I/O: filesystem inspection failed";
    case ARX_RESOURCE_IO_OPEN_FAILED:
      return "Resource I/O: file could not be opened";
    case ARX_RESOURCE_IO_READ_FAILED:
      return "Resource I/O: file could not be read";
    case ARX_RESOURCE_IO_WRITE_FAILED:
      return "Resource I/O: file could not be written";
    case ARX_RESOURCE_IO_AMBIGUOUS_PATH:
      return "Resource I/O: ambiguous path";
    case ARX_RESOURCE_IO_INVALID_METADATA:
      return "Resource I/O: invalid resource metadata";
    case ARX_RESOURCE_IO_DECISION_REQUIRED:
      return "Resource I/O: write decision required";
    default:
      return pistoris::errorString(rc);
  }
}

// NOLINTEND(readability-identifier-naming)
