// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/classification.h"
#include "pipeline/options.h"
#include "resources/texture_io.h"

#include <string_view>

namespace cli {

class IoService;

bool resolveModelTextureInput(const ClassifiedPath& input, const TextureIoOptions& options, IoService& io,
                              std::string_view owner, TextureInput& out);

}  // namespace cli
