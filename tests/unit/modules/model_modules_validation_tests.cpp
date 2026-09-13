// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"

#include "model/internal.h"
#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "modules/textures.h"

#include <array>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

TEST_SUITE("Model modules") {
  TEST_CASE("Module errors map to focused Model return codes") {
    using pistoris::model_detail::actionPointError;
    using pistoris::model_detail::geometryError;
    using pistoris::model_detail::selectionError;
    using pistoris::model_detail::skeletonError;
    using pistoris::model_detail::textureError;

    CHECK(geometryError(pistoris::geometry::Error::kNoGeometry) == ARX_MODEL_NO_GEOMETRY);
    CHECK(geometryError(pistoris::geometry::Error::kTooManyVertices) == ARX_MODEL_TOO_MANY_VERTICES);
    CHECK(geometryError(pistoris::geometry::Error::kTooManyFaces) == ARX_MODEL_TOO_MANY_FACES);
    CHECK(geometryError(pistoris::geometry::Error::kBadVertex) == ARX_MODEL_BAD_VERTEX_POSITION);
    CHECK(textureError(pistoris::textures::Error::kTooManyTextures) == ARX_MODEL_TOO_MANY_TEXTURES);
    CHECK(textureError(pistoris::textures::Error::kBadTexture) == ARX_MODEL_BAD_TEXTURE_PATH);
    CHECK(textureError(pistoris::textures::Error::kDuplicateTexture) == ARX_MODEL_BAD_TEXTURE_PATH);
    CHECK(textureError(pistoris::textures::Error::kBadImage) == ARX_MODEL_BAD_TEXTURE_IMAGE);
    CHECK(geometryError(pistoris::geometry::Error::kBadFaceTexture) == ARX_MODEL_BAD_FACE_TEXTURE);
    CHECK(geometryError(pistoris::geometry::Error::kBadFaceVertex) == ARX_MODEL_BAD_FACE_VERTEX);
    CHECK(geometryError(pistoris::geometry::Error::kBadCornerNormal) == ARX_MODEL_BAD_CORNER_NORMAL);
    CHECK(geometryError(pistoris::geometry::Error::kBadFaceType) == ARX_MODEL_BAD_FACE_TYPE);
    CHECK(geometryError(pistoris::geometry::Error::kBadFaceTransval) == ARX_MODEL_BAD_FACE_TRANSVAL);
    CHECK(geometryError(pistoris::geometry::Error::kBadFaceNormal) == ARX_MODEL_BAD_FACE_NORMAL);
    CHECK(geometryError(pistoris::geometry::Error::kBadFaceUv) == ARX_MODEL_BAD_FACE_UV);
    CHECK(geometryError(pistoris::geometry::Error::kDegenerateFace) == ARX_MODEL_DEGENERATE_FACE);

    CHECK(skeletonError(pistoris::skeleton::Error::kTooManyBones) == ARX_MODEL_TOO_MANY_BONES);
    CHECK(skeletonError(pistoris::skeleton::Error::kBadIndex) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(skeletonError(pistoris::skeleton::Error::kBadName) == ARX_MODEL_BAD_BONE_NAME);
    CHECK(skeletonError(pistoris::skeleton::Error::kDuplicateName) == ARX_MODEL_DUPLICATE_BONE_NAME);
    CHECK(skeletonError(pistoris::skeleton::Error::kBadPosition) == ARX_MODEL_BAD_BONE_POSITION);
    CHECK(skeletonError(pistoris::skeleton::Error::kBadParent) == ARX_MODEL_BAD_BONE_PARENT);
    CHECK(skeletonError(pistoris::skeleton::Error::kBadBlobShadowSize) == ARX_MODEL_BAD_BONE_BLOB_SHADOW_SIZE);
    CHECK(skeletonError(pistoris::skeleton::Error::kBadVertexBoneCount) == ARX_MODEL_BAD_VERTEX_BONE_COUNT);
    CHECK(skeletonError(pistoris::skeleton::Error::kBadVertexBone) == ARX_MODEL_BAD_VERTEX_BONE);
    CHECK(skeletonError(pistoris::skeleton::Error::kBadOriginBone) == ARX_MODEL_BAD_ORIGIN_BONE);

    CHECK(actionPointError(pistoris::action_points::Error::kTooManyActionPoints) == ARX_MODEL_TOO_MANY_ACTION_POINTS);
    CHECK(actionPointError(pistoris::action_points::Error::kBadIndex) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(actionPointError(pistoris::action_points::Error::kBadName) == ARX_MODEL_BAD_ACTION_POINT_NAME);
    CHECK(actionPointError(pistoris::action_points::Error::kBadPosition) == ARX_MODEL_BAD_ACTION_POINT_POSITION);
    CHECK(actionPointError(pistoris::action_points::Error::kBadBone) == ARX_MODEL_BAD_ACTION_POINT_BONE);

    CHECK(selectionError(pistoris::selections::Error::kTooManySelections) == ARX_MODEL_TOO_MANY_SELECTIONS);
    CHECK(selectionError(pistoris::selections::Error::kBadId) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(selectionError(pistoris::selections::Error::kBadCount) == ARX_INTERNAL_ERROR);
    CHECK(selectionError(pistoris::selections::Error::kBadMask) == ARX_INTERNAL_ERROR);
    CHECK(selectionError(pistoris::selections::Error::kBadVertexMember) == ARX_MODEL_BAD_SELECTION_VERTEX);
    CHECK(selectionError(pistoris::selections::Error::kBadBoneMember) == ARX_MODEL_BAD_SELECTION_BONE);
    CHECK(selectionError(pistoris::selections::Error::kBadActionPointMember) == ARX_MODEL_BAD_SELECTION_ACTION_POINT);
    CHECK(selectionError(pistoris::selections::Error::kBadName) == ARX_MODEL_BAD_SELECTION_NAME);
    CHECK(selectionError(pistoris::selections::Error::kDuplicateName) == ARX_MODEL_DUPLICATE_SELECTION_NAME);
    CHECK(selectionError(pistoris::selections::Error::kBadLeadingPosition) == ARX_MODEL_BAD_SELECTION_LEADING_POSITION);
    CHECK(selectionError(pistoris::selections::Error::kBadLeadingBone) == ARX_MODEL_BAD_SELECTION_LEADING_BONE);
  }

  TEST_CASE("Skeleton requires an ordered forest and aligned vertex ownership") {
    pistoris::SkeletonData data;
    data.bones = {
        {.name = "root_0", .position = {}, .parent = pistoris::kInvalidBoneIndex},
        {.name = "root_1", .position = {}, .parent = pistoris::kInvalidBoneIndex},
        {.name = "child", .position = {}, .parent = 1},
    };
    data.vertex_bones = {0, 2};
    CHECK(pistoris::skeleton::validate(data, 2) == pistoris::skeleton::Error::kNone);

    data.bones[2].parent = 2;
    CHECK(pistoris::skeleton::validate(data, 2) == pistoris::skeleton::Error::kBadParent);
    data.bones[2].parent = 1;
    data.bones[2].name = "root_0";
    CHECK(pistoris::skeleton::validate(data, 2) == pistoris::skeleton::Error::kDuplicateName);
  }

  TEST_CASE("Skeleton reference validates compatibility before copying positions") {
    pistoris::SkeletonData target;
    target.bones = {
        {.name = "root_0", .position = {1.0f, 2.0f, 3.0f}, .parent = pistoris::kInvalidBoneIndex},
        {.name = "root_1", .position = {4.0f, 5.0f, 6.0f}, .parent = pistoris::kInvalidBoneIndex},
        {.name = "child", .position = {7.0f, 8.0f, 9.0f}, .parent = 1},
    };
    pistoris::SkeletonData reference = target;
    reference.bones[0].position = {10.0f, 11.0f, 12.0f};
    reference.bones[1].position = {13.0f, 14.0f, 15.0f};
    reference.bones[2].position = {16.0f, 17.0f, 18.0f};

    CHECK(pistoris::skeleton::validateReferenceCompatibility(target, reference) == pistoris::skeleton::Error::kNone);
    pistoris::skeleton::copyBonePositions(target, reference);
    CHECK(target.bones[0].position == reference.bones[0].position);
    CHECK(target.bones[1].position == reference.bones[1].position);
    CHECK(target.bones[2].position == reference.bones[2].position);

    reference.bones.push_back({.name = "extra", .position = {}, .parent = 2});
    CHECK(pistoris::skeleton::validateReferenceCompatibility(target, reference) ==
          pistoris::skeleton::Error::kReferenceBoneCountMismatch);
    reference.bones.pop_back();
    reference.bones[2].parent = 0;
    CHECK(pistoris::skeleton::validateReferenceCompatibility(target, reference) ==
          pistoris::skeleton::Error::kReferenceBoneTopologyMismatch);
  }

  TEST_CASE("Skeleton candidates validate before mutation") {
    pistoris::SkeletonData data;
    CHECK(pistoris::skeleton::validateBoneCount(pistoris::skeleton::kMaxBones) == pistoris::skeleton::Error::kNone);
    CHECK(pistoris::skeleton::validateBoneCount(pistoris::skeleton::kMaxBones + 1U) ==
          pistoris::skeleton::Error::kTooManyBones);
    pistoris::Bone root{.name = "root", .position = {}, .parent = pistoris::kInvalidBoneIndex};
    REQUIRE(pistoris::skeleton::validateBoneCount(1) == pistoris::skeleton::Error::kNone);
    REQUIRE(pistoris::skeleton::validateBone(root, 0) == pistoris::skeleton::Error::kNone);
    pistoris::BoneIndex index = pistoris::skeleton::addBone(data, std::move(root));
    CHECK(index == 0);
    pistoris::Bone other_root{.name = "other_root", .position = {}, .parent = pistoris::kInvalidBoneIndex};
    REQUIRE(pistoris::skeleton::validateBone(other_root, 1) == pistoris::skeleton::Error::kNone);
    index = pistoris::skeleton::addBone(data, std::move(other_root));
    CHECK(index == 1);
    CHECK(data.bones.size() == 2);

    pistoris::Bone child{.name = "child", .position = {}, .parent = 1};
    REQUIRE(pistoris::skeleton::validateBone(child, 2) == pistoris::skeleton::Error::kNone);
    index = pistoris::skeleton::addBone(data, std::move(child));
    CHECK(index == 2);
    pistoris::Bone replacement{.name = "child", .position = {}, .parent = pistoris::kInvalidBoneIndex};
    REQUIRE(pistoris::skeleton::validateBone(replacement, 2) == pistoris::skeleton::Error::kNone);
    pistoris::skeleton::setBone(data, 2, std::move(replacement));

    pistoris::Bone duplicate{.name = "root", .position = {}, .parent = 1};
    pistoris::skeleton::repairNames(data, std::span<pistoris::Bone>(&duplicate, 1), 2);
    CHECK(duplicate.name == "root_1");
    CHECK(data.bones[2].name == "child");

    const std::array<pistoris::BoneIndex, 2> vertex_bones = {0, 2};
    CHECK(pistoris::skeleton::validateBoneReferences(vertex_bones, 1, 2, 3) == pistoris::skeleton::Error::kNone);
    CHECK(pistoris::skeleton::validateBoneReferences(vertex_bones, 3, 2, 3) ==
          pistoris::skeleton::Error::kBadOriginBone);
  }

  TEST_CASE("Selections require aligned masks and valid occupied slots") {
    pistoris::SelectionsData data;
    data.occupied = pistoris::selections::bit(0);
    data.slots[0].name = "cut_head";
    data.vertex_masks = {pistoris::selections::bit(0)};
    data.bone_masks = {0};
    data.action_point_masks = {0};
    CHECK(pistoris::selections::validate(data, 1, 1, 1) == pistoris::selections::Error::kNone);

    data.vertex_masks[0] = pistoris::selections::bit(1);
    CHECK(pistoris::selections::validate(data, 1, 1, 1) == pistoris::selections::Error::kBadMask);
    data.vertex_masks[0] = pistoris::selections::bit(0);
    data.slots[0].name = "CUT_HEAD";
    CHECK(pistoris::selections::validate(data, 1, 1, 1) == pistoris::selections::Error::kBadName);
    data.slots[0].name = "cut_head";
    data.slots[0].leading_vertex = {{}, 1};
    CHECK(pistoris::selections::validate(data, 1, 1, 1) == pistoris::selections::Error::kBadLeadingBone);
  }

  TEST_CASE("Removing a selection clears every membership without moving other slots") {
    pistoris::SelectionsData data;
    data.occupied = pistoris::selections::bit(2) | pistoris::selections::bit(5);
    data.slots[2].name = "head";
    data.slots[5].name = "cut_head";
    data.vertex_masks = {data.occupied};
    data.bone_masks = {pistoris::selections::bit(2)};
    data.action_point_masks = {pistoris::selections::bit(5)};
    data.origin_mask = data.occupied;

    pistoris::selections::removeSelection(data, 2);

    CHECK_FALSE(pistoris::selections::occupied(data, 2));
    CHECK(pistoris::selections::occupied(data, 5));
    CHECK(data.slots[5].name == "cut_head");
    CHECK(data.vertex_masks[0] == pistoris::selections::bit(5));
    CHECK(data.bone_masks[0] == 0);
    CHECK(data.action_point_masks[0] == pistoris::selections::bit(5));
    CHECK(data.origin_mask == pistoris::selections::bit(5));
    CHECK_FALSE(pistoris::selections::occupied(data, 2));
  }

  TEST_CASE("Selection semantic candidates reject unoccupied slots") {
    pistoris::SelectionsData data;
    data.vertex_masks = {0};
    data.bone_masks = {0};
    data.action_point_masks = {0};

    CHECK(pistoris::selections::validateMemberUpdate(data, 0, std::nullopt, std::nullopt, std::nullopt) ==
          pistoris::selections::Error::kBadId);
    CHECK(data.vertex_masks == std::vector<pistoris::SelectionMask>{0});
    CHECK(data.bone_masks == std::vector<pistoris::SelectionMask>{0});
    CHECK(data.action_point_masks == std::vector<pistoris::SelectionMask>{0});
    CHECK(data.origin_mask == 0);
  }

  TEST_CASE("Selection synchronization appends empty masks") {
    pistoris::SelectionsData data;
    data.occupied = pistoris::selections::bit(4);
    data.slots[4].name = "selection";

    pistoris::selections::appendEmptyVertexMask(data);
    pistoris::selections::appendEmptyBoneMask(data);
    pistoris::selections::appendEmptyActionPointMask(data);

    CHECK(data.vertex_masks == std::vector<pistoris::SelectionMask>{0});
    CHECK(data.bone_masks == std::vector<pistoris::SelectionMask>{0});
    CHECK(data.action_point_masks == std::vector<pistoris::SelectionMask>{0});
    CHECK(pistoris::selections::validate(data, 1, 1, 1) == pistoris::selections::Error::kNone);
  }

  TEST_CASE("Selection operations own stable slot allocation and capacity") {
    pistoris::SelectionsData data;
    const auto make_selection = [](std::string name) {
      pistoris::Selection selection;
      selection.name = std::move(name);
      return selection;
    };
    for (pistoris::SelectionId expected = 0; expected < 64U; ++expected) {
      pistoris::Selection selection = make_selection("selection_" + std::to_string(expected));
      REQUIRE(pistoris::selections::validateSelectionAppend(data) == pistoris::selections::Error::kNone);
      REQUIRE(pistoris::selections::validateSelection(selection, 0) == pistoris::selections::Error::kNone);
      const pistoris::SelectionId id = pistoris::selections::addSelection(data, std::move(selection));
      CHECK(id == expected);
    }

    CHECK(pistoris::selections::validateSelectionAppend(data) == pistoris::selections::Error::kTooManySelections);
    CHECK_FALSE(pistoris::selections::occupied(data, 64U));

    pistoris::Selection duplicate = make_selection("selection_0");
    pistoris::selections::repairNames(data, std::span<pistoris::Selection>(&duplicate, 1), 1U);
    CHECK(duplicate.name == "selection_0_1");
  }

  TEST_CASE("Action points require lowercase names and valid bones") {
    pistoris::ActionPointsData points;
    points.points = {
        {.name = "view_attach", .position = {}, .bone = 0},
        {.name = "primary_attach", .position = {}, .bone = 0},
    };
    CHECK(pistoris::action_points::validate(points, 1) == pistoris::action_points::Error::kNone);
    points.points[1].name = "VIEW_ATTACH";
    CHECK(pistoris::action_points::validate(points, 1) == pistoris::action_points::Error::kBadName);
    points.points[1].name = "view_attach";
    CHECK(pistoris::action_points::validate(points, 1) == pistoris::action_points::Error::kNone);
    points.points[1].name = "primary_attach";
    points.points[1].position.x = std::numeric_limits<float>::infinity();
    CHECK(pistoris::action_points::validate(points, 1) == pistoris::action_points::Error::kBadPosition);
  }

  TEST_CASE("Action-point candidates validate before mutation") {
    pistoris::ActionPointsData points;
    pistoris::ActionPoint point{.name = "view_attach", .position = {}, .bone = 0};
    REQUIRE(pistoris::action_points::validateCount(1) == pistoris::action_points::Error::kNone);
    REQUIRE(pistoris::action_points::validatePoint(point, 1) == pistoris::action_points::Error::kNone);
    pistoris::ActionPointIndex index = pistoris::action_points::addActionPoint(points, point);
    CHECK(index == 0);
    index = pistoris::action_points::addActionPoint(points, point);
    CHECK(index == 1);
    CHECK(points.points.size() == 2);
    CHECK(pistoris::action_points::validatePoint({.name = "view_attach", .position = {}, .bone = 1}, 1) ==
          pistoris::action_points::Error::kBadBone);
    CHECK(points.points[0].bone == 0);
  }
}
