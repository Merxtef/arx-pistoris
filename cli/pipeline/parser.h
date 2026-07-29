// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/parsed.h"

namespace cli {

bool parseArgs(int argc, char* argv[], ParsedCli& parsed);

}  // namespace cli
