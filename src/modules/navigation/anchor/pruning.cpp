// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/navigation.h"
#include "utils/disjoint_set.h"
#include "utils/log.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

bool validOptions(const AnchorComponentPruneOptions& options) noexcept {
  return std::isfinite(options.min_component_anchor_ratio) && options.min_component_anchor_ratio >= 0.0f &&
         options.min_component_anchor_ratio <= 1.0f;
}

}  // namespace

Error planAnchorComponentPrune(AnchorComponentPrunePlan& out, std::span<const Anchor> anchors,
                               std::span<const AnchorConnection> connections,
                               const AnchorComponentPruneOptions& options,
                               AnchorComponentPruneDiagnostics* diagnostics) {
  if (!validOptions(options)) return Error::kInvalidOptions;
  Error error = validateConnections(anchors, connections);
  if (error != Error::kNone) return error;
  out = {};
  if (diagnostics) *diagnostics = {};
  if (anchors.empty()) return Error::kNone;

  DisjointSet components(anchors.size());
  for (const AnchorConnection& connection : connections) components.unite(connection.first, connection.second);

  std::vector<std::uint32_t> anchors_by_component(anchors.size(), 0);
  for (std::size_t anchor = 0; anchor < anchors.size(); ++anchor) ++anchors_by_component[components.find(anchor)];
  std::uint32_t largest_count = 0;
  for (std::uint32_t count : anchors_by_component) largest_count = std::max(largest_count, count);
  const double relative_threshold = static_cast<double>(largest_count) * options.min_component_anchor_ratio;

  std::vector<std::uint8_t> keep_component(anchors.size(), 0);
  std::size_t removed_components = 0;
  for (std::size_t component = 0; component < anchors_by_component.size(); ++component) {
    std::uint32_t count = anchors_by_component[component];
    if (count == 0) continue;
    if (static_cast<double>(count) >= relative_threshold && count >= options.min_component_anchor_count) {
      keep_component[component] = 1;
    } else {
      ++removed_components;
    }
  }
  if (removed_components == 0) return Error::kNone;

  std::vector<AnchorIndex> remap(anchors.size(), kInvalidAnchorIndex);
  std::size_t removed_anchors = 0;
  for (std::size_t anchor = 0; anchor < anchors.size(); ++anchor)
    if (keep_component[components.find(anchor)] == 0) ++removed_anchors;
  if (diagnostics) diagnostics->pruned.reserve(removed_anchors);

  AnchorIndex next_anchor = 0;
  for (std::size_t anchor = 0; anchor < anchors.size(); ++anchor) {
    if (keep_component[components.find(static_cast<std::uint32_t>(anchor))] == 0) {
      if (diagnostics) diagnostics->pruned.push_back(anchors[anchor].position);
      continue;
    }
    remap[anchor] = next_anchor++;
  }

  std::size_t kept_connections = 0;
  for (const AnchorConnection& connection : connections)
    if (remap[connection.first] != kInvalidAnchorIndex && remap[connection.second] != kInvalidAnchorIndex)
      ++kept_connections;
  out.remap = std::move(remap);
  out.kept_connection_count = kept_connections;
  out.removed_components = removed_components;
  out.removed_anchors = removed_anchors;
  return Error::kNone;
}

Error pruneAnchorComponents(std::vector<Anchor>& anchors, std::vector<AnchorConnection>& connections,
                            const AnchorComponentPruneOptions& options, AnchorComponentPruneDiagnostics* diagnostics) {
  AnchorComponentPrunePlan plan;
  const Error error = planAnchorComponentPrune(plan, anchors, connections, options, diagnostics);
  if (error == Error::kNone) applyAnchorComponentPrune(anchors, connections, std::move(plan));
  return error;
}

void applyAnchorComponentPrune(std::vector<Anchor>& anchors, std::vector<AnchorConnection>& connections,
                               AnchorComponentPrunePlan&& plan) noexcept {
  if (plan.remap.empty()) return;
  const std::size_t removed_connections = connections.size() - plan.kept_connection_count;
  std::size_t next_connection = 0;
  for (const AnchorConnection& connection : connections) {
    if (plan.remap[connection.first] == kInvalidAnchorIndex || plan.remap[connection.second] == kInvalidAnchorIndex)
      continue;
    connections[next_connection++] = {plan.remap[connection.first], plan.remap[connection.second]};
  }

  for (std::size_t old = 0; old < anchors.size(); ++old) {
    if (plan.remap[old] == kInvalidAnchorIndex || plan.remap[old] == old) continue;
    anchors[plan.remap[old]] = std::move(anchors[old]);
  }
  const std::size_t retained_anchor_count = anchors.size() - plan.removed_anchors;
  while (anchors.size() > retained_anchor_count) anchors.pop_back();
  while (connections.size() > next_connection) connections.pop_back();
  log(ARX_LOG_WARN,
      "Anchor pruning removed {} component(s), {} anchor(s), {} connection(s)",
      plan.removed_components,
      plan.removed_anchors,
      removed_connections);
}

}  // namespace pistoris::navigation
