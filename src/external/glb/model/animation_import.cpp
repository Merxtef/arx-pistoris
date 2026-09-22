// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "animation_import.h"

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "animation/data.h"
#include "animation/internal.h"
#include "cgltf/cgltf.h"
#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/model/animation_identity.h"
#include "external/glb/model/discovery.h"
#include "external/glb/model/internal.h"
#include "external/glb/node_graph.h"
#include "external/glb/object_coordinates.h"
#include "external/glb/utils/names.h"
#include "external/glb/utils/sound.h"
#include "model/data.h"
#include "modules/animation.h"
#include "modules/resource.h"
#include "modules/sounds.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/math/mat4.h"
#include "utils/math/quat.h"
#include "utils/name_tokens.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris::glb_model {
using glb_object::toArxPoint;
namespace {

constexpr std::string_view kPathPrefix = "PATH_";
constexpr std::string_view kSound = "SOUND";
constexpr std::string_view kFramesPrefix = "FRAMES_";
constexpr std::string_view kSettings = "SETTINGS";
constexpr std::string_view kExtraFrame = "EXTRA_FRAME";
constexpr std::string_view kFrameLengthPrefix = "FRAME_LENGTH_";
constexpr std::string_view kStepsPrefix = "STEPS_";
constexpr std::string_view kGroups = "GROUPS";
constexpr std::string_view kVoidPrefix = "VOID_";
constexpr std::string_view kClaimPrefix = "CLAIM_";

ArxReturnCode soundPathImportError(glb::SoundPathImportError error) noexcept {
  switch (error) {
    case glb::SoundPathImportError::kNone:
      return ARX_OK;
    case glb::SoundPathImportError::kBadPath:
      return ARX_ANIMATION_BAD_SOUND_PATH;
    case glb::SoundPathImportError::kTooManySounds:
      return ARX_ANIMATION_TOO_MANY_SOUNDS;
    case glb::SoundPathImportError::kBadKind:
      return ARX_INTERNAL_ERROR;
  }
  return ARX_INTERNAL_ERROR;
}

struct Trs {
  ArxVector3 translation{};
  ArxQuat rotation{};
  ArxVector3 scale{1.0f, 1.0f, 1.0f};
};

struct ResolvedSampler {
  const glb::AccessorView* times = nullptr;
  const glb::AccessorView* values = nullptr;
  cgltf_interpolation_type interpolation = cgltf_interpolation_type_linear;
};

struct NodeChannels {
  int translation = -1;
  int rotation = -1;
  int scale = -1;
};

enum class TimingKind : std::uint8_t {
  kNone,
  kExtra,
  kExact,
};

struct TimingData {
  TimingKind kind = TimingKind::kNone;
  std::uint32_t frame_length = 0;
};

struct HelperData {
  std::string path;
  TimingData timing;
  std::unordered_set<std::uint32_t> footsteps;
  std::map<std::uint32_t, std::string> sounds;
  std::bitset<animation::kMaxGroups> void_groups;
  std::bitset<animation::kMaxGroups> claimed_groups;
};

struct GroupPatch {
  std::bitset<animation::kMaxGroups> void_groups;
  std::bitset<animation::kMaxGroups> claimed_groups;
};

struct SettingsPatch {
  TimingData timing;
  std::vector<std::uint32_t> footsteps;
};

struct HelperEntry {
  std::size_t node = glb::kInvalidNodeIndex;
  std::size_t count = 0;
  bool used = false;
};

struct AnimationImportContext {
  std::size_t root = glb::kInvalidNodeIndex;
  std::size_t motion_node = glb::kInvalidNodeIndex;
  std::vector<std::size_t> bone_nodes;
  std::vector<std::uint8_t> relevant_nodes;
  std::vector<std::size_t> relevant_preorder;
  std::vector<Trs> bind_local;
  std::vector<ArxQuat> bind_world_rotations;
  math::Mat4 inverse_root_bind = math::kIdentityMat4;
  math::Mat4 motion_bind = math::kIdentityMat4;
  math::Mat4 inverse_motion_bind = math::kIdentityMat4;
  ArxQuat motion_bind_rotation{};
  std::vector<Trs> sampled_local;
  std::vector<math::Mat4> sampled_world;
  std::vector<ArxQuat> sampled_world_rotations;
  std::vector<ArxQuat> bone_animation_rotations;
  std::vector<ArxVector3> bone_animation_positions;
  std::vector<math::Mat4> bone_animation_linears;
};

std::size_t discoverMotionNode(const glb::NodeGraph& graph, const ModelDiscovery& discovery,
                               std::span<const BoneIndex> node_bones) {
  if (discovery.root == glb::kInvalidNodeIndex) return glb::kInvalidNodeIndex;
  if (std::ranges::none_of(node_bones, [](BoneIndex bone) { return bone != kInvalidBoneIndex; })) return discovery.root;
  std::size_t common = glb::kInvalidNodeIndex;
  const auto include = [&](std::size_t node) {
    if (!glb::isDescendantOrSelf(graph, node, discovery.root)) return;
    common = common == glb::kInvalidNodeIndex ? node : glb::nearestCommonAncestor(graph, common, node);
  };
  for (std::size_t node = 0; node < node_bones.size(); ++node)
    if (node_bones[node] != kInvalidBoneIndex) include(node);
  for (std::size_t node : discovery.mesh_nodes) include(node);
  for (std::size_t node : discovery.action_nodes) include(node);
  for (std::size_t node : discovery.probe_nodes) include(node);
  if (common == glb::kInvalidNodeIndex) return discovery.root;
  while (common != discovery.root && node_bones[common] != kInvalidBoneIndex) common = graph.parent[common];
  return common;
}

std::size_t nodeIndex(const cgltf_data& data, const cgltf_node* node) {
  return static_cast<std::size_t>(node - data.nodes);
}

bool parseUnsigned(std::string_view text, std::uint32_t& out) {
  if (text.empty()) return false;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), out);
  return error == std::errc{} && end == text.data() + text.size();
}

bool parseFrameList(std::string_view text, std::vector<std::uint32_t>& out) {
  if (text.empty()) return false;
  while (true) {
    const std::size_t separator = text.find('_');
    std::uint32_t frame = 0;
    if (!parseUnsigned(text.substr(0, separator), frame)) return false;
    out.push_back(frame);
    if (separator == std::string_view::npos) return true;
    text.remove_prefix(separator + 1U);
    if (text.empty()) return false;
  }
}

