// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/output.h"

#include "io/service.h"
#include "resources/selector.h"

#include <cstddef>

namespace cli {

bool writeOutput(IoService& io, const OutputTarget& target, const void* data, std::size_t size) {
  return io.writePath(target, data, size);
}

}  // namespace cli
