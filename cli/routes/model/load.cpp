// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/load.h"

#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/files.h"
#include "io/paths.h"
#include "routes/model/invocation.h"
#include "routes/model/state.h"
#include "routes/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli::model {
namespace {

std::string_view textView(const std::vector<std::uint8_t>& buffer) {
  return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
}

}  // namespace

bool loadTeaFile(const ClassifiedPath& input, pistoris::Tea& out) {
  ArxReturnCode rc = ARX_OK;
  switch (input.facts.format) {
    case Format::kTea:
      rc = pistoris::readTea(input.buffer, out);
      break;
    case Format::kJson:
      rc = pistoris::importJson(textView(input.buffer), out);
      break;
    default:
      diagnostic(DiagnosticCode::kModelUnsupportedExtra, "Unsupported extra animation format: %s", input.path.c_str());
      return false;
  }
  if (rc != ARX_OK) {
    diagnostic(DiagnosticCode::kModelInputFailed,
               "TEA parse failed (%s): %s (code %d)",
               input.path.c_str(),
               pistoris::errorString(rc),
               static_cast<int>(rc));
    return false;
  }
  return true;
}

// TODO: Route reference and sidecar discovery through IoService when Model and Animation become intermediate classes
bool loadReferenceFtl(const char* path, Context& ctx) {
  std::vector<std::uint8_t> reference_buffer;
  if (!readFile(path, reference_buffer)) return false;

  pistoris::Ftl reference;
  ArxReturnCode rc = pistoris::readFtl(reference_buffer, reference);
  if (rc != ARX_OK) {
    diagnostic(DiagnosticCode::kModelInputFailed,
               "Reference FTL failed: %s (code %d)",
               pistoris::errorString(rc),
               static_cast<int>(rc));
    return false;
  }

  log(ARX_LOG_INFO, "using reference FTL: %s", path);
  ctx.reference_ftl = std::move(reference);
  ctx.has_reference = true;
  return true;
}

bool loadInput(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, Route route, Context& ctx) {
  const ClassifiedPath& input = inputs[invocation.input];
  ArxReturnCode rc = ARX_OK;
  switch (route.input) {
    case Format::kFtl: {
      pistoris::Ftl ftl;
      rc = pistoris::readFtl(input.buffer, ftl);
      if (rc != ARX_OK) break;
      ctx.ftl = std::move(ftl);
      return true;
    }

    case Format::kObj: {
      std::string mtl_path(input.path.c_str(), fileExtension(input.path.c_str()));
      mtl_path += ".mtl";

      std::vector<std::uint8_t> mtl_buffer;
      bool has_mtl = readFileOptional(mtl_path.c_str(), mtl_buffer);
      if (has_mtl) log(ARX_LOG_INFO, "using MTL: %s", mtl_path.c_str());

      pistoris::Ftl ftl;
      rc = pistoris::importObj(textView(input.buffer),
                               has_mtl ? textView(mtl_buffer) : std::string_view{},
                               pathFilename(input.path.c_str()),
                               ftl);
      if (rc != ARX_OK) break;
      ctx.ftl = std::move(ftl);
      return true;
    }

    case Format::kJson: {
      pistoris::Ftl ftl;
      rc = pistoris::importJson(textView(input.buffer), ftl);
      if (rc != ARX_OK) break;
      ctx.ftl = std::move(ftl);
      return true;
    }

    case Format::kGlb: {
      pistoris::Ftl ftl;
      std::vector<pistoris::Tea> teas;
      rc = pistoris::importGlb(input.buffer, input.path, ftl, teas);
      if (rc != ARX_OK) break;
      ctx.ftl = std::move(ftl);
      ctx.teas = std::move(teas);
      return true;
    }

    default:
      diagnostic(DiagnosticCode::kModelUnsupportedInput, "Unsupported input format: %s", input.path.c_str());
      return false;
  }
  diagnostic(DiagnosticCode::kModelInputFailed,
             "%s input failed: %s (code %d)",
             formatName(route.input),
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool loadExtras(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, Context& ctx) {
  for (std::size_t index : invocation.extras) {
    pistoris::Tea tea;
    if (!loadTeaFile(inputs[index], tea)) return false;
    ctx.teas.push_back(std::move(tea));
  }
  return true;
}

bool validateTeaCompatibility(const Context& ctx) {
  std::size_t group_count = ctx.ftl.groups.size();
  for (std::size_t index = 0; index < ctx.teas.size(); ++index) {
    const pistoris::Tea& tea = ctx.teas[index];
    if (tea.num_groups == static_cast<int32_t>(group_count)) continue;

    const char* name = tea.name[0] != '\0' ? tea.name : "<unnamed>";
    diagnostic(DiagnosticCode::kModelTeaMismatch,
               "TEA group count mismatch (%s, index %zu): animation=%d FTL=%zu",
               name,
               index,
               tea.num_groups,
               group_count);
    return false;
  }
  return true;
}

}  // namespace cli::model
