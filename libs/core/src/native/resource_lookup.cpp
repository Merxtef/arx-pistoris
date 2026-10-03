// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/resource_lookup.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace pistoris {
namespace {

constexpr std::array<std::string_view, 7> kDefaultLooseRoots = {
    "editor", "game", "graph", "localisation", "misc", "sfx", "speech"};

constexpr bool isSeparator(char value) noexcept { return value == '/' || value == '\\'; }

constexpr char lowerAscii(char value) noexcept {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

bool equalAsciiInsensitive(std::string_view lhs, std::string_view rhs) noexcept {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t i = 0; i < lhs.size(); ++i)
    if (lowerAscii(lhs[i]) != lowerAscii(rhs[i])) return false;
  return true;
}

void scanComponentsReverse(std::string_view path, std::size_t& parent_count, std::string_view& root) noexcept {
  std::size_t end = path.size();
  while (end != 0) {
    while (end != 0 && isSeparator(path[end - 1])) --end;
    if (end == 0) break;

    std::size_t begin = end;
    while (begin != 0 && !isSeparator(path[begin - 1])) --begin;
    const std::string_view component = path.substr(begin, end - begin);
    end = begin;

    if (component == ".") continue;
    if (component == "..") {
      ++parent_count;
      continue;
    }
    if (parent_count != 0) {
      --parent_count;
      continue;
    }
    root = component;
  }
}

}  // namespace

bool resolvesThroughDefaultLooseRoot(std::string_view lookup_base, std::string_view stored_path) noexcept {
  std::size_t parent_count = 0;
  std::string_view root;
  scanComponentsReverse(stored_path, parent_count, root);
  scanComponentsReverse(lookup_base, parent_count, root);
  if (parent_count != 0 || root.empty()) return false;

  for (std::string_view candidate : kDefaultLooseRoots)
    if (equalAsciiInsensitive(root, candidate)) return true;
  return false;
}

}  // namespace pistoris
