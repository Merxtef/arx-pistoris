// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/classification.h"

#include <span>
#include <vector>

namespace cli {

class IoService;

bool loadClassifiedInputs(std::span<const char* const> arguments, IoService& io, std::vector<ClassifiedPath>& out);

}  // namespace cli
