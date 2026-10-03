// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/execution_context.h"
#include "routes/ambiance/invocation.h"
#include "routes/ambiance/state.h"

namespace cli::ambiance {

struct OutputConverterDescriptor {
  using NativeWriter = bool (*)(NativeAmbiance& source, const ExecutionContext& execution,
                                const Invocation& invocation);
  using IntermediateWriter = bool (*)(IntermediateAmbiance& source, const ExecutionContext& execution,
                                      const Invocation& invocation);

  NativeWriter write_native = nullptr;
  IntermediateWriter write_intermediate = nullptr;
};

const OutputConverterDescriptor* outputConverterDescriptor(Format output);
bool writeNativeOutput(NativeAmbiance& source, const ExecutionContext& execution, const Invocation& invocation);
bool writeIntermediateOutput(IntermediateAmbiance& source, const ExecutionContext& execution,
                             const Invocation& invocation);

}  // namespace cli::ambiance
