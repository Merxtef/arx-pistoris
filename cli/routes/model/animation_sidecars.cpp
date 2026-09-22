// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/animation_sidecars.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/format.h"
#include "io/native_text.h"
#include "resources/animation_output.h"
#include "resources/layout.h"
#include "resources/selector.h"
#include "routes/model/invocation.h"
#include "routes/model/state.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cli::model {
namespace {

bool isIdentityRotation(const ArxQuat& rotation) noexcept {
  return rotation.w == 1.0f && rotation.x == 0.0f && rotation.y == 0.0f && rotation.z == 0.0f;
}

bool nativeAnimationIsEmpty(const pistoris::tea::Data& animation) noexcept {
  if (animation.num_groups != 0) return false;
  for (const pistoris::tea::Keyframe& keyframe : animation.keyframes) {
    if (keyframe.translate.value_or(ArxVector3{}) != ArxVector3{} ||
        !isIdentityRotation(keyframe.quat.value_or(ArxQuat{})) || keyframe.flag_frame == pistoris::kTeaFlagFrameStep ||
        keyframe.sample) {
      return false;
    }
  }
  return true;
}

bool intermediateAnimationIsEmpty(const pistoris::Animation& animation, bool& out) {
  out = false;
  if (animation.groupCount() != 0) return true;
  for (std::size_t index = 0; index < animation.keyframeCount(); ++index) {
    ArxAnimationKeyframe keyframe{};
    const ArxReturnCode rc = animation.copyKeyframes(index, 1, &keyframe);
    if (rc != ARX_OK) {
      diagnostic(DiagnosticCode::kModelOutputFailed,
                 "Animation inspection failed: %s (code %d)",
                 pistoris::errorString(rc),
                 static_cast<int>(rc));
      return false;
    }
    if (keyframe.root_translation != ArxVector3{} || !isIdentityRotation(keyframe.root_rotation) ||
        keyframe.footstep != 0 || keyframe.sound != ARX_NO_SOUND) {
      return true;
    }
  }
  out = true;
  return true;
}

std::string_view displayName(const AnimationOutputIdentity& identity) noexcept {
  if (!identity.name.empty()) return identity.name;
  if (!identity.resource_path.empty()) return identity.resource_path;
  return "<unnamed>";
}

bool buildIdentities(const ModelInput& input, std::vector<AnimationOutputIdentity>& identities) {
  identities.clear();
  if (const NativeModelFiles* native = std::get_if<NativeModelFiles>(&input)) {
    identities.reserve(native->animations.size());
    for (const NativeAnimationFile& animation : native->animations) {
      const void* terminator = std::memchr(animation.tea.name, '\0', sizeof(animation.tea.name));
      const std::size_t size =
          terminator ? static_cast<const char*>(terminator) - animation.tea.name : sizeof(animation.tea.name);
      std::string name;
      const ArxReturnCode rc = io_detail::nativeTextToUtf8({animation.tea.name, size}, animation.text_mode, name);
      if (rc != ARX_OK) {
        diagnostic(DiagnosticCode::kModelOutputFailed,
                   "Native Animation name cannot be decoded: %s (code %d)",
                   pistoris::errorString(rc),
                   static_cast<int>(rc));
        return false;
      }
      identities.push_back({std::move(name), animation.resource_path});
    }
    return true;
  }

  const IntermediateModel* intermediate = std::get_if<IntermediateModel>(&input);
  if (!intermediate) return false;
  identities.reserve(intermediate->animations.size());
  for (const std::unique_ptr<pistoris::Animation>& animation : intermediate->animations) {
    if (!animation) {
      diagnostic(DiagnosticCode::kModelOutputFailed, "Model contains a null Animation sidecar");
      return false;
    }
    identities.push_back({std::string(animation->name()), std::string(animation->resourcePath())});
  }
  return true;
}

bool sourceIsEmpty(const ModelInput& input, std::size_t index, bool& out) {
  if (const NativeModelFiles* native = std::get_if<NativeModelFiles>(&input)) {
    if (index >= native->animations.size()) return false;
    out = nativeAnimationIsEmpty(native->animations[index].tea);
    return true;
  }
  const IntermediateModel* intermediate = std::get_if<IntermediateModel>(&input);
  if (!intermediate || index >= intermediate->animations.size() || !intermediate->animations[index]) return false;
  return intermediateAnimationIsEmpty(*intermediate->animations[index], out);
}

}  // namespace

