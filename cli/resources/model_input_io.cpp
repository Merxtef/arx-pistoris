// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/model_input_io.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/service.h"
#include "pipeline/options.h"
#include "resources/layout.h"
#include "resources/read_diagnostics.h"
#include "resources/selector.h"
#include "resources/sidecar_io.h"
#include "resources/texture_io.h"

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli {

bool readModelMaterialLibraries(const ClassifiedPath& input, IoService& io, DiagnosticCode failure_code,
                                std::string_view owner, std::vector<ModelMaterialLibraryInput>& out) {
  if (input.facts.format != Format::kObj) {
    out.clear();
    return true;
  }

  std::vector<std::string> paths;
  const ArxReturnCode rc = pistoris::objMaterialLibraryPaths(byteStringView(input.buffer), paths);
  if (rc != ARX_OK) {
    diagnostic(failure_code,
               "Cannot inspect MTL references for %.*s '%s': %s (code %d)",
               static_cast<int>(owner.size()),
               owner.data(),
               input.path.c_str(),
               pistoris::errorString(rc),
               static_cast<int>(rc));
    return false;
  }

  PathLocation parent;
  std::string error;
  if (!io.parentPathLocation(input.location, parent, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid,
               "Cannot resolve MTL base for %.*s '%s': %s",
               static_cast<int>(owner.size()),
               owner.data(),
               input.path.c_str(),
               error.c_str());
    return false;
  }

  std::vector<ModelMaterialLibraryInput> result;
  result.reserve(paths.size());
  for (std::string& path : paths) {
    PathLocation location;
    if (!io.appendPathLocation(parent, path, location, error)) {
      diagnostic(DiagnosticCode::kIoPathInvalid,
                 "Cannot resolve MTL '%s' for %.*s: %s",
                 path.c_str(),
                 static_cast<int>(owner.size()),
                 owner.data(),
                 error.c_str());
      return false;
    }
    ModelMaterialLibraryInput library{.path = std::move(path), .data = {}};
    std::string resolved_path;
    const ResourceReadResult read_result = io.readPath(location, library.data, &resolved_path);
    if (read_result != ResourceReadResult::kSuccess) {
      std::string description(owner);
      description += " MTL";
      reportRequiredReadFailure(read_result, description, location.path);
      return false;
    }
    log(ARX_LOG_INFO, "using MTL for %.*s: %s", static_cast<int>(owner.size()), owner.data(), resolved_path.c_str());
    result.push_back(std::move(library));
  }

  out = std::move(result);
  return true;
}

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