bool parseGroupList(std::string_view text, std::bitset<animation::kMaxGroups>& groups,
                    const std::bitset<animation::kMaxGroups>& conflicts) {
  if (text.empty()) return false;
  while (true) {
    const std::size_t separator = text.find('_');
    const std::string_view item = text.substr(0, separator);
    const std::size_t dash = item.find('-');
    std::uint32_t first = 0;
    std::uint32_t last = 0;
    if (dash == std::string_view::npos) {
      if (!parseUnsigned(item, first)) return false;
      last = first;
    } else {
      if (item.find('-', dash + 1U) != std::string_view::npos || !parseUnsigned(item.substr(0, dash), first) ||
          !parseUnsigned(item.substr(dash + 1U), last) || first > last)
        return false;
    }
    if (last >= animation::kMaxGroups) return false;
    for (std::uint32_t group = first; group <= last; ++group) {
      if (conflicts.test(group)) return false;
      groups.set(group);
    }
    if (separator == std::string_view::npos) return true;
    text.remove_prefix(separator + 1U);
    if (text.empty()) return false;
  }
}

bool parseGroups(std::string_view name, HelperData& out) {
  GroupPatch patch;
  glb::ParsedLabel label;
  const bool valid = glb::parseRecoverableLabel(
      name,
      patch,
      &label,
      {{}, {kVoidPrefix, kClaimPrefix}},
      [](std::span<const std::string_view> tokens, GroupPatch& parsed) {
        if (tokens.size() < 2U || tokens.front() != kGroups) return false;
        for (std::string_view token : tokens.subspan(1)) {
          if (token.starts_with(kVoidPrefix)) {
            if (!parseGroupList(token.substr(kVoidPrefix.size()), parsed.void_groups, parsed.claimed_groups))
              return false;
          } else if (token.starts_with(kClaimPrefix)) {
            if (!parseGroupList(token.substr(kClaimPrefix.size()), parsed.claimed_groups, parsed.void_groups))
              return false;
          } else {
            return false;
          }
        }
        return parsed.void_groups.any() || parsed.claimed_groups.any();
      });
  if (!valid || (patch.void_groups & out.claimed_groups).any() || (patch.claimed_groups & out.void_groups).any())
    return false;
  out.void_groups |= patch.void_groups;
  out.claimed_groups |= patch.claimed_groups;
  glb::reportConventionLabel("GLB -> Model", name, label);
  return true;
}

bool setTiming(TimingData& out, TimingKind kind, std::uint32_t length = 0) {
  if (out.kind == TimingKind::kNone) {
    out.kind = kind;
    out.frame_length = length;
    return true;
  }
  return out.kind == kind && (kind != TimingKind::kExact || out.frame_length == length);
}

bool parseSettings(std::string_view name, HelperData& out) {
  SettingsPatch patch;
  glb::ParsedLabel label;
  const bool valid = glb::parseRecoverableLabel(
      name,
      patch,
      &label,
      {{kExtraFrame}, {kFrameLengthPrefix, kStepsPrefix}},
      [](std::span<const std::string_view> tokens, SettingsPatch& parsed) {
        if (tokens.empty() || tokens.front() != kSettings) return false;
        for (std::string_view token : tokens.subspan(1)) {
          if (token == kExtraFrame) {
            if (!setTiming(parsed.timing, TimingKind::kExtra)) return false;
            continue;
          }
          if (token.starts_with(kFrameLengthPrefix)) {
            std::uint32_t length = 0;
            if (!parseUnsigned(token.substr(kFrameLengthPrefix.size()), length) ||
                length > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
                !setTiming(parsed.timing, TimingKind::kExact, length))
              return false;
            continue;
          }
          if (token.starts_with(kStepsPrefix)) {
            std::vector<std::uint32_t> frames;
            if (!parseFrameList(token.substr(kStepsPrefix.size()), frames)) return false;
            parsed.footsteps.insert(parsed.footsteps.end(), frames.begin(), frames.end());
            continue;
          }
          return false;
        }
        return true;
      });
  if (!valid ||
      (patch.timing.kind != TimingKind::kNone && !setTiming(out.timing, patch.timing.kind, patch.timing.frame_length)))
    return false;
  out.footsteps.insert(patch.footsteps.begin(), patch.footsteps.end());
  glb::reportConventionLabel("GLB -> Model", name, label);
  return true;
}

bool parseSound(const cgltf_node& node, HelperData& out) {
  struct SoundName {
    std::vector<std::uint32_t> frames;
  };
  SoundName parsed_name;
  glb::ParsedLabel label;
  if (!glb::parseRecoverableLabel(node.name,
                                  parsed_name,
                                  &label,
                                  {{}, {kFramesPrefix}},
                                  [](std::span<const std::string_view> tokens, SoundName& parsed) {
                                    return tokens.size() == 2U && tokens[0] == kSound &&
                                           tokens[1].starts_with(kFramesPrefix) &&
                                           parseFrameList(tokens[1].substr(kFramesPrefix.size()), parsed.frames);
                                  }))
    return false;
  std::string_view sample;
  for (std::size_t index = 0; index < node.children_count; ++index) {
    const cgltf_node* child = node.children[index];
    if (child == nullptr || child->name == nullptr) continue;
    const std::string_view name(child->name);
    if (name.starts_with(kPathPrefix)) {
      const auto parsed = glb::splitRequiredLabel(name.substr(kPathPrefix.size()));
      if (!sample.empty() || !parsed) return false;
      sample = parsed->value;
    } else {
      log(ARX_LOG_WARN, "GLB -> Model: sound helper '{}' has unrecognized child '{}'; ignored", node.name, name);
    }
  }
  if (sample.empty()) return false;
  for (std::uint32_t frame : parsed_name.frames) {
    const auto [found, inserted] = out.sounds.emplace(frame, sample);
    if (!inserted && found->second != sample) return false;
  }
  glb::reportConventionLabel("GLB -> Model", node.name, label);
  return true;
}

bool parseHelper(const cgltf_node& node, HelperData& out) {
  bool path_set = false;
  for (std::size_t index = 0; index < node.children_count; ++index) {
    const cgltf_node* child = node.children[index];
    if (child == nullptr || child->name == nullptr) continue;
    const std::string_view name(child->name);
    std::vector<std::string_view> tokens;
    splitDoubleUnderscore(name, tokens);
    const std::string_view first = tokens.empty() ? std::string_view{} : tokens.front();
    if (first.starts_with(kPathPrefix)) {
      const auto parsed = glb::splitRequiredLabel(name.substr(kPathPrefix.size()));
      if (path_set || !parsed) return false;
      out.path = parsed->value;
      path_set = true;
    } else if (first == kSettings) {
      if (!parseSettings(name, out)) return false;
    } else if (first == kSound) {
      if (!parseSound(*child, out)) return false;
    } else if (first == kGroups) {
      if (!parseGroups(name, out)) return false;
    } else {
      log(ARX_LOG_WARN,
          "GLB -> Model: animation helper '{}' has unrecognized child '{}'; ignored",
          node.name ? node.name : "<unnamed>",
          name);
    }
  }
  return true;
}

