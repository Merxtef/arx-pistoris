// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/level/invocation.h"
#include "routes/level/options.h"
#include "routes/level/state.h"
#include "routes/types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace cli {
class IoService;
}

namespace cli::level {

struct InputConverterDescriptor {
  using NativeLoader = bool (*)(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, IoService& io,
                                NativeLevelFiles& out);
  using IntermediateLoader = bool (*)(std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                                      const LevelOptions& options, IoService& io, IntermediateLevel& out);

  NativeLoader load_native = nullptr;
  IntermediateLoader load_intermediate = nullptr;
};

const InputConverterDescriptor* inputConverterDescriptor(Route route);
bool loadInput(const InputConverterDescriptor& converter, std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, const LevelOptions& options, IoService& io, bool native, LevelInput& out);

}  // namespace cli::level
