// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/sound.hpp"

#include "animation/data.h"
#include "cgltf/cgltf.h"
#include "external/glb/model/animation_export.h"
#include "external/glb/model/api.h"
#include "external/glb/model/mesh_export.h"
#include "external/glb/object_coordinates.h"
#include "external/glb/writer.h"
#include "model/data.h"
#include "modules/action_points.h"
#include "modules/selections.h"
#include "modules/skeleton.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pistoris::glb_model {
namespace {

void addSelectionHelpers(const SelectionsData& selections, SelectionMask mask, int parent, glb::Builder& builder) {
  for (SelectionId id = 0; id < 64U; ++id) {
    if ((mask & selections::bit(id)) == 0) continue;
    const int helper = builder.addNode("SELECTION_" + selections.slots[id].name + "__membership");
    builder.addChild(parent, helper);
  }
}

ArxVector3 relativePosition(const ArxVector3& position, BoneIndex bone, const SkeletonData& skeleton) {
  return bone == kInvalidBoneIndex ? position : position - skeleton.bones[bone].position;
}

}  // namespace

ArxReturnCode exportModelToGlb(const ModelModules& model, const Model::GlbExportOptions& options,
                               std::span<const AnimationModules* const> animations,
                               ArxAnimationConversionReport* report, std::vector<std::uint8_t>& out,
                               std::vector<AnimationSoundFile>* sound_files) {
  if (!glb_object::validUnits(options.arx_units_per_glb_unit)) return ARX_INVALID_OPTIONS;
  if (report) *report = {};

  glb::Builder builder;
  int mesh = -1;
  const ModelMeshExportOptions mesh_options{
      .position_scale = 1.0f / options.arx_units_per_glb_unit,
      .include_authoring_attributes = true,
  };
  ArxReturnCode rc = addModelMesh(model, mesh_options, builder, mesh);
  if (rc != ARX_OK) return rc;
  const int mesh_node = builder.addNode("mesh", mesh);
  const int root = builder.addNode("arx_model_origin__origin");
  builder.addRoot(root);

  std::vector<int> bone_nodes;
  int motion_node = root;
  if (!model.skeleton.bones.empty()) {
    motion_node = builder.addNode("skeleton");
    builder.addChild(root, motion_node);
    builder.addChild(motion_node, mesh_node);
    bone_nodes.reserve(model.skeleton.bones.size());
    std::vector<int> bone_helpers;
    bone_helpers.reserve(model.skeleton.bones.size());
    const int bones_parent = builder.addNode("bones_parent");
    builder.addRoot(bones_parent);
    for (std::size_t index = 0; index < model.skeleton.bones.size(); ++index) {
      const Bone& bone = model.skeleton.bones[index];
      const int node = builder.addNode(std::format("{:03}__{}", index, bone.name));
      const ArxVector3 relative = relativePosition(bone.position, bone.parent, model.skeleton);
      const ArxVector3 translation = glb_object::toGlbPoint(relative, options.arx_units_per_glb_unit);
      builder.setNodeTranslation(node, {translation.x, translation.y, translation.z});
      bone_nodes.push_back(node);
      const int helper = builder.addNode("arx_bone__" + bone.name);
      builder.addChild(bones_parent, helper);
      bone_helpers.push_back(helper);
    }
    for (std::size_t index = 0; index < model.skeleton.bones.size(); ++index) {
      const BoneIndex parent = model.skeleton.bones[index].parent;
      builder.addChild(parent == kInvalidBoneIndex ? motion_node : bone_nodes[parent], bone_nodes[index]);
    }

    std::vector<std::array<float, 16>> inverse_bind;
    inverse_bind.reserve(model.skeleton.bones.size());
    for (const Bone& bone : model.skeleton.bones) {
      const ArxVector3 position = glb_object::toGlbPoint(bone.position, options.arx_units_per_glb_unit);
      inverse_bind.push_back({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -position.x, -position.y, -position.z, 1});
    }
    const int inverse_bind_accessor = builder.addAccessor(
        std::span<const std::array<float, 16>>(inverse_bind), cgltf_component_type_r_32f, cgltf_type_mat4);
    const int skin = builder.addSkin("skeleton", bone_nodes, motion_node, inverse_bind_accessor);
    builder.setNodeSkin(mesh_node, skin);

    for (std::size_t index = 0; index < model.skeleton.bones.size(); ++index) {
      const Bone& bone = model.skeleton.bones[index];
      addSelectionHelpers(model.selections, model.selections.bone_masks[index], bone_helpers[index], builder);
      if (bone.blob_shadow_size != 0.0f) {
        const float size = bone.blob_shadow_size / options.arx_units_per_glb_unit;
        const int helper = builder.addNode(std::format("SETTINGS__BLOB_SHADOW_{}__settings", size));
        builder.addChild(bone_helpers[index], helper);
      }
      if (model.skeleton.origin_bone == index && index != 0)
        builder.addChild(bone_helpers[index], builder.addNode("ORIGIN_OWNER__origin"));
    }
  } else {
    builder.addChild(root, mesh_node);
  }

  addSelectionHelpers(model.selections, model.selections.origin_mask, root, builder);

  for (std::size_t index = 0; index < model.action_points.points.size(); ++index) {
    const ActionPoint& action = model.action_points.points[index];
    const int node = builder.addNode(std::format("arx_action__{}__action_{}", action.name, index));
    const ArxVector3 relative = relativePosition(action.position, action.bone, model.skeleton);
    const ArxVector3 translation = glb_object::toGlbPoint(relative, options.arx_units_per_glb_unit);
    builder.setNodeTranslation(node, {translation.x, translation.y, translation.z});
    builder.addChild(action.bone == kInvalidBoneIndex ? motion_node : bone_nodes[action.bone], node);
    addSelectionHelpers(model.selections, model.selections.action_point_masks[index], node, builder);
  }

  for (SelectionId id = 0; id < 64U; ++id) {
    const std::optional<SelectionLeadingVertex>& leading_vertex = model.selections.slots[id].leading_vertex;
    if (!selections::occupied(model.selections, id) || !leading_vertex) continue;
    const SelectionLeadingVertex& leading = *leading_vertex;
    const int node = builder.addNode("arx_selection_probe__" + model.selections.slots[id].name + "__point");
    const ArxVector3 relative = relativePosition(leading.position, leading.bone, model.skeleton);
    const ArxVector3 translation = glb_object::toGlbPoint(relative, options.arx_units_per_glb_unit);
    builder.setNodeTranslation(node, {translation.x, translation.y, translation.z});
    builder.addChild(leading.bone == kInvalidBoneIndex ? motion_node : bone_nodes[leading.bone], node);
  }

  rc = addAnimationsToGlb(
      model, animations, report, motion_node, bone_nodes, options.arx_units_per_glb_unit, builder, sound_files);
  if (rc != ARX_OK) return rc;
  return builder.write(out);
}

}  // namespace pistoris::glb_model

namespace pistoris {

ArxReturnCode exportModelToGlb(const ModelModules& model, const Model::GlbExportOptions& options,
                               std::span<const AnimationModules* const> animations,
                               ArxAnimationConversionReport* report, std::vector<std::uint8_t>& out,
                               std::vector<AnimationSoundFile>* sound_files) {
  return glb_model::exportModelToGlb(model, options, animations, report, out, sound_files);
}

}  // namespace pistoris
