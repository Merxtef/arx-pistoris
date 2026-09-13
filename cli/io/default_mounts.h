// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <filesystem>
#include <string>

namespace cli {

bool defaultGameResourceRoot(std::filesystem::path& out, std::string& error);

}  // namespace cli
