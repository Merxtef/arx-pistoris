// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/native/text.h"

#include "image_helpers.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <ostream>  // IWYU pragma: keep
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

ArxLevelFace triangle() {
  ArxLevelFace face{};
  face.corners[0] = {0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f, {}};
  face.corners[1] = {1, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f, {}};
  face.corners[2] = {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f, {}};
  face.texture = ARX_NO_TEXTURE;
  face.room = 0;
  return face;
}

ArxLevel* makeMinimalLevel() {
  ArxLevel* level = nullptr;
  REQUIRE(arx_pistoris_level_create(&level) == ARX_OK);

  ArxLevelRoom room{view("room")};
  ArxRoomIndex room_index = ARX_INVALID_INDEX;
  REQUIRE(arx_pistoris_level_add_room(level, &room, &room_index) == ARX_OK);

  const std::array<ArxLevelVertex, 3> vertices = {
      ArxLevelVertex{{0.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{1.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{0.0f, 0.0f, 1.0f}},
  };
  const std::array<ArxLevelFace, 1> faces = {triangle()};
  const ArxLevelMeshInput mesh{vertices.data(), vertices.size(), faces.data(), faces.size(), nullptr, 0};
  REQUIRE(arx_pistoris_level_replace_mesh(level, &mesh) == ARX_OK);
  REQUIRE(arx_pistoris_level_validate(level) == ARX_OK);
  return level;
}

}  // namespace

TEST_SUITE("C Level API") {
  TEST_CASE("Level image facade owns, renders, and clears images") {
    ArxLevel* level = makeMinimalLevel();
    const std::vector<std::uint8_t> image = makeTestBmp();

    CHECK(arx_pistoris_level_set_minimap(level, {}, {{0.0f, 0.0f}, {25.0f, 25.0f}}) == ARX_LEVEL_BAD_MINIMAP_IMAGE);
    CHECK(arx_pistoris_level_set_minimap_from_projection(level, {}, {}) == ARX_LEVEL_BAD_MINIMAP_IMAGE);
    REQUIRE(arx_pistoris_level_set_minimap(level, {image.data(), image.size()}, {{0.0f, 0.0f}, {25.0f, 25.0f}}) ==
            ARX_OK);
    ArxLevelMinimapView minimap = ARX_LEVEL_MINIMAP_VIEW_INIT;
    REQUIRE(arx_pistoris_level_minimap(level, &minimap) == ARX_OK);
    CHECK(minimap.encoded_image.size == image.size());
    CHECK(minimap.encoded_image.data != image.data());

    ArxLevelMinimapRenderOptions options = ARX_LEVEL_MINIMAP_RENDER_OPTIONS_INIT;
    uint8_t* rendered = nullptr;
    size_t rendered_size = 0;
    REQUIRE(arx_pistoris_level_render_minimap_png(level, &options, &rendered, &rendered_size) == ARX_OK);
    REQUIRE(rendered != nullptr);
    ArxImageInfo info{};
    REQUIRE(arx_pistoris_binary_inspect_encoded_image({rendered, rendered_size}, &info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    arx_pistoris_free_bytes(rendered);

    ArxLevelGameMinimapRenderOptions game_options = ARX_LEVEL_GAME_MINIMAP_RENDER_OPTIONS_INIT;
    rendered = nullptr;
    rendered_size = 0;
    REQUIRE(arx_pistoris_level_render_game_minimap_png(level, &game_options, &rendered, &rendered_size) == ARX_OK);
    REQUIRE(rendered != nullptr);
    arx_pistoris_free_bytes(rendered);

    CHECK(arx_pistoris_level_set_loading_screen(level, {}) == ARX_LEVEL_BAD_LOADING_SCREEN_IMAGE);
    REQUIRE(arx_pistoris_level_set_loading_screen(level, {image.data(), image.size()}) == ARX_OK);
    ArxEncodedImageView loading{};
    REQUIRE(arx_pistoris_level_loading_screen(level, &loading) == ARX_OK);
    CHECK(loading.size == image.size());
    REQUIRE(arx_pistoris_level_render_loading_screen_png(level, &rendered, &rendered_size) == ARX_OK);
    REQUIRE(arx_pistoris_binary_inspect_encoded_image({rendered, rendered_size}, &info) == ARX_OK);
    CHECK(info.width == 320);
    CHECK(info.height == 390);
    arx_pistoris_free_bytes(rendered);

    REQUIRE(arx_pistoris_level_clear_minimap(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_clear_loading_screen(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_minimap(level, &minimap) == ARX_OK);
    REQUIRE(arx_pistoris_level_loading_screen(level, &loading) == ARX_OK);
    CHECK(minimap.encoded_image.size == 0);
    CHECK(loading.size == 0);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("C facade generates a minimap with default or explicit options") {
    ArxLevel* level = makeMinimalLevel();
    CHECK(arx_pistoris_level_generate_minimap(nullptr, nullptr) == ARX_INVALID_HANDLE);
    REQUIRE(arx_pistoris_level_generate_minimap(level, nullptr) == ARX_OK);

    ArxLevelMinimapView minimap = ARX_LEVEL_MINIMAP_VIEW_INIT;
    REQUIRE(arx_pistoris_level_minimap(level, &minimap) == ARX_OK);
    ArxImageInfo info{};
    REQUIRE(arx_pistoris_binary_inspect_encoded_image(minimap.encoded_image, &info) == ARX_OK);
    CHECK(info.width == 640);
    CHECK(info.height == 640);

    ArxLevelMinimapGenerationOptions options = ARX_LEVEL_MINIMAP_GENERATION_OPTIONS_INIT;
    options.foreground.color = {1.0f, 0.0f, 0.0f};
    options.background.color = {0.0f, 0.0f, 1.0f};
    REQUIRE(arx_pistoris_level_generate_minimap(level, &options) == ARX_OK);
    REQUIRE(arx_pistoris_level_minimap(level, &minimap) == ARX_OK);
    REQUIRE(arx_pistoris_binary_inspect_encoded_image(minimap.encoded_image, &info) == ARX_OK);
    CHECK(info.width == 640);
    CHECK(info.height == 640);
    CHECK(minimap.world_xz_bounds.min.x == doctest::Approx(0.0f));
    CHECK(minimap.world_xz_bounds.min.y == doctest::Approx(0.0f));
    CHECK(minimap.world_xz_bounds.max.x == doctest::Approx(16000.0f));
    CHECK(minimap.world_xz_bounds.max.y == doctest::Approx(16000.0f));
    options.halo_color.r = 1.1f;
    CHECK(arx_pistoris_level_generate_minimap(level, &options) == ARX_INVALID_OPTIONS);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Level handles copy submitted strings and clone independently") {
    CHECK(arx_pistoris_level_create(nullptr) == ARX_INVALID_DATA_POINTER);

    ArxLevel* level = nullptr;
    REQUIRE(arx_pistoris_level_create(&level) == ARX_OK);

    ArxStringView resource_path{};
    REQUIRE(arx_pistoris_level_resource_path(level, &resource_path) == ARX_OK);
    CHECK(resource_path.size == 0);
    REQUIRE(arx_pistoris_level_set_resource_path(level, view("level:7")) == ARX_OK);
    REQUIRE(arx_pistoris_level_resource_path(level, &resource_path) == ARX_OK);
    CHECK(std::string_view(resource_path.data, resource_path.size) == "graph/levels/level7/level7.dlf");
    CHECK(arx_pistoris_level_set_resource_path(level, view("ambiance:cave")) == ARX_LEVEL_BAD_RESOURCE_PATH);

    ArxLevelMeshInput oversized{};
    oversized.vertex_count = std::numeric_limits<std::size_t>::max();
    if constexpr (std::numeric_limits<std::size_t>::max() > static_cast<std::size_t>(ARX_INVALID_INDEX))
      CHECK(arx_pistoris_level_replace_mesh(level, &oversized) == ARX_LEVEL_TOO_MANY_VERTICES);

    ArxLevelMeshInput maximum_count{};
    maximum_count.vertex_count = ARX_INVALID_INDEX;
    CHECK(arx_pistoris_level_replace_mesh(level, &maximum_count) == ARX_INVALID_DATA_POINTER);

    char name[] = "room";
    const ArxLevelRoom room{{name, 4}};
    ArxRoomIndex index = 42;
    REQUIRE(arx_pistoris_level_add_room(level, &room, &index) == ARX_OK);
    CHECK(index == 0);
    name[0] = 'x';

    ArxLevelRoom returned{};
    REQUIRE(arx_pistoris_level_copy_rooms(level, index, 1, &returned) == ARX_OK);
    CHECK(std::string_view(returned.name.data, returned.name.size) == "room");

    ArxRoomIndex duplicate = 42;
    const ArxLevelRoom duplicate_room{view("room")};
    REQUIRE(arx_pistoris_level_add_room(level, &duplicate_room, &duplicate) == ARX_OK);
    CHECK(duplicate == 1);

    ArxLevel* clone = nullptr;
    REQUIRE(arx_pistoris_level_clone(level, &clone) == ARX_OK);
    const ArxLevelRoom second_room{view("second")};
    REQUIRE(arx_pistoris_level_add_room(level, &second_room, &index) == ARX_OK);
    size_t clone_rooms = 0;
    REQUIRE(arx_pistoris_level_room_count(clone, &clone_rooms) == ARX_OK);
    CHECK(clone_rooms == 2);
    REQUIRE(arx_pistoris_level_resource_path(clone, &resource_path) == ARX_OK);
    CHECK(std::string_view(resource_path.data, resource_path.size) == "graph/levels/level7/level7.dlf");

    arx_pistoris_level_destroy(clone);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Level C facade appends owned textures") {
    ArxLevel* level = makeMinimalLevel();

    char path[] = "graph/obj3d/textures/stone.bmp";
    const ArxTextureView submitted = {{path, sizeof(path) - 1}, {}, view(".jpg")};
    ArxTextureIndex index = 42;
    REQUIRE(arx_pistoris_level_add_texture(level, &submitted, &index) == ARX_OK);
    CHECK(index == 0);
    path[0] = 'x';

    size_t count = 0;
    REQUIRE(arx_pistoris_level_texture_count(level, &count) == ARX_OK);
    REQUIRE(count == 1);
    ArxTextureView copied{};
    REQUIRE(arx_pistoris_level_copy_texture_views(level, 0, 1, &copied) == ARX_OK);
    CHECK(std::string_view(copied.path.data, copied.path.size) == "graph/obj3d/textures/stone.bmp");
    CHECK(std::string_view(copied.external_image_extension.data, copied.external_image_extension.size) == ".jpg");
    REQUIRE(arx_pistoris_level_rebase_texture_paths(level, view("custom/textures")) == ARX_OK);
    REQUIRE(arx_pistoris_level_copy_texture_views(level, 0, 1, &copied) == ARX_OK);
    CHECK(std::string_view(copied.path.data, copied.path.size) == "custom/textures/stone.bmp");

    index = 42;
    const ArxTextureView invalid = {view("graph/obj3d/textures/bad__name.bmp"), {}};
    REQUIRE(arx_pistoris_level_add_texture(level, &invalid, &index) == ARX_OK);
    CHECK(index == 1);
    CHECK(arx_pistoris_level_add_texture(level, &submitted, nullptr) == ARX_INVALID_DATA_POINTER);

    size_t removed = 0;
    REQUIRE(arx_pistoris_level_compact_textures(level, &removed) == ARX_OK);
    CHECK(removed == 2);
    REQUIRE(arx_pistoris_level_texture_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    CHECK(arx_pistoris_level_compact_textures(level, nullptr) == ARX_INVALID_DATA_POINTER);

    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Level resource identity validates handles and views") {
    CHECK(arx_pistoris_level_resource_path(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_level_set_resource_path(nullptr, {}) == ARX_INVALID_HANDLE);
    ArxLevel* level = nullptr;
    REQUIRE(arx_pistoris_level_create(&level) == ARX_OK);
    CHECK(arx_pistoris_level_resource_path(level, nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_level_set_resource_path(level, {nullptr, 1}) == ARX_INVALID_DATA_POINTER);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Level C facade converts through GLB and native handles") {
    CHECK(arx_pistoris_strerror(ARX_GLB_NO_LEVEL_GEOMETRY) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_LEVEL_GEOMETRY) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_LEVEL_POSITION_ATTRIBUTE) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_LEVEL_INDEX_ACCESSOR) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_LEVEL_NORMAL_ATTRIBUTE) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_LEVEL_TEXCOORD_ATTRIBUTE) != nullptr);
    CHECK(arx_pistoris_strerror(ARX_GLB_BAD_LEVEL_COLOR_ATTRIBUTE) != nullptr);

    ArxLevel* level = makeMinimalLevel();

    size_t vertex_count = 0;
    REQUIRE(arx_pistoris_level_vertex_count(level, &vertex_count) == ARX_OK);
    REQUIRE(vertex_count == 3);
    std::array<ArxLevelVertex, 3> vertices{};
    REQUIRE(arx_pistoris_level_copy_vertices(level, 0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(vertices[1].position.x == doctest::Approx(1.0f));

    uint8_t* glb = nullptr;
    size_t glb_size = 0;
    REQUIRE(arx_pistoris_level_export_glb(level, nullptr, 0, nullptr, nullptr, &glb, &glb_size) == ARX_OK);
    REQUIRE(glb != nullptr);
    REQUIRE(glb_size != 0);

    ArxLevel* from_glb = nullptr;
    ArxTextureSourcePaths* glb_sources = nullptr;
    REQUIRE(arx_pistoris_level_import_glb(glb, glb_size, nullptr, &from_glb, nullptr, &glb_sources) == ARX_OK);
    REQUIRE(glb_sources != nullptr);
    std::size_t glb_source_count = 1;
    REQUIRE(arx_pistoris_texture_source_paths_count(glb_sources, &glb_source_count) == ARX_OK);
    CHECK(glb_source_count == 0);
    CHECK(arx_pistoris_level_validate(from_glb) == ARX_OK);
    arx_pistoris_free_bytes(glb);
    arx_pistoris_texture_source_paths_destroy(glb_sources);
    arx_pistoris_level_destroy(from_glb);

    ArxLevelNativeBakeOptions options{};
    options.level_name = view("level7");
    options.reconstruct_quads = 1;
    options.include_texture_files = 0;

    ArxFts* fts = nullptr;
    ArxLlf* llf = nullptr;
    ArxDlf* dlf = nullptr;
    ArxNativeTextureFiles* texture_files = nullptr;
    REQUIRE(arx_pistoris_level_bake_native(level, &options, &fts, &llf, &dlf, &texture_files) == ARX_OK);
    CHECK(arx_pistoris_fts_validate(fts) == ARX_OK);
    CHECK(arx_pistoris_llf_validate(llf) == ARX_OK);
    CHECK(arx_pistoris_dlf_validate(dlf) == ARX_OK);

    char* fts_json = nullptr;
    CHECK(arx_pistoris_fts_to_json(fts, 1, ARX_NATIVE_TEXT_UTF8, &fts_json) == ARX_JSON_BAD_SCHEMA);
    CHECK(fts_json == nullptr);

    char* llf_json = nullptr;
    REQUIRE(arx_pistoris_llf_to_json(llf, 0, view("api"), &llf_json) == ARX_OK);
    CHECK(std::string_view(llf_json).find("\"lastModifiedBy\":\"arx-pistoris/api\"") != std::string_view::npos);
    ArxLlf* json_llf = nullptr;
    REQUIRE(arx_pistoris_llf_from_json(reinterpret_cast<const uint8_t*>(llf_json), std::strlen(llf_json), &json_llf) ==
            ARX_OK);
    CHECK(arx_pistoris_llf_validate(json_llf) == ARX_OK);
    arx_pistoris_llf_destroy(json_llf);
    arx_pistoris_free_string(llf_json);

    char* dlf_json = nullptr;
    REQUIRE(arx_pistoris_dlf_to_json(dlf, 0, view("api"), ARX_NATIVE_TEXT_UTF8, &dlf_json) == ARX_OK);
    CHECK(std::string_view(dlf_json).find("\"lastModifiedBy\":\"arx-pistoris/api\"") != std::string_view::npos);
    ArxDlf* json_dlf = nullptr;
    REQUIRE(arx_pistoris_dlf_from_json(
                reinterpret_cast<const uint8_t*>(dlf_json), std::strlen(dlf_json), ARX_NATIVE_TEXT_UTF8, &json_dlf) ==
            ARX_OK);
    CHECK(arx_pistoris_dlf_validate(json_dlf) == ARX_OK);
    arx_pistoris_dlf_destroy(json_dlf);
    arx_pistoris_free_string(dlf_json);

    const std::array<uint8_t, 2> bad_json = {'{', ']'};
    ArxFts* json_fts = fts;
    CHECK(arx_pistoris_fts_from_json(bad_json.data(), bad_json.size(), ARX_NATIVE_TEXT_UTF8, &json_fts) != ARX_OK);
    CHECK(json_fts == nullptr);
    CHECK(arx_pistoris_fts_to_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &fts_json) == ARX_INVALID_HANDLE);

    uint8_t* dlf_bytes = nullptr;
    size_t dlf_byte_count = 0;
    ArxDlfWriteOptions dlf_write_options = ARX_DLF_WRITE_OPTIONS_INIT;
    dlf_write_options.embedded_llf = llf;
    dlf_write_options.signer = view("api");
    REQUIRE(arx_pistoris_dlf_write(dlf, &dlf_write_options, 1, &dlf_bytes, &dlf_byte_count) == ARX_OK);
    ArxDlf* parsed_dlf = nullptr;
    ArxLlf* parsed_embedded_llf = nullptr;
    REQUIRE(arx_pistoris_dlf_read(dlf_bytes, dlf_byte_count, &parsed_dlf, &parsed_embedded_llf) == ARX_OK);
    CHECK(arx_pistoris_dlf_validate(parsed_dlf) == ARX_OK);
    REQUIRE(parsed_embedded_llf != nullptr);
    CHECK(arx_pistoris_llf_validate(parsed_embedded_llf) == ARX_OK);
    arx_pistoris_llf_destroy(parsed_embedded_llf);
    arx_pistoris_dlf_destroy(parsed_dlf);
    parsed_dlf = nullptr;
    REQUIRE(arx_pistoris_dlf_read(dlf_bytes, dlf_byte_count, &parsed_dlf, nullptr) == ARX_OK);
    CHECK(arx_pistoris_dlf_validate(parsed_dlf) == ARX_OK);
    arx_pistoris_dlf_destroy(parsed_dlf);
    arx_pistoris_free_bytes(dlf_bytes);

    uint8_t* raw_dlf_bytes = nullptr;
    size_t raw_dlf_byte_count = 0;
    REQUIRE(arx_pistoris_dlf_write(dlf, &dlf_write_options, 0, &raw_dlf_bytes, &raw_dlf_byte_count) == ARX_OK);
    constexpr size_t kDlfRawPrefixSize = 8520;
    REQUIRE(raw_dlf_byte_count >= kDlfRawPrefixSize + 6);
    CHECK(std::memcmp(raw_dlf_bytes + 20, "arx-pistoris/api", sizeof("arx-pistoris/api")) == 0);
    CHECK(std::memcmp(raw_dlf_bytes + kDlfRawPrefixSize, "graph/", 6) == 0);
    ArxDlf* parsed_raw_dlf = nullptr;
    ArxLlf* parsed_raw_embedded_llf = nullptr;
    REQUIRE(arx_pistoris_dlf_read(raw_dlf_bytes, raw_dlf_byte_count, &parsed_raw_dlf, &parsed_raw_embedded_llf) ==
            ARX_OK);
    arx_pistoris_llf_destroy(parsed_raw_embedded_llf);
    arx_pistoris_dlf_destroy(parsed_raw_dlf);
    arx_pistoris_free_bytes(raw_dlf_bytes);

    uint8_t* llf_bytes = nullptr;
    size_t llf_byte_count = 0;
    ArxLlfWriteOptions llf_write_options = ARX_LLF_WRITE_OPTIONS_INIT;
    llf_write_options.signer = view("api");
    REQUIRE(arx_pistoris_llf_write(llf, &llf_write_options, 1, &llf_bytes, &llf_byte_count) == ARX_OK);
    ArxLlf* parsed_llf = nullptr;
    REQUIRE(arx_pistoris_llf_read(llf_bytes, llf_byte_count, &parsed_llf) == ARX_OK);
    CHECK(arx_pistoris_llf_validate(parsed_llf) == ARX_OK);
    arx_pistoris_llf_destroy(parsed_llf);
    arx_pistoris_free_bytes(llf_bytes);

    uint8_t* raw_llf_bytes = nullptr;
    size_t raw_llf_byte_count = 0;
    REQUIRE(arx_pistoris_llf_write(llf, &llf_write_options, 0, &raw_llf_bytes, &raw_llf_byte_count) == ARX_OK);
    REQUIRE(raw_llf_byte_count >= sizeof(float) + 14);
    CHECK(std::memcmp(raw_llf_bytes + sizeof(float), "DANAE_LLH_FILE", 14) == 0);
    CHECK(std::memcmp(raw_llf_bytes + 20, "arx-pistoris/api", sizeof("arx-pistoris/api")) == 0);
    ArxLlf* parsed_raw_llf = nullptr;
    REQUIRE(arx_pistoris_llf_read(raw_llf_bytes, raw_llf_byte_count, &parsed_raw_llf) == ARX_OK);
    arx_pistoris_llf_destroy(parsed_raw_llf);
    arx_pistoris_free_bytes(raw_llf_bytes);

    size_t texture_count = 1;
    REQUIRE(arx_pistoris_native_texture_files_count(texture_files, &texture_count) == ARX_OK);
    CHECK(texture_count == 0);

    ArxLevel* from_native = nullptr;
    ArxTextureSourcePaths* native_sources = nullptr;
    REQUIRE(arx_pistoris_level_import_native(fts, llf, dlf, &from_native, &native_sources, ARX_NATIVE_TEXT_AUTO) ==
            ARX_OK);
    REQUIRE(native_sources != nullptr);
    std::size_t native_source_count = 1;
    REQUIRE(arx_pistoris_texture_source_paths_count(native_sources, &native_source_count) == ARX_OK);
    CHECK(native_source_count == 0);
    CHECK(arx_pistoris_level_validate(from_native) == ARX_OK);

    arx_pistoris_texture_source_paths_destroy(native_sources);
    arx_pistoris_level_destroy(from_native);
    arx_pistoris_native_texture_files_destroy(texture_files);
    arx_pistoris_dlf_destroy(dlf);
    arx_pistoris_llf_destroy(llf);
    arx_pistoris_fts_destroy(fts);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Failed Level GLB imports clear C result information") {
    const std::uint8_t invalid = 0;
    ArxLevelGlbImportInfo info{{1.0f, 2.0f, 3.0f}};
    ArxLevel* level = reinterpret_cast<ArxLevel*>(1);

    CHECK(arx_pistoris_level_import_glb(&invalid, 1, nullptr, &level, &info, nullptr) != ARX_OK);
    CHECK(level == nullptr);
    CHECK(info.applied_arx_offset.x == 0.0f);
    CHECK(info.applied_arx_offset.y == 0.0f);
    CHECK(info.applied_arx_offset.z == 0.0f);
  }

  TEST_CASE("Level C facade attaches Model previews") {
    ArxLevel* level = makeMinimalLevel();
    ArxLevelEntity entity{};
    entity.class_path = view("graph/obj3d/interactive/npc/human_base/human_base");
    entity.ident = -1;
    entity.rotation.w = 1.0f;
    ArxEntityIndex entity_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_entity(level, &entity, &entity_index) == ARX_OK);

    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_import_obj(
                reinterpret_cast<const std::uint8_t*>(kObj.data()), kObj.size(), nullptr, 0, &model, nullptr) ==
            ARX_OK);
    REQUIRE(arx_pistoris_model_set_resource_path(model, view("model:npc:human_base")) == ARX_OK);

    const ArxModel* models[] = {model};
    ArxLevelModelPreviewReport report{};
    std::uint8_t* glb = nullptr;
    std::size_t glb_size = 0;
    REQUIRE(arx_pistoris_level_export_glb(level, models, 1, nullptr, &report, &glb, &glb_size) == ARX_OK);
    CHECK(report.mapped_models == 1);
    CHECK(report.previewed_entities == 1);
    CHECK(glb != nullptr);
    CHECK(glb_size != 0);

    arx_pistoris_free_bytes(glb);
    arx_pistoris_model_destroy(model);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Level C facade replaces corner normals directly") {
    ArxLevel* level = makeMinimalLevel();
    ArxLevelFace face{};
    REQUIRE(arx_pistoris_level_copy_faces(level, 0, 1, &face) == ARX_OK);
    face.corners[0].normal = {1.0f, 0.0f, 0.0f};
    REQUIRE(arx_pistoris_level_set_face(level, 0, &face) == ARX_OK);
    face = {};
    REQUIRE(arx_pistoris_level_copy_faces(level, 0, 1, &face) == ARX_OK);
    CHECK(face.corners[0].normal.x == doctest::Approx(1.0f));
    CHECK(face.corners[0].normal.y == doctest::Approx(0.0f));
    CHECK(face.corners[0].normal.z == doctest::Approx(0.0f));
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("C facade rejects enum values that would narrow to valid C++ values") {
    ArxLevel* level = makeMinimalLevel();

    const ArxLevelRoom second_room{view("second")};
    ArxRoomIndex room_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_room(level, &second_room, &room_index) == ARX_OK);

    ArxLevelPortal portal{};
    portal.name = view("portal");
    portal.room_1 = 0;
    portal.room_2 = 1;
    portal.shape = ARX_PORTAL_TRIANGLE + 256U;
    portal.vertices[0] = {0.0f, 0.0f, 0.0f};
    portal.vertices[1] = {1.0f, 0.0f, 0.0f};
    portal.vertices[2] = {0.0f, 1.0f, 0.0f};
    ArxPortalIndex portal_index = 42;
    CHECK(arx_pistoris_level_add_portal(level, &portal, &portal_index) == ARX_LEVEL_BAD_PORTAL_SHAPE);
    CHECK(portal_index == ARX_INVALID_INDEX);

    const std::array<ArxVector2, 3> perimeter = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}}};
    ArxLevelZoneInput zone{};
    zone.value.name = view("zone");
    zone.value.perimeter_count = perimeter.size();
    zone.value.height_mode = ARX_ZONE_HEIGHT_FINITE + 256U;
    zone.value.height = 1.0f;
    zone.perimeter_xz = perimeter.data();
    ArxZoneIndex zone_index = 42;
    CHECK(arx_pistoris_level_add_zone(level, &zone, &zone_index) == ARX_LEVEL_BAD_ZONE_HEIGHT_MODE);
    CHECK(zone_index == ARX_INVALID_INDEX);

    std::array<ArxLevelPathNode, 1> nodes{};
    nodes[0].type = 2U;
    const ArxLevelPathInput path{view("path"), {}, nodes.data(), nodes.size()};
    ArxPathIndex path_index = 42;
    CHECK(arx_pistoris_level_add_path(level, &path, &path_index) == ARX_LEVEL_BAD_PATH_NODE_TYPE);
    CHECK(path_index == ARX_INVALID_INDEX);

    ArxLevelVertexWeldOptions weld_options{0.01f, ARX_LEVEL_WELD_EUCLIDEAN + 256U, ARX_LEVEL_DEGENERATE_FACE_PRESERVE};
    CHECK(arx_pistoris_level_weld_vertices(level, &weld_options) == ARX_INVALID_OPTIONS);
    weld_options.metric = ARX_LEVEL_WELD_EUCLIDEAN;
    weld_options.degenerate_faces = ARX_LEVEL_DEGENERATE_FACE_PRESERVE + 256U;
    CHECK(arx_pistoris_level_weld_vertices(level, &weld_options) == ARX_INVALID_OPTIONS);

    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("C facade sets and retrieves canonical room distances") {
    ArxLevel* level = makeMinimalLevel();

    const ArxLevelRoom second_room{view("second")};
    ArxRoomIndex room_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_room(level, &second_room, &room_index) == ARX_OK);

    ArxLevelPortal portal{};
    portal.name = view("portal");
    portal.room_1 = 0;
    portal.room_2 = 1;
    portal.shape = ARX_PORTAL_TRIANGLE;
    portal.vertices[0] = {0.0f, 0.0f, 0.0f};
    portal.vertices[1] = {1.0f, 0.0f, 0.0f};
    portal.vertices[2] = {0.0f, 1.0f, 0.0f};
    ArxPortalIndex portal_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_portal(level, &portal, &portal_index) == ARX_OK);

    ArxLevelRoomDistance input{};
    input.room_a = 1;
    input.room_b = 0;
    input.distance = -1.0f;
    input.portal_a = portal_index;
    input.portal_b = portal_index;
    CHECK(arx_pistoris_level_set_room_distance(nullptr, &input) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_level_set_room_distance(level, nullptr) == ARX_INVALID_DATA_POINTER);
    REQUIRE(arx_pistoris_level_set_room_distance(level, &input) == ARX_OK);

    size_t count = 0;
    REQUIRE(arx_pistoris_level_room_distance_count(level, &count) == ARX_OK);
    CHECK(count == 1);

    uint8_t has_distance = 0;
    ArxLevelRoomDistance output{};
    REQUIRE(arx_pistoris_level_room_distance(level, 0, 1, &has_distance, &output) == ARX_OK);
    CHECK(has_distance == 1);
    CHECK(output.room_a == 0);
    CHECK(output.room_b == 1);
    CHECK(output.distance == doctest::Approx(-1.0f));
    CHECK(output.portal_a == portal_index);
    CHECK(output.portal_b == portal_index);

    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("C generation facade preserves C++ defaults") {
    CHECK(arx_pistoris_level_generate_nav_surface(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_level_set_nav_surface_from_floor(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_level_prune_nav_surface_islands(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_level_generate_anchors(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_level_generate_anchor_connections(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_level_prune_anchor_islands(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_level_generate_room_distances(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_level_generate_static_lighting(nullptr, nullptr) == ARX_INVALID_HANDLE);

    const auto check_defaults = [](auto default_call, auto explicit_call) {
      ArxLevel* default_level = nullptr;
      ArxLevel* explicit_level = nullptr;
      REQUIRE(arx_pistoris_level_create(&default_level) == ARX_OK);
      REQUIRE(arx_pistoris_level_create(&explicit_level) == ARX_OK);
      CHECK(default_call(default_level) == explicit_call(explicit_level));
      arx_pistoris_level_destroy(explicit_level);
      arx_pistoris_level_destroy(default_level);
    };

    const ArxLevelNavSurfaceGenOptions nav_gen = ARX_LEVEL_NAV_SURFACE_GEN_OPTIONS_INIT;
    check_defaults([](ArxLevel* level) { return arx_pistoris_level_generate_nav_surface(level, nullptr); },
                   [&](ArxLevel* level) { return arx_pistoris_level_generate_nav_surface(level, &nav_gen); });

    const ArxLevelNavSurfaceSourceOptions nav_source = ARX_LEVEL_NAV_SURFACE_SOURCE_OPTIONS_INIT;
    check_defaults([](ArxLevel* level) { return arx_pistoris_level_set_nav_surface_from_floor(level, nullptr); },
                   [&](ArxLevel* level) { return arx_pistoris_level_set_nav_surface_from_floor(level, &nav_source); });

    const ArxLevelNavSurfacePruneOptions nav_prune = ARX_LEVEL_NAV_SURFACE_PRUNE_OPTIONS_INIT;
    check_defaults([](ArxLevel* level) { return arx_pistoris_level_prune_nav_surface_islands(level, nullptr); },
                   [&](ArxLevel* level) { return arx_pistoris_level_prune_nav_surface_islands(level, &nav_prune); });

    const ArxLevelAnchorGenOptions anchor_gen = ARX_LEVEL_ANCHOR_GEN_OPTIONS_INIT;
    check_defaults([](ArxLevel* level) { return arx_pistoris_level_generate_anchors(level, nullptr); },
                   [&](ArxLevel* level) { return arx_pistoris_level_generate_anchors(level, &anchor_gen); });

    const ArxLevelAnchorConnectionGenOptions connection_gen = ARX_LEVEL_ANCHOR_CONNECTION_GEN_OPTIONS_INIT;
    check_defaults(
        [](ArxLevel* level) { return arx_pistoris_level_generate_anchor_connections(level, nullptr); },
        [&](ArxLevel* level) { return arx_pistoris_level_generate_anchor_connections(level, &connection_gen); });

    const ArxLevelAnchorPruneOptions anchor_prune = ARX_LEVEL_ANCHOR_PRUNE_OPTIONS_INIT;
    check_defaults([](ArxLevel* level) { return arx_pistoris_level_prune_anchor_islands(level, nullptr); },
                   [&](ArxLevel* level) { return arx_pistoris_level_prune_anchor_islands(level, &anchor_prune); });

    const ArxLevelRoomDistanceGenOptions room_distance = ARX_LEVEL_ROOM_DISTANCE_GEN_OPTIONS_INIT;
    check_defaults([](ArxLevel* level) { return arx_pistoris_level_generate_room_distances(level, nullptr); },
                   [&](ArxLevel* level) { return arx_pistoris_level_generate_room_distances(level, &room_distance); });

    const ArxLevelStaticLightingGenOptions lighting = ARX_LEVEL_STATIC_LIGHTING_GEN_OPTIONS_INIT;
    check_defaults([](ArxLevel* level) { return arx_pistoris_level_generate_static_lighting(level, nullptr); },
                   [&](ArxLevel* level) { return arx_pistoris_level_generate_static_lighting(level, &lighting); });
  }

  TEST_CASE("C facade exposes detached Level image rendering") {
    ArxVector2 projection_offset{};
    REQUIRE(arx_pistoris_level_image_projection_offset_from_mini_offset({1.0f, 2.0f}, &projection_offset) == ARX_OK);
    CHECK(projection_offset.x == doctest::Approx(65.0f));
    CHECK(projection_offset.y == doctest::Approx(124.0f));
    ArxVector2 mini_offset{};
    REQUIRE(arx_pistoris_level_image_mini_offset_from_projection_offset(projection_offset, &mini_offset) == ARX_OK);
    CHECK(mini_offset.x == doctest::Approx(1.0f));
    CHECK(mini_offset.y == doctest::Approx(2.0f));

    const std::vector<std::uint8_t> image = makeSolidTestBmp(2, 1);
    const ArxEncodedImageView view{image.data(), image.size()};
    const ArxLevelMinimapReprojectionOptions options = ARX_LEVEL_MINIMAP_REPROJECTION_OPTIONS_INIT;
    std::uint8_t* rendered = nullptr;
    std::size_t rendered_size = 0;
    REQUIRE(arx_pistoris_level_image_reproject_minimap_png(view, &options, &rendered, &rendered_size) == ARX_OK);
    CHECK(rendered != nullptr);
    CHECK(rendered_size != 0);
    arx_pistoris_free_bytes(rendered);

    const ArxLevelGameMinimapReprojectionOptions game_options = ARX_LEVEL_GAME_MINIMAP_REPROJECTION_OPTIONS_INIT;
    rendered = nullptr;
    rendered_size = 0;
    REQUIRE(arx_pistoris_level_image_reproject_game_minimap_png(view, &game_options, &rendered, &rendered_size) ==
            ARX_OK);
    CHECK(rendered != nullptr);
    CHECK(rendered_size != 0);
    arx_pistoris_free_bytes(rendered);

    rendered = nullptr;
    rendered_size = 0;
    REQUIRE(arx_pistoris_level_image_render_loading_screen_png(
                view, ARX_LEVEL_LOADING_SCREEN_LAYOUT_NORMAL, &rendered, &rendered_size) == ARX_OK);
    CHECK(rendered != nullptr);
    CHECK(rendered_size != 0);
    arx_pistoris_free_bytes(rendered);
  }
}
