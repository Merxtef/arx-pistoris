// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/model_input_io.h"

#include "formats/classification.h"
#include "formats/format.h"
#include "io/service.h"
#include "pipeline/options.h"
#include "resources/layout.h"
#include "resources/sidecar_io.h"
#include "resources/texture_io.h"

#include <string_view>

namespace cli {

bool resolveModelTextureInput(const ClassifiedPath& input, const TextureIoOptions& options, IoService& io,
                              std::string_view owner, TextureInput& out) {
  const bool native = input.facts.format == Format::kFtl || input.facts.format == Format::kJson;
  out.use_format_sources = input.layout == ResourceLayout::kLoose;
  out.source_lookup = native ? ImageLookupMode::kGamePriority : ImageLookupMode::kExact;
  return resolveSidecarInputBase(input,
                                 out.use_format_sources,
                                 options.input_folder_specified,
                                 options.input_folder,
                                 owner,
                                 "texture",
                                 io,
                                 out.source_base);
}

}  // namespace cli
