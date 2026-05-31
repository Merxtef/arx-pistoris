// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "model/modules.h"

#include "xform.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cli::model {
namespace {

bool renameSelections(pistoris::Ftl& ftl, std::string_view csv) {
  std::size_t pos = 0;
  std::size_t idx = 0;
  while (pos <= csv.size()) {
    std::size_t comma = csv.find(',', pos);
    std::size_t end   = comma == std::string_view::npos ? csv.size() : comma;
    if (idx >= ftl.selections.size()) {
      std::fprintf(stderr, "--rename-selections: got more names than selections (%zu)\n", ftl.selections.size());
      return false;
    }

    std::string_view name = csv.substr(pos, end - pos);
    if (!name.empty()) {
      auto& sel = ftl.selections[idx];
      std::fprintf(stdout, "renaming selection: %s -> %.*s\n", sel.name, static_cast<int>(name.size()), name.data());
      std::memset(sel.name, 0, sizeof(sel.name));
      std::size_t n = name.size();
      if (n >= sizeof(sel.name)) {
        n = sizeof(sel.name) - 1;
        std::fprintf(stderr, "--rename-selections: selection name '%.*s' truncated to %zu chars\n",
                     static_cast<int>(name.size()), name.data(), n);
      }
      std::memcpy(sel.name, name.data(), n);
    }

    ++idx;
    if (comma == std::string_view::npos) break;
    pos = comma + 1;
  }
  return true;
}

bool referenceOperationRequested(const CliArgs& args) {
  return args.autosize_to_reference || args.bone_origin_reference_mode != BoneOriginReferenceMode::kNone ||
         args.snap_action_points || args.copy_reference_affiliations;
}

bool requireReference(const Context& ctx) {
  if (ctx.has_reference) return true;
  std::fprintf(stderr, "reference operation requires --reference-ftl\n");
  return false;
}

bool validVertex(const pistoris::Ftl& ftl, std::uint32_t idx) { return idx < ftl.vertices.size(); }

bool validVertex(const pistoris::Ftl& ftl, std::int32_t idx) {
  return idx >= 0 && static_cast<std::size_t>(idx) < ftl.vertices.size();
}

pistoris::ArxVector3 add(const pistoris::ArxVector3& a, const pistoris::ArxVector3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

pistoris::ArxVector3 sub(const pistoris::ArxVector3& a, const pistoris::ArxVector3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

pistoris::ArxVector3 mul(const pistoris::ArxVector3& v, float s) { return {v.x * s, v.y * s, v.z * s}; }

pistoris::ArxVector3 div(const pistoris::ArxVector3& v, float s) { return {v.x / s, v.y / s, v.z / s}; }

float dot(const pistoris::ArxVector3& a, const pistoris::ArxVector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

float lengthSquared(const pistoris::ArxVector3& v) { return dot(v, v); }

float length(const pistoris::ArxVector3& v) { return std::sqrt(lengthSquared(v)); }

pistoris::ArxVector3 cross(const pistoris::ArxVector3& a, const pistoris::ArxVector3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

pistoris::ArxVector3 normalizeOr(const pistoris::ArxVector3& v, const pistoris::ArxVector3& fallback) {
  float len = length(v);
  if (len <= 1e-8f) return fallback;
  return div(v, len);
}

pistoris::ArxVector3 rotateAroundAxis(const pistoris::ArxVector3& v, const pistoris::ArxVector3& axis, float c,
                                      float s) {
  return add(add(mul(v, c), mul(cross(axis, v), s)), mul(axis, dot(axis, v) * (1.0f - c)));
}

pistoris::ArxVector3 rotateBetween(const pistoris::ArxVector3& v, const pistoris::ArxVector3& from,
                                   const pistoris::ArxVector3& to) {
  pistoris::ArxVector3 f = normalizeOr(from, {1.0f, 0.0f, 0.0f});
  pistoris::ArxVector3 t = normalizeOr(to, f);
  float c                = std::clamp(dot(f, t), -1.0f, 1.0f);
  if (c > 0.9999f) return v;

  pistoris::ArxVector3 axis = cross(f, t);
  float axis_len            = length(axis);
  if (axis_len <= 1e-6f) {
    pistoris::ArxVector3 fallback =
        std::abs(f.x) < 0.9f ? pistoris::ArxVector3{1.0f, 0.0f, 0.0f} : pistoris::ArxVector3{0.0f, 1.0f, 0.0f};
    axis = normalizeOr(cross(f, fallback), {0.0f, 0.0f, 1.0f});
    return rotateAroundAxis(v, axis, -1.0f, 0.0f);
  }

  axis    = div(axis, axis_len);
  float s = std::sqrt(std::max(0.0f, 1.0f - c * c));
  return rotateAroundAxis(v, axis, c, s);
}

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

float dist(const pistoris::ArxVector3& a, const pistoris::ArxVector3& b) {
  pistoris::ArxVector3 d = sub(a, b);
  return length(d);
}

void warnGroupMismatch(const pistoris::Ftl& target, const pistoris::Ftl& ref) {
  if (target.groups.size() != ref.groups.size()) {
    std::fprintf(stderr, "reference: group count mismatch target=%zu reference=%zu\n", target.groups.size(),
                 ref.groups.size());
    return;
  }
  for (std::size_t i = 0; i < target.groups.size(); ++i) {
    std::string tn = stripOrdinalPrefix(target.groups[i].name);
    std::string rn = stripOrdinalPrefix(ref.groups[i].name);
    if (tn != rn) {
      std::fprintf(stderr, "reference: group %zu name mismatch target='%s' reference='%s'\n", i, target.groups[i].name,
                   ref.groups[i].name);
    }
  }
  if (target.extras.parent_bone.size() == ref.extras.parent_bone.size()) {
    for (std::size_t i = 0; i < target.extras.parent_bone.size(); ++i) {
      if (target.extras.parent_bone[i] != ref.extras.parent_bone[i]) {
        std::fprintf(stderr, "reference: group %zu parent mismatch target=%d reference=%d\n", i,
                     target.extras.parent_bone[i], ref.extras.parent_bone[i]);
      }
    }
  }
}

struct Landmark {
  std::string name;
  pistoris::ArxVector3 target;
  pistoris::ArxVector3 reference;
  float weight = 1.0f;
};

float actionWeight(std::string_view name) {
  std::string n = lowerName(name);
  if (n == "head2chest" || n == "chest2leggings") return 10.0f;
  return 2.0f;
}

std::unordered_map<std::string, std::size_t> actionMap(const pistoris::Ftl& ftl) {
  std::unordered_map<std::string, std::size_t> out;
  for (std::size_t i = 0; i < ftl.actions.size(); ++i) out.emplace(ftl.actions[i].name, i);
  return out;
}

std::vector<Landmark> collectReferenceLandmarks(const pistoris::Ftl& target, const pistoris::Ftl& ref) {
  std::vector<Landmark> out;
  if (validVertex(target, target.header.origin) && validVertex(ref, ref.header.origin)) {
    out.push_back({"header.origin", target.vertices[target.header.origin].position,
                   ref.vertices[ref.header.origin].position, 1.0f});
  }

  std::size_t ng = std::min(target.groups.size(), ref.groups.size());
  for (std::size_t i = 0; i < ng; ++i) {
    if (!validVertex(target, target.groups[i].origin) || !validVertex(ref, ref.groups[i].origin)) continue;
    out.push_back({std::string{"group:"} + target.groups[i].name, target.vertices[target.groups[i].origin].position,
                   ref.vertices[ref.groups[i].origin].position, 1.0f});
  }

  auto target_actions = actionMap(target);
  for (const auto& ref_action : ref.actions) {
    auto it = target_actions.find(ref_action.name);
    if (it == target_actions.end()) continue;
    const auto& target_action = target.actions[it->second];
    if (!validVertex(target, target_action.vertex_idx) || !validVertex(ref, ref_action.vertex_idx)) continue;
    out.push_back({std::string{"action:"} + ref_action.name, target.vertices[target_action.vertex_idx].position,
                   ref.vertices[ref_action.vertex_idx].position, actionWeight(ref_action.name)});
  }
  return out;
}

bool autosizeToReference(pistoris::Ftl& target, const pistoris::Ftl& ref) {
  warnGroupMismatch(target, ref);
  std::vector<Landmark> lm = collectReferenceLandmarks(target, ref);
  if (lm.empty()) {
    std::fprintf(stderr, "--autosize-to-reference: no usable landmarks\n");
    return false;
  }

  float wsum = 0.0f;
  pistoris::ArxVector3 ct{0.0f, 0.0f, 0.0f};
  pistoris::ArxVector3 cr{0.0f, 0.0f, 0.0f};
  for (const auto& l : lm) {
    wsum += l.weight;
    ct = add(ct, mul(l.target, l.weight));
    cr = add(cr, mul(l.reference, l.weight));
  }
  ct = div(ct, wsum);
  cr = div(cr, wsum);

  float num = 0.0f, den = 0.0f;
  for (const auto& l : lm) {
    pistoris::ArxVector3 a = sub(l.target, ct);
    pistoris::ArxVector3 b = sub(l.reference, cr);
    num += l.weight * dot(a, b);
    den += l.weight * dot(a, a);
  }

  float scale = den > 1e-8f ? num / den : 1.0f;
  if (scale <= 0.0f || !std::isfinite(scale)) {
    std::fprintf(stderr, "--autosize-to-reference: invalid fitted scale %f\n", scale);
    return false;
  }
  pistoris::ArxVector3 offset = sub(cr, mul(ct, scale));

  for (auto& v : target.vertices) v.position = add(mul(v.position, scale), offset);

  float sum_sq  = 0.0f;
  float max_err = -1.0f;
  std::string worst;
  for (const auto& l : lm) {
    pistoris::ArxVector3 fitted = add(mul(l.target, scale), offset);
    float e                     = dist(fitted, l.reference);
    sum_sq += l.weight * e * e;
    if (e > max_err) {
      max_err = e;
      worst   = l.name;
    }
  }
  float rms = std::sqrt(sum_sq / wsum);
  std::fprintf(stdout, "autosize-to-reference: landmarks=%zu scale=%f offset=(%f,%f,%f) rms=%f max=%f (%s)\n",
               lm.size(), scale, offset.x, offset.y, offset.z, rms, max_err, worst.c_str());
  return true;
}

std::set<std::int32_t> syntheticVertices(const pistoris::Ftl& ftl) {
  std::set<std::int32_t> out;
  if (validVertex(ftl, ftl.header.origin)) out.insert(static_cast<std::int32_t>(ftl.header.origin));
  for (const auto& group : ftl.groups)
    if (validVertex(ftl, group.origin)) out.insert(static_cast<std::int32_t>(group.origin));
  for (const auto& action : ftl.actions)
    if (validVertex(ftl, action.vertex_idx)) out.insert(action.vertex_idx);
  return out;
}

bool deltaDeformBoneOrigins(pistoris::Ftl& target, const pistoris::Ftl& ref) {
  if (target.groups.size() != ref.groups.size()) {
    std::fprintf(stderr,
                 "--snap-bone-origins-to-reference delta-deform: group count mismatch target=%zu reference=%zu\n",
                 target.groups.size(), ref.groups.size());
    return false;
  }
  warnGroupMismatch(target, ref);

  std::vector<pistoris::ArxVector3> deltas(target.groups.size());
  float max_delta = 0.0f;
  float sum_delta = 0.0f;
  for (std::size_t i = 0; i < target.groups.size(); ++i) {
    if (!validVertex(target, target.groups[i].origin) || !validVertex(ref, ref.groups[i].origin)) {
      std::fprintf(stderr, "--snap-bone-origins-to-reference delta-deform: group %zu has invalid origin\n", i);
      return false;
    }
    const auto& target_pos = target.vertices[target.groups[i].origin].position;
    const auto& ref_pos    = ref.vertices[ref.groups[i].origin].position;
    deltas[i]              = sub(ref_pos, target_pos);
    float d                = dist(ref_pos, target_pos);
    max_delta              = std::max(max_delta, d);
    sum_delta += d;
  }

  std::set<std::int32_t> synthetic = syntheticVertices(target);
  std::vector<std::size_t> moved_per_group(target.groups.size(), 0);
  std::size_t moved   = 0;
  std::size_t unowned = 0;
  for (std::size_t vi = 0; vi < target.vertices.size(); ++vi) {
    if (synthetic.contains(static_cast<std::int32_t>(vi))) continue;
    int32_t owner = (vi < target.extras.vertex_to_bone.size()) ? target.extras.vertex_to_bone[vi] : -1;
    if (owner < 0 || static_cast<std::size_t>(owner) >= deltas.size()) {
      ++unowned;
      continue;
    }
    target.vertices[vi].position = add(target.vertices[vi].position, deltas[owner]);
    ++moved;
    ++moved_per_group[owner];
  }

  for (std::size_t i = 0; i < target.groups.size(); ++i)
    target.vertices[target.groups[i].origin].position = ref.vertices[ref.groups[i].origin].position;

  std::size_t empty_groups = 0;
  for (std::size_t count : moved_per_group)
    if (count == 0) ++empty_groups;

  float avg_delta = deltas.empty() ? 0.0f : sum_delta / static_cast<float>(deltas.size());
  std::fprintf(stdout,
               "snap-bone-origins-to-reference delta-deform: groups=%zu moved_vertices=%zu unowned_vertices=%zu "
               "empty_groups=%zu avg_delta=%f max_delta=%f\n",
               target.groups.size(), moved, unowned, empty_groups, avg_delta, max_delta);
  return true;
}

struct EdgeTransform {
  std::size_t parent = 0;
  std::size_t child  = 0;
  pistoris::ArxVector3 parent_current{};
  pistoris::ArxVector3 parent_reference{};
  pistoris::ArxVector3 child_current{};
  pistoris::ArxVector3 source_axis{};
  pistoris::ArxVector3 target_axis{};
  float source_len  = 0.0f;
  float target_len  = 0.0f;
  float axial_scale = 1.0f;
  float width_scale = 1.0f;
};

constexpr float kBoneEdgeEpsilon               = 1e-5f;
constexpr float kLargeHierarchyLengthScaleLow  = 0.5f;
constexpr float kLargeHierarchyLengthScaleHigh = 2.0f;

pistoris::ArxVector3 applyEdgeTransform(const EdgeTransform& edge, const pistoris::ArxVector3& point) {
  pistoris::ArxVector3 local        = sub(point, edge.parent_current);
  pistoris::ArxVector3 src_dir      = div(edge.source_axis, edge.source_len);
  pistoris::ArxVector3 dst_dir      = div(edge.target_axis, edge.target_len);
  float parallel_len                = dot(local, src_dir);
  pistoris::ArxVector3 parallel     = mul(dst_dir, parallel_len * edge.axial_scale);
  pistoris::ArxVector3 perp         = sub(local, mul(src_dir, parallel_len));
  pistoris::ArxVector3 rotated_perp = rotateBetween(perp, edge.source_axis, edge.target_axis);
  return add(edge.parent_reference, add(parallel, mul(rotated_perp, edge.width_scale)));
}

pistoris::ArxVector3 applyEdgeTransformRigid(const EdgeTransform& edge, const pistoris::ArxVector3& point) {
  pistoris::ArxVector3 local        = sub(point, edge.parent_current);
  pistoris::ArxVector3 src_dir      = div(edge.source_axis, edge.source_len);
  pistoris::ArxVector3 dst_dir      = div(edge.target_axis, edge.target_len);
  float parallel_len                = dot(local, src_dir);
  pistoris::ArxVector3 parallel     = mul(dst_dir, parallel_len);
  pistoris::ArxVector3 perp         = sub(local, mul(src_dir, parallel_len));
  pistoris::ArxVector3 rotated_perp = rotateBetween(perp, edge.source_axis, edge.target_axis);
  return add(edge.parent_reference, add(parallel, rotated_perp));
}

std::vector<std::vector<std::size_t>> buildChildren(const pistoris::Ftl& ftl) {
  std::vector<std::vector<std::size_t>> children(ftl.groups.size());
  for (std::size_t gi = 0; gi < ftl.extras.parent_bone.size(); ++gi) {
    int32_t parent = ftl.extras.parent_bone[gi];
    if (parent >= 0 && static_cast<std::size_t>(parent) < children.size()) children[parent].push_back(gi);
  }
  return children;
}

bool isInSubtree(std::size_t group, std::size_t root, const std::vector<int32_t>& parent) {
  std::size_t cur = group;
  while (cur < parent.size()) {
    if (cur == root) return true;
    int32_t p = parent[cur];
    if (p < 0) return false;
    cur = static_cast<std::size_t>(p);
  }
  return false;
}

std::vector<std::size_t> collectRoots(const pistoris::Ftl& ftl) {
  std::vector<std::size_t> roots;
  for (std::size_t gi = 0; gi < ftl.groups.size(); ++gi)
    if (gi >= ftl.extras.parent_bone.size() || ftl.extras.parent_bone[gi] < 0) roots.push_back(gi);
  return roots;
}

bool sameTopology(const pistoris::Ftl& target, const pistoris::Ftl& ref) {
  if (target.extras.parent_bone.size() != ref.extras.parent_bone.size()) return false;
  for (std::size_t i = 0; i < target.extras.parent_bone.size(); ++i)
    if (target.extras.parent_bone[i] != ref.extras.parent_bone[i]) return false;
  return true;
}

float medianDirectChildLengthRatio(const pistoris::Ftl& target, const pistoris::Ftl& ref, std::size_t root,
                                   const std::vector<std::vector<std::size_t>>& children) {
  std::vector<float> ratios;
  if (root >= children.size()) return 1.0f;
  for (std::size_t child : children[root]) {
    if (!validVertex(target, target.groups[root].origin) || !validVertex(target, target.groups[child].origin) ||
        !validVertex(ref, ref.groups[root].origin) || !validVertex(ref, ref.groups[child].origin))
      continue;
    float src = dist(target.vertices[target.groups[child].origin].position,
                     target.vertices[target.groups[root].origin].position);
    float dst = dist(ref.vertices[ref.groups[child].origin].position, ref.vertices[ref.groups[root].origin].position);
    if (src > 1e-5f && dst > 1e-5f) ratios.push_back(dst / src);
  }
  if (ratios.empty()) return 1.0f;
  std::sort(ratios.begin(), ratios.end());
  return ratios[ratios.size() / 2];
}

bool hierarchyDeformBoneOrigins(pistoris::Ftl& target, const pistoris::Ftl& ref, std::size_t step_limit) {
  if (target.groups.size() != ref.groups.size()) {
    std::fprintf(stderr,
                 "--snap-bone-origins-to-reference hierarchy-deform: group count mismatch target=%zu reference=%zu\n",
                 target.groups.size(), ref.groups.size());
    return false;
  }
  if (target.groups.empty()) {
    std::fprintf(stdout, "snap-bone-origins-to-reference hierarchy-deform: no bone groups\n");
    return true;
  }
  if (!sameTopology(target, ref)) {
    std::fprintf(stderr, "--snap-bone-origins-to-reference hierarchy-deform: parent topology mismatch\n");
    return false;
  }
  warnGroupMismatch(target, ref);

  for (std::size_t i = 0; i < target.groups.size(); ++i)
    if (!validVertex(target, target.groups[i].origin) || !validVertex(ref, ref.groups[i].origin)) {
      std::fprintf(stderr, "--snap-bone-origins-to-reference hierarchy-deform: group %zu has invalid origin\n", i);
      return false;
    }

  std::vector<std::size_t> roots = collectRoots(target);
  if (roots.size() != 1) {
    std::fprintf(stderr, "--snap-bone-origins-to-reference hierarchy-deform: expected exactly one root, got %zu\n",
                 roots.size());
    return false;
  }

  const std::set<std::int32_t> synthetic         = syntheticVertices(target);
  std::vector<std::vector<std::size_t>> children = buildChildren(target);
  std::vector<float> width(target.groups.size(), 1.0f);
  std::set<std::size_t> moved_vertices;
  std::set<std::size_t> moved_synthetic;
  std::size_t zero_edges         = 0;
  std::size_t alias_edges        = 0;
  std::size_t large_ratio_edges  = 0;
  std::size_t processed_steps    = 0;
  float max_presnap              = 0.0f;
  float max_length_scale         = 0.0f;
  float sum_length_scale         = 0.0f;
  std::size_t length_scale_count = 0;

  std::size_t root = roots.front();
  width[root]      = std::clamp(medianDirectChildLengthRatio(target, ref, root, children), 0.7f, 1.3f);
  pistoris::ArxVector3 root_delta =
      sub(ref.vertices[ref.groups[root].origin].position, target.vertices[target.groups[root].origin].position);

  for (std::size_t vi = 0; vi < target.vertices.size(); ++vi) {
    bool should_move = false;
    int32_t owner    = (vi < target.extras.vertex_to_bone.size()) ? target.extras.vertex_to_bone[vi] : -1;
    if (owner >= 0 && static_cast<std::size_t>(owner) < target.groups.size() &&
        isInSubtree(static_cast<std::size_t>(owner), root, target.extras.parent_bone)) {
      should_move = true;
    }
    if (!should_move && synthetic.contains(static_cast<std::int32_t>(vi))) should_move = true;
    if (!should_move) continue;
    target.vertices[vi].position = add(target.vertices[vi].position, root_delta);
    if (synthetic.contains(static_cast<std::int32_t>(vi)))
      moved_synthetic.insert(vi);
    else
      moved_vertices.insert(vi);
  }
  target.vertices[target.groups[root].origin].position = ref.vertices[ref.groups[root].origin].position;

  auto move_subtree_with_delta = [&](std::size_t child_root, const pistoris::ArxVector3& delta) {
    for (std::size_t vi = 0; vi < target.vertices.size(); ++vi) {
      bool should_move = false;
      int32_t owner    = (vi < target.extras.vertex_to_bone.size()) ? target.extras.vertex_to_bone[vi] : -1;
      if (owner >= 0 && static_cast<std::size_t>(owner) < target.groups.size() &&
          isInSubtree(static_cast<std::size_t>(owner), child_root, target.extras.parent_bone)) {
        should_move = true;
      }
      if (!should_move && synthetic.contains(static_cast<std::int32_t>(vi))) {
        for (std::size_t gi = child_root; gi < target.groups.size(); ++gi) {
          if (!isInSubtree(gi, child_root, target.extras.parent_bone)) continue;
          if (static_cast<std::int32_t>(target.groups[gi].origin) == static_cast<std::int32_t>(vi)) {
            should_move = true;
            break;
          }
        }
        for (const auto& action : target.actions) {
          if (action.vertex_idx != static_cast<std::int32_t>(vi)) continue;
          int32_t action_owner = (vi < target.extras.vertex_to_bone.size()) ? target.extras.vertex_to_bone[vi] : -1;
          if (action_owner >= 0 && static_cast<std::size_t>(action_owner) < target.groups.size() &&
              isInSubtree(static_cast<std::size_t>(action_owner), child_root, target.extras.parent_bone)) {
            should_move = true;
          }
        }
      }
      if (!should_move) continue;
      target.vertices[vi].position = add(target.vertices[vi].position, delta);
      if (synthetic.contains(static_cast<std::int32_t>(vi)))
        moved_synthetic.insert(vi);
      else
        moved_vertices.insert(vi);
    }
  };

  auto move_subtree_rigid = [&](std::size_t child_root, const EdgeTransform& edge) {
    for (std::size_t vi = 0; vi < target.vertices.size(); ++vi) {
      bool should_move = false;
      int32_t owner    = (vi < target.extras.vertex_to_bone.size()) ? target.extras.vertex_to_bone[vi] : -1;
      if (owner >= 0 && static_cast<std::size_t>(owner) < target.groups.size() &&
          isInSubtree(static_cast<std::size_t>(owner), child_root, target.extras.parent_bone)) {
        should_move = true;
      }
      if (!should_move && synthetic.contains(static_cast<std::int32_t>(vi))) {
        for (std::size_t gi = child_root; gi < target.groups.size(); ++gi) {
          if (!isInSubtree(gi, child_root, target.extras.parent_bone)) continue;
          if (static_cast<std::int32_t>(target.groups[gi].origin) == static_cast<std::int32_t>(vi)) {
            should_move = true;
            break;
          }
        }
        for (const auto& action : target.actions) {
          if (action.vertex_idx != static_cast<std::int32_t>(vi)) continue;
          int32_t action_owner = (vi < target.extras.vertex_to_bone.size()) ? target.extras.vertex_to_bone[vi] : -1;
          if (action_owner >= 0 && static_cast<std::size_t>(action_owner) < target.groups.size() &&
              isInSubtree(static_cast<std::size_t>(action_owner), child_root, target.extras.parent_bone)) {
            should_move = true;
          }
        }
      }
      if (!should_move) continue;
      target.vertices[vi].position = applyEdgeTransformRigid(edge, target.vertices[vi].position);
      if (synthetic.contains(static_cast<std::int32_t>(vi)))
        moved_synthetic.insert(vi);
      else
        moved_vertices.insert(vi);
    }
  };

  auto print_summary = [&]() {
    float avg_length_scale = length_scale_count == 0 ? 1.0f : sum_length_scale / static_cast<float>(length_scale_count);
    std::fprintf(stdout,
                 "snap-bone-origins-to-reference hierarchy-deform: groups=%zu moved_vertices=%zu moved_synthetic=%zu "
                 "steps=%zu avg_length_scale=%f max_length_scale=%f max_origin_presnap_delta=%f alias_edges=%zu "
                 "zero_target_edges=%zu large_ratio_edges=%zu\n",
                 target.groups.size(), moved_vertices.size(), moved_synthetic.size(), processed_steps, avg_length_scale,
                 max_length_scale, max_presnap, alias_edges, zero_edges, large_ratio_edges);
  };

  if (step_limit == 0) {
    std::fprintf(stdout, "snap-bone-origins-to-reference hierarchy-deform: stopped after debug step limit 0\n");
    print_summary();
    return true;
  }

  for (std::size_t parent = 0; parent < target.groups.size(); ++parent) {
    if (parent >= children.size() || children[parent].empty()) continue;

    std::vector<EdgeTransform> edges;
    for (std::size_t child : children[parent]) {
      if (processed_steps >= step_limit) {
        std::fprintf(stdout, "snap-bone-origins-to-reference hierarchy-deform: stopped after debug step limit %zu\n",
                     step_limit);
        print_summary();
        return true;
      }
      EdgeTransform edge;
      edge.parent           = parent;
      edge.child            = child;
      edge.parent_current   = target.vertices[target.groups[parent].origin].position;
      edge.parent_reference = ref.vertices[ref.groups[parent].origin].position;
      edge.child_current    = target.vertices[target.groups[child].origin].position;
      edge.source_axis      = sub(edge.child_current, edge.parent_current);
      edge.target_axis      = sub(ref.vertices[ref.groups[child].origin].position, edge.parent_reference);
      edge.source_len       = length(edge.source_axis);
      edge.target_len       = length(edge.target_axis);
      max_presnap = std::max(max_presnap, dist(edge.child_current, ref.vertices[ref.groups[child].origin].position));
      if (edge.target_len <= kBoneEdgeEpsilon) {
        std::fprintf(stderr,
                     "snap-bone-origins-to-reference hierarchy-deform: aliased zero-length reference edge %s -> %s; "
                     "translated subtree to aliased child origin\n",
                     target.groups[parent].name, target.groups[child].name);
        move_subtree_with_delta(child, sub(ref.vertices[ref.groups[child].origin].position, edge.child_current));
        target.vertices[target.groups[child].origin].position = ref.vertices[ref.groups[child].origin].position;
        width[child]                                          = width[parent];
        ++alias_edges;
        ++processed_steps;
        continue;
      }
      if (edge.source_len <= kBoneEdgeEpsilon) {
        std::fprintf(stderr,
                     "snap-bone-origins-to-reference hierarchy-deform: zero-length target edge %s -> %s; "
                     "translated subtree to child origin\n",
                     target.groups[parent].name, target.groups[child].name);
        move_subtree_with_delta(child, sub(ref.vertices[ref.groups[child].origin].position, edge.child_current));
        target.vertices[target.groups[child].origin].position = ref.vertices[ref.groups[child].origin].position;
        width[child]                                          = width[parent];
        ++zero_edges;
        ++processed_steps;
        continue;
      }
      edge.axial_scale = edge.target_len / edge.source_len;
      if (edge.axial_scale < kLargeHierarchyLengthScaleLow || edge.axial_scale > kLargeHierarchyLengthScaleHigh) {
        std::fprintf(stderr,
                     "snap-bone-origins-to-reference hierarchy-deform: large length scale %.3f on edge %s -> %s; "
                     "propagating rigid subtree and applying scale locally\n",
                     edge.axial_scale, target.groups[parent].name, target.groups[child].name);
        ++large_ratio_edges;
      }
      edge.width_scale = std::clamp(width[parent] * std::pow(edge.axial_scale, 0.25f), 0.7f, 1.3f);
      width[child]     = edge.width_scale;
      max_length_scale = std::max(max_length_scale, edge.axial_scale);
      sum_length_scale += edge.axial_scale;
      ++length_scale_count;
      move_subtree_rigid(edge.child, edge);
      target.vertices[target.groups[edge.child].origin].position = ref.vertices[ref.groups[edge.child].origin].position;
      edges.push_back(edge);
      ++processed_steps;
    }

    for (std::size_t vi = 0; vi < target.vertices.size(); ++vi) {
      if (synthetic.contains(static_cast<std::int32_t>(vi))) continue;
      int32_t owner = (vi < target.extras.vertex_to_bone.size()) ? target.extras.vertex_to_bone[vi] : -1;
      if (owner != static_cast<int32_t>(parent)) continue;

      pistoris::ArxVector3 original = target.vertices[vi].position;
      float parent_w =
          2.0f / std::max(1e-4f, lengthSquared(sub(original, target.vertices[target.groups[parent].origin].position)));
      pistoris::ArxVector3 accum = mul(original, parent_w);
      float wsum                 = parent_w;
      for (const auto& edge : edges) {
        float w = 1.0f / std::max(1e-4f, lengthSquared(sub(original, edge.child_current)));
        accum   = add(accum, mul(applyEdgeTransform(edge, original), w));
        wsum += w;
      }
      target.vertices[vi].position = div(accum, wsum);
      moved_vertices.insert(vi);
    }
  }

  print_summary();
  return true;
}

}  // namespace

bool applyModules(Context& ctx, const CliArgs& args) {
  if (referenceOperationRequested(args)) {
    if (!requireReference(ctx)) return false;
  }

  if (args.has_xform) {
    pistoris::AffineXform xform = cli::makeAffineXform(args);
    ArxReturnCode rc            = pistoris::applyTransform(ctx.ftl, xform);
    if (rc != ARX_OK) {
      std::fprintf(stderr, "FTL transform failed: %s (code %d)\n", pistoris::errorString(rc), static_cast<int>(rc));
      return false;
    }
    for (pistoris::Tea& tea : ctx.teas) {
      rc = pistoris::applyTransform(tea, xform);
      if (rc != ARX_OK) {
        std::fprintf(stderr, "TEA transform failed: %s (code %d)\n", pistoris::errorString(rc), static_cast<int>(rc));
        return false;
      }
    }
  }

  if (args.overwrite_texture) {
    ArxReturnCode rc = pistoris::overwriteTexturePaths(ctx.ftl, args.overwrite_texture);
    if (rc != ARX_OK) {
      std::fprintf(stderr, "Texture overwrite failed: %s (code %d)\n", pistoris::errorString(rc), static_cast<int>(rc));
      return false;
    }
  }

  if (args.rename_selections) {
    if (!renameSelections(ctx.ftl, args.rename_selections)) return false;
  }

  if (args.autosize_to_reference) {
    if (!autosizeToReference(ctx.ftl, ctx.reference_ftl)) return false;
  }

  switch (args.bone_origin_reference_mode) {
    case BoneOriginReferenceMode::kNone:
      break;
    case BoneOriginReferenceMode::kSnapOrigins: {
      ArxReturnCode rc = pistoris::snapFtlBoneOriginsToReference(ctx.ftl, ctx.reference_ftl);
      if (rc != ARX_OK) {
        std::fprintf(stderr, "--snap-bone-origins-to-reference failed: %s (code %d)\n", pistoris::errorString(rc),
                     static_cast<int>(rc));
        return false;
      }
      break;
    }
    case BoneOriginReferenceMode::kDeltaDeform: {
      if (!deltaDeformBoneOrigins(ctx.ftl, ctx.reference_ftl)) return false;
      break;
    }
    case BoneOriginReferenceMode::kHierarchyDeform: {
      if (!hierarchyDeformBoneOrigins(ctx.ftl, ctx.reference_ftl, args.hierarchy_deform_step_limit)) return false;
      break;
    }
  }

  if (args.snap_action_points) {
    ArxReturnCode rc = pistoris::snapFtlActionPointsToReference(ctx.ftl, ctx.reference_ftl);
    if (rc != ARX_OK) {
      std::fprintf(stderr, "--snap-action-points-to-reference failed: %s (code %d)\n", pistoris::errorString(rc),
                   static_cast<int>(rc));
      return false;
    }
  }

  if (args.copy_reference_affiliations) {
    ArxReturnCode rc = pistoris::copyFtlSyntheticSelectionAffiliations(ctx.ftl, ctx.reference_ftl);
    if (rc != ARX_OK) {
      std::fprintf(stderr, "--copy-reference-affiliations failed: %s (code %d)\n", pistoris::errorString(rc),
                   static_cast<int>(rc));
      return false;
    }
  }

  return true;
}

}  // namespace cli::model
