// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/cinematic/invocation.h"
#include "routes/cinematic/state.h"
#include "routes/types.h"

#include <vector>

namespace cli {
class IoService;
}

namespace cli::cinematic {

struct InputConverterDescriptor {
  using NativeLoader = bool (*)(ClassifiedPath& input, const Invocation& invocation, IoService& io,
                                NativeCinematic& out);
  using IntermediateLoader = bool (*)(ClassifiedPath& input, const Invocation& invocation, IoService& io,
                                      IntermediateCinematic& out);

  NativeLoader load_native = nullptr;
  IntermediateLoader load_intermediate = nullptr;
};

const InputConverterDescriptor* inputConverterDescriptor(Route route);
bool loadInput(const InputConverterDescriptor& converter, std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, IoService& io, bool native, CinematicInput& out);

}  // namespace cli::cinematic
