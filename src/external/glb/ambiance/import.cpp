// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "ambiance/data.h"
#include "ambiance/internal.h"
#include "api.h"
#include "cgltf/cgltf.h"
#include "external/glb/container.h"
#include "external/glb/node_graph.h"
#include "external/glb/object_coordinates.h"
#include "external/glb/utils/names.h"
#include "external/glb/utils/node.h"
#include "external/glb/utils/sound.h"
#include "external/glb/utils/tokens.h"
#include "modules/ambiance.h"
#include "modules/sounds.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

constexpr float kPanTolerance = 1.0e-4f;

ArxReturnCode soundPathImportError(glb::SoundPathImportError error) noexcept {
  switch (error) {
    case glb::SoundPathImportError::kNone:
      return ARX_OK;
    case glb::SoundPathImportError::kBadPath:
      return ARX_AMBIANCE_BAD_SOUND_PATH;
    case glb::SoundPathImportError::kTooManySounds:
      return ARX_AMBIANCE_TOO_MANY_SOUNDS;
    case glb::SoundPathImportError::kBadKind:
      return ARX_INTERNAL_ERROR;
  }
  return ARX_INTERNAL_ERROR;
}

enum class AutomationKind : std::uint8_t {
  kVolume,
  kPitch,
  kX,
  kY,
  kZ,
  kPan,
};

struct ParsedAutomation {
  AutomationKind kind = AutomationKind::kVolume;
  bool has_value = false;
  float value = 0.0f;
  bool has_range = false;
  float range = 0.0f;
  std::uint32_t interval_ms = 1000;
  DynamicAutomationMode mode = DynamicAutomationMode::kStep;
};

struct PendingTrack {
  std::uint32_t ordinal = 0;
  std::string source_path;
  AmbianceTrack track;
};

struct PendingKey {
  std::uint32_t ordinal = 0;
  bool panned = false;
  PannedAmbianceKey panned_key;
  PositionedAmbianceKey positioned_key;
};

std::string_view nodeName(const cgltf_node& node) noexcept {
  return node.name != nullptr ? std::string_view(node.name) : std::string_view{};
}

math::Mat4 localTransform(const cgltf_node& node) noexcept {
  cgltf_float values[16];
  cgltf_node_transform_local(&node, values);
  math::Mat4 result{};
  for (std::size_t index = 0; index < 16; ++index) result.m[index] = values[index];
  return result;
}

bool parseUnsigned(std::string_view value, std::uint32_t& out) {
  const std::optional<std::uint32_t> parsed = glb::parseUnsignedToken(value);
  if (!parsed.has_value()) return false;
  out = parsed.value_or(0);
  return true;
}

bool ambianceRootCandidate(std::string_view name) noexcept {
  return name == "arx_ambiance" || name.starts_with("arx_ambiance__");
}

bool rootName(std::string_view name, std::optional<std::uint32_t>& master, glb::ParsedLabel& label) {
  return glb::parseRecoverableLabel(name,
                                    master,
                                    &label,
                                    {{}, {"MASTER_"}},
                                    [](std::span<const std::string_view> tokens, std::optional<std::uint32_t>& parsed) {
                                      if (tokens.empty() || tokens.front() != "arx_ambiance" || tokens.size() > 2)
                                        return false;
                                      parsed.reset();
                                      if (tokens.size() == 1) return true;
                                      if (!tokens[1].starts_with("MASTER_")) return false;
                                      std::uint32_t ordinal = 0;
                                      if (!parseUnsigned(tokens[1].substr(7), ordinal)) return false;
                                      parsed = ordinal;
                                      return true;
                                    });
}

bool trackName(std::string_view name, std::uint32_t& ordinal, std::string_view& sample_path) {
  constexpr std::string_view kPrefix = "TRACK_";
  if (!name.starts_with(kPrefix)) return false;
  const std::optional<glb::LabeledValue> labeled = glb::splitRequiredLabel(name);
  if (!labeled) return false;
  const std::size_t first = labeled->value.find("__", kPrefix.size());
  if (first == std::string_view::npos) return false;
  if (!parseUnsigned(name.substr(kPrefix.size(), first - kPrefix.size()), ordinal)) return false;
  sample_path = labeled->value.substr(first + 2);
  return !sample_path.empty();
}

