// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "model/data.h"
#include "model/internal.h"
#include "modules/action_points.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

struct SelectionReferenceMap {
  std::array<SelectionMask, 64> target_bits{};
  std::array<std::size_t, 64> omitted_memberships{};
  std::size_t shared_selections = 0;
};

struct ActionProjection {
  std::vector<SelectionMask> masks;
  std::size_t matched_points = 0;
  std::size_t copied_memberships = 0;
  std::size_t cleared_target_memberships = 0;
  std::size_t unmatched_reference_points = 0;
  std::size_t unmatched_reference_memberships = 0;
};

SelectionReferenceMap mapReferenceSelections(const SelectionsData& target, const SelectionsData& reference) noexcept {
  SelectionReferenceMap result;
  for (SelectionId reference_id = 0; reference_id < 64U; ++reference_id) {
    if (!selections::occupied(reference, reference_id)) continue;
    for (SelectionId target_id = 0; target_id < 64U; ++target_id) {
      if (!selections::occupied(target, target_id)) continue;
      if (reference.slots[reference_id].name != target.slots[target_id].name) continue;
      result.target_bits[reference_id] = selections::bit(target_id);
      ++result.shared_selections;
      break;
    }
  }
  return result;
}

SelectionMask translateReferenceMask(SelectionMask source, SelectionReferenceMap& mapping) noexcept {
  SelectionMask result = 0;
  while (source != 0) {
    const SelectionId reference_id = static_cast<SelectionId>(std::countr_zero(source));
    const SelectionMask target = mapping.target_bits[reference_id];
    if (target == 0) {
      ++mapping.omitted_memberships[reference_id];
    } else {
      result |= target;
    }
    source &= source - 1;
  }
  return result;
}

std::vector<SelectionMask> projectBoneMasks(const SelectionsData& reference, SelectionReferenceMap& mapping,
                                            std::size_t bone_count, std::size_t& copied_memberships) {
  std::vector<SelectionMask> result(bone_count);
  for (std::size_t bone = 0; bone < bone_count; ++bone) {
    result[bone] = translateReferenceMask(reference.bone_masks[bone], mapping);
    copied_memberships += std::popcount(result[bone]);
  }
  return result;
}

ActionProjection projectActionMasks(const ActionPointsData& target_points, const SelectionsData& target_selections,
                                    const ActionPointsData& reference_points,
                                    const SelectionsData& reference_selections, SelectionReferenceMap& mapping) {
  ActionProjection result;
  result.masks.resize(target_points.points.size());
  std::vector<std::size_t> target_order(target_points.points.size());
  std::vector<std::size_t> reference_order(reference_points.points.size());
  std::iota(target_order.begin(), target_order.end(), std::size_t{0});
  std::iota(reference_order.begin(), reference_order.end(), std::size_t{0});

  const auto target_less = [&](std::size_t first, std::size_t second) {
    const std::string& first_name = target_points.points[first].name;
    const std::string& second_name = target_points.points[second].name;
    return first_name != second_name ? first_name < second_name : first < second;
  };
  const auto reference_less = [&](std::size_t first, std::size_t second) {
    const std::string& first_name = reference_points.points[first].name;
    const std::string& second_name = reference_points.points[second].name;
    return first_name != second_name ? first_name < second_name : first < second;
  };
  std::sort(target_order.begin(), target_order.end(), target_less);
  std::sort(reference_order.begin(), reference_order.end(), reference_less);

  std::size_t target = 0;
  std::size_t reference = 0;
  while (target < target_order.size() && reference < reference_order.size()) {
    const std::size_t target_index = target_order[target];
    const std::size_t reference_index = reference_order[reference];
    const std::string& target_name = target_points.points[target_index].name;
    const std::string& reference_name = reference_points.points[reference_index].name;
    if (target_name < reference_name) {
      result.cleared_target_memberships += std::popcount(target_selections.action_point_masks[target_index]);
      ++target;
      continue;
    }
    if (reference_name < target_name) {
      const SelectionMask translated =
          translateReferenceMask(reference_selections.action_point_masks[reference_index], mapping);
      result.unmatched_reference_memberships += std::popcount(translated);
      if (translated != 0) ++result.unmatched_reference_points;
      ++reference;
      continue;
    }

    result.masks[target_index] =
        translateReferenceMask(reference_selections.action_point_masks[reference_index], mapping);
    result.copied_memberships += std::popcount(result.masks[target_index]);
    ++result.matched_points;
    ++target;
    ++reference;
  }
  while (target < target_order.size()) {
    const std::size_t target_index = target_order[target++];
    result.cleared_target_memberships += std::popcount(target_selections.action_point_masks[target_index]);
  }
  while (reference < reference_order.size()) {
    const std::size_t reference_index = reference_order[reference++];
    const SelectionMask translated =
        translateReferenceMask(reference_selections.action_point_masks[reference_index], mapping);
    result.unmatched_reference_memberships += std::popcount(translated);
    if (translated != 0) ++result.unmatched_reference_points;
  }
  return result;
}

