// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/model/types.h"

#include "image_helpers.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

ArxStringView modelStringView(std::string_view value) { return {value.data(), value.size()}; }

}  // namespace

TEST_SUITE("C Model editing") {
  TEST_CASE("Model sections support inspection, mutation, compaction, and clearing") {
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_create(&model) == ARX_OK);

    ArxModelBone root{};
    root.name = modelStringView("root");
    root.position = {0.0f, 0.0f, 0.0f};
    ArxBoneIndex root_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_bone(model, &root, &root_index) == ARX_OK);
    ArxModelBone child{};
    child.name = modelStringView("child");
    child.position = {0.0f, 1.0f, 0.0f};
    child.parent = root_index;
    ArxBoneIndex child_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_bone(model, &child, &child_index) == ARX_OK);
    root.position.x = 0.25f;
    REQUIRE(arx_pistoris_model_set_bone(model, root_index, &root) == ARX_OK);
    ArxModelBone copied_bone{};
    REQUIRE(arx_pistoris_model_copy_bones(model, root_index, 1, &copied_bone) == ARX_OK);
    CHECK(copied_bone.position.x == doctest::Approx(0.25f));
    REQUIRE(arx_pistoris_model_set_origin(model, {root_index}) == ARX_OK);

    ArxModelActionPoint action{};
    action.name = modelStringView("view_attach");
    action.position = {0.0f, 1.0f, 0.0f};
    action.bone = child_index;
    ArxActionPointIndex action_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_action_point(model, &action, &action_index) == ARX_OK);
    action.position.y = 2.0f;
    REQUIRE(arx_pistoris_model_set_action_point(model, action_index, &action) == ARX_OK);
    ArxModelActionPoint copied_action{};
    REQUIRE(arx_pistoris_model_copy_action_points(model, action_index, 1, &copied_action) == ARX_OK);
    CHECK(copied_action.position.y == doctest::Approx(2.0f));

    std::array<ArxModelVertex, 4> vertices{};
    vertices[0] = {{0.0f, 0.0f, 0.0f}, root_index};
    vertices[1] = {{1.0f, 0.0f, 0.0f}, root_index};
    vertices[2] = {{0.0f, 1.0f, 0.0f}, root_index};
    vertices[3] = {{2.0f, 2.0f, 0.0f}, child_index};
    ArxVertexIndex first_vertex = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_vertices(model, vertices.data(), vertices.size(), &first_vertex) == ARX_OK);
    vertices[3].position = {3.0f, 3.0f, 0.0f};
    REQUIRE(arx_pistoris_model_set_vertex(model, 3, &vertices[3]) == ARX_OK);
    ArxModelVertex copied_vertex{};
    REQUIRE(arx_pistoris_model_copy_vertices(model, 3, 1, &copied_vertex) == ARX_OK);
    CHECK(copied_vertex.position.x == doctest::Approx(3.0f));
    CHECK(copied_vertex.position.y == doctest::Approx(3.0f));

    const std::vector<std::uint8_t> image = makeTestBmp();
    ArxTextureView texture{modelStringView("graph/obj3d/textures/wall.png"), {image.data(), image.size()}};
    ArxTextureIndex texture_index = ARX_NO_TEXTURE;
    REQUIRE(arx_pistoris_model_add_texture(model, &texture, &texture_index) == ARX_OK);
    texture.path = modelStringView("graph/obj3d/textures/stone.png");
    texture.encoded_image = {};
    REQUIRE(arx_pistoris_model_set_texture(model, texture_index, &texture) == ARX_OK);
    ArxTextureView copied_texture{};
    REQUIRE(arx_pistoris_model_copy_texture_views(model, texture_index, 1, &copied_texture) == ARX_OK);
    CHECK((std::string_view(copied_texture.path.data, copied_texture.path.size) == "graph/obj3d/textures/stone.png"));
    REQUIRE(arx_pistoris_model_set_texture_image(model, texture_index, image.data(), image.size()) == ARX_OK);
    REQUIRE(arx_pistoris_model_copy_texture_views(model, texture_index, 1, &copied_texture) == ARX_OK);
    REQUIRE(copied_texture.encoded_image.size == image.size());
    CHECK(std::equal(image.begin(), image.end(), copied_texture.encoded_image.data));
    REQUIRE(arx_pistoris_model_clear_texture_image(model, texture_index) == ARX_OK);
    REQUIRE(arx_pistoris_model_copy_texture_views(model, texture_index, 1, &copied_texture) == ARX_OK);
    CHECK(copied_texture.encoded_image.size == 0);
    ArxTextureView unused_texture{modelStringView("graph/obj3d/textures/unused.png"), {}};
    ArxTextureIndex unused_texture_index = ARX_NO_TEXTURE;
    REQUIRE(arx_pistoris_model_add_texture(model, &unused_texture, &unused_texture_index) == ARX_OK);

    ArxModelFace face{};
    face.normal = {0.0f, 0.0f, 1.0f};
    face.texture = texture_index;
    for (std::size_t corner = 0; corner < 3; ++corner) {
      face.corners[corner].vertex = static_cast<ArxVertexIndex>(corner);
      face.corners[corner].normal = face.normal;
    }
    ArxFaceIndex face_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_face(model, &face, &face_index) == ARX_OK);

    ArxModelSelection selection{};
    selection.name = modelStringView("armor");
    selection.has_leading_vertex = 1;
    selection.leading_position = vertices[0].position;
    selection.leading_bone = root_index;
    ArxSelectionId selection_id = ARX_INVALID_SELECTION_ID;
    REQUIRE(arx_pistoris_model_add_selection(model, &selection, &selection_id) == ARX_OK);
    const ArxVertexIndex selected_vertex = 0;
    const ArxBoneIndex selected_bone = child_index;
    const ArxActionPointIndex selected_action = action_index;
    const ArxModelSelectionMembersInput members{&selected_vertex, 1, &selected_bone, 1, &selected_action, 1};
    REQUIRE(arx_pistoris_model_update_selection_members(model, selection_id, &members) == ARX_OK);
    selection.name = modelStringView("heavy_armor");
    REQUIRE(arx_pistoris_model_set_selection(model, selection_id, &selection) == ARX_OK);
    ArxModelSelection copied_selection{};
    REQUIRE(arx_pistoris_model_selection(model, selection_id, &copied_selection) == ARX_OK);
    CHECK((std::string_view(copied_selection.name.data, copied_selection.name.size) == "heavy_armor"));
    REQUIRE(arx_pistoris_model_set_selection_includes_origin(model, selection_id, 1) == ARX_OK);

    REQUIRE(arx_pistoris_model_validate_mesh(model) == ARX_OK);
    REQUIRE(arx_pistoris_model_validate_skeleton(model) == ARX_OK);
    REQUIRE(arx_pistoris_model_validate_action_points(model) == ARX_OK);
    REQUIRE(arx_pistoris_model_validate_selections(model) == ARX_OK);

    std::size_t count = 0;
    REQUIRE(arx_pistoris_model_texture_count(model, &count) == ARX_OK);
    CHECK(count == 2);
    REQUIRE(arx_pistoris_model_action_point_count(model, &count) == ARX_OK);
    CHECK(count == 1);
    ArxModelOrigin origin{};
    REQUIRE(arx_pistoris_model_origin(model, &origin) == ARX_OK);
    CHECK(origin.bone == root_index);
    std::uint8_t includes_origin = 0;
    REQUIRE(arx_pistoris_model_selection_includes_origin(model, selection_id, &includes_origin) == ARX_OK);
    CHECK(includes_origin == 1);
    REQUIRE(arx_pistoris_model_selection_vertex_count(model, selection_id, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_model_selection_bone_count(model, selection_id, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_model_selection_action_point_count(model, selection_id, &count) == ARX_OK);
    CHECK(count == 1);

    REQUIRE(arx_pistoris_model_clear_selection_vertices(model, selection_id) == ARX_OK);
    REQUIRE(arx_pistoris_model_selection_vertex_count(model, selection_id, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_model_clear_selection_bones(model, selection_id) == ARX_OK);
    REQUIRE(arx_pistoris_model_selection_bone_count(model, selection_id, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_model_clear_selection_action_points(model, selection_id) == ARX_OK);
    REQUIRE(arx_pistoris_model_selection_action_point_count(model, selection_id, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_model_set_selection_includes_origin(model, selection_id, 0) == ARX_OK);
    REQUIRE(arx_pistoris_model_selection_includes_origin(model, selection_id, &includes_origin) == ARX_OK);
    CHECK(includes_origin == 0);
    REQUIRE(arx_pistoris_model_remove_selection(model, selection_id) == ARX_OK);
    REQUIRE(arx_pistoris_model_selection_count(model, &count) == ARX_OK);
    CHECK(count == 0);
    selection.name = modelStringView("temporary");
    REQUIRE(arx_pistoris_model_add_selection(model, &selection, &selection_id) == ARX_OK);
    REQUIRE(arx_pistoris_model_clear_selections(model) == ARX_OK);
    REQUIRE(arx_pistoris_model_selection_count(model, &count) == ARX_OK);
    CHECK(count == 0);

    std::size_t removed = 0;
    REQUIRE(arx_pistoris_model_compact_vertices(model, &removed) == ARX_OK);
    CHECK(removed == 1);
    REQUIRE(arx_pistoris_model_vertex_count(model, &count) == ARX_OK);
    CHECK(count == 3);
    REQUIRE(arx_pistoris_model_compact_textures(model, &removed) == ARX_OK);
    CHECK(removed == 1);
    REQUIRE(arx_pistoris_model_texture_count(model, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_model_remove_face(model, face_index) == ARX_OK);
    REQUIRE(arx_pistoris_model_face_count(model, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_model_clear_mesh(model) == ARX_OK);
    REQUIRE(arx_pistoris_model_vertex_count(model, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_model_texture_count(model, &count) == ARX_OK);
    CHECK(count == 0);

    REQUIRE(arx_pistoris_model_remove_action_point(model, action_index) == ARX_OK);
    REQUIRE(arx_pistoris_model_action_point_count(model, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_model_add_action_point(model, &action, &action_index) == ARX_OK);
    REQUIRE(arx_pistoris_model_clear_action_points(model) == ARX_OK);
    REQUIRE(arx_pistoris_model_action_point_count(model, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_model_remove_bone(model, child_index) == ARX_OK);
    REQUIRE(arx_pistoris_model_bone_count(model, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_model_clear_skeleton(model) == ARX_OK);
    REQUIRE(arx_pistoris_model_bone_count(model, &count) == ARX_OK);
    CHECK(count == 0);

    arx_pistoris_model_destroy(model);
  }
}
