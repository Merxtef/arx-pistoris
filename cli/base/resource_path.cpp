// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "base/resource_path.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace cli {

std::string resourceParentPath(std::string_view path) {
  const std::size_t separator = path.find_last_of("/\\");
  return separator == std::string_view::npos ? std::string{} : std::string(path.substr(0, separator + 1));
}

std::string_view resourceFilename(std::string_view path) noexcept {
  const std::size_t separator = path.find_last_of("/\\");
  if (separator != std::string_view::npos) path.remove_prefix(separator + 1);
  return path;
}

std::string resourceStem(std::string_view path) {
  path = resourceFilename(path);
  const std::size_t dot = path.find_last_of('.');
  if (dot != std::string_view::npos) path = path.substr(0, dot);
  return std::string(path);
}

}  // namespace cli
