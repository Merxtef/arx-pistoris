// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "io/files.h"

#include "console/diagnostics.h"
#include "io/files_internal.h"
#include "io/native_path.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cli::io_detail {

bool readFileSized(const std::filesystem::path& path, std::vector<std::uint8_t>& out, bool quiet) {
  const std::string display_path = pathToUtf8(path);
  std::error_code ec;
  std::uintmax_t file_size = std::filesystem::file_size(path, ec);
  if (ec) {
    if (!quiet) cli::diagnostic(cli::DiagnosticCode::kIoStatFailed, "Cannot stat: %s", display_path.c_str());
    return false;
  }
  if (!std::in_range<std::size_t>(file_size) || !std::in_range<std::streamsize>(file_size)) {
    if (!quiet) cli::diagnostic(cli::DiagnosticCode::kIoFileTooLarge, "File too large: %s", display_path.c_str());
    return false;
  }

  try {
    out.resize(static_cast<std::size_t>(file_size));
  } catch (const std::bad_alloc&) {
    if (!quiet) {
      cli::diagnostic(
          cli::DiagnosticCode::kIoAllocationFailed, "Unable to allocate space for: %s", display_path.c_str());
    }
    return false;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    if (!quiet) cli::diagnostic(cli::DiagnosticCode::kIoOpenFailed, "Cannot open: %s", display_path.c_str());
    return false;
  }

  if (!out.empty()) file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
  if (!file) {
    if (!quiet) cli::diagnostic(cli::DiagnosticCode::kIoReadFailed, "Failed to read: %s", display_path.c_str());
    return false;
  }
  return true;
}

}  // namespace cli::io_detail

bool readFile(const char* path, std::vector<std::uint8_t>& out) {
  std::filesystem::path native_path;
  std::string error;
  if (!path || !cli::io_detail::pathFromUtf8(path ? std::string_view(path) : std::string_view(), native_path, error)) {
    cli::diagnostic(cli::DiagnosticCode::kIoOpenFailed, "Invalid input path: %s", error.c_str());
    return false;
  }
  return cli::io_detail::readFileSized(native_path, out, false);
}

bool readFileOptional(const char* path, std::vector<std::uint8_t>& out) {
  std::filesystem::path native_path;
  std::string error;
  if (!path || !cli::io_detail::pathFromUtf8(path ? std::string_view(path) : std::string_view(), native_path, error)) {
    return false;
  }
  return cli::io_detail::readFileSized(native_path, out, true);
}
