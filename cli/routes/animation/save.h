// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/execution_context.h"
#include "routes/animation/invocation.h"
#include "routes/animation/state.h"

namespace cli::animation {

struct OutputConverterDescriptor {
  using NativeWriter = bool (*)(NativeAnimation& animation, const ExecutionContext& execution,
                                const Invocation& invocation);
  using IntermediateWriter = bool (*)(IntermediateAnimation& animation, const ExecutionContext& execution,
                                      const Invocation& invocation);

  NativeWriter write_native = nullptr;
  IntermediateWriter write_intermediate = nullptr;
};

const OutputConverterDescriptor* outputConverterDescriptor(Format output);
bool writeNativeOutput(NativeAnimation& animation, const ExecutionContext& execution, const Invocation& invocation);
bool writeIntermediateOutput(IntermediateAnimation& animation, const ExecutionContext& execution,
                             const Invocation& invocation);

}  // namespace cli::animation