bool setMode(std::string_view token, DynamicAutomationMode& mode) {
  if (token == "STEP") {
    mode = DynamicAutomationMode::kStep;
  } else if (token == "RANDOM_STEP") {
    mode = DynamicAutomationMode::kRandomStep;
  } else if (token == "INTERPOLATED") {
    mode = DynamicAutomationMode::kInterpolated;
  } else if (token == "RANDOM_INTERPOLATED") {
    mode = DynamicAutomationMode::kRandomInterpolated;
  } else {
    return false;
  }
  return true;
}

std::optional<AutomationKind> automationKind(std::string_view token) {
  if (token == "VOLUME") return AutomationKind::kVolume;
  if (token == "PITCH") return AutomationKind::kPitch;
  if (token == "X") return AutomationKind::kX;
  if (token == "Y") return AutomationKind::kY;
  if (token == "Z") return AutomationKind::kZ;
  if (token == "PAN") return AutomationKind::kPan;
  return std::nullopt;
}

std::optional<AutomationKind> automationNameKind(std::string_view name) {
  const std::size_t separator = name.find("__");
  return automationKind(name.substr(0, separator));
}

bool parseAutomationName(std::string_view name, ParsedAutomation& out, glb::ParsedLabel& label) {
  return glb::parseRecoverableLabel(
      name,
      out,
      &label,
      {{"STEP", "RANDOM_STEP", "INTERPOLATED", "RANDOM_INTERPOLATED"}, {"VAL_", "RANGE_", "INTERVAL_"}},
      [](std::span<const std::string_view> tokens, ParsedAutomation& parsed) {
        if (tokens.empty()) return false;
        const std::optional<AutomationKind> kind = automationKind(tokens.front());
        if (!kind.has_value()) return false;
        ParsedAutomation result;
        result.kind = *kind;

        bool interval_seen = false;
        bool mode_seen = false;
        for (std::string_view token : tokens.subspan(1)) {
          if (token.starts_with("VAL_")) {
            if (result.has_value || !glb::parseFloatToken(token.substr(4), result.value)) return false;
            result.has_value = true;
          } else if (token.starts_with("RANGE_")) {
            if (result.has_range || !glb::parseFloatToken(token.substr(6), result.range) || result.range == 0.0f)
              return false;
            result.has_range = true;
          } else if (token.starts_with("INTERVAL_")) {
            if (interval_seen || !parseUnsigned(token.substr(9), result.interval_ms)) return false;
            interval_seen = true;
          } else {
            if (mode_seen || !setMode(token, result.mode)) return false;
            mode_seen = true;
          }
        }

        if ((interval_seen || mode_seen) && !result.has_range) return false;
        if ((result.kind == AutomationKind::kVolume || result.kind == AutomationKind::kPitch) && !result.has_value)
          return false;
        if ((result.kind == AutomationKind::kX || result.kind == AutomationKind::kY ||
             result.kind == AutomationKind::kZ) &&
            result.has_value)
          return false;
        parsed = result;
        return true;
      });
}

bool makeAutomation(float center, float range, const ParsedAutomation& parsed, Automation& out) {
  if (!std::isfinite(center) || !std::isfinite(range)) return false;
  if (!parsed.has_range) {
    out = ConstantAutomation{center};
    return true;
  }
  double first = static_cast<double>(center) - static_cast<double>(range);
  double second = static_cast<double>(center) + static_cast<double>(range);
  if (!std::isfinite(first) || !std::isfinite(second) || first < -std::numeric_limits<float>::max() ||
      first > std::numeric_limits<float>::max() || second < -std::numeric_limits<float>::max() ||
      second > std::numeric_limits<float>::max())
    return false;
  out = DynamicAutomation{static_cast<float>(first), static_cast<float>(second), parsed.interval_ms, parsed.mode};
  return true;
}

