// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "ambiance/data.h"
#include "api.h"
#include "external/glb/accessor.h"
#include "external/glb/model/mesh_export.h"
#include "external/glb/object_coordinates.h"
#include "external/glb/utils/tokens.h"
#include "external/glb/writer.h"
#include "model/data.h"
#include "modules/action_points.h"
#include "modules/ambiance.h"
#include "utils/log.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace pistoris {
namespace {

constexpr float kPanRadiusArx = 50.0f;

struct AutomationParts {
  float center = 0.0f;
  float range = 0.0f;
  std::uint32_t interval_ms = 0;
  DynamicAutomationMode mode = DynamicAutomationMode::kStep;
  bool dynamic = false;
};

glb::Vec3 toGlbPoint(const ArxVector3& value, float units) noexcept {
  const ArxVector3 point = glb_object::toGlbPoint(value, units);
  return {point.x, point.y, point.z};
}

AutomationParts splitAutomation(const Automation& automation) noexcept {
  if (const auto* constant = std::get_if<ConstantAutomation>(&automation)) return {constant->value};
  const auto* dynamic = std::get_if<DynamicAutomation>(&automation);
  if (dynamic == nullptr) return {};
  const double center = (static_cast<double>(dynamic->first) + static_cast<double>(dynamic->second)) * 0.5;
  const double range = (static_cast<double>(dynamic->second) - static_cast<double>(dynamic->first)) * 0.5;
  return {static_cast<float>(center), static_cast<float>(range), dynamic->interval_ms, dynamic->mode, true};
}

void scaleAutomationRange(AutomationParts& parts, float divisor, std::string_view type, std::string_view sample_path) {
  if (!parts.dynamic) return;
  parts.range /= divisor;
  if (parts.range != 0.0f) return;
  parts.dynamic = false;
  log(ARX_LOG_WARN,
      "Ambiance -> GLB: {} automation for '{}' is too small at the selected scale; using its "
      "midpoint as a constant",
      type,
      sample_path);
}

std::string modeToken(DynamicAutomationMode mode) {
  switch (mode) {
    case DynamicAutomationMode::kStep:
      return {};
    case DynamicAutomationMode::kRandomStep:
      return "RANDOM_STEP";
    case DynamicAutomationMode::kInterpolated:
      return "INTERPOLATED";
    case DynamicAutomationMode::kRandomInterpolated:
      return "RANDOM_INTERPOLATED";
  }
  return {};
}

std::string automationName(std::string_view type, const AutomationParts& parts, bool include_value,
                           std::string_view label) {
  std::string name(type);
  if (include_value) name += "__VAL_" + glb::formatFloatToken(parts.center);
  if (parts.dynamic) {
    name += "__RANGE_" + glb::formatFloatToken(parts.range);
    if (parts.interval_ms != 1000) name += "__INTERVAL_" + std::to_string(parts.interval_ms);
    const std::string mode = modeToken(parts.mode);
    if (!mode.empty()) name += "__" + mode;
  }
  name += "__";
  name += label;
  return name;
}

std::string keyName(std::size_t index, const AmbianceKeyCommon& key) {
  std::string name = std::format("KEY_{:03}", index);
  if (key.play_count != 1) name += "__PLAY_COUNT_" + std::to_string(key.play_count);
  if (key.start_delay_ms != 0) name += "__START_" + std::to_string(key.start_delay_ms);
  if (key.delay_min_ms != 0) name += "__DELAY_MIN_" + std::to_string(key.delay_min_ms);
  if (key.delay_max_ms != key.delay_min_ms) name += "__DELAY_MAX_" + std::to_string(key.delay_max_ms);
  name += "__key_" + std::to_string(index);
  return name;
}

bool isDefault(const AutomationParts& parts, float value) noexcept { return !parts.dynamic && parts.center == value; }

void addCommonAutomation(glb::Builder& builder, int key_node, const AmbianceKeyCommon& key,
                         std::string_view sample_path) {
  AutomationParts volume = splitAutomation(key.volume);
  scaleAutomationRange(volume, 1.0f, "VOLUME", sample_path);
  if (!isDefault(volume, 1.0f))
    builder.addChild(key_node, builder.addNode(automationName("VOLUME", volume, true, "volume")));
  AutomationParts pitch = splitAutomation(key.pitch);
  scaleAutomationRange(pitch, 1.0f, "PITCH", sample_path);
  if (!isDefault(pitch, 1.0f))
    builder.addChild(key_node, builder.addNode(automationName("PITCH", pitch, true, "pitch")));
}

void addPositionedKey(glb::Builder& builder, int track_node, const PositionedAmbianceKey& key, std::size_t index,
                      float units, std::string_view sample_path) {
  AutomationParts x = splitAutomation(key.x);
  AutomationParts y = splitAutomation(key.y);
  AutomationParts z = splitAutomation(key.z);
  scaleAutomationRange(x, units, "X", sample_path);
  scaleAutomationRange(y, units, "Y", sample_path);
  scaleAutomationRange(z, units, "Z", sample_path);
  const int key_node = builder.addNode(keyName(index, key));
  builder.setNodeTranslation(key_node, toGlbPoint({x.center, y.center, z.center}, units));
  builder.addChild(track_node, key_node);
  addCommonAutomation(builder, key_node, key, sample_path);
  if (x.dynamic) builder.addChild(key_node, builder.addNode(automationName("X", x, false, "x")));
  if (y.dynamic) builder.addChild(key_node, builder.addNode(automationName("Y", y, false, "y")));
  if (z.dynamic) builder.addChild(key_node, builder.addNode(automationName("Z", z, false, "z")));
}

void addPannedKey(glb::Builder& builder, int track_node, const PannedAmbianceKey& key, std::size_t index, float units,
                  std::string_view sample_path) {
  AutomationParts pan = splitAutomation(key.pan);
  scaleAutomationRange(pan, 1.0f, "PAN", sample_path);
  const float clamped = std::clamp(pan.center, -1.0f, 1.0f);
  const float radius = kPanRadiusArx / units;
  const int key_node = builder.addNode(keyName(index, key));
  builder.setNodeTranslation(key_node, {-radius * clamped, 0.0f, radius * std::sqrt(1.0f - clamped * clamped)});
  builder.addChild(track_node, key_node);
  addCommonAutomation(builder, key_node, key, sample_path);

  const int pan_node = builder.addNode(automationName("PAN", pan, pan.center < -1.0f || pan.center > 1.0f, "pan"));
  builder.addChild(key_node, pan_node);
}

}  // namespace

