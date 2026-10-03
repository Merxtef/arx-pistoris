// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "anchor_metadata.h"

#include "arx_pistoris/base/indices.h"

#include "modules/navigation.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb_level {
namespace {

constexpr const char* kAnchorMetadataKey = "arx_pistoris_anchor";

bool readAnchorIndex(const nlohmann::json& value, AnchorIndex& out) {
  std::uint64_t parsed = 0;
  if (value.is_number_unsigned()) {
    parsed = value.get<std::uint64_t>();
  } else if (value.is_number_integer()) {
    const std::int64_t signed_value = value.get<std::int64_t>();
    if (signed_value < 0) return false;
    parsed = static_cast<std::uint64_t>(signed_value);
  } else {
    return false;
  }
  if (parsed >= static_cast<std::uint64_t>(kInvalidAnchorIndex)) return false;
  out = static_cast<AnchorIndex>(parsed);
  return true;
}

bool connectionLess(const AnchorConnection& left, const AnchorConnection& right) noexcept {
  return left.first < right.first || (left.first == right.first && left.second < right.second);
}

}  // namespace

std::string anchorMetadataJson(AnchorIndex id, std::span<const AnchorConnection> outgoing) {
  nlohmann::json links = nlohmann::json::array();
  for (const AnchorConnection& connection : outgoing) {
    assert(connection.first == id);
    links.push_back(connection.second);
  }
  return nlohmann::json{{kAnchorMetadataKey, {{"id", id}, {"links", std::move(links)}}}}.dump();
}

void appendAnchorMetadata(const cgltf_extras& extras, AnchorMetadataImport& out) {
  AnchorMetadataRecord record;
  record.links_offset = out.links.size();
  if (extras.data == nullptr) {
    out.records.push_back(record);
    return;
  }

  const nlohmann::json root = nlohmann::json::parse(extras.data, nullptr, false);
  if (!root.is_object()) {
    out.records.push_back(record);
    return;
  }
  const auto metadata_it = root.find(kAnchorMetadataKey);
  if (metadata_it == root.end()) {
    out.records.push_back(record);
    return;
  }
  if (!metadata_it->is_object()) {
    ++out.malformed_records;
    out.records.push_back(record);
    return;
  }

  const auto id_it = metadata_it->find("id");
  if (id_it == metadata_it->end() || !readAnchorIndex(*id_it, record.id)) {
    ++out.malformed_records;
    out.records.push_back(record);
    return;
  }
  const auto links_it = metadata_it->find("links");
  if (links_it == metadata_it->end()) {
    out.records.push_back(record);
    return;
  }
  if (!links_it->is_array()) {
    ++out.malformed_records;
    out.records.push_back(record);
    return;
  }
  for (const nlohmann::json& value : *links_it) {
    AnchorIndex link = kInvalidAnchorIndex;
    if (!readAnchorIndex(value, link)) {
      ++out.malformed_links;
      continue;
    }
    out.links.push_back(link);
  }
  record.links_count = out.links.size() - record.links_offset;
  out.records.push_back(record);
}

AnchorMetadataDiagnostics importAnchorConnections(const AnchorMetadataImport& metadata, std::size_t anchor_count,
                                                  std::vector<AnchorConnection>& out) {
  struct IdMapping {
    AnchorIndex id = kInvalidAnchorIndex;
    AnchorIndex anchor = kInvalidAnchorIndex;
  };

  AnchorMetadataDiagnostics diagnostics;
  std::vector<IdMapping> mappings;
  mappings.reserve(metadata.records.size());
  const std::size_t record_count = std::min(metadata.records.size(), anchor_count);
  for (std::size_t anchor = 0; anchor < record_count; ++anchor) {
    const AnchorIndex id = metadata.records[anchor].id;
    if (id == kInvalidAnchorIndex) continue;
    if (anchor >= static_cast<std::size_t>(kInvalidAnchorIndex)) {
      diagnostics.graph_discarded = true;
      out.clear();
      return diagnostics;
    }
    mappings.push_back({id, static_cast<AnchorIndex>(anchor)});
  }
  std::sort(mappings.begin(), mappings.end(), [](const IdMapping& left, const IdMapping& right) {
    return left.id < right.id || (left.id == right.id && left.anchor < right.anchor);
  });
  for (std::size_t begin = 0; begin < mappings.size();) {
    std::size_t end = begin + 1U;
    while (end < mappings.size() && mappings[end].id == mappings[begin].id) ++end;
    if (end - begin > 1U) ++diagnostics.ambiguous_ids;
    begin = end;
  }

  auto resolve = [&mappings](AnchorIndex id) -> std::span<const IdMapping> {
    const std::span<const IdMapping> view(mappings);
    const auto begin = std::lower_bound(
        view.begin(), view.end(), id, [](const IdMapping& mapping, AnchorIndex value) { return mapping.id < value; });
    const auto end = std::upper_bound(
        begin, view.end(), id, [](AnchorIndex value, const IdMapping& mapping) { return value < mapping.id; });
    const std::size_t offset = static_cast<std::size_t>(begin - view.begin());
    const std::size_t count = static_cast<std::size_t>(end - begin);
    return view.subspan(offset, count);
  };

  std::vector<AnchorConnection> connections;
  connections.reserve(metadata.links.size());
  for (std::size_t source_anchor = 0; source_anchor < record_count; ++source_anchor) {
    const AnchorMetadataRecord& record = metadata.records[source_anchor];
    if (record.id == kInvalidAnchorIndex || record.links_offset > metadata.links.size() ||
        record.links_count > metadata.links.size() - record.links_offset)
      continue;
    const std::span<const IdMapping> source = resolve(record.id);
    if (source.size() != 1U) {
      if (!source.empty()) diagnostics.ambiguous_links += record.links_count;
      continue;
    }
    for (AnchorIndex target_id : std::span(metadata.links).subspan(record.links_offset, record.links_count)) {
      const std::span<const IdMapping> target = resolve(target_id);
      if (target.empty()) {
        ++diagnostics.dangling_links;
        continue;
      }
      if (target.size() != 1U) {
        ++diagnostics.ambiguous_links;
        continue;
      }
      AnchorIndex first = source.front().anchor;
      AnchorIndex second = target.front().anchor;
      if (first == second) {
        ++diagnostics.self_links;
        continue;
      }
      if (second < first) std::swap(first, second);
      if (connections.size() >= static_cast<std::size_t>(kInvalidAnchorConnectionIndex)) {
        diagnostics.graph_discarded = true;
        out.clear();
        return diagnostics;
      }
      connections.push_back({first, second});
    }
  }

  std::sort(connections.begin(), connections.end(), connectionLess);
  connections.erase(std::unique(connections.begin(),
                                connections.end(),
                                [](const AnchorConnection& left, const AnchorConnection& right) {
                                  return left.first == right.first && left.second == right.second;
                                }),
                    connections.end());
  out = std::move(connections);
  return diagnostics;
}

}  // namespace pistoris::glb_level