void normalizeRandomRange(ParsedAutomation& parsed, std::string_view helper_name, std::string_view sample_path) {
  if (!parsed.has_range || parsed.range >= 0.0f ||
      (parsed.mode != DynamicAutomationMode::kRandomStep && parsed.mode != DynamicAutomationMode::kRandomInterpolated))
    return;
  log(ARX_LOG_WARN,
      "GLB -> Ambiance: random automation '{}' for '{}' has a negative RANGE; using its absolute value",
      helper_name,
      sample_path);
  parsed.range = std::abs(parsed.range);
}

struct ParsedKeyName {
  std::uint32_t ordinal = 0;
  AmbianceKeyCommon common;
};

bool keyName(std::string_view name, ParsedKeyName& out, glb::ParsedLabel& label) {
  return glb::parseRecoverableLabel(
      name,
      out,
      &label,
      {{}, {"PLAY_COUNT_", "START_", "DELAY_MIN_", "DELAY_MAX_"}},
      [](std::span<const std::string_view> tokens, ParsedKeyName& parsed) {
        if (tokens.empty() || !tokens.front().starts_with("KEY_") ||
            !parseUnsigned(tokens.front().substr(4), parsed.ordinal))
          return false;

        bool play_seen = false;
        bool start_seen = false;
        bool minimum_seen = false;
        bool maximum_seen = false;
        for (std::string_view token : tokens.subspan(1)) {
          if (token.starts_with("PLAY_COUNT_")) {
            if (play_seen || !parseUnsigned(token.substr(11), parsed.common.play_count) ||
                parsed.common.play_count == 0)
              return false;
            play_seen = true;
          } else if (token.starts_with("START_")) {
            if (start_seen || !parseUnsigned(token.substr(6), parsed.common.start_delay_ms)) return false;
            start_seen = true;
          } else if (token.starts_with("DELAY_MIN_")) {
            if (minimum_seen || !parseUnsigned(token.substr(10), parsed.common.delay_min_ms)) return false;
            minimum_seen = true;
          } else if (token.starts_with("DELAY_MAX_")) {
            if (maximum_seen || !parseUnsigned(token.substr(10), parsed.common.delay_max_ms)) return false;
            maximum_seen = true;
          } else {
            return false;
          }
        }
        if (minimum_seen && !maximum_seen) parsed.common.delay_max_ms = parsed.common.delay_min_ms;
        return parsed.common.delay_min_ms <= parsed.common.delay_max_ms;
      });
}

float panFromPosition(const ArxVector3& position, std::string_view sample_path) {
  const float radius = std::hypot(position.x, position.z);
  if (radius <= kPanTolerance) {
    log(ARX_LOG_WARN, "GLB -> Ambiance: PAN point for '{}' is at the horizontal origin; using 0", sample_path);
    return 0.0f;
  }
  if (position.z < -kPanTolerance)
    log(ARX_LOG_WARN,
        "GLB -> Ambiance: PAN point for '{}' is behind the listener and mapped to the front arc",
        sample_path);
  return std::clamp(-position.x / radius, -1.0f, 1.0f);
}

