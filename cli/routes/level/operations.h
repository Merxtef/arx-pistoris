// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/debug/level.hpp"

#include "routes/level/options.h"

#include <variant>

namespace cli::level::operations {

using OperationDiagnostics = std::variant<std::monostate, pistoris::level_debug::NavigationDiagnostics,
                                          pistoris::level_debug::RoomDistanceGenDiagnostics>;

bool applyLevelOperations(pistoris::Level& level, const LevelOptions& options, OperationDiagnostics& diagnostics);

}  // namespace cli::level::operations
