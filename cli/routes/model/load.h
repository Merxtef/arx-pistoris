// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/text.hpp"

#include "routes/model/invocation.h"
#include "routes/model/options.h"
#include "routes/model/state.h"
#include "routes/types.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace cli::model {

struct InputConverterDescriptor {
  using NativeLoader = bool (*)(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                                NativeModelFiles& out);
  using IntermediateLoader = bool (*)(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                                      const ModelOptions& options, IntermediateModel& out);

  NativeLoader load_native = nullptr;
  IntermediateLoader load_intermediate = nullptr;
};

const InputConverterDescriptor* inputConverterDescriptor(Route route);
bool loadInput(const InputConverterDescriptor& converter, const std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, const ModelOptions& options, bool native, ModelInput& out);
bool loadReferenceModel(std::span<const std::uint8_t> data, std::string_view path, pistoris::NativeTextMode text_mode,
                        IntermediateModel& out);

}  // namespace cli::model
