// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"

#include "formats/classification.h"

#include <span>
#include <vector>

namespace cli {

class IoService;

bool appendClassifiedInput(std::string path, PathLocation location, std::vector<std::uint8_t> buffer,
                           std::size_t positional_index, ArxResourceKind resource_kind,
                           std::vector<ClassifiedPath>& out);
bool loadClassifiedInputs(std::span<const char* const> arguments, IoService& io, std::vector<ClassifiedPath>& out);

}  // namespace cli