ArxReturnCode exportAmbianceToGlb(const AmbianceModules& modules, const Ambiance::GlbExportOptions& options,
                                  const ModelModules* reference_model, std::vector<std::uint8_t>& out) {
  if (!glb_object::validUnits(options.arx_units_per_glb_unit)) return ARX_INVALID_OPTIONS;

  glb::Builder builder;
  std::string root_name = "arx_ambiance";
  if (modules.ambiance.master_track != 0) root_name += "__MASTER_" + std::to_string(modules.ambiance.master_track);
  root_name += "__ambiance";
  const int root = builder.addNode(std::move(root_name));
  builder.addRoot(root);

  if (reference_model != nullptr) {
    int mesh = -1;
    const glb_model::ModelMeshExportOptions mesh_options{
        .position_scale = 1.0f / options.arx_units_per_glb_unit,
        .context = "Model reference preview -> GLB",
        .mesh_name = "reference_model",
    };
    const ArxReturnCode rc = glb_model::addModelMesh(*reference_model, mesh_options, builder, mesh);
    if (rc != ARX_OK) return rc;
    builder.addRoot(builder.addNode("reference_model", mesh));

    const ActionPoint* view_attach = nullptr;
    std::size_t view_attach_count = 0;
    for (const ActionPoint& point : reference_model->action_points.points) {
      if (point.name != "view_attach") continue;
      if (view_attach == nullptr) view_attach = &point;
      ++view_attach_count;
    }
    if (view_attach_count > 1)
      log(ARX_LOG_WARN,
          "Ambiance -> GLB: reference Model has {} view_attach action points; using the first",
          view_attach_count);
    if (view_attach != nullptr)
      builder.setNodeTranslation(root, toGlbPoint(view_attach->position, options.arx_units_per_glb_unit));
  }

  for (std::size_t track_index = 0; track_index < modules.ambiance.tracks.size(); ++track_index) {
    const AmbianceTrack& track = modules.ambiance.tracks[track_index];
    if (track.sound >= modules.sounds.sounds.size()) return ARX_AMBIANCE_BAD_TRACK_SOUND;
    const std::string& sample_path = modules.sounds.sounds[track.sound].path;
    const int track_node =
        builder.addNode(std::format("TRACK_{:03}__{}__track_{}", track_index, sample_path, track_index));
    builder.addChild(root, track_node);
    if (const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&track.keys)) {
      for (std::size_t key_index = 0; key_index < keys->size(); ++key_index)
        addPannedKey(builder, track_node, (*keys)[key_index], key_index, options.arx_units_per_glb_unit, sample_path);
    } else if (const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&track.keys)) {
      for (std::size_t key_index = 0; key_index < keys->size(); ++key_index)
        addPositionedKey(
            builder, track_node, (*keys)[key_index], key_index, options.arx_units_per_glb_unit, sample_path);
    } else {
      return ARX_INTERNAL_ERROR;
    }
  }

  return builder.write(out);
}

}  // namespace pistoris
