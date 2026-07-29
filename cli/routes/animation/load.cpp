// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/load.h"

#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "routes/animation/invocation.h"
#include "routes/animation/state.h"
#include "routes/types.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace cli::animation {
namespace {

std::string_view textView(const std::vector<std::uint8_t>& buffer) {
  return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
}

}  // namespace

bool loadInput(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, Route route, Context& ctx) {
  ctx.teas.reserve(invocation.inputs.size());
  for (std::size_t index : invocation.inputs) {
    const ClassifiedPath& input = inputs[index];
    pistoris::Tea tea;
    ArxReturnCode rc = ARX_OK;
    switch (route.input) {
      case Format::kTea:
        rc = pistoris::readTea(input.buffer, tea);
        break;
      case Format::kJson:
        rc = pistoris::importJson(textView(input.buffer), tea);
        break;
      default:
        diagnostic(DiagnosticCode::kAnimationUnsupportedInput, "Unsupported input format");
        return false;
    }
    if (rc != ARX_OK) {
      diagnostic(DiagnosticCode::kAnimationInputFailed,
                 "%s input failed (%s): %s (code %d)",
                 formatName(route.input),
                 input.path.c_str(),
                 pistoris::errorString(rc),
                 static_cast<int>(rc));
      return false;
    }
    ctx.teas.push_back(std::move(tea));
  }
  return true;
}

}  // namespace cli::animation
