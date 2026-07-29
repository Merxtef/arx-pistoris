// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <cstddef>
#include <cstdint>

namespace cli::modules {

bool parseFloat(const char* text, float& out);
bool parseDouble(const char* text, double& out);
bool parseSize(const char* text, std::size_t& out);
bool parseUint32(const char* text, std::uint32_t& out);
bool consumeFloat(ModuleParseContext& ctx, float& out, const char* name, const char* expected);
bool consumeDouble(ModuleParseContext& ctx, double& out, const char* name, const char* expected);
bool consumeFloats(ModuleParseContext& ctx, float* out, int count, const char* name, const char* expected);

}  // namespace cli::modules
