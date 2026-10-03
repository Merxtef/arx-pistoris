// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "console/help_request.h"

#include <cstdio>

namespace cli {

void printHelp(std::FILE* output, const HelpRequest& request);

}  // namespace cli