bool decompose(const math::Mat4& matrix, Trs& out) {
  out.translation = math::translation(matrix);
  ArxVector3 columns[3] = {
      {matrix(0, 0), matrix(1, 0), matrix(2, 0)},
      {matrix(0, 1), matrix(1, 1), matrix(2, 1)},
      {matrix(0, 2), matrix(1, 2), matrix(2, 2)},
  };
  if (!math::finite(out.translation) || !math::finite(columns[0]) || !math::finite(columns[1]) ||
      !math::finite(columns[2]))
    return false;
  out.scale = {math::lengthf(columns[0]), math::lengthf(columns[1]), math::lengthf(columns[2])};
  if (out.scale.x <= 1.0e-6f || out.scale.y <= 1.0e-6f || out.scale.z <= 1.0e-6f) return false;
  columns[0] = columns[0] * (1.0f / out.scale.x);
  columns[1] = columns[1] * (1.0f / out.scale.y);
  columns[2] = columns[2] * (1.0f / out.scale.z);
  if (std::abs(math::dotf(columns[0], columns[1])) > 1.0e-4f ||
      std::abs(math::dotf(columns[0], columns[2])) > 1.0e-4f || std::abs(math::dotf(columns[1], columns[2])) > 1.0e-4f)
    return false;
  float determinant = math::dotf(columns[0], math::cross(columns[1], columns[2]));
  if (std::abs(std::abs(determinant) - 1.0f) > 1.0e-3f) return false;
  if (determinant < 0.0f) {
    columns[0] = columns[0] * -1.0f;
    out.scale.x = -out.scale.x;
  }
  ArxMat3 rotation{};
  rotation(0, 0) = columns[0].x;
  rotation(1, 0) = columns[0].y;
  rotation(2, 0) = columns[0].z;
  rotation(0, 1) = columns[1].x;
  rotation(1, 1) = columns[1].y;
  rotation(2, 1) = columns[1].z;
  rotation(0, 2) = columns[2].x;
  rotation(1, 2) = columns[2].y;
  rotation(2, 2) = columns[2].z;
  out.rotation = math::rotationToQuat(rotation);
  return true;
}

bool localTrs(const cgltf_node& node, Trs& out) {
  if (!node.has_matrix) {
    out.translation = {node.translation[0], node.translation[1], node.translation[2]};
    out.rotation = {node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]};
    out.scale = {node.scale[0], node.scale[1], node.scale[2]};
    const float rotation_length = std::sqrt(out.rotation.w * out.rotation.w + out.rotation.x * out.rotation.x +
                                            out.rotation.y * out.rotation.y + out.rotation.z * out.rotation.z);
    if (!math::finite(out.translation) || !math::finite(out.scale) || !std::isfinite(rotation_length) ||
        rotation_length <= 1.0e-6f)
      return false;
    out.rotation = math::normalize(out.rotation);
    return true;
  }
  cgltf_float values[16];
  cgltf_node_transform_local(&node, values);
  math::Mat4 matrix{};
  std::copy_n(values, 16U, matrix.m);
  return decompose(matrix, out);
}

void readValue(const glb::AccessorView& values, std::size_t index, float* out, std::size_t components) {
  std::copy_n(values.floats.data() + index * components, components, out);
}

void normalizeRotation(float* values) {
  const ArxQuat rotation = math::normalize({values[3], values[0], values[1], values[2]});
  values[0] = rotation.x;
  values[1] = rotation.y;
  values[2] = rotation.z;
  values[3] = rotation.w;
}

bool validRotations(const glb::AccessorView& values) {
  constexpr float kMinNormSquared = 1.0e-12f;
  for (std::size_t index = 0; index < values.count; ++index) {
    const float* value = values.floats.data() + index * 4U;
    const float norm_squared = value[0] * value[0] + value[1] * value[1] + value[2] * value[2] + value[3] * value[3];
    if (!std::isfinite(norm_squared) || norm_squared <= kMinNormSquared) return false;
  }
  return true;
}

void sample(const ResolvedSampler& sampler, float time, float* out, std::size_t components, bool rotation) {
  const std::span<const float> times(sampler.times->floats);
  if (time <= times.front()) {
    readValue(*sampler.values, 0, out, components);
    if (rotation) normalizeRotation(out);
    return;
  }
  if (time >= times.back()) {
    readValue(*sampler.values, times.size() - 1U, out, components);
    if (rotation) normalizeRotation(out);
    return;
  }
  const auto upper = std::upper_bound(times.begin(), times.end(), time);
  const std::size_t second = static_cast<std::size_t>(upper - times.begin());
  const std::size_t first = second - 1U;
  if (sampler.interpolation == cgltf_interpolation_type_step) {
    readValue(*sampler.values, first, out, components);
    if (rotation) normalizeRotation(out);
    return;
  }
  const float amount = (time - times[first]) / (times[second] - times[first]);
  float left[4]{};
  float right[4]{};
  readValue(*sampler.values, first, left, components);
  readValue(*sampler.values, second, right, components);
  if (rotation) {
    const ArxQuat interpolated =
        math::slerp({left[3], left[0], left[1], left[2]}, {right[3], right[0], right[1], right[2]}, amount);
    out[0] = interpolated.x;
    out[1] = interpolated.y;
    out[2] = interpolated.z;
    out[3] = interpolated.w;
  } else {
    for (std::size_t component = 0; component < components; ++component)
      out[component] = left[component] + (right[component] - left[component]) * amount;
  }
}

ArxQuat toArxRotation(const ArxQuat& rotation) {
  constexpr ArxQuat kBasis{0.0f, 1.0f, 0.0f, 0.0f};
  return kBasis * rotation * math::conjugate(kBasis);
}

double frameRoundingTolerance(float time, float shift) {
  const auto ulp = [](float value) {
    const float up = std::nextafter(value, std::numeric_limits<float>::infinity());
    const float down = std::nextafter(value, -std::numeric_limits<float>::infinity());
    return std::max(std::abs(static_cast<double>(up) - value), std::abs(static_cast<double>(value) - down));
  };
  constexpr double kMinimumFrameTolerance = 1.0e-5;
  return std::max(kMinimumFrameTolerance, (ulp(time) + ulp(shift)) * static_cast<double>(kTeaFps) * 2.0);
}

