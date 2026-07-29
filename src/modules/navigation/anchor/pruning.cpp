// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris_types.h"

#include "modules/navigation.h"
#include "utils/log.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

struct DisjointSet {
  std::vector<std::uint32_t> parent;
  std::vector<std::uint32_t> rank;

  explicit DisjointSet(std::size_t count) : parent(count), rank(count, 0) {
    for (std::uint32_t i = 0; i < parent.size(); ++i) parent[i] = i;
  }

  std::uint32_t find(std::uint32_t value) {
    if (parent[value] != value) parent[value] = find(parent[value]);
    return parent[value];
  }

  void unite(std::uint32_t first, std::uint32_t second) {
    first = find(first);
    second = find(second);
    if (first == second) return;
    if (rank[first] < rank[second]) std::swap(first, second);
    parent[second] = first;
    if (rank[first] == rank[second]) ++rank[first];
  }
};

bool validOptions(const AnchorComponentPruneOptions& options) noexcept {
  return std::isfinite(options.min_component_anchor_ratio) && options.min_component_anchor_ratio >= 0.0f &&
         options.min_component_anchor_ratio <= 1.0f;
}

}  // namespace

Error pruneAnchorComponents(std::vector<Anchor>& anchors, std::vector<AnchorConnection>& connections,
                            const AnchorComponentPruneOptions& options, AnchorComponentPruneDiagnostics* diagnostics) {
  if (!validOptions(options)) return Error::kInvalidOptions;
  Error error = validateConnections(anchors, connections);
  if (error != Error::kNone) return error;
  if (diagnostics) *diagnostics = {};
  if (anchors.empty()) return Error::kNone;

  DisjointSet components(anchors.size());
  for (const AnchorConnection& connection : connections) components.unite(connection.first, connection.second);

  std::vector<std::uint32_t> anchors_by_component(anchors.size(), 0);
  for (std::uint32_t anchor = 0; anchor < anchors.size(); ++anchor) ++anchors_by_component[components.find(anchor)];
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
  std::vector<Anchor> kept_anchors;
  kept_anchors.reserve(anchors.size());
  for (std::size_t anchor = 0; anchor < anchors.size(); ++anchor) {
    if (keep_component[components.find(static_cast<std::uint32_t>(anchor))] == 0) {
      if (diagnostics) diagnostics->pruned.push_back(anchors[anchor].position);
      continue;
    }
    if (kept_anchors.size() >= static_cast<std::size_t>(std::numeric_limits<AnchorIndex>::max()))
      return Error::kTooManyAnchors;
    remap[anchor] = static_cast<AnchorIndex>(kept_anchors.size());
    kept_anchors.push_back(anchors[anchor]);
  }

  std::vector<AnchorConnection> kept_connections;
  kept_connections.reserve(connections.size());
  for (const AnchorConnection& connection : connections) {
    if (remap[connection.first] == kInvalidAnchorIndex || remap[connection.second] == kInvalidAnchorIndex) continue;
    kept_connections.push_back({remap[connection.first], remap[connection.second]});
  }
  error = validateConnections(kept_anchors, kept_connections);
  if (error != Error::kNone) return error;

  const std::size_t removed_anchors = anchors.size() - kept_anchors.size();
  const std::size_t removed_connections = connections.size() - kept_connections.size();
  anchors = std::move(kept_anchors);
  connections = std::move(kept_connections);
  log(ARX_LOG_WARN,
      std::format("Level anchor pruning removed {} component(s), {} anchor(s), {} connection(s)",
                  removed_components,
                  removed_anchors,
                  removed_connections));
  return Error::kNone;
}

}  // namespace pistoris::navigation
