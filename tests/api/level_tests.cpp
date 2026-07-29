// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/level/types.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <ostream>  // IWYU pragma: keep
#include <string_view>

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
  TEST_CASE("Level handles copy submitted strings and clone independently") {
    CHECK(arx_pistoris_level_create(nullptr) == ARX_INVALID_DATA_POINTER);

    ArxLevel* level = nullptr;
    REQUIRE(arx_pistoris_level_create(&level) == ARX_OK);

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
    CHECK(arx_pistoris_level_add_room(level, &duplicate_room, &duplicate) == ARX_LEVEL_DUPLICATE_ROOM_NAME);
    CHECK(duplicate == ARX_INVALID_INDEX);

    ArxLevel* clone = nullptr;
    REQUIRE(arx_pistoris_level_clone(level, &clone) == ARX_OK);
    const ArxLevelRoom second_room{view("second")};
    REQUIRE(arx_pistoris_level_add_room(level, &second_room, &index) == ARX_OK);
    size_t clone_rooms = 0;
    REQUIRE(arx_pistoris_level_room_count(clone, &clone_rooms) == ARX_OK);
    CHECK(clone_rooms == 1);

    arx_pistoris_level_destroy(clone);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Level C facade appends owned textures") {
    ArxLevel* level = makeMinimalLevel();

    char path[] = "graph/obj3d/textures/stone.bmp";
    const ArxLevelTextureView submitted = {{path, sizeof(path) - 1}, {}};
    ArxTextureIndex index = 42;
    REQUIRE(arx_pistoris_level_add_texture(level, &submitted, &index) == ARX_OK);
    CHECK(index == 0);
    path[0] = 'x';

    size_t count = 0;
    REQUIRE(arx_pistoris_level_texture_count(level, &count) == ARX_OK);
    REQUIRE(count == 1);
    ArxLevelTextureView copied{};
    REQUIRE(arx_pistoris_level_copy_texture_views(level, 0, 1, &copied) == ARX_OK);
    CHECK(std::string_view(copied.path.data, copied.path.size) == "graph/obj3d/textures/stone.bmp");

    index = 42;
    const ArxLevelTextureView invalid = {view("graph/obj3d/textures/bad__name.bmp"), {}};
    CHECK(arx_pistoris_level_add_texture(level, &invalid, &index) == ARX_LEVEL_BAD_TEXTURE_PATH);
    CHECK(index == ARX_INVALID_INDEX);
    CHECK(arx_pistoris_level_add_texture(level, &submitted, nullptr) == ARX_INVALID_DATA_POINTER);

    size_t removed = 0;
    REQUIRE(arx_pistoris_level_compact_textures(level, &removed) == ARX_OK);
    CHECK(removed == 1);
    REQUIRE(arx_pistoris_level_texture_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    CHECK(arx_pistoris_level_compact_textures(level, nullptr) == ARX_INVALID_DATA_POINTER);

    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Level C facade converts through GLB and native handles") {
    ArxLevel* level = makeMinimalLevel();

    size_t vertex_count = 0;
    REQUIRE(arx_pistoris_level_vertex_count(level, &vertex_count) == ARX_OK);
    REQUIRE(vertex_count == 3);
    std::array<ArxLevelVertex, 3> vertices{};
    REQUIRE(arx_pistoris_level_copy_vertices(level, 0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(vertices[1].position.x == doctest::Approx(1.0f));

    uint8_t* glb = nullptr;
    size_t glb_size = 0;
    REQUIRE(arx_pistoris_level_export_glb(level, nullptr, &glb, &glb_size) == ARX_OK);
    REQUIRE(glb != nullptr);
    REQUIRE(glb_size != 0);

    ArxLevel* from_glb = nullptr;
    REQUIRE(arx_pistoris_level_from_glb(glb, glb_size, nullptr, nullptr, &from_glb) == ARX_OK);
    CHECK(arx_pistoris_level_validate(from_glb) == ARX_OK);
    arx_pistoris_free_bytes(glb);
    arx_pistoris_level_destroy(from_glb);

    ArxLevelNativeBakeOptions options{};
    options.level_name = view("level7");
    options.texture_path_mode = ARX_NATIVE_TEXTURE_PATH_PRESERVE;
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
    CHECK(arx_pistoris_fts_to_json(fts, 1, &fts_json) == ARX_JSON_BAD_SCHEMA);
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
    REQUIRE(arx_pistoris_dlf_to_json(dlf, 0, view("api"), &dlf_json) == ARX_OK);
    CHECK(std::string_view(dlf_json).find("\"lastModifiedBy\":\"arx-pistoris/api\"") != std::string_view::npos);
    ArxDlf* json_dlf = nullptr;
    REQUIRE(arx_pistoris_dlf_from_json(reinterpret_cast<const uint8_t*>(dlf_json), std::strlen(dlf_json), &json_dlf) ==
            ARX_OK);
    CHECK(arx_pistoris_dlf_validate(json_dlf) == ARX_OK);
    arx_pistoris_dlf_destroy(json_dlf);
    arx_pistoris_free_string(dlf_json);

    const std::array<uint8_t, 2> bad_json = {'{', ']'};
    ArxFts* json_fts = fts;
    CHECK(arx_pistoris_fts_from_json(bad_json.data(), bad_json.size(), &json_fts) != ARX_OK);
    CHECK(json_fts == nullptr);
    CHECK(arx_pistoris_fts_to_json(nullptr, 0, &fts_json) == ARX_INVALID_HANDLE);

    uint8_t* dlf_bytes = nullptr;
    size_t dlf_byte_count = 0;
    ArxDlfWriteOptions dlf_write_options = ARX_DLF_WRITE_OPTIONS_INIT;
    dlf_write_options.embedded_llf = llf;
    dlf_write_options.signer = view("api");
    REQUIRE(arx_pistoris_dlf_write(dlf, &dlf_write_options, 1, &dlf_bytes, &dlf_byte_count) == ARX_OK);
    ArxDlf* parsed_dlf = nullptr;
    ArxLlf* parsed_embedded_llf = nullptr;
    REQUIRE(arx_pistoris_dlf_parse(dlf_bytes, dlf_byte_count, &parsed_dlf, &parsed_embedded_llf) == ARX_OK);
    CHECK(arx_pistoris_dlf_validate(parsed_dlf) == ARX_OK);
    REQUIRE(parsed_embedded_llf != nullptr);
    CHECK(arx_pistoris_llf_validate(parsed_embedded_llf) == ARX_OK);
    arx_pistoris_llf_destroy(parsed_embedded_llf);
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
    REQUIRE(arx_pistoris_dlf_parse(raw_dlf_bytes, raw_dlf_byte_count, &parsed_raw_dlf, &parsed_raw_embedded_llf) ==
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
    REQUIRE(arx_pistoris_llf_parse(llf_bytes, llf_byte_count, &parsed_llf) == ARX_OK);
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
    REQUIRE(arx_pistoris_llf_parse(raw_llf_bytes, raw_llf_byte_count, &parsed_raw_llf) == ARX_OK);
    arx_pistoris_llf_destroy(parsed_raw_llf);
    arx_pistoris_free_bytes(raw_llf_bytes);

    size_t texture_count = 1;
    REQUIRE(arx_pistoris_native_texture_files_count(texture_files, &texture_count) == ARX_OK);
    CHECK(texture_count == 0);

    ArxLevel* from_native = nullptr;
    REQUIRE(arx_pistoris_level_from_native(fts, llf, dlf, &from_native) == ARX_OK);
    CHECK(arx_pistoris_level_validate(from_native) == ARX_OK);

    arx_pistoris_level_destroy(from_native);
    arx_pistoris_native_texture_files_destroy(texture_files);
    arx_pistoris_dlf_destroy(dlf);
    arx_pistoris_llf_destroy(llf);
    arx_pistoris_fts_destroy(fts);
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
    nodes[0].type = ARX_PATH_NODE_STANDARD + 256U;
    const ArxLevelPathInput path{view("path"), {}, nodes.data(), nodes.size()};
    ArxPathIndex path_index = 42;
    CHECK(arx_pistoris_level_add_path(level, &path, &path_index) == ARX_LEVEL_BAD_PATH_NODE_TYPE);
    CHECK(path_index == ARX_INVALID_INDEX);

    ArxLevelVertexWeldOptions weld_options{0.01f, ARX_LEVEL_WELD_EUCLIDEAN + 256U, ARX_LEVEL_DEGENERATE_FACE_PRESERVE};
    CHECK(arx_pistoris_level_weld_vertices(level, &weld_options) == ARX_INVALID_OPTIONS);
    weld_options.metric = ARX_LEVEL_WELD_EUCLIDEAN;
    weld_options.degenerate_faces = ARX_LEVEL_DEGENERATE_FACE_PRESERVE + 256U;
    CHECK(arx_pistoris_level_weld_vertices(level, &weld_options) == ARX_INVALID_OPTIONS);

    ArxLevelNativeBakeOptions options{};
    options.level_name = view("test");
    options.texture_path_mode = ARX_NATIVE_TEXTURE_PATH_PRESERVE + 256U;
    ArxFts* fts = nullptr;
    ArxLlf* llf = nullptr;
    ArxDlf* dlf = nullptr;
    ArxNativeTextureFiles* texture_files = nullptr;
    CHECK(arx_pistoris_level_bake_native(level, &options, &fts, &llf, &dlf, &texture_files) == ARX_INVALID_OPTIONS);
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
    REQUIRE(arx_pistoris_level_get_room_distance(level, 0, 1, &has_distance, &output) == ARX_OK);
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
}
