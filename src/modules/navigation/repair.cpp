// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/navigation.h"
#include "utils/identifier.h"

#include <cassert>
#include <cstddef>
#include <span>

namespace pistoris::navigation {

std::size_t repairAnchorNames(std::span<Anchor> anchors) {
  IdentifierUniquifier names({.allow_empty = true});
  names.reserve(anchors.size());
  for (Anchor& anchor : anchors) names.add(anchor.name);
  const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
  return summary.changed;
}

void repairAnchorName(const NavigationData& navigation, Anchor& anchor, AnchorIndex ignored) {
  if (anchor.name.empty()) return;
  IdentifierUniquifier names({.allow_empty = true});
  names.reserve(1, navigation.anchors.size());
  for (std::size_t index = 0; index < navigation.anchors.size(); ++index) {
    if (index == static_cast<std::size_t>(ignored) || navigation.anchors[index].name.empty()) continue;
    names.occupy(navigation.anchors[index].name);
  }
  names.add(anchor.name);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
}

}  // namespace pistoris::navigation
