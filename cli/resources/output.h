// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "resources/selector.h"

#include <cstddef>

namespace cli {

class IoService;

bool writeOutput(IoService& io, const OutputTarget& target, const void* data, std::size_t size);

}  // namespace cli
