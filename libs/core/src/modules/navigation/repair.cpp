// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/navigation.h"
#include "utils/identifier.h"

#include <cassert>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <unordered_set>

namespace pistoris::navigation {
namespace {

std::string nextAnchorName(std::unordered_set<std::string>& occupied, std::size_t& ordinal) {
  for (;;) {
    std::string candidate = std::format("anchor_{}", ordinal++);
    if (occupied.insert(candidate).second) return candidate;
  }
}

}  // namespace

std::size_t repairAnchorNames(std::span<Anchor> anchors) {
  IdentifierUniquifier names;
  names.reserve(anchors.size());
  for (Anchor& anchor : anchors)
    if (!anchor.name.empty()) names.add(anchor.name);
  IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);

  std::unordered_set<std::string> occupied;
  occupied.reserve(anchors.size());
  for (const Anchor& anchor : anchors)
    if (!anchor.name.empty()) occupied.insert(anchor.name);
  std::size_t ordinal = 0;
  for (Anchor& anchor : anchors) {
    if (!anchor.name.empty()) continue;
    anchor.name = nextAnchorName(occupied, ordinal);
    ++summary.changed;
  }
  return summary.changed;
}

void repairAnchorName(NavigationData& navigation, Anchor& anchor, AnchorIndex ignored) {
  if (anchor.name.empty()) {
    std::unordered_set<std::string> occupied;
    occupied.reserve(navigation.anchors.size());
    for (std::size_t index = 0; index < navigation.anchors.size(); ++index) {
      if (index != static_cast<std::size_t>(ignored)) occupied.insert(navigation.anchors[index].name);
    }
    anchor.name = nextAnchorName(occupied, navigation.next_anchor_ordinal);
    return;
  }
  IdentifierUniquifier names;
  names.reserve(1, navigation.anchors.size());
  for (std::size_t index = 0; index < navigation.anchors.size(); ++index) {
    if (index == static_cast<std::size_t>(ignored)) continue;
    names.occupy(navigation.anchors[index].name);
  }
  names.add(anchor.name);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
}

}  // namespace pistoris::navigation