void logBoneNameMismatches(const SkeletonData& target, const SkeletonData& reference) noexcept {
  for (std::size_t index = 0; index < target.bones.size(); ++index) {
    if (target.bones[index].name == reference.bones[index].name) continue;
    log(ARX_LOG_WARN,
        "Model reference: bone {} name mismatch target='{}' reference='{}'",
        index,
        target.bones[index].name,
        reference.bones[index].name);
  }
}

void logOmittedSelections(const SelectionsData& reference, const SelectionReferenceMap& mapping) noexcept {
  for (SelectionId id = 0; id < 64U; ++id) {
    if (mapping.omitted_memberships[id] == 0) continue;
    log(ARX_LOG_WARN,
        "Model reference: selection '{}' is absent from target, omitted {} membership(s)",
        reference.slots[id].name,
        mapping.omitted_memberships[id]);
  }
}

}  // namespace

ArxReturnCode Model::applyReference(const Model& reference, const ReferenceOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const bool use_bones = options.snap_bone_origins || options.copy_bone_origin_selections;
    const bool use_selections = options.copy_bone_origin_selections || options.copy_action_point_selections;
    if (!use_bones && !use_selections) return ARX_INVALID_OPTIONS;

    if (use_bones) {
      const ArxReturnCode rc = model_detail::skeletonError(
          skeleton::validateReferenceCompatibility(data_->skeleton, reference.data_->skeleton));
      if (rc != ARX_OK) return rc;
    }

    SelectionReferenceMap selection_mapping;
    std::vector<SelectionMask> bone_masks;
    std::size_t copied_bone_memberships = 0;
    ActionProjection action_projection;
    if (use_selections) selection_mapping = mapReferenceSelections(data_->selections, reference.data_->selections);
    if (options.copy_bone_origin_selections) {
      bone_masks = projectBoneMasks(
          reference.data_->selections, selection_mapping, data_->skeleton.bones.size(), copied_bone_memberships);
    }
    if (options.copy_action_point_selections) {
      action_projection = projectActionMasks(data_->action_points,
                                             data_->selections,
                                             reference.data_->action_points,
                                             reference.data_->selections,
                                             selection_mapping);
    }

    float max_bone_distance = 0.0f;
    if (options.snap_bone_origins) {
      for (std::size_t index = 0; index < data_->skeleton.bones.size(); ++index) {
        max_bone_distance = std::max(
            max_bone_distance,
            math::lengthf(data_->skeleton.bones[index].position - reference.data_->skeleton.bones[index].position));
      }
      skeleton::copyBonePositions(data_->skeleton, reference.data_->skeleton);
    }
    if (options.copy_bone_origin_selections) selections::replaceBoneMasks(data_->selections, std::move(bone_masks));
    if (options.copy_action_point_selections)
      selections::replaceActionPointMasks(data_->selections, std::move(action_projection.masks));

    if (use_bones) logBoneNameMismatches(data_->skeleton, reference.data_->skeleton);
    if (use_selections) logOmittedSelections(reference.data_->selections, selection_mapping);
    if (options.snap_bone_origins) {
      log(ARX_LOG_INFO,
          "Model reference: snapped {} bone origin(s), max previous distance={}",
          data_->skeleton.bones.size(),
          max_bone_distance);
    }
    if (options.copy_bone_origin_selections) {
      log(ARX_LOG_INFO,
          "Model reference: copied {} bone-origin selection membership(s) across {} shared selection(s)",
          copied_bone_memberships,
          selection_mapping.shared_selections);
    }
    if (options.copy_action_point_selections) {
      if (action_projection.unmatched_reference_memberships != 0) {
        log(ARX_LOG_WARN,
            "Model reference: omitted {} selection membership(s) from {} unmatched reference action point(s)",
            action_projection.unmatched_reference_memberships,
            action_projection.unmatched_reference_points);
      }
      log(ARX_LOG_INFO,
          "Model reference: matched {} action point(s), copied {} selection membership(s), cleared {} unmatched target "
          "membership(s)",
          action_projection.matched_points,
          action_projection.copied_memberships,
          action_projection.cleared_target_memberships);
    }
    return ARX_OK;
  });
}

}  // namespace pistoris
