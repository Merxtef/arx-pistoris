// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <span>

namespace cli {

struct RouteDescriptor;

enum class HelpPage : std::uint8_t {
  kGeneral,
  kSelectors,
  kFormats,
  kDebug,
};

struct HelpRequest {
  const RouteDescriptor* route = nullptr;
  HelpPage page = HelpPage::kGeneral;
};

bool resolveHelpRequest(std::span<const char* const> arguments, HelpRequest& out);

}  // namespace cli
