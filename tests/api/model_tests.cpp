// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/model/types.h"

#include "image_helpers.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

bool equal(ArxVector3 first, ArxVector3 second) {
  return first.x == second.x && first.y == second.y && first.z == second.z;
}

}  // namespace

TEST_SUITE("C Model API") {
  TEST_CASE("Stores and renders an optional inventory icon") {
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_create(&model) == ARX_OK);

    ArxModelInventoryIconView borrowed = ARX_MODEL_INVENTORY_ICON_VIEW_INIT;
    REQUIRE(arx_pistoris_model_inventory_icon(model, &borrowed) == ARX_OK);
    CHECK(borrowed.encoded_image.data == nullptr);
    CHECK(borrowed.encoded_image.size == 0);
    CHECK(borrowed.width_slots == 0);
    CHECK(borrowed.height_slots == 0);

    ArxModelInventoryIconSetOptions set_options = ARX_MODEL_INVENTORY_ICON_SET_OPTIONS_INIT;
    CHECK(arx_pistoris_model_set_inventory_icon(model, {}, nullptr) == ARX_INVALID_OPTIONS);
    CHECK(arx_pistoris_model_set_inventory_icon(model, {}, &set_options) == ARX_MODEL_BAD_INVENTORY_ICON);

    ArxModelInventoryIconRenderOptions options = ARX_MODEL_INVENTORY_ICON_RENDER_OPTIONS_INIT;
    std::uint8_t sentinel = 0;
    std::uint8_t* rendered = &sentinel;
    std::size_t rendered_size = 1;
    REQUIRE(arx_pistoris_model_render_icon_png(model, &options, &rendered, &rendered_size) == ARX_OK);
    CHECK(rendered == nullptr);
    CHECK(rendered_size == 0);

    const std::vector<std::uint8_t> icon = makeTestBmp(255, 0, 0);
    CHECK(arx_pistoris_model_set_inventory_icon(model, {icon.data(), 1}, &set_options) == ARX_MODEL_BAD_INVENTORY_ICON);
    REQUIRE(arx_pistoris_model_set_inventory_icon(model, {icon.data(), icon.size()}, &set_options) == ARX_OK);
    REQUIRE(arx_pistoris_model_inventory_icon(model, &borrowed) == ARX_OK);
    REQUIRE(borrowed.encoded_image.size == icon.size());
    CHECK(std::equal(icon.begin(), icon.end(), borrowed.encoded_image.data));
    CHECK(borrowed.width_slots == 1);
    CHECK(borrowed.height_slots == 1);
    set_options.width_slots = 3;
    set_options.height_slots = 2;
    REQUIRE(arx_pistoris_model_set_inventory_icon(model, {icon.data(), icon.size()}, &set_options) == ARX_OK);
    REQUIRE(arx_pistoris_model_render_icon_png(model, &options, &rendered, &rendered_size) == ARX_OK);
    REQUIRE(rendered != nullptr);
    REQUIRE(rendered_size != 0);
    ArxImageInfo info{};
    REQUIRE(arx_pistoris_binary_inspect_encoded_image({rendered, rendered_size}, &info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width == 96);
    CHECK(info.height == 64);
    arx_pistoris_free_bytes(rendered);

    options.layout = 255;
    rendered = &sentinel;
    rendered_size = 1;
    CHECK(arx_pistoris_model_render_icon_png(model, &options, &rendered, &rendered_size) == ARX_INVALID_OPTIONS);
    CHECK(rendered == nullptr);
    CHECK(rendered_size == 0);

    REQUIRE(arx_pistoris_model_clear_inventory_icon(model) == ARX_OK);
    REQUIRE(arx_pistoris_model_inventory_icon(model, &borrowed) == ARX_OK);
    CHECK(borrowed.encoded_image.data == nullptr);
    CHECK(borrowed.encoded_image.size == 0);
    CHECK(borrowed.width_slots == 0);
    CHECK(borrowed.height_slots == 0);
    arx_pistoris_model_destroy(model);
  }

  TEST_CASE("Exposes index-aligned texture source paths through an owned handle") {
    constexpr std::string_view kObj = R"(mtllib materials.mtl
v 0 0 0
v 1 0 0
v 0 1 0
usemtl wall
f 1 2 3
)";
    constexpr std::string_view kMtl = R"(newmtl wall
map_Kd imported/wall.bmp
)";

    ArxObjMaterialLibraryPaths* paths = nullptr;
    REQUIRE(arx_pistoris_obj_material_library_paths(
                reinterpret_cast<const std::uint8_t*>(kObj.data()), kObj.size(), &paths) == ARX_OK);
    REQUIRE(paths != nullptr);
    std::size_t path_count = 0;
    REQUIRE(arx_pistoris_obj_material_library_paths_count(paths, &path_count) == ARX_OK);
    REQUIRE(path_count == 1);
    ArxStringView path{};
    REQUIRE(arx_pistoris_obj_material_library_paths_get(paths, 0, &path) == ARX_OK);
    CHECK((std::string_view(path.data, path.size) == "materials.mtl"));
    CHECK(arx_pistoris_obj_material_library_paths_get(paths, 1, &path) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(path.data == nullptr);
    CHECK(path.size == 0);
    arx_pistoris_obj_material_library_paths_destroy(paths);

    ArxModel* model = nullptr;
    ArxTextureSourcePaths* sources = nullptr;
    const ArxObjMaterialLibraryView library{
        .path = view("materials.mtl"),
        .data = reinterpret_cast<const std::uint8_t*>(kMtl.data()),
        .size = kMtl.size(),
    };
    REQUIRE(arx_pistoris_model_import_obj(
                reinterpret_cast<const std::uint8_t*>(kObj.data()), kObj.size(), &library, 1, &model, &sources) ==
            ARX_OK);
    REQUIRE(model != nullptr);
    REQUIRE(sources != nullptr);

    std::size_t count = 0;
    REQUIRE(arx_pistoris_texture_source_paths_count(sources, &count) == ARX_OK);
    REQUIRE(count == 1);
    ArxStringView source{};
    REQUIRE(arx_pistoris_texture_source_paths_get(sources, 0, &source) == ARX_OK);
    CHECK((std::string_view(source.data, source.size) == "imported/wall.bmp"));
    CHECK(arx_pistoris_texture_source_paths_get(sources, 1, &source) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(source.data == nullptr);
    CHECK(source.size == 0);

    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(arx_pistoris_model_set_texture_image(model, 0, image.data(), image.size()) == ARX_OK);
    const ArxObjExportOptions options = ARX_OBJ_EXPORT_OPTIONS_INIT;
    char* obj = nullptr;
    char* mtl = nullptr;
    ArxObjTextureFiles* textures = nullptr;
    REQUIRE(arx_pistoris_model_export_obj(model, view("static_model"), &options, &obj, &mtl, &textures) == ARX_OK);
    REQUIRE(obj != nullptr);
    REQUIRE(mtl != nullptr);
    REQUIRE(textures != nullptr);
    std::size_t texture_count = 0;
    REQUIRE(arx_pistoris_obj_texture_files_count(textures, &texture_count) == ARX_OK);
    REQUIRE(texture_count == 1);
    ArxObjTextureFile texture{};
    REQUIRE(arx_pistoris_obj_texture_files_get(textures, 0, &texture) == ARX_OK);
    CHECK(texture.source_texture == 0);
    CHECK((std::string_view(texture.path.data, texture.path.size) == "imported/wall.bmp"));
    CHECK(texture.encoded_image.size == image.size());
    CHECK(std::equal(image.begin(), image.end(), texture.encoded_image.data));
    arx_pistoris_obj_texture_files_destroy(textures);
    arx_pistoris_free_string(mtl);
    arx_pistoris_free_string(obj);

    arx_pistoris_texture_source_paths_destroy(sources);
    arx_pistoris_model_destroy(model);
  }

  TEST_CASE("Converts static OBJ through opaque handles") {
    CHECK(arx_pistoris_strerror(ARX_OBJ_BAD_FORMAT) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_BAD_VERTEX_IDX) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_TOO_MANY_VERTICES) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_TOO_MANY_TEXTURES) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_TOO_MANY_FACES) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_BAD_POSITION_INDEX) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_BAD_TEXCOORD_INDEX) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_BAD_NORMAL_INDEX) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_BAD_FACE) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_BAD_MTL) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_BAD_ACTION_POINT) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_OBJ_TOO_MANY_ACTION_POINTS) != nullptr);
    constexpr std::string_view kObj = R"(# arx_action WEAPON_ATTACH 4 5 6
