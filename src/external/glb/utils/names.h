// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <optional>
#include <string_view>

namespace pistoris::glb {

struct LabeledValue {
  std::string_view value;
  std::string_view label;
};

std::optional<LabeledValue> splitRequiredLabel(std::string_view text) noexcept;

}  // namespace pistoris::glb
