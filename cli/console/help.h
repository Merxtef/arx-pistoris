// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdio>
#include <vector>

namespace cli {

bool validateHelpTopics(const std::vector<const char*>& topics);
void printUsage(std::FILE* output, const char* argv0, const std::vector<const char*>& topics = {});

}  // namespace cli
