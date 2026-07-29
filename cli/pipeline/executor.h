// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/parsed.h"
#include "pipeline/resolver.h"

namespace cli {

class IoService;

int executeResolved(const ParsedCli& parsed, CliResolution& resolved, IoService& io);

}  // namespace cli