ArxReturnCode parseKey(const cgltf_node& node, const math::Mat4& transform, std::string_view sample_path, float units,
                       PendingKey& out) {
  ParsedKeyName parsed_name;
  parsed_name.common.volume = ConstantAutomation{1.0f};
  parsed_name.common.pitch = ConstantAutomation{1.0f};
  glb::ParsedLabel label;
  if (!keyName(nodeName(node), parsed_name, label) || !glb::simpleEmptyNode(node)) return ARX_GLB_BAD_AMBIANCE_KEY;
  out.ordinal = parsed_name.ordinal;
  glb::reportConventionLabel("GLB -> Ambiance", nodeName(node), label);

  struct AutomationHelper {
    const cgltf_node* node = nullptr;
    ParsedAutomation parsed;
  };
  std::array<AutomationHelper, 6> helpers{};
  for (std::size_t index = 0; index < node.children_count; ++index) {
    const cgltf_node* child = node.children[index];
    if (child == nullptr) return ARX_GLB_BAD_FORMAT;
    const std::optional<AutomationKind> kind = automationNameKind(nodeName(*child));
    if (!kind.has_value()) {
      log(ARX_LOG_WARN, "GLB -> Ambiance: unexpected key child '{}' for '{}' ignored", nodeName(*child), sample_path);
      continue;
    }
    ParsedAutomation parsed;
    glb::ParsedLabel helper_label;
    if (!parseAutomationName(nodeName(*child), parsed, helper_label)) return ARX_GLB_BAD_AMBIANCE_AUTOMATION;
    const std::size_t slot = static_cast<std::size_t>(parsed.kind);
    if (helpers[slot].node != nullptr) return ARX_GLB_BAD_AMBIANCE_AUTOMATION;
    glb::reportConventionLabel("GLB -> Ambiance", nodeName(*child), helper_label);
    helpers[slot] = {child, parsed};
  }

  const ArxVector3 key_glb_position = math::translation(transform);
  const ArxVector3 key_position = glb_object::toArxPoint(key_glb_position, units);
  PositionedAmbianceKey positioned;
  static_cast<AmbianceKeyCommon&>(positioned) = parsed_name.common;
  positioned.x = ConstantAutomation{key_position.x};
  positioned.y = ConstantAutomation{key_position.y};
  positioned.z = ConstantAutomation{key_position.z};
  PannedAmbianceKey panned;
  static_cast<AmbianceKeyCommon&>(panned) = parsed_name.common;
  panned.pan = ConstantAutomation{0.0f};

  bool has_spatial_axis = false;
  bool has_pan = false;
  for (std::size_t slot = 0; slot < helpers.size(); ++slot) {
    const cgltf_node* helper = helpers[slot].node;
    if (helper == nullptr) continue;
    if (!glb::simpleEmptyNode(*helper)) return ARX_GLB_BAD_AMBIANCE_AUTOMATION;
    if (helper->children_count != 0)
      log(ARX_LOG_WARN,
          "GLB -> Ambiance: descendants of automation '{}' for '{}' ignored",
          nodeName(*helper),
          sample_path);
    ParsedAutomation parsed = helpers[slot].parsed;
    normalizeRandomRange(parsed, nodeName(*helper), sample_path);
    Automation automation;
    switch (parsed.kind) {
      case AutomationKind::kVolume:
        if (!makeAutomation(parsed.value, parsed.range, parsed, automation)) return ARX_GLB_BAD_AMBIANCE_AUTOMATION;
        positioned.volume = automation;
        panned.volume = automation;
        break;
      case AutomationKind::kPitch:
        if (!makeAutomation(parsed.value, parsed.range, parsed, automation)) return ARX_GLB_BAD_AMBIANCE_AUTOMATION;
        positioned.pitch = automation;
        panned.pitch = automation;
        break;
      case AutomationKind::kX:
        has_spatial_axis = true;
        if (!makeAutomation(key_position.x, parsed.range * units, parsed, positioned.x))
          return ARX_GLB_BAD_AMBIANCE_AUTOMATION;
        break;
      case AutomationKind::kY:
        has_spatial_axis = true;
        if (!makeAutomation(key_position.y, parsed.range * units, parsed, positioned.y))
          return ARX_GLB_BAD_AMBIANCE_AUTOMATION;
        break;
      case AutomationKind::kZ:
        has_spatial_axis = true;
        if (!makeAutomation(key_position.z, parsed.range * units, parsed, positioned.z))
          return ARX_GLB_BAD_AMBIANCE_AUTOMATION;
        break;
      case AutomationKind::kPan: {
        has_pan = true;
        const float derived = panFromPosition(key_glb_position, sample_path);
        const float center = parsed.has_value ? parsed.value : derived;
        if (parsed.has_value && std::abs(derived - std::clamp(center, -1.0f, 1.0f)) > kPanTolerance)
          log(ARX_LOG_WARN, "GLB -> Ambiance: PAN value and point for '{}' disagree; using VAL", sample_path);
        if (!makeAutomation(center, parsed.range, parsed, panned.pan)) return ARX_GLB_BAD_AMBIANCE_AUTOMATION;
        break;
      }
    }
  }

  if (has_spatial_axis) {
    if (has_pan)
      log(ARX_LOG_WARN, "GLB -> Ambiance: key for '{}' contains PAN and 3D axes; treating it as 3D", sample_path);
    out.positioned_key = positioned;
  } else if (has_pan) {
    out.panned = true;
    out.panned_key = panned;
  } else {
    out.positioned_key = positioned;
  }
  return ARX_OK;
}