void canonicalizeIdentityGroups(AnimationData& animation, const HelperData& helper, std::string_view name) {
  const std::size_t source_count = animation.group_count;
  std::size_t retained_count = 0;
  for (std::size_t group = 0; group < source_count; ++group) {
    if (helper.claimed_groups.test(group)) {
      animation.claimed_groups.set(group);
      retained_count = group + 1U;
      continue;
    }
    bool exact_identity = true;
    bool tolerance_identity = true;
    for (std::size_t frame = 0; frame < animation.keyframes.size(); ++frame) {
      const AnimationGroupTransform& transform = animation.group_transforms[frame * source_count + group];
      exact_identity = exact_identity && animation::isIdentityTransform(transform);
      tolerance_identity = tolerance_identity && isAnimationGlbIdentity(transform);
    }
    if (helper.void_groups.test(group)) {
      if (!exact_identity)
        log(ARX_LOG_WARN,
            "GLB -> Model: animation '{}' group {} marked VOID; sampled transform discarded",
            name,
            group);
    } else if (!tolerance_identity) {
      retained_count = group + 1U;
      continue;
    }
    for (std::size_t frame = 0; frame < animation.keyframes.size(); ++frame)
      animation.group_transforms[frame * source_count + group] = {};
  }
  if (retained_count == source_count) return;
  for (std::size_t frame = 1; frame < animation.keyframes.size(); ++frame) {
    auto source = animation.group_transforms.begin() + static_cast<std::ptrdiff_t>(frame * source_count);
    auto destination = animation.group_transforms.begin() + static_cast<std::ptrdiff_t>(frame * retained_count);
    std::move(source, source + static_cast<std::ptrdiff_t>(retained_count), destination);
  }
  animation.group_transforms.resize(animation.keyframes.size() * retained_count);
  animation.group_count = retained_count;
  for (std::size_t group = retained_count; group < animation::kMaxGroups; ++group)
    animation.claimed_groups.reset(group);
}

ArxReturnCode resolveSampler(glb::AccessorCache& accessors, const cgltf_animation_sampler& source,
                             ResolvedSampler& out) {
  if (source.interpolation == cgltf_interpolation_type_cubic_spline) return ARX_GLB_UNSUPPORTED_FEATURE;
  if (source.interpolation != cgltf_interpolation_type_linear && source.interpolation != cgltf_interpolation_type_step)
    return ARX_GLB_BAD_ANIMATION_SAMPLER;
  ArxReturnCode rc = accessors.get(source.input, out.times);
  if (rc != ARX_OK) return rc;
  rc = accessors.get(source.output, out.values);
  if (rc != ARX_OK) return rc;
  if (out.times->type != cgltf_type_scalar || out.times->component_type != cgltf_component_type_r_32f ||
      out.times->normalized || out.times->count != out.values->count || out.times->floats.empty())
    return ARX_GLB_BAD_ANIMATION_SAMPLER;
  for (std::size_t index = 0; index < out.times->count; ++index) {
    if (!std::isfinite(out.times->floats[index])) return ARX_GLB_BAD_ANIMATION_SAMPLER;
    if (index != 0 && out.times->floats[index] <= out.times->floats[index - 1U]) return ARX_GLB_BAD_ANIMATION_SAMPLER;
  }
  out.interpolation = source.interpolation;
  return ARX_OK;
}

ArxReturnCode collectChannels(const cgltf_data& data, glb::AccessorCache& accessors, const glb::NodeGraph& graph,
                              std::size_t root, const cgltf_animation& animation,
                              const std::vector<BoneIndex>& node_bones, std::span<const std::uint8_t> relevant_nodes,
                              std::vector<ResolvedSampler>& samplers,
                              std::unordered_map<std::size_t, NodeChannels>& channels) {
  samplers.resize(animation.samplers_count);
  std::vector<bool> resolved(animation.samplers_count, false);
  const std::size_t animation_index = cgltf_animation_index(&data, &animation);
  const std::string_view animation_name = animation.name != nullptr ? animation.name : "";
  const auto fail = [&]<class... Args>(
                        ArxReturnCode code, std::size_t channel, std::format_string<Args...> reason, Args&&... args) {
    logLazy(ARX_LOG_DEBUG, [&] {
      return std::format("GLB -> Model animation failure: animation {} '{}' channel {} {} (code {})",
                         animation_index,
                         animation_name,
                         channel,
                         std::format(reason, std::forward<Args>(args)...),
                         code);
    });
    return code;
  };
  auto sampler_index_of = [&](const cgltf_animation_sampler* sampler) -> std::optional<std::size_t> {
    if (sampler == nullptr || sampler < animation.samplers || sampler >= animation.samplers + animation.samplers_count)
      return std::nullopt;
    return static_cast<std::size_t>(sampler - animation.samplers);
  };
  for (std::size_t index = 0; index < animation.channels_count; ++index) {
    const cgltf_animation_channel& channel = animation.channels[index];
    if (channel.target_node == nullptr) continue;
    const std::size_t node = nodeIndex(data, channel.target_node);
    if (node >= data.nodes_count || (root != glb::kInvalidNodeIndex && !glb::isDescendantOrSelf(graph, node, root)))
      continue;
    if (channel.target_path == cgltf_animation_path_type_weights)
      return fail(ARX_GLB_UNSUPPORTED_FEATURE, index, "targets morph weights");
    if (relevant_nodes[node] == 0U)
      return fail(ARX_GLB_BAD_ANIMATION_BINDING, index, "targets a node outside the Model skeleton");
    const std::optional<std::size_t> sampler_index = sampler_index_of(channel.sampler);
    if (!sampler_index) return fail(ARX_GLB_BAD_ANIMATION_CHANNEL, index, "references an invalid sampler");
    if (!resolved[*sampler_index]) {
      const ArxReturnCode rc = resolveSampler(accessors, *channel.sampler, samplers[*sampler_index]);
      if (rc != ARX_OK) return fail(rc, index, "has invalid sampler {}", *sampler_index);
      resolved[*sampler_index] = true;
    }
    NodeChannels& target = channels[node];
    int* slot = nullptr;
    cgltf_type expected = cgltf_type_invalid;
    switch (channel.target_path) {
      case cgltf_animation_path_type_translation:
        slot = &target.translation;
        expected = cgltf_type_vec3;
        break;
      case cgltf_animation_path_type_rotation:
        slot = &target.rotation;
        expected = cgltf_type_vec4;
        break;
      case cgltf_animation_path_type_scale:
        if (node_bones[node] == kInvalidBoneIndex)
          return fail(ARX_GLB_BAD_ANIMATION_BINDING, index, "scales a non-bone node");
        slot = &target.scale;
        expected = cgltf_type_vec3;
        break;
      default:
        return fail(ARX_GLB_BAD_ANIMATION_CHANNEL, index, "has an invalid target path");
    }
    if (*slot >= 0) return fail(ARX_GLB_BAD_ANIMATION_CHANNEL, index, "duplicates a transform channel");
    if (samplers[*sampler_index].values->type != expected ||
        samplers[*sampler_index].values->component_type != cgltf_component_type_r_32f ||
        samplers[*sampler_index].values->normalized)
      return fail(ARX_GLB_BAD_ANIMATION_SAMPLER, index, "has an invalid output accessor");
    if (channel.target_path == cgltf_animation_path_type_rotation && !validRotations(*samplers[*sampler_index].values))
      return fail(ARX_GLB_BAD_ANIMATION_SAMPLER, index, "contains an invalid rotation");
    *slot = static_cast<int>(*sampler_index);
  }
  return channels.empty() ? fail(ARX_GLB_BAD_ANIMATION_CHANNEL, 0, "has no usable channels") : ARX_OK;
}

