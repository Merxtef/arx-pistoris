// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/execution_context.h"
#include "pipeline/options.h"
#include "routes/level/invocation.h"
#include "routes/level/operations.h"

namespace cli::level {

struct OutputConverterDescriptor {
  using NativeWriter = bool (*)(NativeLevelFiles& files, const ExecutionContext& execution,
                                const Invocation& invocation);
  using IntermediateWriter = bool (*)(IntermediateLevel& level, const ExecutionContext& execution,
                                      const Invocation& invocation,
                                      const operations::OperationDiagnostics& diagnostics);
  using DiagnosticFactory = operations::OperationDiagnostics (*)();

  NativeWriter write_native = nullptr;
  IntermediateWriter write_intermediate = nullptr;
  DiagnosticFactory create_diagnostics = nullptr;
  bool supports_model_previews = false;
};

const OutputConverterDescriptor* outputConverterDescriptor(Format output, const Module* module);
bool writeNativeOutput(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation);
bool writeIntermediateOutput(IntermediateLevel& level, const ExecutionContext& execution, const Invocation& invocation,
                             const operations::OperationDiagnostics& diagnostics);

}  // namespace cli::level
