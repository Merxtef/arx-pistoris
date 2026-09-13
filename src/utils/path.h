// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <string_view>

namespace pistoris {

inline std::string_view pathFilename(std::string_view path) noexcept {
  const std::size_t separator = path.find_last_of("/\\");
  return separator == std::string_view::npos ? path : path.substr(separator + 1U);
}

inline std::string_view pathExtension(std::string_view path) noexcept {
  const std::size_t separator = path.find_last_of("/\\");
  const std::size_t dot = path.find_last_of('.');
  if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator)) return {};
  return path.substr(dot);
}

inline std::string_view pathWithoutExtension(std::string_view path) noexcept {
  return path.substr(0, path.size() - pathExtension(path).size());
}

inline std::string_view pathStem(std::string_view path) noexcept { return pathFilename(pathWithoutExtension(path)); }

}  // namespace pistoris