ArxReturnCode animationNodes(const glb::NodeGraph& graph, std::size_t root, const std::vector<BoneIndex>& node_bones,
                             std::vector<std::size_t>& bone_nodes, std::vector<std::uint8_t>& relevant_nodes) {
  bone_nodes.assign(static_cast<std::size_t>(
                        std::ranges::count_if(node_bones, [](BoneIndex bone) { return bone != kInvalidBoneIndex; })),
                    glb::kInvalidNodeIndex);
  relevant_nodes.assign(node_bones.size(), 0U);
  if (root != glb::kInvalidNodeIndex) relevant_nodes[root] = 1U;
  for (std::size_t node = 0; node < node_bones.size(); ++node) {
    const BoneIndex bone = node_bones[node];
    if (bone == kInvalidBoneIndex) continue;
    if (bone >= bone_nodes.size() || bone_nodes[bone] != glb::kInvalidNodeIndex) return ARX_GLB_BAD_ANIMATION_BINDING;
    bone_nodes[bone] = node;
    std::size_t ancestor = node;
    while (ancestor != glb::kInvalidNodeIndex && ancestor != root) {
      relevant_nodes[ancestor] = 1U;
      ancestor = graph.parent[ancestor];
    }
    if (root != glb::kInvalidNodeIndex && ancestor != root) return ARX_GLB_BAD_ANIMATION_BINDING;
  }
  return std::ranges::find(bone_nodes, glb::kInvalidNodeIndex) == bone_nodes.end() ? ARX_OK
                                                                                   : ARX_GLB_BAD_ANIMATION_BINDING;
}

ArxReturnCode prepareAnimationImport(const cgltf_data& data, const glb::NodeGraph& graph,
                                     const ModelDiscovery& discovery, const ModelModules& model,
                                     const std::vector<BoneIndex>& node_bones, AnimationImportContext& out) {
  out.root = discovery.root;
  out.motion_node = discoverMotionNode(graph, discovery, node_bones);
  ArxReturnCode rc = animationNodes(graph, out.root, node_bones, out.bone_nodes, out.relevant_nodes);
  if (rc != ARX_OK) return rc;
  if (out.bone_nodes.size() != model.skeleton.bones.size()) return ARX_GLB_BAD_ANIMATION_BINDING;

  out.relevant_preorder.clear();
  out.relevant_preorder.reserve(
      static_cast<std::size_t>(std::ranges::count(out.relevant_nodes, static_cast<std::uint8_t>(1U))));
  for (std::size_t node : graph.preorder)
    if (out.relevant_nodes[node] != 0U) out.relevant_preorder.push_back(node);

  out.bind_local.resize(data.nodes_count);
  for (std::size_t node : out.relevant_preorder)
    if (!localTrs(data.nodes[node], out.bind_local[node])) return ARX_GLB_BAD_ANIMATION_BINDING;

  if (out.root != glb::kInvalidNodeIndex) {
    const std::optional<math::Mat4> inverse_root_bind = math::inverseAffine(math::fromTrs(
        out.bind_local[out.root].translation, out.bind_local[out.root].rotation, out.bind_local[out.root].scale));
    if (!inverse_root_bind) return ARX_GLB_BAD_ANIMATION_BINDING;
    out.inverse_root_bind = *inverse_root_bind;
  }

  out.bind_world_rotations.resize(data.nodes_count);
  out.sampled_world.assign(data.nodes_count, math::kIdentityMat4);
  for (std::size_t node : out.relevant_preorder) {
    if (node == out.root) continue;
    const std::size_t parent = graph.parent[node];
    if (parent == glb::kInvalidNodeIndex) {
      if (out.root != glb::kInvalidNodeIndex) return ARX_GLB_BAD_ANIMATION_BINDING;
      out.sampled_world[node] =
          math::fromTrs(out.bind_local[node].translation, out.bind_local[node].rotation, out.bind_local[node].scale);
      out.bind_world_rotations[node] = out.bind_local[node].rotation;
    } else {
      if (out.relevant_nodes[parent] == 0U) return ARX_GLB_BAD_ANIMATION_BINDING;
      out.sampled_world[node] =
          out.sampled_world[parent] *
          math::fromTrs(out.bind_local[node].translation, out.bind_local[node].rotation, out.bind_local[node].scale);
      out.bind_world_rotations[node] = out.bind_world_rotations[parent] * out.bind_local[node].rotation;
    }
  }
  if (out.motion_node != glb::kInvalidNodeIndex) {
    out.motion_bind = out.sampled_world[out.motion_node];
    const std::optional<math::Mat4> inverse_motion_bind = math::inverseAffine(out.motion_bind);
    if (!inverse_motion_bind) return ARX_GLB_BAD_ANIMATION_BINDING;
    out.inverse_motion_bind = *inverse_motion_bind;
    out.motion_bind_rotation = out.bind_world_rotations[out.motion_node];
  }

  out.sampled_local.resize(data.nodes_count);
  out.sampled_world_rotations.resize(data.nodes_count);
  out.bone_animation_rotations.resize(out.bone_nodes.size());
  out.bone_animation_positions.resize(out.bone_nodes.size());
  out.bone_animation_linears.assign(out.bone_nodes.size(), math::kIdentityMat4);
  return ARX_OK;
}

void sampleTrs(const Trs& bind, const NodeChannels* channels, std::span<const ResolvedSampler> samplers, float time,
               Trs& out) {
  out = bind;
  if (!channels) return;
  if (channels->translation >= 0) {
    float values[3]{};
    sample(samplers[static_cast<std::size_t>(channels->translation)], time, values, 3U, false);
    out.translation = {values[0], values[1], values[2]};
  }
  if (channels->rotation >= 0) {
    float values[4]{};
    sample(samplers[static_cast<std::size_t>(channels->rotation)], time, values, 4U, true);
    out.rotation = {values[3], values[0], values[1], values[2]};
  }
  if (channels->scale >= 0) {
    float values[3]{};
    sample(samplers[static_cast<std::size_t>(channels->scale)], time, values, 3U, false);
    out.scale = {values[0], values[1], values[2]};
  }
}

