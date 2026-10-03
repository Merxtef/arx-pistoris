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

namespace cli::level {

struct InputConverterDescriptor {
  struct DecodedDlfInput {
    pistoris::Dlf dlf;
    std::optional<pistoris::Llf> embedded_lighting;
  };

  using NativeLoader = bool (*)(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                                DecodedDlfInput* decoded_dlf, NativeLevelFiles& out);
  using IntermediateLoader = bool (*)(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                                      const LevelOptions& options, DecodedDlfInput* decoded_dlf,
                                      IntermediateLevel& out);

  NativeLoader load_native = nullptr;
  IntermediateLoader load_intermediate = nullptr;
};

const InputConverterDescriptor* inputConverterDescriptor(Route route);
bool loadInput(const InputConverterDescriptor& converter, const std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, const LevelOptions& options,
               InputConverterDescriptor::DecodedDlfInput* decoded_dlf, bool native, LevelInput& out);

}  // namespace cli::level
