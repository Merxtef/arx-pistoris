// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "pipeline/executor.h"

#include "io/service.h"
#include "pipeline/execution_context.h"
#include "pipeline/parsed.h"
#include "pipeline/resolver.h"
#include "resources/resource_output.h"

namespace cli {

int executeResolved(const ParsedCli& parsed, CliResolution& resolved, IoService& io) {
  if (!resolved.route_descriptor || !resolved.route_descriptor->execute || !resolved.route_invocation) return 1;
  ResourceOutputService resource_outputs{io, parsed.options.dry_run, parsed.options.keep_first_resource};
  ExecutionContext common{io, resource_outputs, resolved.route};
  return resolved.route_descriptor->execute(common, *resolved.route_invocation);
}

}  // namespace cli
