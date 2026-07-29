// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace cli::io_detail {

bool readFileSized(const std::filesystem::path& path, std::vector<std::uint8_t>& out, bool quiet);

}  // namespace cli::io_detail