std::vector<std::uint8_t> groupsAffectedByChannels(const glb::NodeGraph& graph, std::size_t motion_node,
                                                   const ModelModules& model, std::span<const std::size_t> bone_nodes,
                                                   const std::unordered_map<std::size_t, NodeChannels>& channels) {
  std::vector<std::uint8_t> affected(bone_nodes.size(), 0U);
  for (std::size_t group = 0; group < bone_nodes.size(); ++group) {
    const BoneIndex parent = model.skeleton.bones[group].parent;
    const std::size_t stop = parent == kInvalidBoneIndex ? motion_node : bone_nodes[parent];
    for (std::size_t node = bone_nodes[group]; node != stop && node != glb::kInvalidNodeIndex;
         node = graph.parent[node]) {
      if (channels.contains(node)) {
        affected[group] = 1U;
        break;
      }
    }
  }
  return affected;
}

ArxReturnCode importOne(const cgltf_data& data, glb::AccessorCache& accessors, const glb::NodeGraph& graph, float units,
                        const ModelModules& model, const std::vector<BoneIndex>& node_bones,
                        const cgltf_animation& source, const HelperData& helper, AnimationImportContext& context,
                        AnimationModules& out, std::vector<SoundSourceReference>& sound_sources) {
  if (source.name == nullptr || std::string_view(source.name).empty()) return ARX_GLB_BAD_ANIMATION_NAME;
  std::vector<ResolvedSampler> samplers;
  std::unordered_map<std::size_t, NodeChannels> channels;
  ArxReturnCode rc = collectChannels(
      data, accessors, graph, context.root, source, node_bones, context.relevant_nodes, samplers, channels);
  if (rc != ARX_OK) return rc;
  const std::vector<std::uint8_t> affected_groups =
      groupsAffectedByChannels(graph, context.motion_node, model, context.bone_nodes, channels);

  float shift = 0.0f;
  std::vector<std::uint32_t> frames;
  for (const auto& [node, node_channels] : channels) {
    const int indices[] = {node_channels.translation, node_channels.rotation, node_channels.scale};
    for (int sampler_index : indices) {
      if (sampler_index < 0) continue;
      const ResolvedSampler& sampler = samplers[static_cast<std::size_t>(sampler_index)];
      shift = std::max(shift, -sampler.times->floats.front());
    }
  }
  std::unordered_set<int> used_samplers;
  struct QuantizedFrame {
    std::uint32_t frame = 0;
    double raw_frame = 0.0;
    double tolerance = 0.0;
  };
  std::vector<QuantizedFrame> quantized;
  std::size_t rounded_timestamps = 0;
  double maximum_adjustment = 0.0;
  for (const auto& [node, node_channels] : channels) {
    const int indices[] = {node_channels.translation, node_channels.rotation, node_channels.scale};
    for (int sampler_index : indices) {
      if (sampler_index < 0 || !used_samplers.insert(sampler_index).second) continue;
      const ResolvedSampler& sampler = samplers[static_cast<std::size_t>(sampler_index)];
      for (float time : sampler.times->floats) {
        const double raw_frame = (static_cast<double>(time) + static_cast<double>(shift)) * kTeaFps;
        const double rounded = std::round(raw_frame);
        if (!std::isfinite(rounded) || rounded < 0.0 || rounded > std::numeric_limits<std::int32_t>::max())
          return ARX_GLB_BAD_ANIMATION_SAMPLER;
        const double adjustment = std::abs(raw_frame - rounded);
        const double tolerance = frameRoundingTolerance(time, shift);
        if (adjustment > tolerance) {
          ++rounded_timestamps;
          maximum_adjustment = std::max(maximum_adjustment, adjustment);
        }
        quantized.push_back({static_cast<std::uint32_t>(rounded), raw_frame, tolerance});
      }
    }
  }
  if (quantized.empty()) return ARX_GLB_BAD_ANIMATION_SAMPLER;
  std::ranges::sort(quantized, {}, [](const QuantizedFrame& value) { return std::pair{value.frame, value.raw_frame}; });
  std::size_t frame_collisions = 0;
  double last_raw_frame = 0.0;
  double last_tolerance = 0.0;
  for (const QuantizedFrame& value : quantized) {
    if (frames.empty() || frames.back() != value.frame) {
      frames.push_back(value.frame);
      last_raw_frame = value.raw_frame;
      last_tolerance = value.tolerance;
      continue;
    }
    if (std::abs(value.raw_frame - last_raw_frame) > std::max(value.tolerance, last_tolerance)) {
      ++frame_collisions;
      last_raw_frame = value.raw_frame;
      last_tolerance = value.tolerance;
    }
  }
  const bool shifted_timeline = static_cast<double>(shift) * kTeaFps > frameRoundingTolerance(-shift, 0.0f);
  if (shifted_timeline || rounded_timestamps != 0 || frame_collisions != 0)
    logLazy(ARX_LOG_WARN, [&] {
      std::string warning = std::format("GLB -> Model: animation '{}' timeline repaired:", source.name);
      if (shifted_timeline) warning += std::format(" shifted by {} second(s);", shift);
      if (rounded_timestamps != 0)
        warning += std::format(" {} timestamp(s) rounded to 24 Hz, maximum adjustment {} frame(s);",
                               rounded_timestamps,
                               maximum_adjustment);
      if (frame_collisions != 0)
        warning += std::format(" {} source timestamp collision(s) after rounding;", frame_collisions);
      warning.pop_back();
      return warning;
    });
  const std::uint32_t last_transform_frame = *std::ranges::max_element(frames);
  frames.insert(frames.end(), helper.footsteps.begin(), helper.footsteps.end());
  for (const auto& [frame, sample_path] : helper.sounds) frames.push_back(frame);
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  if (frames.empty()) return ARX_GLB_BAD_ANIMATION_SAMPLER;

  std::size_t discarded_frames = 0;
  std::size_t discarded_events = 0;
  if (helper.timing.kind == TimingKind::kExact) {
    discarded_frames = static_cast<std::size_t>(
        std::ranges::count_if(frames, [&](std::uint32_t frame) { return frame > helper.timing.frame_length; }));
    discarded_events = static_cast<std::size_t>(std::ranges::count_if(
        helper.footsteps, [&](std::uint32_t frame) { return frame > helper.timing.frame_length; }));
    discarded_events += static_cast<std::size_t>(std::ranges::count_if(
        helper.sounds, [&](const auto& value) { return value.first > helper.timing.frame_length; }));
    std::erase_if(frames, [&](std::uint32_t frame) { return frame > helper.timing.frame_length; });
    if (helper.timing.frame_length < last_transform_frame &&
        !std::ranges::binary_search(frames, helper.timing.frame_length))
      frames.push_back(helper.timing.frame_length);
    std::sort(frames.begin(), frames.end());
  }

  const std::size_t group_count = context.bone_nodes.size();
  if (frames.size() > animation::kMaxKeyframes) return ARX_ANIMATION_TOO_MANY_KEYFRAMES;
  if (group_count > animation::kMaxGroups) return ARX_ANIMATION_TOO_MANY_GROUPS;
  for (std::size_t group = group_count; group < animation::kMaxGroups; ++group)
    if (helper.void_groups.test(group) || helper.claimed_groups.test(group)) return ARX_GLB_BAD_ANIMATION_HELPER;

  AnimationModules result;
  result.animation.name = source.name;
  (void)animation::repairName(result.animation.name);
  if (!helper.path.empty()) {
    std::string resource_path;
    if (resource::repairPath(ARX_RESOURCE_KIND_ANIMATION, helper.path, resource_path) != resource::Error::kNone)
      return ARX_GLB_BAD_ANIMATION_HELPER;
    resource::setPath(result.resource, std::move(resource_path));
  }
  result.animation.group_count = group_count;
  std::vector<glb::ImportedSoundSource> imported_sources;
  imported_sources.reserve(helper.sounds.size());
  const std::string sound_log_prefix = std::format("GLB -> Model: animation '{}'", source.name);
  glb::SoundPathImporter sound_importer(result.sounds, &imported_sources, sound_log_prefix);
  std::map<std::uint32_t, SoundHandle> frame_sounds;
  sound_sources.reserve(helper.sounds.size());
  for (const auto& [frame, source_path] : helper.sounds) {
    SoundHandle sound = kNoSoundHandle;
    rc = soundPathImportError(sound_importer.import(SoundKind::kEffect, source_path, sound));
    if (rc != ARX_OK) return rc;
    frame_sounds.emplace(frame, sound);
  }
  rc = soundPathImportError(sound_importer.finish());
  if (rc != ARX_OK) return rc;
  for (glb::ImportedSoundSource& source_reference : imported_sources) {
    SoundIndex sound = kNoSound;
    if (!sounds::effectIndex(source_reference.sound, sound)) return ARX_INTERNAL_ERROR;
    sound_sources.push_back({sound, std::move(source_reference.path)});
  }
  result.animation.keyframes.resize(frames.size());
  result.animation.group_transforms.resize(frames.size() * result.animation.group_count);
  for (std::size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
    const std::uint32_t frame = frames[frame_index];
    const float time = static_cast<float>(frame) / kTeaFps - shift;
    AnimationKeyframe& keyframe = result.animation.keyframes[frame_index];
    keyframe.frame = frame;
    keyframe.footstep = helper.footsteps.contains(frame);
    if (const auto sound = frame_sounds.find(frame); sound != frame_sounds.end()) keyframe.sound = sound->second;
    for (std::size_t node : context.relevant_preorder) {
      const auto found = channels.find(node);
      sampleTrs(context.bind_local[node],
                found == channels.end() ? nullptr : &found->second,
                samplers,
                time,
                context.sampled_local[node]);
      const math::Mat4 local = math::fromTrs(context.sampled_local[node].translation,
                                             context.sampled_local[node].rotation,
                                             context.sampled_local[node].scale);
      if (node == context.root) {
        context.sampled_world[node] = context.inverse_root_bind * local;
        context.sampled_world_rotations[node] =
            math::conjugate(context.bind_local[node].rotation) * context.sampled_local[node].rotation;
      } else {
        const std::size_t parent = graph.parent[node];
        if (parent == glb::kInvalidNodeIndex) {
          if (context.root != glb::kInvalidNodeIndex) return ARX_GLB_BAD_ANIMATION_BINDING;
          context.sampled_world[node] = local;
          context.sampled_world_rotations[node] = context.sampled_local[node].rotation;
        } else {
          if (context.relevant_nodes[parent] == 0U) return ARX_GLB_BAD_ANIMATION_BINDING;
          context.sampled_world[node] = context.sampled_world[parent] * local;
          context.sampled_world_rotations[node] =
              context.sampled_world_rotations[parent] * context.sampled_local[node].rotation;
        }
      }
      if (node == context.motion_node) {
        Trs motion_delta;
        if (!decompose(context.sampled_world[node] * context.inverse_motion_bind, motion_delta)) {
          log(ARX_LOG_DEBUG,
              "GLB -> Model animation failure: animation '{}' frame {} has a non-decomposable motion-carrier "
              "transform",
              source.name,
              frame);
          return ARX_GLB_BAD_ANIMATION_BINDING;
        }
        constexpr float kScaleTolerance = 1.0e-4f;
        if (std::abs(motion_delta.scale.x - 1.0f) > kScaleTolerance ||
            std::abs(motion_delta.scale.y - 1.0f) > kScaleTolerance ||
            std::abs(motion_delta.scale.z - 1.0f) > kScaleTolerance) {
          log(ARX_LOG_DEBUG,
              "GLB -> Model animation failure: animation '{}' frame {} animates motion-carrier scale",
              source.name,
              frame);
          return ARX_GLB_BAD_ANIMATION_BINDING;
        }
        keyframe.root_translation = toArxPoint(motion_delta.translation, units);
        keyframe.root_rotation = toArxRotation(motion_delta.rotation);
        context.sampled_world[node] = context.motion_bind;
        context.sampled_world_rotations[node] = context.motion_bind_rotation;
      }
    }

    for (std::size_t group = 0; group < context.bone_nodes.size(); ++group) {
      AnimationGroupTransform& transform =
          result.animation.group_transforms[frame_index * result.animation.group_count + group];
      const std::size_t bone_node = context.bone_nodes[group];
      const ArxQuat target_rotation = toArxRotation(context.sampled_world_rotations[bone_node] *
                                                    math::conjugate(context.bind_world_rotations[bone_node]));
      const ArxVector3 target_position = toArxPoint(math::translation(context.sampled_world[bone_node]), units);
      ArxQuat parent_rotation{};
      ArxVector3 parent_position{};
      math::Mat4 parent_linear = math::kIdentityMat4;
      ArxVector3 bind_offset = model.skeleton.bones[group].position;
      const BoneIndex parent = model.skeleton.bones[group].parent;
      if (parent != kInvalidBoneIndex) {
        parent_rotation = context.bone_animation_rotations[parent];
        parent_position = context.bone_animation_positions[parent];
        parent_linear = context.bone_animation_linears[parent];
        bind_offset = bind_offset - model.skeleton.bones[parent].position;
      }
      if (affected_groups[group] != 0U) {
        transform.rotation = math::canonicalizeQuaternionSign(math::conjugate(parent_rotation) * target_rotation);
        const Trs& sampled = context.sampled_local[bone_node];
        const Trs& bind = context.bind_local[bone_node];
        if (std::abs(bind.scale.x) <= 1.0e-6f || std::abs(bind.scale.y) <= 1.0e-6f || std::abs(bind.scale.z) <= 1.0e-6f)
          return ARX_GLB_BAD_ANIMATION_BINDING;
        transform.scale = {
            sampled.scale.x / bind.scale.x, sampled.scale.y / bind.scale.y, sampled.scale.z / bind.scale.z};
        const std::optional<math::Mat4> inverse_parent = math::inverseAffine(parent_linear);
        if (inverse_parent) {
          transform.translation = math::xformDir(*inverse_parent, target_position - parent_position) - bind_offset;
        } else if (parent != kInvalidBoneIndex && graph.parent[bone_node] == context.bone_nodes[parent]) {
          transform.translation = toArxPoint(sampled.translation - bind.translation, units);
        } else {
          return ARX_GLB_BAD_ANIMATION_BINDING;
        }
      }
      context.bone_animation_rotations[group] = target_rotation;
      context.bone_animation_positions[group] = target_position;
      context.bone_animation_linears[group] = parent_linear * math::fromTrs({}, transform.rotation, transform.scale);
    }
  }
  const std::uint32_t last = result.animation.keyframes.back().frame;
  if (helper.timing.kind == TimingKind::kExtra) {
    if (last == static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
      return ARX_GLB_BAD_ANIMATION_HELPER;
    result.animation.frame_length = last + 1U;
  } else if (helper.timing.kind == TimingKind::kExact) {
    result.animation.frame_length = helper.timing.frame_length;
  } else {
    result.animation.frame_length = last;
  }
  canonicalizeIdentityGroups(result.animation, helper, source.name);
  if (discarded_frames != 0 || discarded_events != 0)
    log(ARX_LOG_WARN,
        "GLB -> Model: animation '{}' FRAME_LENGTH_{} discarded {} later frame(s) and {} event(s)",
        source.name,
        helper.timing.frame_length,
        discarded_frames,
        discarded_events);
  const ArxReturnCode validation = animation_detail::validateStructure(result);
  if (validation != ARX_OK) {
    log(ARX_LOG_DEBUG,
        "GLB -> Model animation '{}' constructed Animation validation failed with code {}",
        source.name,
        validation);
    return validation;
  }
  out = std::move(result);
  return ARX_OK;
}

}  // namespace

