// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/runtime/types.h"

namespace cli {

void setLogLevel(ArxLogLevel level);
ArxLogLevel logLevel();

const char* logLevelName(ArxLogLevel level);

bool shouldLog(ArxLogLevel level);
void log(ArxLogLevel level, const char* fmt, ...);
void pistorisLog(ArxLogLevel level, const char* msg, void* userdata);

}  // namespace cli