ArxReturnCode parseTrack(const cgltf_node& node, const math::Mat4& transform, float units, PendingTrack& out) {
  std::string_view sample_path;
  if (!trackName(nodeName(node), out.ordinal, sample_path) || !glb::simpleEmptyNode(node))
    return ARX_GLB_BAD_AMBIANCE_TRACK;
  out.source_path = sample_path;

  std::vector<PendingKey> keys;
  keys.reserve(node.children_count);
  for (std::size_t index = 0; index < node.children_count; ++index) {
    const cgltf_node* child = node.children[index];
    if (child == nullptr) return ARX_GLB_BAD_FORMAT;
    if (!nodeName(*child).starts_with("KEY_")) {
      log(ARX_LOG_WARN, "GLB -> Ambiance: unexpected track child '{}' for '{}' ignored", nodeName(*child), sample_path);
      continue;
    }
    PendingKey key;
    const ArxReturnCode rc = parseKey(*child, transform * localTransform(*child), sample_path, units, key);
    if (rc != ARX_OK) return rc;
    keys.push_back(key);
  }

  if (keys.empty()) {
    PositionedAmbianceKey key;
    const ArxVector3 position = glb_object::toArxPoint(math::translation(transform), units);
    key.volume = ConstantAutomation{1.0f};
    key.pitch = ConstantAutomation{1.0f};
    key.x = ConstantAutomation{position.x};
    key.y = ConstantAutomation{position.y};
    key.z = ConstantAutomation{position.z};
    out.track.keys = std::vector<PositionedAmbianceKey>{key};
    return ARX_OK;
  }

  std::ranges::sort(keys, {}, &PendingKey::ordinal);
  for (std::size_t index = 1; index < keys.size(); ++index)
    if (keys[index - 1].ordinal == keys[index].ordinal) return ARX_GLB_BAD_AMBIANCE_KEY;

  const bool panned = keys.front().panned;
  if (std::ranges::any_of(keys, [panned](const PendingKey& key) { return key.panned != panned; }))
    return ARX_GLB_BAD_AMBIANCE_TRACK;
  if (panned) {
    std::vector<PannedAmbianceKey> result;
    result.reserve(keys.size());
    for (const PendingKey& key : keys) result.push_back(key.panned_key);
    out.track.keys = std::move(result);
  } else {
    std::vector<PositionedAmbianceKey> result;
    result.reserve(keys.size());
    for (const PendingKey& key : keys) result.push_back(key.positioned_key);
    out.track.keys = std::move(result);
  }
  return ARX_OK;
}

}  // namespace

