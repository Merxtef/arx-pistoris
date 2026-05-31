// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "api/api_helpers.h"

#include "utils/log.h"
#include "utils/math/xform.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris::api {
namespace {

bool validVertex(const ftl::Data& ftl, std::uint32_t idx) { return idx < ftl.vertices.size(); }

bool validVertex(const ftl::Data& ftl, std::int32_t idx) {
  return idx >= 0 && static_cast<std::size_t>(idx) < ftl.vertices.size();
}

ArxVector3 sub(const ArxVector3& a, const ArxVector3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

float lengthSquared(const ArxVector3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

float dist(const ArxVector3& a, const ArxVector3& b) { return std::sqrt(lengthSquared(sub(a, b))); }

std::string lowerName(std::string_view name) {
  std::string out(name);
  for (char& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return out;
}

std::string stripOrdinalPrefix(std::string_view name) {
  if (name.size() >= 5 && std::isdigit(static_cast<unsigned char>(name[0])) &&
      std::isdigit(static_cast<unsigned char>(name[1])) && std::isdigit(static_cast<unsigned char>(name[2])) &&
      name[3] == '_' && name[4] == '_') {
    name.remove_prefix(5);
  }
  return lowerName(name);
}

void warnGroupMismatch(const ftl::Data& target, const ftl::Data& reference, std::string_view prefix) {
  if (target.groups.size() != reference.groups.size()) return;
  for (std::size_t i = 0; i < target.groups.size(); ++i) {
    std::string tn = stripOrdinalPrefix(target.groups[i].name);
    std::string rn = stripOrdinalPrefix(reference.groups[i].name);
    if (tn != rn) {
      log(ARX_LOG_WARN, std::format("{}: group {} name mismatch target='{}' reference='{}'", prefix, i,
                                    target.groups[i].name, reference.groups[i].name));
    }
  }
  if (target.extras.parent_bone.size() == reference.extras.parent_bone.size()) {
    for (std::size_t i = 0; i < target.extras.parent_bone.size(); ++i) {
      if (target.extras.parent_bone[i] != reference.extras.parent_bone[i]) {
        log(ARX_LOG_WARN, std::format("{}: group {} parent mismatch target={} reference={}", prefix, i,
                                      target.extras.parent_bone[i], reference.extras.parent_bone[i]));
      }
    }
  }
}

std::unordered_map<std::string, std::size_t> actionMap(const ftl::Data& ftl) {
  std::unordered_map<std::string, std::size_t> out;
  for (std::size_t i = 0; i < ftl.actions.size(); ++i) out.emplace(ftl.actions[i].name, i);
  return out;
}

bool containsIndex(const std::vector<std::int32_t>& values, std::int32_t idx) {
  return std::find(values.begin(), values.end(), idx) != values.end();
}

ftl::Selection& findOrCreateSelection(ftl::Data& ftl, std::string_view name) {
  for (auto& sel : ftl.selections)
    if (std::string_view(sel.name) == name) return sel;

  ftl::Selection sel{};
  std::size_t n = std::min(name.size(), sizeof(sel.name) - 1);
  std::memcpy(sel.name, name.data(), n);
  ftl.selections.push_back(std::move(sel));
  return ftl.selections.back();
}

void addSyntheticPair(std::vector<std::pair<std::int32_t, std::int32_t>>& pairs, std::int32_t ref_idx,
                      std::int32_t target_idx) {
  if (ref_idx >= 0 && target_idx >= 0) pairs.emplace_back(ref_idx, target_idx);
}

}  // namespace

ArxReturnCode overwriteTexturePaths(ftl::Data& ftl, std::string_view path, std::string_view log_prefix) {
  if (ftl.texture_containers.empty()) return ARX_OK;

  std::size_t n      = path.size();
  std::size_t copy_n = std::min(n, sizeof(ftl::TextureContainer::filename) - 1);
  if (n > copy_n) log(ARX_LOG_WARN, std::string(log_prefix) + ": path truncated to 255 chars");

  ftl::TextureContainer tc{};
  std::memcpy(tc.filename, path.data(), copy_n);
  ftl.texture_containers.clear();
  ftl.texture_containers.push_back(tc);

  for (auto& face : ftl.faces) {
    if (face.texture_id != kFtlTextureNone) face.texture_id = 0;
  }

  return ARX_OK;
}

ArxReturnCode applyTransform(ftl::Data& ftl, const AffineXform& xform) { return applyXformFtl(ftl, xform); }

ArxReturnCode applyTransform(tea::Data& tea, const AffineXform& xform) { return applyXformTea(tea, xform); }

ArxReturnCode snapFtlBoneOriginsToReference(ftl::Data& target, const ftl::Data& reference) {
  constexpr std::string_view kPrefix = "snapFtlBoneOriginsToReference";
  if (target.groups.size() != reference.groups.size()) {
    log(ARX_LOG_ERROR, std::format("{}: group count mismatch target={} reference={}", kPrefix, target.groups.size(),
                                   reference.groups.size()));
    return ARX_FTL_BAD_GROUP_N;
  }
  warnGroupMismatch(target, reference, kPrefix);

  float max_before = 0.0f;
  for (std::size_t i = 0; i < target.groups.size(); ++i) {
    if (!validVertex(target, target.groups[i].origin) || !validVertex(reference, reference.groups[i].origin)) {
      log(ARX_LOG_ERROR, std::format("{}: group {} has invalid origin", kPrefix, i));
      return ARX_FTL_BAD_GROUP_ORIGIN;
    }
    auto& target_pos    = target.vertices[target.groups[i].origin].position;
    const auto& ref_pos = reference.vertices[reference.groups[i].origin].position;
    max_before          = std::max(max_before, dist(target_pos, ref_pos));
    target_pos          = ref_pos;
  }

  log(ARX_LOG_INFO,
      std::format("{}: snapped {} bone origins, max previous distance={}", kPrefix, target.groups.size(), max_before));
  return ARX_OK;
}

ArxReturnCode snapFtlActionPointsToReference(ftl::Data& target, const ftl::Data& reference) {
  constexpr std::string_view kPrefix = "snapFtlActionPointsToReference";
  auto target_actions                = actionMap(target);
  std::size_t snapped                = 0;
  for (const auto& ref_action : reference.actions) {
    auto it = target_actions.find(ref_action.name);
    if (it == target_actions.end()) {
      log(ARX_LOG_WARN, std::format("{}: target action '{}' missing", kPrefix, ref_action.name));
      continue;
    }
    auto& target_action = target.actions[it->second];
    if (!validVertex(target, target_action.vertex_idx) || !validVertex(reference, ref_action.vertex_idx)) {
      log(ARX_LOG_ERROR, std::format("{}: action '{}' has invalid vertex index", kPrefix, ref_action.name));
      return ARX_FTL_BAD_ACTION_VERT_IDX;
    }
    target.vertices[target_action.vertex_idx].position = reference.vertices[ref_action.vertex_idx].position;
    ++snapped;
    std::string n = lowerName(ref_action.name);
    if (n == "head2chest" || n == "chest2leggings")
      log(ARX_LOG_INFO, std::format("{}: snapped key action '{}'", kPrefix, ref_action.name));
  }
  log(ARX_LOG_INFO,
      std::format("{}: snapped {}/{} reference action points", kPrefix, snapped, reference.actions.size()));
  return ARX_OK;
}

ArxReturnCode copyFtlSyntheticSelectionAffiliations(ftl::Data& target, const ftl::Data& reference) {
  constexpr std::string_view kPrefix = "copyFtlSyntheticSelectionAffiliations";
  if (target.groups.size() != reference.groups.size()) {
    log(ARX_LOG_ERROR, std::format("{}: group count mismatch target={} reference={}", kPrefix, target.groups.size(),
                                   reference.groups.size()));
    return ARX_FTL_BAD_GROUP_N;
  }

  std::vector<std::pair<std::int32_t, std::int32_t>> pairs;
  if (validVertex(target, target.header.origin) && validVertex(reference, reference.header.origin)) {
    addSyntheticPair(pairs, static_cast<std::int32_t>(reference.header.origin),
                     static_cast<std::int32_t>(target.header.origin));
  }

  for (std::size_t i = 0; i < target.groups.size(); ++i) {
    if (!validVertex(target, target.groups[i].origin) || !validVertex(reference, reference.groups[i].origin)) {
      log(ARX_LOG_ERROR, std::format("{}: group {} has invalid origin", kPrefix, i));
      return ARX_FTL_BAD_GROUP_ORIGIN;
    }
    addSyntheticPair(pairs, static_cast<std::int32_t>(reference.groups[i].origin),
                     static_cast<std::int32_t>(target.groups[i].origin));
  }

  auto target_actions = actionMap(target);
  for (const auto& ref_action : reference.actions) {
    auto it = target_actions.find(ref_action.name);
    if (it == target_actions.end()) continue;
    const auto& target_action = target.actions[it->second];
    if (!validVertex(target, target_action.vertex_idx) || !validVertex(reference, ref_action.vertex_idx)) {
      log(ARX_LOG_ERROR, std::format("{}: action '{}' has invalid vertex index", kPrefix, ref_action.name));
      return ARX_FTL_BAD_ACTION_VERT_IDX;
    }
    addSyntheticPair(pairs, ref_action.vertex_idx, target_action.vertex_idx);
  }

  std::size_t total_added = 0;
  for (const auto& ref_sel : reference.selections) {
    std::size_t added_for_selection = 0;
    for (const auto& [ref_idx, target_idx] : pairs) {
      if (!containsIndex(ref_sel.selected, ref_idx)) continue;
      auto& target_sel = findOrCreateSelection(target, ref_sel.name);
      if (!containsIndex(target_sel.selected, target_idx)) {
        target_sel.selected.push_back(target_idx);
        ++added_for_selection;
        ++total_added;
      }
    }
    if (added_for_selection > 0) {
      log(ARX_LOG_INFO, std::format("{}: {} +{} synthetic vertices", kPrefix, ref_sel.name, added_for_selection));
    }
  }
  log(ARX_LOG_INFO, std::format("{}: copied {} synthetic affiliations across {} matched vertices", kPrefix, total_added,
                                pairs.size()));
  return ARX_OK;
}

}  // namespace pistoris::api