v 1 2 3
v 2 2 3
v 1 3 3
vn 0 0 1
f 1//1 2//1 3//1
)";
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_import_obj(
                reinterpret_cast<const std::uint8_t*>(kObj.data()), kObj.size(), nullptr, 0, &model, nullptr) ==
            ARX_OK);
    REQUIRE(model != nullptr);
    CHECK(arx_pistoris_model_validate(model) == ARX_OK);

    char* obj = nullptr;
    char* mtl = nullptr;
    REQUIRE(arx_pistoris_model_export_obj(model, view("static_model"), nullptr, &obj, &mtl, nullptr) == ARX_OK);
    REQUIRE(obj != nullptr);
    REQUIRE(mtl != nullptr);
    CHECK(std::string_view(obj).find("# arx_action weapon_attach 4 5 6") != std::string_view::npos);
    CHECK(std::string_view(obj).find("# origin") == std::string_view::npos);

    arx_pistoris_free_string(mtl);
    arx_pistoris_free_string(obj);
    arx_pistoris_model_destroy(model);
  }

  TEST_CASE("Converts Model GLB through opaque handles") {
    CHECK(arx_pistoris_strerror(ARX_GLB_NO_MODEL_GEOMETRY) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_GEOMETRY) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_POSITION_ATTRIBUTE) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_INDEX_ACCESSOR) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_NORMAL_ATTRIBUTE) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_TEXCOORD_ATTRIBUTE) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_MATERIAL) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_HIERARCHY) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_SKELETON) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_SKINNING) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_BONE_HELPER) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_ACTION_POINT) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_MODEL_SELECTION) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_ANIMATION_NAME) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_ANIMATION_HELPER) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_ANIMATION_SAMPLER) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_ANIMATION_CHANNEL) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_ANIMATION_BINDING) != nullptr);

    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_create(&model) == ARX_OK);
    std::array<ArxModelVertex, 3> vertices{};
    vertices[1].position.x = 1.0f;
    vertices[2].position.y = 1.0f;
    ArxVertexIndex first = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_vertices(model, vertices.data(), vertices.size(), &first) == ARX_OK);
    ArxModelFace face{};
    face.normal = {0.0f, 0.0f, 1.0f};
    face.texture = ARX_NO_TEXTURE;
    for (std::size_t index = 0; index < 3; ++index) {
      face.corners[index].vertex = static_cast<ArxVertexIndex>(index);
      face.corners[index].normal = face.normal;
    }
    ArxFaceIndex face_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_face(model, &face, &face_index) == ARX_OK);

    const ArxModelGlbExportOptions export_options = ARX_MODEL_GLB_EXPORT_OPTIONS_INIT;
    std::uint8_t* encoded = nullptr;
    std::size_t encoded_size = 0;
    REQUIRE(arx_pistoris_model_export_glb(
                model, nullptr, 0, &export_options, nullptr, &encoded, &encoded_size, nullptr) == ARX_OK);
    REQUIRE(encoded != nullptr);
    REQUIRE(encoded_size != 0);

    const ArxModelGlbImportOptions import_options = ARX_MODEL_GLB_IMPORT_OPTIONS_INIT;
    ArxModel* imported = nullptr;
    ArxTextureSourcePaths* sources = nullptr;
    REQUIRE(arx_pistoris_model_import_glb(
                encoded, encoded_size, &import_options, &imported, nullptr, nullptr, &sources, nullptr) == ARX_OK);
    REQUIRE(imported != nullptr);
    REQUIRE(sources != nullptr);
    std::size_t source_count = 1;
    REQUIRE(arx_pistoris_texture_source_paths_count(sources, &source_count) == ARX_OK);
    CHECK(source_count == 0);
    CHECK(arx_pistoris_model_validate(imported) == ARX_OK);
    std::size_t count = 0;
    REQUIRE(arx_pistoris_model_face_count(imported, &count) == ARX_OK);
    CHECK(count == 1);

    arx_pistoris_texture_source_paths_destroy(sources);
    arx_pistoris_model_destroy(imported);
    arx_pistoris_free_bytes(encoded);
    arx_pistoris_model_destroy(model);
  }

  TEST_CASE("Exports a static Level preview through an opaque handle") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_import_obj(
                reinterpret_cast<const std::uint8_t*>(kObj.data()), kObj.size(), nullptr, 0, &model, nullptr) ==
            ARX_OK);

    ArxModelLevelPreviewGlbOptions options = ARX_MODEL_LEVEL_PREVIEW_GLB_OPTIONS_INIT;
    options.class_path = view("model:weapons:long_sword");
    options.asset_name = view("long_sword");
    std::uint8_t* encoded = nullptr;
    std::size_t encoded_size = 0;
    REQUIRE(arx_pistoris_model_export_level_preview_glb(model, &options, &encoded, &encoded_size) == ARX_OK);
    CHECK(encoded != nullptr);
    CHECK(encoded_size != 0);

    arx_pistoris_free_bytes(encoded);
    arx_pistoris_model_destroy(model);
  }

  TEST_CASE("Intermediate counts may exceed native FTL limits") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_import_obj(
                reinterpret_cast<const std::uint8_t*>(kObj.data()), kObj.size(), nullptr, 0, &model, nullptr) ==
            ARX_OK);

    std::vector<ArxModelActionPoint> action_points(1025);
    for (ArxModelActionPoint& point : action_points) {
      point.name = view("hit_30");
      point.bone = ARX_INVALID_INDEX;
    }
    const ArxModelActionPointsInput input{action_points.data(), action_points.size()};
    REQUIRE(arx_pistoris_model_replace_action_points(model, &input) == ARX_OK);
    CHECK(arx_pistoris_model_validate(model) == ARX_OK);

    const ArxNativeTextureBakeOptions options = ARX_NATIVE_TEXTURE_BAKE_OPTIONS_INIT;
    ArxFtl* native = nullptr;
    ArxNativeTextureFiles* textures = nullptr;
    CHECK(arx_pistoris_model_bake_native(model, &options, &native, &textures) == ARX_MODEL_TOO_MANY_ACTION_POINTS);
    CHECK(native == nullptr);
    CHECK(textures == nullptr);
    arx_pistoris_model_destroy(model);
  }

  TEST_CASE("Edits, bakes, and reimports through opaque handles") {
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_create(&model) == ARX_OK);
    REQUIRE(model != nullptr);
    REQUIRE(arx_pistoris_model_set_resource_path(model, view("model:npc:human_base")) == ARX_OK);

    ArxModelBone root{};
    root.name = view("ROOT");
    ArxBoneIndex root_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_bone(model, &root, &root_index) == ARX_OK);
    CHECK(root_index == 0);

    ArxModelActionPoint hit{};
    hit.name = view("HIT_30");
    hit.bone = root_index;
    ArxActionPointIndex first_hit = ARX_INVALID_INDEX;
    ArxActionPointIndex second_hit = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_action_point(model, &hit, &first_hit) == ARX_OK);
    REQUIRE(arx_pistoris_model_add_action_point(model, &hit, &second_hit) == ARX_OK);
    CHECK(first_hit == 0);
    CHECK(second_hit == 1);
    std::array<ArxModelActionPoint, 2> hits{};
    REQUIRE(arx_pistoris_model_copy_action_points(model, 0, hits.size(), hits.data()) == ARX_OK);
    CHECK((std::string_view(hits[0].name.data, hits[0].name.size) == "hit_30"));
    CHECK((std::string_view(hits[1].name.data, hits[1].name.size) == "hit_30"));

    ArxModelSelection cut_head{};
    cut_head.name = view("CUT_HEAD");
    cut_head.has_leading_vertex = 2;
    cut_head.leading_position = {0.25f, 0.25f, 0.0f};
    cut_head.leading_bone = root_index;
    ArxSelectionId cut_head_id = ARX_INVALID_SELECTION_ID;
    REQUIRE(arx_pistoris_model_add_selection(model, &cut_head, &cut_head_id) == ARX_OK);
    CHECK(cut_head_id == 0);

    std::array<ArxModelVertex, 3> input_vertices{};
    input_vertices[0].position = {0.0f, 0.0f, 0.0f};
    input_vertices[1].position = {1.0f, 0.0f, 0.0f};
    input_vertices[2].position = {0.0f, 1.0f, 0.0f};
    for (ArxModelVertex& vertex : input_vertices) vertex.bone = root_index;
    ArxVertexIndex first_vertex = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_vertices(model, input_vertices.data(), input_vertices.size(), &first_vertex) ==
            ARX_OK);
    CHECK(first_vertex == 0);

    const std::vector<std::uint8_t> image = makeTestBmp();
    ArxTextureView texture{};
    texture.path = view("wall");
    texture.encoded_image = {image.data(), image.size()};
    ArxTextureIndex texture_index = ARX_NO_TEXTURE;
    REQUIRE(arx_pistoris_model_add_texture(model, &texture, &texture_index) == ARX_OK);
    CHECK(texture_index == 0);
    REQUIRE(arx_pistoris_model_rebase_texture_paths(model, view("graph/obj3d/textures")) == ARX_OK);

    const ArxVertexIndex cut_vertex = 0;
    ArxModelSelectionMembersInput members{};
    members.vertices = &cut_vertex;
    members.vertex_count = 1;
    REQUIRE(arx_pistoris_model_update_selection_members(model, cut_head_id, &members) == ARX_OK);

    ArxModelFace face{};
    face.normal = {1.0f, 0.0f, 0.0f};
    face.texture = texture_index;
    for (std::size_t corner = 0; corner < 3; ++corner) {
      face.corners[corner].vertex = static_cast<ArxVertexIndex>(corner);
      face.corners[corner].normal = {0.0f, 0.0f, 1.0f};
    }
    ArxFaceIndex face_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_face(model, &face, &face_index) == ARX_OK);
    CHECK(arx_pistoris_model_validate(model) == ARX_OK);
    ArxModelFace copied_face{};
    REQUIRE(arx_pistoris_model_copy_faces(model, 0, 1, &copied_face) == ARX_OK);
    CHECK(equal(copied_face.normal, face.normal));
    copied_face.corners[0].normal = {1.0f, 0.0f, 0.0f};
    REQUIRE(arx_pistoris_model_set_face(model, 0, &copied_face) == ARX_OK);
    copied_face = {};
    REQUIRE(arx_pistoris_model_copy_faces(model, 0, 1, &copied_face) == ARX_OK);
    CHECK(equal(copied_face.corners[0].normal, {1.0f, 0.0f, 0.0f}));
    std::size_t vertex_count = 0;
    std::size_t bone_count = 0;
    std::size_t selection_count = 0;
    REQUIRE(arx_pistoris_model_vertex_count(model, &vertex_count) == ARX_OK);
    REQUIRE(arx_pistoris_model_bone_count(model, &bone_count) == ARX_OK);
    REQUIRE(arx_pistoris_model_selection_count(model, &selection_count) == ARX_OK);
    CHECK(vertex_count == 3);
    CHECK(bone_count == 1);
    CHECK(selection_count == 1);

    std::array<ArxModelVertex, 3> vertices{};
    REQUIRE(arx_pistoris_model_copy_vertices(model, 0, vertices.size(), vertices.data()) == ARX_OK);
    std::size_t selection_vertex_count = 0;
    REQUIRE(arx_pistoris_model_selection_vertex_count(model, cut_head_id, &selection_vertex_count) == ARX_OK);
    CHECK(selection_vertex_count == 1);
    ArxVertexIndex selected_vertex = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_copy_selection_vertices(model, cut_head_id, 0, 1, &selected_vertex) == ARX_OK);
    CHECK(selected_vertex == 0);
    ArxSelectionId copied_id = ARX_INVALID_SELECTION_ID;
    REQUIRE(arx_pistoris_model_copy_selection_ids(model, 0, 1, &copied_id) == ARX_OK);
    CHECK(copied_id == cut_head_id);
    ArxModelSelection copied_selection{};
    REQUIRE(arx_pistoris_model_selection(model, copied_id, &copied_selection) == ARX_OK);
    CHECK((std::string_view(copied_selection.name.data, copied_selection.name.size) == "cut_head"));
    CHECK(copied_selection.has_leading_vertex == 1);
    CHECK(equal(copied_selection.leading_position, cut_head.leading_position));
    CHECK(copied_selection.leading_bone == root_index);

    ArxStringView path{};
    REQUIRE(arx_pistoris_model_resource_path(model, &path) == ARX_OK);
    CHECK((std::string_view(path.data, path.size) == "game/graph/obj3d/interactive/npc/human_base/human_base.ftl"));
    REQUIRE(arx_pistoris_model_set_resource_path(model, {nullptr, 0}) == ARX_OK);
    REQUIRE(arx_pistoris_model_resource_path(model, &path) == ARX_OK);
    CHECK(path.size == 0);
    CHECK(arx_pistoris_model_validate(model) == ARX_OK);

    const ArxNativeTextureBakeOptions bake_options = ARX_NATIVE_TEXTURE_BAKE_OPTIONS_INIT;
    ArxFtl* baked = nullptr;
    ArxNativeTextureFiles* texture_files = nullptr;
    REQUIRE(arx_pistoris_model_bake_native(model, &bake_options, &baked, &texture_files) == ARX_OK);
    REQUIRE(baked != nullptr);
    REQUIRE(texture_files != nullptr);
    CHECK(arx_pistoris_ftl_validate(baked) == ARX_OK);
    std::size_t native_texture_count = 0;
    REQUIRE(arx_pistoris_native_texture_files_count(texture_files, &native_texture_count) == ARX_OK);
    REQUIRE(native_texture_count == 1);
    ArxNativeTextureFile native_texture{};
    REQUIRE(arx_pistoris_native_texture_files_get(texture_files, 0, &native_texture) == ARX_OK);
    CHECK(std::string(native_texture.resource_path.data, native_texture.resource_path.size) ==
          "graph/obj3d/textures/wall.bmp");
    CHECK(native_texture.encoded_image.size == image.size());
    CHECK(std::equal(image.begin(), image.end(), native_texture.encoded_image.data));

    ArxModel* imported = nullptr;
    ArxTextureSourcePaths* imported_sources = nullptr;
    REQUIRE(arx_pistoris_model_import_native(baked, &imported, &imported_sources) == ARX_OK);
    REQUIRE(imported_sources != nullptr);
    std::size_t source_count = 0;
    REQUIRE(arx_pistoris_texture_source_paths_count(imported_sources, &source_count) == ARX_OK);
    REQUIRE(source_count == 1);
    ArxStringView source{};
    REQUIRE(arx_pistoris_texture_source_paths_get(imported_sources, 0, &source) == ARX_OK);
    CHECK((std::string_view(source.data, source.size) == "graph/obj3d/textures/wall."));
    CHECK(arx_pistoris_model_validate(imported) == ARX_OK);
    REQUIRE(arx_pistoris_model_copy_faces(imported, 0, 1, &copied_face) == ARX_OK);
    CHECK(equal(copied_face.normal, face.normal));
    REQUIRE(arx_pistoris_model_resource_path(imported, &path) == ARX_OK);
    CHECK(path.size == 0);

    ArxModel* clone = nullptr;
    REQUIRE(arx_pistoris_model_clone(model, &clone) == ARX_OK);
    REQUIRE(arx_pistoris_model_reset(model) == ARX_OK);
    CHECK(arx_pistoris_model_validate(model) == ARX_MODEL_NO_GEOMETRY);
    CHECK(arx_pistoris_model_validate(clone) == ARX_OK);

    arx_pistoris_model_destroy(clone);
    arx_pistoris_texture_source_paths_destroy(imported_sources);
    arx_pistoris_model_destroy(imported);
    arx_pistoris_ftl_destroy(baked);
    arx_pistoris_native_texture_files_destroy(texture_files);
    arx_pistoris_model_destroy(model);
  }

  TEST_CASE("Validates opaque handles and submitted pointers") {
    CHECK(arx_pistoris_strerror(ARX_MODEL_TOO_MANY_NATIVE_VERTICES) != nullptr);
    CHECK(arx_pistoris_model_create(nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_model_validate(nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_model_clone(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_model_scale(nullptr, 1.0f) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_model_rotate(nullptr, {}) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_model_translate(nullptr, {}) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_model_apply_reference(nullptr, nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_model_infer_bone_origin_selections(nullptr) == ARX_INVALID_HANDLE);

    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_create(&model) == ARX_OK);
    CHECK(arx_pistoris_model_apply_reference(model, model, nullptr) == ARX_INVALID_OPTIONS);
    const ArxModelReferenceOptions empty_reference = ARX_MODEL_REFERENCE_OPTIONS_INIT;
    CHECK(arx_pistoris_model_apply_reference(model, model, &empty_reference) == ARX_INVALID_OPTIONS);
    CHECK(arx_pistoris_model_resource_path(model, nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_model_vertex_count(model, nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_model_copy_vertices(model, 0, 1, nullptr) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(arx_pistoris_model_add_vertex(model, nullptr, nullptr) == ARX_INVALID_DATA_POINTER);
    ArxVertexIndex first_vertex = 42;
    CHECK(arx_pistoris_model_add_vertices(model, nullptr, 0, &first_vertex) == ARX_INVALID_OPTIONS);
    CHECK(first_vertex == ARX_INVALID_INDEX);
    ArxTextureView texture{};
    texture.path = view("wall.bmp");
    ArxTextureIndex texture_index = ARX_NO_TEXTURE;
    REQUIRE(arx_pistoris_model_add_texture(model, &texture, &texture_index) == ARX_OK);
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(arx_pistoris_model_set_texture_image(model, texture_index, image.data(), image.size()) == ARX_OK);
    CHECK(arx_pistoris_model_set_texture_image(model, texture_index, nullptr, 0) == ARX_MODEL_BAD_TEXTURE_IMAGE);
    ArxTextureView copied_texture{};
    REQUIRE(arx_pistoris_model_copy_texture_views(model, 0, 1, &copied_texture) == ARX_OK);
    REQUIRE(copied_texture.encoded_image.size == image.size());
    CHECK(std::equal(image.begin(), image.end(), copied_texture.encoded_image.data));
    REQUIRE(arx_pistoris_model_clear_texture_image(model, texture_index) == ARX_OK);
    REQUIRE(arx_pistoris_model_copy_texture_views(model, 0, 1, &copied_texture) == ARX_OK);
    CHECK(copied_texture.encoded_image.size == 0);
    CHECK(std::string(copied_texture.external_image_extension.data, copied_texture.external_image_extension.size) ==
          ".bmp");

    ArxModelMeshInput oversized{};
    oversized.vertex_count = std::numeric_limits<std::size_t>::max();
    CHECK(arx_pistoris_model_replace_mesh(model, &oversized) == ARX_MODEL_TOO_MANY_VERTICES);

    ArxModelSkeletonInput oversized_skeleton{};
    oversized_skeleton.bone_count = 1025;
    CHECK(arx_pistoris_model_replace_skeleton(model, &oversized_skeleton) == ARX_MODEL_TOO_MANY_BONES);

    ArxModelActionPointsInput oversized_action_points{};
    oversized_action_points.action_point_count = std::numeric_limits<std::size_t>::max();
    CHECK(arx_pistoris_model_replace_action_points(model, &oversized_action_points) ==
          ARX_MODEL_TOO_MANY_ACTION_POINTS);

    ArxModelVertex vertex{};
    ArxVertexIndex index = 42;
    REQUIRE(arx_pistoris_model_add_vertex(model, &vertex, &index) == ARX_OK);

    ArxModelSelection selection{};
    selection.name = view("test");
    ArxSelectionId selection_id = ARX_INVALID_SELECTION_ID;
    REQUIRE(arx_pistoris_model_add_selection(model, &selection, &selection_id) == ARX_OK);
    ArxModelSelectionMembersInput invalid_members{};
    invalid_members.vertex_count = 1;
    CHECK(arx_pistoris_model_update_selection_members(model, selection_id, &invalid_members) ==
          ARX_INVALID_DATA_POINTER);

    ArxModelBone bone{};
    bone.name = {nullptr, 1};
    ArxBoneIndex bone_index = 42;
    CHECK(arx_pistoris_model_add_bone(model, &bone, &bone_index) == ARX_INVALID_DATA_POINTER);
    CHECK(bone_index == 42);
    bone.name = {nullptr, 0};
    CHECK(arx_pistoris_model_add_bone(model, &bone, &bone_index) == ARX_MODEL_BAD_BONE_NAME);

    CHECK(arx_pistoris_model_bake_native(model, nullptr, nullptr, nullptr) == ARX_INVALID_OPTIONS);
    const ArxNativeTextureBakeOptions bake_options = ARX_NATIVE_TEXTURE_BAKE_OPTIONS_INIT;
    CHECK(arx_pistoris_model_bake_native(model, &bake_options, nullptr, nullptr) == ARX_INVALID_DATA_POINTER);
    arx_pistoris_model_destroy(model);
  }

  TEST_CASE("Transforms Model positions through the C boundary") {
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_create(&model) == ARX_OK);
    ArxModelVertex vertex{{1.0f, 0.0f, 0.0f}};
    ArxVertexIndex index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_vertex(model, &vertex, &index) == ARX_OK);

    REQUIRE(arx_pistoris_model_scale(model, 2.0f) == ARX_OK);
    REQUIRE(arx_pistoris_model_rotate(model, {2.0f, 0.0f, 0.0f, 2.0f}) == ARX_OK);
    REQUIRE(arx_pistoris_model_translate(model, {3.0f, 4.0f, 5.0f}) == ARX_OK);
    REQUIRE(arx_pistoris_model_copy_vertices(model, 0, 1, &vertex) == ARX_OK);
    CHECK(vertex.position.x == doctest::Approx(3.0f));
    CHECK(vertex.position.y == doctest::Approx(6.0f));
    CHECK(vertex.position.z == doctest::Approx(5.0f));

    CHECK(arx_pistoris_model_scale(model, 0.0f) == ARX_INVALID_OPTIONS);
    CHECK(arx_pistoris_model_rotate(model, {0.0f, 0.0f, 0.0f, 0.0f}) == ARX_INVALID_OPTIONS);
    arx_pistoris_model_destroy(model);
  }

  TEST_CASE("Applies Model reference and inference through the C boundary") {
    ArxModel* target = nullptr;
    ArxModel* reference = nullptr;
    REQUIRE(arx_pistoris_model_create(&target) == ARX_OK);
    REQUIRE(arx_pistoris_model_create(&reference) == ARX_OK);

    ArxModelBone target_root{view("root"), {1.0f, 2.0f, 3.0f}, ARX_INVALID_INDEX, 0.0f};
    ArxModelBone reference_root{view("root"), {4.0f, 5.0f, 6.0f}, ARX_INVALID_INDEX, 0.0f};
    ArxBoneIndex index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_bone(target, &target_root, &index) == ARX_OK);
    REQUIRE(arx_pistoris_model_add_bone(reference, &reference_root, &index) == ARX_OK);
    ArxModelBone target_other_root{view("other_root"), {7.0f, 8.0f, 9.0f}, ARX_INVALID_INDEX, 0.0f};
    ArxModelBone reference_other_root{view("other_root"), {10.0f, 11.0f, 12.0f}, ARX_INVALID_INDEX, 0.0f};
    REQUIRE(arx_pistoris_model_add_bone(target, &target_other_root, &index) == ARX_OK);
    REQUIRE(index == 1);
    REQUIRE(arx_pistoris_model_add_bone(reference, &reference_other_root, &index) == ARX_OK);
    REQUIRE(index == 1);

    ArxModelActionPoint action{view("attach"), {}, 0};
    ArxActionPointIndex target_action = ARX_INVALID_INDEX;
    ArxActionPointIndex reference_action = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_action_point(target, &action, &target_action) == ARX_OK);
    REQUIRE(arx_pistoris_model_add_action_point(reference, &action, &reference_action) == ARX_OK);

    ArxModelSelection selection{};
    selection.name = view("armor");
    ArxSelectionId target_selection = ARX_INVALID_SELECTION_ID;
    ArxSelectionId reference_selection = ARX_INVALID_SELECTION_ID;
    REQUIRE(arx_pistoris_model_add_selection(target, &selection, &target_selection) == ARX_OK);
    REQUIRE(arx_pistoris_model_add_selection(reference, &selection, &reference_selection) == ARX_OK);

    ArxModelVertex vertex{};
    vertex.bone = 1;
    ArxVertexIndex vertex_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_vertex(target, &vertex, &vertex_index) == ARX_OK);
    const ArxBoneIndex target_bone = 1;
    const ArxModelSelectionMembersInput target_members{
        .vertices = &vertex_index,
        .vertex_count = 1,
        .bones = &target_bone,
        .bone_count = 1,
    };
    REQUIRE(arx_pistoris_model_update_selection_members(target, target_selection, &target_members) == ARX_OK);
    const ArxBoneIndex reference_bone = 0;
    const ArxModelSelectionMembersInput reference_members{
        .bones = &reference_bone,
        .bone_count = 1,
        .action_points = &reference_action,
        .action_point_count = 1,
    };
    REQUIRE(arx_pistoris_model_update_selection_members(reference, reference_selection, &reference_members) == ARX_OK);

    ArxModelReferenceOptions options = ARX_MODEL_REFERENCE_OPTIONS_INIT;
    options.snap_bone_origins = 1U;
    options.copy_bone_origin_selections = 1U;
    options.copy_action_point_selections = 1U;
    REQUIRE(arx_pistoris_model_apply_reference(target, reference, &options) == ARX_OK);

    std::array<ArxModelBone, 2> snapped{};
    REQUIRE(arx_pistoris_model_copy_bones(target, 0, snapped.size(), snapped.data()) == ARX_OK);
    CHECK(equal(snapped[0].position, reference_root.position));
    CHECK(equal(snapped[1].position, reference_other_root.position));

    std::size_t member_count = 0;
    REQUIRE(arx_pistoris_model_selection_bone_count(target, target_selection, &member_count) == ARX_OK);
    REQUIRE(member_count == 1);
    ArxBoneIndex selected_bone = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_copy_selection_bones(target, target_selection, 0, 1, &selected_bone) == ARX_OK);
    CHECK(selected_bone == reference_bone);
    REQUIRE(arx_pistoris_model_selection_action_point_count(target, target_selection, &member_count) == ARX_OK);
    REQUIRE(member_count == 1);
    ArxActionPointIndex selected_action = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_copy_selection_action_points(target, target_selection, 0, 1, &selected_action) ==
            ARX_OK);
    CHECK(selected_action == target_action);

    REQUIRE(arx_pistoris_model_infer_bone_origin_selections(target) == ARX_OK);
    REQUIRE(arx_pistoris_model_copy_selection_bones(target, target_selection, 0, 1, &selected_bone) == ARX_OK);
    CHECK(selected_bone == target_bone);
    REQUIRE(arx_pistoris_model_copy_selection_action_points(target, target_selection, 0, 1, &selected_action) ==
            ARX_OK);
    CHECK(selected_action == target_action);

    REQUIRE(arx_pistoris_model_clear_selection_action_points(target, target_selection) == ARX_OK);
    REQUIRE(arx_pistoris_model_selection_action_point_count(target, target_selection, &member_count) == ARX_OK);
    CHECK(member_count == 0);

    ArxModelBone reference_extra_root{view("extra_root"), {}, ARX_INVALID_INDEX, 0.0f};
    REQUIRE(arx_pistoris_model_add_bone(reference, &reference_extra_root, &index) == ARX_OK);
    options = ARX_MODEL_REFERENCE_OPTIONS_INIT;
    options.copy_action_point_selections = 1U;
    REQUIRE(arx_pistoris_model_apply_reference(target, reference, &options) == ARX_OK);
    REQUIRE(arx_pistoris_model_selection_action_point_count(target, target_selection, &member_count) == ARX_OK);
    REQUIRE(member_count == 1);
    REQUIRE(arx_pistoris_model_copy_selection_action_points(target, target_selection, 0, 1, &selected_action) ==
            ARX_OK);
    CHECK(selected_action == target_action);

    arx_pistoris_model_destroy(reference);
    arx_pistoris_model_destroy(target);
  }
}
