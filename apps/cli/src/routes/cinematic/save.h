// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/execution_context.h"
#include "routes/cinematic/invocation.h"
#include "routes/cinematic/state.h"

namespace cli::cinematic {

struct OutputConverterDescriptor {
  using NativeWriter = bool (*)(NativeCinematic& source, const ExecutionContext& execution,
                                const Invocation& invocation);
  using IntermediateWriter = bool (*)(IntermediateCinematic& source, const ExecutionContext& execution,
                                      const Invocation& invocation);

  NativeWriter write_native = nullptr;
  IntermediateWriter write_intermediate = nullptr;
};

const OutputConverterDescriptor* outputConverterDescriptor(Format output);
bool writeNativeOutput(NativeCinematic& source, const ExecutionContext& execution, const Invocation& invocation);
bool writeIntermediateOutput(IntermediateCinematic& source, const ExecutionContext& execution,
                             const Invocation& invocation);

}  // namespace cli::cinematic
