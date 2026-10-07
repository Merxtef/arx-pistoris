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

namespace cli {
class IoService;
}

namespace cli::model {

struct InputConverterDescriptor {
  using NativeLoader = bool (*)(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, IoService& io,
                                NativeModelFiles& out);
  using IntermediateLoader = bool (*)(std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                                      const ModelOptions& options, IoService& io, IntermediateModel& out);

  NativeLoader load_native = nullptr;
  IntermediateLoader load_intermediate = nullptr;
};

const InputConverterDescriptor* inputConverterDescriptor(Route route);
bool loadInput(const InputConverterDescriptor& converter, std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, const ModelOptions& options, IoService& io, bool native, ModelInput& out);
bool loadReferenceModel(std::span<const std::uint8_t> data, std::string_view path, pistoris::NativeTextMode text_mode,
                        IntermediateModel& out);

}  // namespace cli::model
