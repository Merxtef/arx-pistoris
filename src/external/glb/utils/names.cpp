// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "names.h"

#include "arx_pistoris/runtime/types.h"

#include "utils/log.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace pistoris::glb {

std::optional<LabeledValue> splitRequiredLabel(std::string_view text) noexcept {
  const std::size_t separator = text.rfind("__");
  if (separator == std::string_view::npos || separator == 0 || separator + 2 == text.size()) return std::nullopt;
  return LabeledValue{text.substr(0, separator), text.substr(separator + 2)};
}

bool looksLikeConventionToken(std::string_view label) noexcept {
  if (label.size() < 4) return false;
  bool has_uppercase = false;
  for (char value : label) {
    if (value >= 'A' && value <= 'Z') {
      has_uppercase = true;
    } else if ((value < '0' || value > '9') && value != '_' && value != '.') {
      return false;
    }
  }
  return has_uppercase;
}

void reportConventionLabel(std::string_view context, std::string_view name, const ParsedLabel& label) {
  if (label.presence == LabelPresence::kMissing) {
    log(ARX_LOG_WARN, "{}: node '{}' has no final label; semantic values recovered", context, name);
  } else if (looksLikeConventionToken(label.text)) {
    log(ARX_LOG_INFO,
        "{}: final label '{}' on node '{}' resembles a convention token; use a descriptive label",
        context,
        label.text,
        name);
  }
}

}  // namespace pistoris::glb