bool resolveAnimationSidecarOutputs(Invocation& invocation) {
  const Format output = invocation.output.format;
  invocation.animation_outputs.clear();
  if (output != Format::kFtl && output != Format::kJson) return true;

  std::vector<AnimationOutputIdentity> identities;
  if (!buildIdentities(invocation.state, identities)) return false;

  std::vector<std::size_t> exported_indices;
  std::vector<std::size_t> omitted_indices;
  exported_indices.reserve(identities.size());
  omitted_indices.reserve(identities.size());
  for (std::size_t index = 0; index < identities.size(); ++index) {
    bool empty = false;
    if (!sourceIsEmpty(invocation.state, index, empty)) {
      diagnostic(DiagnosticCode::kModelOutputFailed, "Cannot inspect Model Animation sidecar");
      return false;
    }
    (empty && !invocation.format.allow_empty_animation ? omitted_indices : exported_indices).push_back(index);
  }

  const auto select_identities = [&](std::span<const std::size_t> indices) {
    std::vector<AnimationOutputIdentity> selected;
    selected.reserve(indices.size());
    for (std::size_t index : indices) selected.push_back(identities[index]);
    return selected;
  };
  const auto plan_targets = [&](std::span<const AnimationOutputIdentity> selected,
                                std::span<const std::string_view>
                                    reserved,
                                std::vector<OutputTarget>& targets,
                                std::string& error) {
    if (output == Format::kFtl && invocation.output.layout == ResourceLayout::kGame) {
      return buildGameAnimationTargets(selected, invocation.animation_fallback_type, reserved, targets, error);
    }
    return buildAnimationTargets(selected,
                                 invocation.output,
                                 {},
                                 output == Format::kFtl ? Format::kTea : Format::kJson,
                                 reserved,
                                 targets,
                                 error);
  };

  const std::array<std::string_view, 1> primary_path = {invocation.output.path};
  const std::span<const std::string_view> primary_reserved =
      output == Format::kJson ? std::span<const std::string_view>(primary_path) : std::span<const std::string_view>{};
  const std::vector<AnimationOutputIdentity> exported = select_identities(exported_indices);
  std::vector<OutputTarget> targets;
  std::string error;
  if (!plan_targets(exported, primary_reserved, targets, error)) {
    diagnostic(DiagnosticCode::kModelOutputFailed, "%s", error.c_str());
    return false;
  }

  invocation.animation_outputs.reserve(targets.size());
  for (std::size_t index = 0; index < targets.size(); ++index)
    invocation.animation_outputs.push_back({exported_indices[index], std::move(targets[index])});

  if (omitted_indices.empty()) return true;

  std::vector<std::string_view> warning_reserved;
  warning_reserved.reserve(primary_reserved.size() + invocation.animation_outputs.size());
  warning_reserved.insert(warning_reserved.end(), primary_reserved.begin(), primary_reserved.end());
  for (const AnimationSidecarOutput& animation : invocation.animation_outputs)
    warning_reserved.push_back(animation.target.path);

  const std::vector<AnimationOutputIdentity> omitted = select_identities(omitted_indices);
  std::vector<OutputTarget> warning_targets;
  if (!plan_targets(omitted, warning_reserved, warning_targets, error)) {
    diagnostic(DiagnosticCode::kModelOutputFailed, "%s", error.c_str());
    return false;
  }
  for (std::size_t index = 0; index < warning_targets.size(); ++index) {
    const std::size_t source = omitted_indices[index];
    const std::string_view name = displayName(identities[source]);
    log(ARX_LOG_WARN,
        "Empty Animation '%.*s' omitted from '%s'; use --allow-empty-animation to include it",
        static_cast<int>(name.size()),
        name.data(),
        warning_targets[index].path.c_str());
  }
  return true;
}

}  // namespace cli::model
