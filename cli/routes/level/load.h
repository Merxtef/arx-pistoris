// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/level/invocation.h"
#include "routes/level/options.h"
#include "routes/level/state.h"
#include "routes/types.h"

#include <cstdint>
#include <vector>

namespace cli::level {

struct InputConverterDescriptor {
  using NativeLoader = bool (*)(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                                NativeLevelFiles& out);
  using IntermediateLoader = bool (*)(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                                      const LevelOptions& options, IntermediateLevel& out);

  NativeLoader load_native = nullptr;
  IntermediateLoader load_intermediate = nullptr;
};

const InputConverterDescriptor* inputConverterDescriptor(Route route);
bool loadInput(const InputConverterDescriptor& converter, const std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, const LevelOptions& options, bool native, LevelInput& out);

}  // namespace cli::level
