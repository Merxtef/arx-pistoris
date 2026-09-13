// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "names.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace pistoris::glb {

std::optional<LabeledValue> splitRequiredLabel(std::string_view text) noexcept {
  const std::size_t separator = text.rfind("__");
  if (separator == std::string_view::npos || separator == 0 || separator + 2 == text.size()) return std::nullopt;
  return LabeledValue{text.substr(0, separator), text.substr(separator + 2)};
}

}  // namespace pistoris::glb
