// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "pipeline/options.h"
#include "resources/texture_io.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

class IoService;

struct ModelMaterialLibraryInput {
  std::string path;
  std::vector<std::uint8_t> data;
};

bool readModelMaterialLibraries(const ClassifiedPath& input, IoService& io, DiagnosticCode failure_code,
                                std::string_view owner, std::vector<ModelMaterialLibraryInput>& out);
bool resolveModelTextureInput(const ClassifiedPath& input, const TextureIoOptions& options, IoService& io,
                              std::string_view owner, TextureInput& out);

}  // namespace cli
