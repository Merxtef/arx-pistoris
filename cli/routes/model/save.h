// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/execution_context.h"
#include "routes/model/invocation.h"
#include "routes/model/state.h"

namespace cli::model {

struct OutputConverterDescriptor {
  using NativeWriter = bool (*)(NativeModelFiles& files, const ExecutionContext& execution,
                                const Invocation& invocation);
  using IntermediateWriter = bool (*)(IntermediateModel& model, const ExecutionContext& execution,
                                      const Invocation& invocation);

  NativeWriter write_native = nullptr;
  IntermediateWriter write_intermediate = nullptr;
  bool uses_sound_files = false;
};

const OutputConverterDescriptor* outputConverterDescriptor(Format output, const Module* module);
bool writeNativeOutput(NativeModelFiles& files, const ExecutionContext& execution, const Invocation& invocation);
bool writeIntermediateOutput(IntermediateModel& model, const ExecutionContext& execution, const Invocation& invocation);

}  // namespace cli::model
