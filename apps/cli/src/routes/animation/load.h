// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/animation/invocation.h"
#include "routes/animation/state.h"
#include "routes/types.h"

#include <vector>

namespace cli {
class IoService;
}

namespace cli::animation {

struct InputConverterDescriptor {
  using NativeLoader = bool (*)(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, IoService& io,
                                NativeAnimation& out);
  using IntermediateLoader = bool (*)(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, IoService& io,
                                      IntermediateAnimation& out);

  NativeLoader load_native = nullptr;
  IntermediateLoader load_intermediate = nullptr;
};

const InputConverterDescriptor* inputConverterDescriptor(Route route);
bool loadInput(const InputConverterDescriptor& converter, std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, IoService& io, bool native, AnimationInput& out);

}  // namespace cli::animation
