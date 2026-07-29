// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "pipeline/executor.h"

#include "io/service.h"
#include "pipeline/execution_context.h"
#include "pipeline/parsed.h"
#include "pipeline/resolver.h"

namespace cli {

int executeResolved(const ParsedCli&, CliResolution& resolved, IoService& io) {
  if (!resolved.route_descriptor || !resolved.route_descriptor->execute || !resolved.route_invocation) return 1;
  ExecutionContext common{resolved.output, io, resolved.inputs, resolved.route, resolved.effective_modules};
  return resolved.route_descriptor->execute(common, *resolved.route_invocation);
}

}  // namespace cli