ArxReturnCode importAmbianceFromGlb(std::span<const std::uint8_t> bytes, const Ambiance::GlbImportOptions& options,
                                    AmbianceModules& out, std::vector<SoundSourceReference>* sound_sources) {
  if (!glb_object::validUnits(options.arx_units_per_glb_unit)) return ARX_INVALID_OPTIONS;

  glb::Asset asset;
  ArxReturnCode rc = glb::parse(bytes, asset);
  if (rc != ARX_OK) return rc;
  cgltf_data& data = *asset.data();
  rc = glb::validateRequiredExtensions(data, {});
  if (rc != ARX_OK) return rc;

  glb::NodeGraph graph;
  rc = glb::buildNodeGraph(data, graph);
  if (rc != ARX_OK) return rc;

  std::vector<std::size_t> roots;
  for (std::size_t index : graph.preorder) {
    const std::string_view name = nodeName(data.nodes[index]);
    if (ambianceRootCandidate(name)) roots.push_back(index);
  }
  if (roots.empty()) return ARX_GLB_NO_AMBIANCE;
  if (roots.size() != 1) {
    for (std::size_t candidate : roots)
      for (std::size_t ancestor : roots)
        if (candidate != ancestor && glb::isDescendantOrSelf(graph, candidate, ancestor))
          return ARX_GLB_BAD_AMBIANCE_ROOT;
    return ARX_GLB_AMBIGUOUS_AMBIANCE;
  }
  const std::size_t root_index = roots.front();
  const cgltf_node& root = data.nodes[root_index];

  std::optional<std::uint32_t> master_ordinal;
  glb::ParsedLabel root_label;
  if (!rootName(nodeName(root), master_ordinal, root_label) || !glb::simpleEmptyNode(root))
    return ARX_GLB_BAD_AMBIANCE_ROOT;
  glb::reportConventionLabel("GLB -> Ambiance", nodeName(root), root_label);
  for (std::size_t index : graph.preorder) {
    if (index == root_index || !glb::isDescendantOrSelf(graph, index, root_index)) continue;
    if (nodeName(data.nodes[index]).starts_with("arx_")) return ARX_GLB_BAD_AMBIANCE_ROOT;
  }
  for (std::size_t index : graph.preorder) {
    if (index == root_index || glb::isDescendantOrSelf(graph, index, root_index)) continue;
    if (nodeName(data.nodes[index]).starts_with("arx_"))
      log(ARX_LOG_WARN, "GLB -> Ambiance: unrecognized node '{}' ignored", nodeName(data.nodes[index]));
  }

  std::vector<PendingTrack> tracks;
  tracks.reserve(root.children_count);
  for (std::size_t index = 0; index < root.children_count; ++index) {
    const cgltf_node* child = root.children[index];
    if (child == nullptr) return ARX_GLB_BAD_FORMAT;
    if (!nodeName(*child).starts_with("TRACK_")) {
      log(ARX_LOG_WARN, "GLB -> Ambiance: unexpected root child '{}' ignored", nodeName(*child));
      continue;
    }
    PendingTrack track;
    rc = parseTrack(*child, localTransform(*child), options.arx_units_per_glb_unit, track);
    if (rc != ARX_OK) return rc;
    tracks.push_back(std::move(track));
  }
  if (tracks.empty()) return ARX_AMBIANCE_NO_TRACKS;

  std::ranges::sort(tracks, {}, &PendingTrack::ordinal);
  for (std::size_t index = 1; index < tracks.size(); ++index)
    if (tracks[index - 1].ordinal == tracks[index].ordinal) return ARX_GLB_BAD_AMBIANCE_TRACK;

  AmbianceModules result;
  std::vector<SoundSourceReference> sources;
  if (sound_sources) sources.reserve(tracks.size());
  std::vector<glb::ImportedSoundSource> imported_sources;
  glb::SoundPathImporter sound_importer(
      result.sounds, sound_sources != nullptr ? &imported_sources : nullptr, "GLB -> Ambiance");
  result.ambiance.tracks.reserve(tracks.size());
  bool master_found = !master_ordinal.has_value();
  const std::uint32_t selected_ordinal = master_ordinal.value_or(tracks.front().ordinal);
  for (std::size_t index = 0; index < tracks.size(); ++index) {
    rc = soundPathImportError(
        sound_importer.import(SoundKind::kEffect, tracks[index].source_path, tracks[index].track.sound));
    if (rc != ARX_OK) return rc;
    if (tracks[index].ordinal == selected_ordinal) {
      result.ambiance.master_track = static_cast<AmbianceTrackIndex>(index);
      master_found = true;
    }
    result.ambiance.tracks.push_back(std::move(tracks[index].track));
  }
  if (!master_found) return ARX_GLB_BAD_AMBIANCE_ROOT;
  rc = soundPathImportError(sound_importer.finish());
  if (rc != ARX_OK) return rc;
  for (glb::ImportedSoundSource& source : imported_sources) {
    SoundIndex sound = kNoSound;
    if (!sounds::effectIndex(source.sound, sound)) return ARX_INTERNAL_ERROR;
    sources.push_back({sound, std::move(source.path)});
  }
  rc = ambiance_detail::validateStructure(result);
  if (rc != ARX_OK) return rc;
  rc = ambiance_detail::soundErrorCode(sounds::validateAudio(result.sounds));
  if (rc != ARX_OK) return rc;
  out = std::move(result);
  if (sound_sources) *sound_sources = std::move(sources);
  return ARX_OK;
}

}  // namespace pistoris
