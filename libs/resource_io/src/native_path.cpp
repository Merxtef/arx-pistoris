// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_path.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace pistoris::resource_io::detail {

bool canonicalizeProspectivePath(const std::filesystem::path& requested, std::filesystem::path& normalized,
                                 bool& exists, std::string& error) {
  std::error_code filesystem_error;
  std::filesystem::path ancestor = std::filesystem::absolute(requested, filesystem_error).lexically_normal();
  if (filesystem_error) {
    error = filesystem_error.message();
    return false;
  }

  std::vector<std::filesystem::path> suffix;
  for (;;) {
    filesystem_error.clear();
    const std::filesystem::file_status status = std::filesystem::symlink_status(ancestor, filesystem_error);
    if (!filesystem_error && std::filesystem::exists(status)) break;
    if (filesystem_error && filesystem_error != std::errc::no_such_file_or_directory) {
      error = filesystem_error.message();
      return false;
    }
    filesystem_error.clear();
    const std::filesystem::path parent = ancestor.parent_path();
    if (parent == ancestor || parent.empty()) {
      error = "path has no accessible ancestor";
      return false;
    }
    suffix.push_back(ancestor.filename());
    ancestor = parent;
  }

  if (!suffix.empty() && !std::filesystem::is_directory(ancestor, filesystem_error)) {
    error = filesystem_error ? filesystem_error.message() : "path has a file ancestor";
    return false;
  }
  normalized = std::filesystem::canonical(ancestor, filesystem_error);
  if (filesystem_error) {
    error = filesystem_error.message();
    return false;
  }
  for (auto component = suffix.rbegin(); component != suffix.rend(); ++component) normalized /= *component;
  exists = suffix.empty();
  return true;
}

}  // namespace pistoris::resource_io::detail
