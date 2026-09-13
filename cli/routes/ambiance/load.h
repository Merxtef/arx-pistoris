// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/ambiance/invocation.h"
#include "routes/ambiance/options.h"
#include "routes/ambiance/state.h"
#include "routes/types.h"

#include <vector>

namespace cli::ambiance {

struct InputConverterDescriptor {
  using NativeLoader = bool (*)(const ClassifiedPath& input, NativeAmbiance& out);
  using IntermediateLoader = bool (*)(const ClassifiedPath& input, const AmbianceOptions& options,
                                      IntermediateAmbiance& out);

  NativeLoader load_native = nullptr;
  IntermediateLoader load_intermediate = nullptr;
};

const InputConverterDescriptor* inputConverterDescriptor(Route route);
bool loadInput(const InputConverterDescriptor& converter, const std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, bool native, AmbianceInput& out);

}  // namespace cli::ambiance
