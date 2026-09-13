// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"

#include "cgltf/cgltf.h"
#include "modules/navigation.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace pistoris::glb_level {

struct AnchorMetadataRecord {
  AnchorIndex id = kInvalidAnchorIndex;
  std::size_t links_offset = 0;
  std::size_t links_count = 0;
};

struct AnchorMetadataImport {
  std::vector<AnchorMetadataRecord> records;
  std::vector<AnchorIndex> links;
  std::size_t malformed_records = 0;
  std::size_t malformed_links = 0;
};

struct AnchorMetadataDiagnostics {
  std::size_t ambiguous_ids = 0;
  std::size_t ambiguous_links = 0;
  std::size_t dangling_links = 0;
  std::size_t self_links = 0;
  bool graph_discarded = false;
};

std::string anchorMetadataJson(AnchorIndex id, std::span<const AnchorConnection> outgoing);
void appendAnchorMetadata(const cgltf_extras& extras, AnchorMetadataImport& out);
AnchorMetadataDiagnostics importAnchorConnections(const AnchorMetadataImport& metadata, std::size_t anchor_count,
                                                  std::vector<AnchorConnection>& out);

}  // namespace pistoris::glb_level