ArxReturnCode importAnimations(const glb::Asset& asset, glb::AccessorCache& accessors, const glb::NodeGraph& graph,
                               const ModelDiscovery& discovery, float units, const ModelModules& model,
                               const std::vector<BoneIndex>& node_bones, std::vector<AnimationModules>& out,
                               ArxAnimationConversionReport* report,
                               std::vector<AnimationSoundSourceReference>* sound_sources) {
  const cgltf_data& data = *asset.data();
  std::unordered_map<std::string, HelperEntry> helpers;
  for (std::size_t node : discovery.animation_helpers) {
    if (data.nodes[node].name == nullptr) continue;
    const std::string_view name(data.nodes[node].name);
    if (!name.starts_with(kAnimationPrefix)) continue;
    if (name.size() == kAnimationPrefix.size()) {
      log(ARX_LOG_WARN, "GLB -> Model: animation helper has no animation name; ignored");
      continue;
    }
    HelperEntry& entry = helpers[std::string(name.substr(kAnimationPrefix.size()))];
    if (entry.count++ == 0) entry.node = node;
  }

  std::vector<AnimationModules> imported;
  imported.reserve(data.animations_count);
  std::vector<std::string_view> original_names;
  original_names.reserve(data.animations_count);
  AnimationImportContext context;
  std::vector<AnimationSoundSourceReference> imported_sources;
  const ArxReturnCode context_rc =
      data.animations_count == 0 ? ARX_OK : prepareAnimationImport(data, graph, discovery, model, node_bones, context);
  for (std::size_t index = 0; index < data.animations_count; ++index) {
    const cgltf_animation& source = data.animations[index];
    const std::string_view name = source.name ? std::string_view(source.name) : std::string_view{};
    HelperData helper;
    bool helper_valid = true;
    if (const auto found = helpers.find(std::string(name)); found != helpers.end()) {
      found->second.used = true;
      helper_valid = found->second.count == 1U && parseHelper(data.nodes[found->second.node], helper);
    }
    AnimationModules animation;
    std::vector<SoundSourceReference> animation_sources;
    const ArxReturnCode rc = hasDoubleUnderscore(name) ? ARX_GLB_BAD_ANIMATION_NAME
                             : !helper_valid           ? ARX_GLB_BAD_ANIMATION_HELPER
                             : context_rc != ARX_OK    ? context_rc
                                                       : importOne(data,
                                                                   accessors,
                                                                   graph,
                                                                   units,
                                                                   model,
                                                                   node_bones,
                                                                   source,
                                                                   helper,
                                                                   context,
                                                                   animation,
                                                                   animation_sources);
    if (rc != ARX_OK) {
      if (report) ++report->skipped;
      log(ARX_LOG_WARN, "GLB -> Model: animation '{}' skipped with code {}", name.empty() ? "<unnamed>" : name, rc);
      continue;
    }
    imported.push_back(std::move(animation));
    const std::size_t imported_index = imported.size() - 1U;
    for (SoundSourceReference& reference : animation_sources)
      imported_sources.push_back({imported_index, std::move(reference)});
    original_names.push_back(name);
    if (report) ++report->converted;
  }

  for (const auto& [name, helper] : helpers)
    if (!helper.used) log(ARX_LOG_WARN, "GLB -> Model: animation helper '{}' has no matching animation; ignored", name);

  IdentifierUniquifier names({.max_length = animation::kMaxNameLength});
  names.reserve(imported.size());
  for (AnimationModules& animation : imported) names.add(animation.animation.name);
  std::vector<IdentifierRepair> repairs(imported.size());
  if (names.apply(repairs).exhausted) return ARX_GLB_BAD_ANIMATION_NAME;
  for (std::size_t index = 0; index < imported.size(); ++index)
    if (original_names[index] != imported[index].animation.name)
      log(ARX_LOG_INFO,
          "GLB -> Model: animation name '{}' normalized to '{}'",
          original_names[index],
          imported[index].animation.name);
  out = std::move(imported);
  if (sound_sources) *sound_sources = std::move(imported_sources);
  return ARX_OK;
}

}  // namespace pistoris::glb_model
