// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "io/path_location.h"

#include <cstddef>

namespace cli {

class IoService;

bool writeOutput(IoService& io, const PathLocation& target, const void* data, std::size_t size);

}  // namespace cli
