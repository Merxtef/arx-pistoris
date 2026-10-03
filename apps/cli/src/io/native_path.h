// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace cli::io_detail {

bool pathFromUtf8(std::string_view utf8, std::filesystem::path& out, std::string& error);
std::string pathToUtf8(const std::filesystem::path& path);
bool validateNativePathSyntax(const std::filesystem::path& path, std::string& error);
bool resolveProspectiveNativePath(const std::filesystem::path& path, std::filesystem::path& out, bool& exists,
                                  std::string& error);
bool replaceNativeFile(const std::filesystem::path& source, const std::filesystem::path& destination,
                       std::error_code& error) noexcept;
std::uint64_t processId() noexcept;

#ifdef _WIN32
bool wideToUtf8(std::wstring_view wide, std::string& out);
#endif

}  // namespace cli::io_detail
