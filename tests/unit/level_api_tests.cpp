// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/level.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/pistoris_types.h"

#include "image_helpers.h"
#include "level_add_helpers.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace pistoris;

static_assert(!std::is_move_constructible_v<Level>);
static_assert(!std::is_move_assignable_v<Level>);
static_assert(kLevelFaceBitsAll == ARX_LEVEL_FACE_BITS_ALL);
static_assert((kLevelFaceBitsAll & kFaceBitQuad) == 0);

namespace {

constexpr bool cOptionDefaultsMatchCpp() {
  constexpr ArxLevelVertexWeldOptions kCWeld = ARX_LEVEL_VERTEX_WELD_OPTIONS_INIT;
  constexpr Level::VertexWeldOptions kCppWeld{};
  if (kCWeld.radius != kCppWeld.radius || kCWeld.metric != static_cast<ArxLevelWeldMetric>(kCppWeld.metric) ||
      kCWeld.degenerate_faces != static_cast<ArxLevelDegenerateFacePolicy>(kCppWeld.degenerate_faces))
    return false;

  constexpr ArxLevelNavSurfaceSourceOptions kCNavSource = ARX_LEVEL_NAV_SURFACE_SOURCE_OPTIONS_INIT;
  constexpr Level::NavSurfaceSourceOptions kCppNavSource{};
  if (kCNavSource.clearance != kCppNavSource.clearance ||
      kCNavSource.support_min_up_cos != kCppNavSource.support_min_up_cos ||
      kCNavSource.support_ignore_flags != kCppNavSource.support_ignore_flags)
    return false;

  constexpr ArxLevelNavSurfaceGenOptions kCNavGen = ARX_LEVEL_NAV_SURFACE_GEN_OPTIONS_INIT;
  constexpr Level::NavSurfaceGenOptions kCppNavGen{};
  if (kCNavGen.clearance != kCppNavGen.clearance || kCNavGen.support_min_up_cos != kCppNavGen.support_min_up_cos ||
      kCNavGen.support_ignore_flags != kCppNavGen.support_ignore_flags || kCNavGen.radius != kCppNavGen.radius ||
      kCNavGen.height != kCppNavGen.height || kCNavGen.max_step_up != kCppNavGen.max_step_up)
    return false;

  constexpr ArxLevelNavSurfacePruneOptions kCNavPrune = ARX_LEVEL_NAV_SURFACE_PRUNE_OPTIONS_INIT;
  constexpr Level::NavSurfacePruneOptions kCppNavPrune{};
  if (kCNavPrune.min_component_area_ratio != kCppNavPrune.min_component_area_ratio ||
      kCNavPrune.min_component_area != kCppNavPrune.min_component_area)
    return false;

  constexpr ArxLevelAnchorGenOptions kCAnchorGen = ARX_LEVEL_ANCHOR_GEN_OPTIONS_INIT;
  constexpr Level::AnchorGenOptions kCppAnchorGen{};
  if (kCAnchorGen.sample_spacing != kCppAnchorGen.sample_spacing || kCAnchorGen.radius != kCppAnchorGen.radius ||
      kCAnchorGen.height != kCppAnchorGen.height)
    return false;

  constexpr ArxLevelAnchorPruneOptions kCAnchorPrune = ARX_LEVEL_ANCHOR_PRUNE_OPTIONS_INIT;
  constexpr Level::AnchorPruneOptions kCppAnchorPrune{};
  if (kCAnchorPrune.min_component_anchor_ratio != kCppAnchorPrune.min_component_anchor_ratio ||
      kCAnchorPrune.min_component_anchor_count != kCppAnchorPrune.min_component_anchor_count)
    return false;

  constexpr ArxLevelAnchorConnectionGenOptions kCConnection = ARX_LEVEL_ANCHOR_CONNECTION_GEN_OPTIONS_INIT;
  constexpr Level::AnchorConnectionGenOptions kCppConnection{};
  if (kCConnection.max_distance != kCppConnection.max_distance ||
      kCConnection.max_step_distance != kCppConnection.max_step_distance ||
      kCConnection.max_step_up != kCppConnection.max_step_up ||
      kCConnection.radius_scale != kCppConnection.radius_scale || kCConnection.max_steps != kCppConnection.max_steps)
    return false;

  constexpr ArxLevelRoomDistanceGenOptions kCDistance = ARX_LEVEL_ROOM_DISTANCE_GEN_OPTIONS_INIT;
  constexpr Level::RoomDistanceGenOptions kCppDistance{};
  if (kCDistance.portal_side_offset != kCppDistance.portal_side_offset ||
      kCDistance.sample_spacing != kCppDistance.sample_spacing ||
      kCDistance.sample_height_offset != kCppDistance.sample_height_offset ||
      kCDistance.max_link_distance != kCppDistance.max_link_distance)
    return false;

  constexpr ArxLevelStaticLightingGenOptions kCLighting = ARX_LEVEL_STATIC_LIGHTING_GEN_OPTIONS_INIT;
  constexpr Level::StaticLightingGenOptions kCppLighting{};
  if (kCLighting.ambient_color.r != kCppLighting.ambient_color.r ||
      kCLighting.ambient_color.g != kCppLighting.ambient_color.g ||
      kCLighting.ambient_color.b != kCppLighting.ambient_color.b ||
      kCLighting.global_factor != kCppLighting.global_factor || kCLighting.use_normals != kCppLighting.use_normals ||
      kCLighting.use_shadows != kCppLighting.use_shadows)
    return false;

  constexpr ArxLevelGlbImportOptions kCGlbImport = ARX_LEVEL_GLB_IMPORT_OPTIONS_INIT;
  constexpr Level::GlbImportOptions kCppGlbImport{};
  if (kCGlbImport.arx_units_per_glb_unit != kCppGlbImport.arx_units_per_glb_unit ||
      kCGlbImport.has_arx_offset != kCppGlbImport.arx_offset.has_value() || kCGlbImport.arx_offset.x != 0.0f ||
      kCGlbImport.arx_offset.y != 0.0f || kCGlbImport.arx_offset.z != 0.0f)
    return false;

  constexpr ArxLevelGlbExportOptions kCGlbExport = ARX_LEVEL_GLB_EXPORT_OPTIONS_INIT;
  constexpr Level::GlbExportOptions kCppGlbExport{};
  if (kCGlbExport.arx_units_per_glb_unit != kCppGlbExport.arx_units_per_glb_unit ||
      kCGlbExport.arx_offset.x != kCppGlbExport.arx_offset.x ||
      kCGlbExport.arx_offset.y != kCppGlbExport.arx_offset.y || kCGlbExport.arx_offset.z != kCppGlbExport.arx_offset.z)
    return false;

  constexpr ArxLevelNativeBakeOptions kCNative = ARX_LEVEL_NATIVE_BAKE_OPTIONS_INIT;
  constexpr Level::NativeBakeOptions kCppNative{};
  if (kCNative.level_name.data != nullptr || kCNative.level_name.size != kCppNative.level_name.size() ||
      kCNative.texture_folder.data != nullptr || kCNative.texture_folder.size != kCppNative.texture_folder.size() ||
      kCNative.texture_path_mode != static_cast<ArxNativeTexturePathMode>(kCppNative.texture_path_mode) ||
      kCNative.reconstruct_quads != kCppNative.reconstruct_quads ||
      kCNative.include_texture_files != kCppNative.include_texture_files || kCNative.dlf_scene_path.data != nullptr ||
      kCNative.dlf_scene_path.size != kCppNative.dlf_scene_path.size())
    return false;

  constexpr ArxLevelNativeDlfBakeOptions kCDlf = ARX_LEVEL_NATIVE_DLF_BAKE_OPTIONS_INIT;
  constexpr Level::NativeDlfBakeOptions kCppDlf{};
  return kCDlf.level_name.data == nullptr && kCDlf.level_name.size == kCppDlf.level_name.size() &&
         kCDlf.target_fts_offset.x == kCppDlf.target_fts_offset.x &&
         kCDlf.target_fts_offset.y == kCppDlf.target_fts_offset.y &&
         kCDlf.target_fts_offset.z == kCppDlf.target_fts_offset.z && kCDlf.dlf_scene_path.data == nullptr &&
         kCDlf.dlf_scene_path.size == kCppDlf.dlf_scene_path.size();
}

static_assert(cOptionDefaultsMatchCpp());

Vertex vertex(float x, float y, float z) {
  Vertex out;
  out.position = {x, y, z};
  return out;
}

Face triangle(TextureIndex texture = kNoTexture) {
  Face face;
  face.corners[0].vertex = 0;
  face.corners[1].vertex = 1;
  face.corners[2].vertex = 2;
  face.corners[0].normal = {0.0f, -1.0f, 0.0f};
  face.corners[1].normal = {0.0f, -1.0f, 0.0f};
  face.corners[2].normal = {0.0f, -1.0f, 0.0f};
  face.texture = texture;
  return face;
}

Level makeLevelWithRoomAndTriangle() {
  Level level;
  REQUIRE(test::addRoom(level, {"room"}) == 0);
  test::MeshSnapshot mesh;
  mesh.vertices = {vertex(0.0f, 0.0f, 0.0f), vertex(1.0f, 0.0f, 0.0f), vertex(0.0f, 0.0f, 1.0f)};
  mesh.faces = {triangle()};
  mesh.face_rooms = {0};
  REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);
  return Level(level);
}

Portal portal(RoomIndex first, RoomIndex second) {
  Portal out;
  out.name = "portal";
  out.room_1 = first;
  out.room_2 = second;
  out.vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
  return out;
}

Anchor anchor(float x) {
  Anchor out;
  out.position = {x, 0.0f, 0.0f};
  return out;
}

NavSurface navSurface() {
  NavSurface surface;
  surface.vertices = {vertex(0.0f, 0.0f, 0.0f), vertex(1.0f, 0.0f, 0.0f), vertex(0.0f, 0.0f, 1.0f)};
  surface.triangles.push_back({{0, 1, 2}});
  return surface;
}

Light light(std::string name = "light") {
  Light out;
  out.name = std::move(name);
  out.color = {0.5f, 0.5f, 0.5f};
  out.fallstart = 0.0f;
  out.fallend = 10.0f;
  return out;
}

Entity entity(std::string path = "graph/obj3d/interactive/items/torch") {
  Entity out;
  out.class_path = std::move(path);
  return out;
}

Zone zone(std::string name = "zone") {
  Zone out;
  out.name = std::move(name);
  out.perimeter_xz = {{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f}};
  out.height_mode = ZoneHeightMode::kFinite;
  out.height = 3.0f;
  return out;
}

Path path(std::string name = "path") {
  Path out;
  out.name = std::move(name);
  out.nodes.push_back({});
  out.nodes.push_back({{1.0f, 0.0f, 0.0f}, PathNodeType::kBezier, 100});
  return out;
}

}  // namespace

TEST_SUITE("Level edit API") {
  TEST_CASE("Add operations return exact failures and invalidate their output index") {
    Level level;

    RoomIndex room_index = 42;
    CHECK(level.addRoom({{"", 0}}, room_index) == ARX_LEVEL_BAD_ROOM_NAME);
    CHECK(room_index == kInvalidRoomIndex);
    REQUIRE(level.addRoom({{"room", 4}}, room_index) == ARX_OK);
    CHECK(room_index == 0);

    room_index = 42;
    CHECK(level.addRoom({{"room", 4}}, room_index) == ARX_LEVEL_DUPLICATE_ROOM_NAME);
    CHECK(room_index == kInvalidRoomIndex);

    VertexIndex vertex_index = 42;
    CHECK(level.addVertices(nullptr, 0, vertex_index) == ARX_INVALID_OPTIONS);
    CHECK(vertex_index == kInvalidVertexIndex);
    CHECK(level.addVertex({{-1.0f, 0.0f, 0.0f}}, vertex_index) == ARX_LEVEL_VERTEX_OUT_OF_BOUNDS);
    CHECK(vertex_index == kInvalidVertexIndex);
  }

  TEST_CASE("Sets and clears validated texture image data") {
    Level level = makeLevelWithRoomAndTriangle();
    test::MeshSnapshot mesh;
    REQUIRE(test::copyMesh(level, mesh) == ARX_OK);
    mesh.textures.push_back({"graph/tex.bmp"});
    mesh.faces[0].texture = 0;
    REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);

    std::vector<std::uint8_t> encoded = makeTestBmp();
    REQUIRE(level.setTextureImage(0, {encoded.data(), encoded.size()}) == ARX_OK);
    REQUIRE(test::texture(level, 0).encoded_image == encoded);
    encoded[0] = 0;
    CHECK(test::texture(level, 0).encoded_image != encoded);

    CHECK(level.setTextureImage(0, {}) == ARX_LEVEL_BAD_TEXTURE_IMAGE);
    CHECK(test::texture(level, 0).encoded_image == makeTestBmp());
    CHECK(level.setTextureImage(0, {nullptr, 1}) == ARX_INVALID_DATA_POINTER);
    CHECK(test::texture(level, 0).encoded_image == makeTestBmp());
    const std::array<std::uint8_t, 3> malformed = {1, 2, 3};
    CHECK(level.setTextureImage(0, {malformed.data(), malformed.size()}) == ARX_LEVEL_BAD_TEXTURE_IMAGE);
    CHECK(test::texture(level, 0).encoded_image == makeTestBmp());
    const std::vector<std::uint8_t> another_image = makeTestBmp();
    CHECK(level.setTextureImage(1, {another_image.data(), another_image.size()}) == ARX_INDEX_OUT_OF_RANGE);

    CHECK(level.clearTextureImage(0) == ARX_OK);
    CHECK(test::texture(level, 0).encoded_image.empty());
    CHECK(level.clearTextureImage(1) == ARX_INDEX_OUT_OF_RANGE);
  }

  TEST_CASE("Adds owned textures without discarding dependent authored data") {
    Level level = makeLevelWithRoomAndTriangle();
    REQUIRE(test::setNavSurface(level, navSurface()) == ARX_OK);
    REQUIRE(test::addAnchor(level, anchor(0.0f)) == 0);

    std::string path = "graph/obj3d/textures/stone.bmp";
    std::vector<std::uint8_t> image = makeTestBmp();
    const ArxLevelTextureView submitted = {{path.data(), path.size()}, {image.data(), image.size()}};
    TextureIndex index = 42;
    REQUIRE(level.addTexture(submitted, index) == ARX_OK);
    CHECK(index == 0);

    path[0] = 'x';
    image[0] = 0;
    REQUIRE(level.textureCount() == 1);
    CHECK(test::texture(level, 0).path == "graph/obj3d/textures/stone.bmp");
    CHECK(test::texture(level, 0).encoded_image == makeTestBmp());
    CHECK(test::navSurface(level).has_value());
    CHECK(level.anchorCount() == 1);
    CHECK(level.validateMesh() == ARX_OK);

    index = 42;
    const std::string bad_path = "graph/obj3d/textures/bad__name.bmp";
    CHECK(level.addTexture({{bad_path.data(), bad_path.size()}, {}}, index) == ARX_LEVEL_BAD_TEXTURE_PATH);
    CHECK(index == kNoTexture);
    CHECK(level.textureCount() == 1);
  }

  TEST_CASE("Native bundle bake rejects an empty level name") {
    Level level = makeLevelWithRoomAndTriangle();
    NativeLevelBundle bundle;
    bundle.dlf.scene_path = "unchanged";

    CHECK(level.bakeNativeBundle({.level_name = "", .texture_folder = ""}, bundle) == ARX_DLF_BAD_SCENE_PATH);
    CHECK(bundle.dlf.scene_path == "unchanged");

    dlf::Data dlf;
    dlf.scene_path = "unchanged";
    CHECK(level.bakeNativeDlf({.level_name = ""}, dlf) == ARX_DLF_BAD_SCENE_PATH);
    CHECK(dlf.scene_path == "unchanged");
  }

  TEST_CASE("Native DLF bake validates only scene data and preserves the target FTS offset") {
    Level level = makeLevelWithRoomAndTriangle();
    REQUIRE(test::setPlayerSpawn(level, {{11.0f, 22.0f, 33.0f}, {}}) == ARX_OK);

    Entity authored_entity = entity();
    authored_entity.position = {14.0f, 25.0f, 36.0f};
    REQUIRE(test::addEntity(level, authored_entity) == 0);

    Fog authored_fog;
    authored_fog.position = {17.0f, 28.0f, 39.0f};
    REQUIRE(test::addFog(level, authored_fog) == 0);

    Zone authored_zone = zone();
    authored_zone.reference_y = 32.0f;
    REQUIRE(test::addZone(level, authored_zone) == 0);

    Path authored_path = path();
    authored_path.position = {20.0f, 31.0f, 42.0f};
    REQUIRE(test::addPath(level, authored_path) == 0);

    Vertex outside = test::vertex(level, 0);
    outside.position = {-0.01f, 0.0f, 0.0f};
    REQUIRE(test::setVertex(level, 0, outside) == ARX_OK);
    REQUIRE(level.validateVertices() == ARX_LEVEL_VERTEX_OUT_OF_BOUNDS);

    dlf::Data dlf;
    REQUIRE(level.bakeNativeDlf({.level_name = "level7", .target_fts_offset = {10.0f, 20.0f, 30.0f}}, dlf) == ARX_OK);

    CHECK(dlf.scene_path == "graph/levels/level7");
    CHECK(dlf.player_spawn.position.x == doctest::Approx(1.0f));
    CHECK(dlf.player_spawn.position.y == doctest::Approx(2.0f));
    CHECK(dlf.player_spawn.position.z == doctest::Approx(3.0f));
    REQUIRE(dlf.entities.size() == 1);
    CHECK(dlf.entities[0].position.x == doctest::Approx(4.0f));
    CHECK(dlf.entities[0].position.y == doctest::Approx(5.0f));
    CHECK(dlf.entities[0].position.z == doctest::Approx(6.0f));
    REQUIRE(dlf.fogs.size() == 1);
    CHECK(dlf.fogs[0].position.x == doctest::Approx(7.0f));
    CHECK(dlf.fogs[0].position.y == doctest::Approx(8.0f));
    CHECK(dlf.fogs[0].position.z == doctest::Approx(9.0f));
    REQUIRE(dlf.zones.size() == 1);
    CHECK(dlf.zones[0].position.x == doctest::Approx(-10.0f));
    CHECK(dlf.zones[0].position.y == doctest::Approx(12.0f));
    CHECK(dlf.zones[0].position.z == doctest::Approx(-30.0f));
    REQUIRE(dlf.zones[0].points.size() == 4);
    CHECK(dlf.zones[0].points[1].x == doctest::Approx(2.0f));
    REQUIRE(dlf.paths.size() == 1);
    CHECK(dlf.paths[0].position.x == doctest::Approx(10.0f));
    CHECK(dlf.paths[0].position.y == doctest::Approx(11.0f));
    CHECK(dlf.paths[0].position.z == doctest::Approx(12.0f));
    REQUIRE(dlf.paths[0].nodes.size() == 2);
    CHECK(dlf.paths[0].nodes[1].relative_position.x == doctest::Approx(1.0f));

    Level::NativeDlfBakeOptions explicit_scene_options;
    explicit_scene_options.dlf_scene_path = "graph/levels/custom";
    REQUIRE(level.bakeNativeDlf(explicit_scene_options, dlf) == ARX_OK);
    CHECK(dlf.scene_path == "graph/levels/custom");
  }

  TEST_CASE("Copies independently and swaps complete Level state") {
    Level first = makeLevelWithRoomAndTriangle();
    Level second(first);

    Vertex changed = test::vertex(first, 1);
    changed.position = {2.0f, 0.0f, 0.0f};
    REQUIRE(test::setVertex(first, 1, changed) == ARX_OK);
    CHECK(test::vertex(second, 1).position.x == doctest::Approx(1.0f));

    swap(first, second);
    CHECK(test::vertex(first, 1).position.x == doctest::Approx(1.0f));
    CHECK(test::vertex(second, 1).position.x == doctest::Approx(2.0f));
  }

  TEST_CASE("Reset restores the default editing state") {
    Level level = makeLevelWithRoomAndTriangle();
    REQUIRE(test::addAnchor(level, anchor(0.0f)) == 0);
    REQUIRE(test::setPlayerSpawn(level, {{1.0f, 2.0f, 3.0f}, {}}) == ARX_OK);

    level.reset();

    CHECK(level.vertexCount() == 0);
    CHECK(level.faceCount() == 0);
    CHECK(level.roomCount() == 0);
    CHECK(level.anchorCount() == 0);
    bool has_usable_spawn = true;
    test::playerSpawn(level, &has_usable_spawn);
    CHECK_FALSE(has_usable_spawn);
    CHECK(level.validatePlayerSpawn() == ARX_OK);
    CHECK(level.validate() == ARX_LEVEL_NO_GEOMETRY);
  }

  TEST_CASE("Copies and replaces coherent mesh snapshot") {
    Level level = makeLevelWithRoomAndTriangle();

    test::MeshSnapshot snapshot;
    REQUIRE(test::copyMesh(level, snapshot) == ARX_OK);
    snapshot.textures.push_back({"graph/tex.bmp"});
    snapshot.faces[0].texture = 0;
    snapshot.corner_colors.resize(3, {0.5f, 0.5f, 0.5f});
    REQUIRE(test::replaceMesh(level, snapshot) == ARX_OK);
    REQUIRE(level.bounds().has_value());
    CHECK(level.bounds()->max.x == doctest::Approx(1.0f));

    REQUIRE(test::copyMesh(level, snapshot) == ARX_OK);
    REQUIRE(snapshot.vertices.size() == 3);
    REQUIRE(snapshot.faces.size() == 1);
    snapshot.vertices[0].position.x = 10.0f;
    snapshot.corner_colors = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}, {0.7f, 0.8f, 0.9f}};

    CHECK(test::replaceMesh(level, snapshot) == ARX_OK);
    CHECK(test::vertex(level, 0).position.x == doctest::Approx(10.0f));
    CHECK(level.faceCount() == 1);
    REQUIRE(test::cornerColors(level).size() == 3);
    CHECK(test::cornerColor(level, 0, 1).g == doctest::Approx(0.5f));
    REQUIRE(level.textureCount() == 1);
    CHECK(test::face(level, 0).texture == 0);
    CHECK(level.validateMesh() == ARX_OK);
    REQUIRE(level.bounds().has_value());
    CHECK(level.bounds()->min.x == doctest::Approx(0.0f));
    CHECK(level.bounds()->max.x == doctest::Approx(10.0f));
    REQUIRE(level.referencedBounds().has_value());
    CHECK(level.referencedBounds()->min.x == doctest::Approx(0.0f));
    CHECK(level.referencedBounds()->max.x == doctest::Approx(10.0f));
  }

  TEST_CASE("Rejects incoherent mesh replacement without changing current mesh") {
    Level level = makeLevelWithRoomAndTriangle();

    test::MeshSnapshot snapshot;
    REQUIRE(test::copyMesh(level, snapshot) == ARX_OK);
    snapshot.faces[0].corners[0].vertex = 99;

    CHECK(test::replaceMesh(level, snapshot) == ARX_LEVEL_BAD_FACE_VERTEX);
    CHECK(test::face(level, 0).corners[0].vertex == 0);

    CHECK(test::replaceMesh(level, snapshot) == ARX_LEVEL_BAD_FACE_VERTEX);
    REQUIRE(snapshot.faces.size() == 1);
    CHECK(snapshot.faces[0].corners[0].vertex == 99);
    CHECK(test::face(level, 0).corners[0].vertex == 0);

    REQUIRE(test::copyMesh(level, snapshot) == ARX_OK);
    snapshot.faces.clear();
    snapshot.face_rooms.clear();
    CHECK(test::replaceMesh(level, snapshot) == ARX_LEVEL_NO_GEOMETRY);

    REQUIRE(test::copyMesh(level, snapshot) == ARX_OK);
    snapshot.corner_colors = {{1.1f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}};
    CHECK(test::replaceMesh(level, snapshot) == ARX_LEVEL_BAD_CORNER_COLOR);

    REQUIRE(test::copyMesh(level, snapshot) == ARX_OK);
    snapshot.vertices[0].position.x = std::numeric_limits<float>::infinity();
    snapshot.textures = {""};
    snapshot.faces.clear();
    snapshot.face_rooms.clear();
    CHECK(test::replaceMesh(level, snapshot) == ARX_LEVEL_BAD_VERTEX_POSITION);
    CHECK(test::vertex(level, 0).position.x == doctest::Approx(0.0f));
  }

  TEST_CASE("Level enforces native XZ bounds while Geometry remains reusable") {
    Level level = makeLevelWithRoomAndTriangle();

    CHECK(test::addVertex(level, vertex(-0.01f, 0.0f, 0.0f)) == kInvalidVertexIndex);
    CHECK(test::addVertex(level, vertex(kLevelMaxXZ + 0.01f, 0.0f, 0.0f)) == kInvalidVertexIndex);
    CHECK(level.vertexCount() == 3);

    Vertex outside = test::vertex(level, 0);
    outside.position = {-0.01f, 0.0f, 0.0f};
    REQUIRE(test::setVertex(level, 0, outside) == ARX_OK);
    CHECK_FALSE(level.bounds().has_value());
    CHECK(level.validateVertices() == ARX_LEVEL_VERTEX_OUT_OF_BOUNDS);

    test::MeshSnapshot snapshot;
    REQUIRE(test::copyMesh(level, snapshot) == ARX_OK);
    snapshot.vertices[0].position = {kLevelMaxXZ + 0.01f, 0.0f, 0.0f};
    CHECK(test::replaceMesh(level, snapshot) == ARX_LEVEL_VERTEX_OUT_OF_BOUNDS);
  }

  TEST_CASE("Face edits update room and corner-color collections atomically") {
    Level level = makeLevelWithRoomAndTriangle();
    REQUIRE(test::addRoom(level, {"second"}) == 1);
    REQUIRE(level.setCornerColor(0, 0, {0.1f, 0.2f, 0.3f}) == ARX_OK);
    std::array<RoomDistance, 1> distances = {{{.distance = -1.0f}}};
    REQUIRE(test::replaceRoomDistances(level, distances) == ARX_OK);
    REQUIRE(test::addVertex(level, vertex(1.0f, 0.0f, 1.0f)) == 3);

    Face added = triangle();
    added.corners[0].vertex = 1;
    added.corners[1].vertex = 3;
    added.corners[2].vertex = 2;
    const std::size_t old_faces = level.faceCount();
    CHECK(test::addFace(level, added, 2) == kInvalidFaceIndex);
    CHECK(level.faceCount() == old_faces);
    added.flags = kFaceBitQuad;
    CHECK(test::addFace(level, added, 1) == kInvalidFaceIndex);
    CHECK(level.faceCount() == old_faces);
    added.flags = 0;

    REQUIRE(test::addFace(level, added, 1) == 1);
    REQUIRE(level.faceCount() == 2);
    CHECK(test::faceRoom(level, 1) == 1);
    REQUIRE(test::cornerColors(level).size() == 6);
    CHECK(test::cornerColor(level, 1, 0).r == doctest::Approx(0.5f));
    CHECK(level.roomDistanceCount() == 1);
    CHECK(level.validateMesh() == ARX_OK);

    REQUIRE(level.removeFace(0) == ARX_OK);
    REQUIRE(level.faceCount() == 1);
    CHECK(test::faceRoom(level, 0) == 1);
    REQUIRE(test::cornerColors(level).size() == 3);
    CHECK(test::cornerColor(level, 0, 0).r == doctest::Approx(0.5f));
    CHECK(level.roomDistanceCount() == 1);
    CHECK(level.removeFace(1) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(level.validateMesh() == ARX_OK);
  }

  TEST_CASE("Level mesh replacement rejects flags reserved for other geometry consumers") {
    Level level = makeLevelWithRoomAndTriangle();
    test::MeshSnapshot snapshot;
    REQUIRE(test::copyMesh(level, snapshot) == ARX_OK);
    snapshot.faces[0].flags = kFaceBitQuad;

    CHECK(test::replaceMesh(level, snapshot) == ARX_LEVEL_BAD_FACE_TYPE);
    CHECK(test::face(level, 0).flags == 0);
  }

  TEST_CASE("Compaction removes unreferenced vertices and refreshes bounds") {
    Level level = makeLevelWithRoomAndTriangle();
    REQUIRE(test::addVertex(level, vertex(100.0f, 10.0f, 100.0f)) == 3);
    REQUIRE(level.bounds().has_value());
    CHECK(level.bounds()->max.x == doctest::Approx(100.0f));

    std::size_t removed = 0;
    REQUIRE(level.compactVertices(&removed) == ARX_OK);
    CHECK(removed == 1);
    CHECK(level.vertexCount() == 3);
    REQUIRE(level.bounds().has_value());
    CHECK(level.bounds()->max.x == doctest::Approx(1.0f));
    CHECK(level.validateMesh() == ARX_OK);
  }

  TEST_CASE("Texture compaction removes unused textures and preserves referenced images") {
    Level level = makeLevelWithRoomAndTriangle();
    std::vector<std::uint8_t> second_image = makeTestBmp();
    second_image.back() ^= 0xffU;
    const std::string unused_path = "graph/obj3d/textures/unused.bmp";
    const std::string retained_path = "graph/obj3d/textures/retained.bmp";

    TextureIndex unused = kNoTexture;
    REQUIRE(level.addTexture({{unused_path.data(), unused_path.size()}, {}}, unused) == ARX_OK);
    REQUIRE(unused == 0);
    TextureIndex retained = kNoTexture;
    REQUIRE(level.addTexture({{retained_path.data(), retained_path.size()}, {second_image.data(), second_image.size()}},
                             retained) == ARX_OK);
    REQUIRE(retained == 1);

    Face face = test::face(level, 0);
    face.texture = retained;
    REQUIRE(test::setFace(level, 0, face) == ARX_OK);

    std::size_t removed = 0;
    REQUIRE(level.compactTextures(&removed) == ARX_OK);
    CHECK(removed == 1);
    REQUIRE(level.textureCount() == 1);
    CHECK(test::face(level, 0).texture == 0);
    CHECK(test::texture(level, 0).path == "graph/obj3d/textures/retained.bmp");
    CHECK(test::texture(level, 0).encoded_image == second_image);
    CHECK(level.validateMesh() == ARX_OK);
  }

  TEST_CASE("Welding discarded faces remaps rooms and corner colors") {
    Level level;
    REQUIRE(test::addRoom(level, {"first"}) == 0);
    REQUIRE(test::addRoom(level, {"second"}) == 1);

    test::MeshSnapshot mesh;
    mesh.vertices = {
        vertex(0.0f, 0.0f, 0.0f), vertex(0.1f, 0.0f, 0.1f), vertex(1.0f, 0.0f, 0.0f), vertex(0.0f, 0.0f, 1.0f)};
    Face first = triangle();
    Face second = triangle();
    second.corners[0].vertex = 1;
    second.corners[1].vertex = 2;
    second.corners[2].vertex = 3;
    mesh.faces = {first, second};
    mesh.textures = {"graph/obj3d/textures/weld.bmp"};
    mesh.faces[0].texture = 0;
    mesh.faces[1].texture = 0;
    mesh.face_rooms = {0, 1};
    mesh.corner_colors = {{0.1f, 0.1f, 0.1f},
                          {0.2f, 0.2f, 0.2f},
                          {0.3f, 0.3f, 0.3f},
                          {0.4f, 0.4f, 0.4f},
                          {0.5f, 0.5f, 0.5f},
                          {0.6f, 0.6f, 0.6f}};
    REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(level.setTextureImage(0, {image.data(), image.size()}) == ARX_OK);
    Portal distance_portal = portal(0, 1);
    for (ArxVector3& position : distance_portal.vertices) {
      position.x += 100.0f;
      position.z += 100.0f;
    }
    REQUIRE(test::addPortal(level, distance_portal) == 0);
    std::array<RoomDistance, 1> distances = {{{.distance = 10.0f, .low_room_portal = 0, .high_room_portal = 0}}};
    REQUIRE(test::replaceRoomDistances(level, distances) == ARX_OK);

    Level::VertexWeldOptions options;
    options.radius = 0.2f;
    options.degenerate_faces = Level::DegenerateFacePolicy::kDiscard;
    REQUIRE(level.weldVertices(options) == ARX_OK);

    REQUIRE(level.faceCount() == 1);
    CHECK(test::faceRoom(level, 0) == 1);
    REQUIRE(test::cornerColors(level).size() == 3);
    CHECK(test::cornerColor(level, 0, 0).r == doctest::Approx(0.4f));
    CHECK(test::cornerColor(level, 0, 2).r == doctest::Approx(0.6f));
    REQUIRE(level.roomDistanceCount() == 1);
    CHECK(test::roomDistances(level)[0].distance == doctest::Approx(10.0f));
    CHECK(test::texture(level, 0).path == "graph/obj3d/textures/weld.bmp");
    CHECK(test::texture(level, 0).encoded_image == makeTestBmp());
    CHECK(level.validateMesh() == ARX_OK);
  }

  TEST_CASE("Welding protects vertices shared by rooms") {
    Level level;
    REQUIRE(test::addRoom(level, {"first"}) == 0);
    REQUIRE(test::addRoom(level, {"second"}) == 1);

    test::MeshSnapshot mesh;
    mesh.vertices = {vertex(1.0f, 0.0f, 1.0f),
                     vertex(1.05f, 0.0f, 1.0f),
                     vertex(2.0f, 0.0f, 1.0f),
                     vertex(1.0f, 0.0f, 2.0f),
                     vertex(2.0f, 0.0f, 2.0f),
                     vertex(3.0f, 0.0f, 1.0f)};
    Face first = triangle();
    first.corners[0].vertex = 0;
    first.corners[1].vertex = 2;
    first.corners[2].vertex = 3;
    Face nearby = triangle();
    nearby.corners[0].vertex = 1;
    nearby.corners[1].vertex = 4;
    nearby.corners[2].vertex = 5;
    Face other_room = triangle();
    other_room.corners[0].vertex = 0;
    other_room.corners[1].vertex = 3;
    other_room.corners[2].vertex = 4;
    mesh.faces = {first, nearby, other_room};
    mesh.face_rooms = {0, 0, 1};
    REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);

    Level::VertexWeldOptions options;
    options.radius = 0.1f;
    REQUIRE(level.weldVertices(options) == ARX_OK);

    const VertexIndex shared = test::face(level, 0).corners[0].vertex;
    CHECK(test::face(level, 1).corners[0].vertex == shared);
    CHECK(test::face(level, 2).corners[0].vertex == shared);
    CHECK(test::vertex(level, shared).position.x == doctest::Approx(1.0f));
    CHECK(level.vertexCount() == 5);
    CHECK(level.validateMesh() == ARX_OK);
  }

  TEST_CASE("Welding preserves faces that would become degenerate by default") {
    Level level;
    REQUIRE(test::addRoom(level, {"room"}) == 0);

    test::MeshSnapshot mesh;
    mesh.vertices = {vertex(0.0f, 0.0f, 0.0f), vertex(0.05f, 0.0f, 0.0f), vertex(0.0f, 0.0f, 1.0f)};
    mesh.faces = {triangle()};
    mesh.face_rooms = {0};
    REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);

    Level::VertexWeldOptions options;
    options.radius = 0.1f;
    REQUIRE(level.weldVertices(options) == ARX_OK);

    REQUIRE(level.faceCount() == 1);
    CHECK(level.vertexCount() == 3);
    CHECK(test::face(level, 0).corners[0].vertex != test::face(level, 0).corners[1].vertex);
    CHECK(level.validateMesh() == ARX_OK);
  }

  TEST_CASE("Vertex edits eagerly invalidate cached face validity") {
    Level level = makeLevelWithRoomAndTriangle();
    REQUIRE(level.validate() == ARX_OK);

    Vertex moved = test::vertex(level, 2);
    moved.position = {2.0f, 0.0f, 0.0f};
    REQUIRE(test::setVertex(level, 2, moved) == ARX_OK);
    CHECK(level.validateVertices() == ARX_OK);
    CHECK(level.validateFaces() == ARX_LEVEL_DEGENERATE_FACE);

    moved.position = {0.0f, 0.0f, 1.0f};
    REQUIRE(test::setVertex(level, 2, moved) == ARX_OK);
    CHECK(level.validateFaces() == ARX_OK);
  }

  TEST_CASE("Locally complete item edits reject invalid values") {
    Level level = makeLevelWithRoomAndTriangle();
    test::MeshSnapshot mesh;
    REQUIRE(test::copyMesh(level, mesh) == ARX_OK);
    mesh.textures.push_back({"graph/tex.bmp"});
    mesh.faces[0].texture = 0;
    REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);
    REQUIRE(level.validate() == ARX_OK);

    CHECK(test::setTexture(level, 0, {""}) == ARX_LEVEL_BAD_TEXTURE_PATH);
    CHECK(test::setTexture(level, 0, {"graph/bad__texture.bmp"}) == ARX_LEVEL_BAD_TEXTURE_PATH);
    CHECK(test::texture(level, 0).path == "graph/tex.bmp");
    REQUIRE(test::setTexture(level, 0, {"graph/fixed.bmp"}) == ARX_OK);
    CHECK(level.validateFaces() == ARX_OK);

    CHECK(test::setRoom(level, 0, {""}) == ARX_LEVEL_BAD_ROOM_NAME);
    CHECK(test::setRoom(level, 0, {"room__one"}) == ARX_LEVEL_BAD_ROOM_NAME);
    CHECK(test::room(level, 0).name == "room");
    CHECK(test::addRoom(level, {"room"}) == kInvalidRoomIndex);
    CHECK(test::addRoom(level, {"room__two"}) == kInvalidRoomIndex);
    REQUIRE(test::addRoom(level, {"second"}) == 1);
    CHECK(test::setRoom(level, 1, {"room"}) == ARX_LEVEL_DUPLICATE_ROOM_NAME);
    REQUIRE(test::setRoom(level, 1, {"renamed"}) == ARX_OK);
    CHECK(level.validateFaceRooms() == ARX_OK);
  }

  TEST_CASE("LevelAllowsAtMost254Rooms") {
    Level level;
    for (std::size_t i = 0; i < 254; ++i) {
      const std::string name = "room_" + std::to_string(i);
      RoomIndex index = kInvalidRoomIndex;
      REQUIRE(level.addRoom({{name.data(), name.size()}}, index) == ARX_OK);
      REQUIRE(index == i);
    }
    CHECK(level.validateRooms() == ARX_OK);

    constexpr char kOverflowName[] = "overflow";
    RoomIndex index = kInvalidRoomIndex;
    CHECK(level.addRoom({{kOverflowName, sizeof(kOverflowName) - 1U}}, index) == ARX_LEVEL_TOO_MANY_ROOMS);
    CHECK(index == kInvalidRoomIndex);
    CHECK(level.roomCount() == 254);
  }

  TEST_CASE("Portal edits validate complete definitions") {
    Level level = makeLevelWithRoomAndTriangle();
    REQUIRE(test::addRoom(level, {"second"}) == 1);
    REQUIRE(test::addPortal(level, portal(0, 1)) == 0);
    REQUIRE(level.validatePortals() == ARX_OK);
    CHECK(test::addPortal(level, portal(0, 1)) == kInvalidPortalIndex);

    Portal invalid = portal(0, 1);
    invalid.name = "portal__one";
    CHECK(test::setPortal(level, 0, invalid) == ARX_LEVEL_BAD_PORTAL_NAME);
    CHECK(test::addPortal(level, invalid) == kInvalidPortalIndex);
    CHECK(test::portal(level, 0).name == "portal");

    invalid = portal(0, 1);
    invalid.vertices[2] = invalid.vertices[1];
    CHECK(test::setPortal(level, 0, invalid) == ARX_LEVEL_BAD_PORTAL_VERTEX);

    invalid = portal(0, 1);
    invalid.room_2 = 2;
    CHECK(test::setPortal(level, 0, invalid) == ARX_LEVEL_BAD_PORTAL_ROOM);

    invalid = portal(0, 1);
    invalid.vertices[0].x = -0.01f;
    CHECK(test::setPortal(level, 0, invalid) == ARX_LEVEL_PORTAL_OUT_OF_BOUNDS);
    CHECK(test::addPortal(level, invalid) == kInvalidPortalIndex);

    Portal replacement = portal(0, 1);
    replacement.name = "door";
    CHECK(test::setPortal(level, 0, replacement) == ARX_OK);
    CHECK(test::portal(level, 0).name == "door");
    CHECK(level.validatePortals() == ARX_OK);
  }

  TEST_CASE("Bounds validation is independent from face validity") {
    Level level = makeLevelWithRoomAndTriangle();
    REQUIRE(level.validate() == ARX_OK);

    Face invalid = test::face(level, 0);
    invalid.corners[0].normal = {};
    REQUIRE(test::setFace(level, 0, invalid) == ARX_OK);

    REQUIRE(level.bounds().has_value());
    CHECK(level.bounds()->max.x == doctest::Approx(1.0f));
    CHECK_FALSE(level.referencedBounds().has_value());
  }

  TEST_CASE("Referenced bounds changes preserve cached anchor validity") {
    Level level = makeLevelWithRoomAndTriangle();
    Anchor authored;
    authored.position = {0.75f, 0.0f, 0.0f};
    REQUIRE(test::addAnchor(level, authored) == 0);
    REQUIRE(level.validateAnchors() == ARX_OK);

    Vertex moved = test::vertex(level, 1);
    moved.position.x = 0.5f;
    REQUIRE(test::setVertex(level, 1, moved) == ARX_OK);
    CHECK(level.validateAnchors() == ARX_OK);
  }

  TEST_CASE("Anchor validation uses native XZ bounds instead of referenced geometry") {
    Level level = makeLevelWithRoomAndTriangle();
    Anchor authored;
    authored.position = {2.0f, 0.0f, 0.0f};

    REQUIRE(test::addAnchor(level, authored) == 0);
    CHECK(level.validateAnchors() == ARX_OK);

    authored.position.x = -0.01f;
    REQUIRE(test::setAnchor(level, 0, authored) == ARX_OK);
    CHECK(level.validateAnchors() == ARX_LEVEL_ANCHOR_OUT_OF_BOUNDS);
  }

  TEST_CASE("Strict interior vertex updates expand cached full bounds") {
    Level level;
    REQUIRE(test::addRoom(level, {"room"}) == 0);
    test::MeshSnapshot mesh;
    mesh.vertices = {vertex(0.0f, 0.0f, 0.0f),
                     vertex(10.0f, 0.0f, 0.0f),
                     vertex(0.0f, 0.0f, 10.0f),
                     vertex(0.0f, -10.0f, 0.0f),
                     vertex(10.0f, 10.0f, 10.0f),
                     vertex(5.0f, 5.0f, 5.0f)};
    mesh.faces = {triangle()};
    mesh.face_rooms = {0};
    REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);
    REQUIRE(level.bounds().has_value());

    Vertex moved = test::vertex(level, 5);
    moved.position.x = 20.0f;
    REQUIRE(test::setVertex(level, 5, moved) == ARX_OK);
    REQUIRE(level.bounds().has_value());
    CHECK(level.bounds()->max.x == doctest::Approx(20.0f));
  }

  TEST_CASE("Clears mesh and generated dependent data explicitly") {
    Level level = makeLevelWithRoomAndTriangle();
    CHECK(test::addRoom(level, {"second"}) == 1);
    CHECK(level.setCornerColor(0, 0, {0.5f, 0.5f, 0.5f}) == ARX_OK);
    std::array<RoomDistance, 1> distances = {{{.distance = -1.0f}}};
    CHECK(test::replaceRoomDistances(level, distances) == ARX_OK);
    CHECK(test::setNavSurface(level, navSurface()) == ARX_OK);
    CHECK(test::addAnchor(level, anchor(0.0f)) == 0);
    CHECK(test::addAnchor(level, anchor(1.0f)) == 1);
    CHECK(test::addAnchorConnection(level, {0, 1}) == 0);
    REQUIRE(level.bounds().has_value());

    level.clearMesh();

    CHECK(level.vertexCount() == 0);
    CHECK(level.faceCount() == 0);
    CHECK(test::cornerColors(level).empty());
    CHECK_FALSE(test::navSurface(level).has_value());
    CHECK(level.anchorCount() == 0);
    CHECK(level.anchorConnectionCount() == 0);
    CHECK(level.roomDistanceCount() == 1);
    CHECK(level.validateMesh() == ARX_LEVEL_NO_GEOMETRY);
    CHECK_FALSE(level.bounds().has_value());
    CHECK_FALSE(level.referencedBounds().has_value());
  }

  TEST_CASE("Room removal rejects referenced rooms and remaps portals") {
    Level level = makeLevelWithRoomAndTriangle();
    CHECK(test::addRoom(level, {"middle"}) == 1);
    CHECK(test::addRoom(level, {"last"}) == 2);
    CHECK(level.setFaceRoom(0, 2) == ARX_OK);
    CHECK(test::faceRoom(level, 0) == 2);
    CHECK(level.setFaceRoom(0, 0) == ARX_OK);
    CHECK(level.faceCount() == 1);
    Portal first_portal = portal(0, 1);
    first_portal.name = "first";
    Portal second_portal = portal(1, 2);
    second_portal.name = "second";
    Portal third_portal = portal(0, 2);
    third_portal.name = "third";
    CHECK(test::addPortal(level, first_portal) == 0);
    CHECK(test::addPortal(level, second_portal) == 1);
    CHECK(test::addPortal(level, third_portal) == 2);
    std::array<RoomDistance, 3> distances = {{{.distance = -1.0f, .low_room_portal = 0, .high_room_portal = 0},
                                              {.distance = -1.0f, .low_room_portal = 2, .high_room_portal = 2},
                                              {.distance = -1.0f, .low_room_portal = 1, .high_room_portal = 1}}};
    CHECK(test::replaceRoomDistances(level, distances) == ARX_OK);

    CHECK(level.removeRoom(0) == ARX_LEVEL_BAD_FACE_ROOM_INDEX);
    REQUIRE(level.roomCount() == 3);

    CHECK(level.removeRoom(1) == ARX_OK);
    REQUIRE(level.roomCount() == 2);
    REQUIRE(level.portalCount() == 1);
    CHECK(test::portal(level, 0).room_1 == 0);
    CHECK(test::portal(level, 0).room_2 == 1);
    CHECK(level.roomDistanceCount() == 0);
  }

  TEST_CASE("Room distances follow room and portal topology") {
    Level level = makeLevelWithRoomAndTriangle();
    REQUIRE(test::addRoom(level, {"second"}) == 1);
    REQUIRE(test::addPortal(level, portal(0, 1)) == 0);
    std::array<RoomDistance, 1> distances = {{{.distance = 10.0f, .low_room_portal = 0, .high_room_portal = 0}}};
    REQUIRE(test::replaceRoomDistances(level, distances) == ARX_OK);

    Portal renamed = portal(0, 1);
    renamed.name = "renamed";
    REQUIRE(test::setPortal(level, 0, renamed) == ARX_OK);
    CHECK(level.roomDistanceCount() == 1);

    Portal moved = renamed;
    for (ArxVector3& vertex : moved.vertices) vertex.x += 1.0f;
    REQUIRE(test::setPortal(level, 0, moved) == ARX_OK);
    CHECK(level.roomDistanceCount() == 0);

    REQUIRE(test::replaceRoomDistances(level, distances) == ARX_OK);
    REQUIRE(test::addRoom(level, {"third"}) == 2);
    CHECK(level.roomDistanceCount() == 0);
  }

  TEST_CASE("Room distance API preserves canonical room pairs") {
    Level level = makeLevelWithRoomAndTriangle();
    CHECK(test::addRoom(level, {"second"}) == 1);
    CHECK(test::addRoom(level, {"third"}) == 2);
    Portal first_portal = portal(0, 1);
    first_portal.name = "first";
    Portal second_portal = portal(0, 2);
    second_portal.name = "second";
    Portal third_portal = portal(1, 2);
    third_portal.name = "third";
    CHECK(test::addPortal(level, first_portal) == 0);
    CHECK(test::addPortal(level, second_portal) == 1);
    CHECK(test::addPortal(level, third_portal) == 2);
    std::array<RoomDistance, 3> distances = {{{.distance = 10.0f, .low_room_portal = 0, .high_room_portal = 0},
                                              {.distance = 20.0f, .low_room_portal = 1, .high_room_portal = 1},
                                              {.distance = 30.0f, .low_room_portal = 2, .high_room_portal = 2}}};

    CHECK(test::replaceRoomDistances(level, distances) == ARX_OK);
    REQUIRE(level.roomDistanceCount() == 3);
    REQUIRE(test::roomDistance(level, 2, 0).has_value());
    CHECK(test::roomDistance(level, 2, 0)->distance == doctest::Approx(20.0f));
    CHECK_FALSE(test::roomDistance(level, 0, 0).has_value());
    CHECK_FALSE(test::roomDistance(level, 0, 3).has_value());

    const std::array<ArxLevelRoomDistance, 3> reversed = {{
        {1, 0, 10.0f, 2, 1},
        {2, 0, 20.0f, 2, 0},
        {2, 1, 30.0f, 1, 0},
    }};
    CHECK(level.replaceRoomDistances(reversed.data(), reversed.size()) == ARX_OK);
    REQUIRE(test::roomDistance(level, 0, 1).has_value());
    CHECK(test::roomDistance(level, 0, 1)->portal_a == 1);
    CHECK(test::roomDistance(level, 0, 1)->portal_b == 2);

    CHECK(level.setRoomDistance({2, 0, 40.0f, 2, 0}) == ARX_OK);
    REQUIRE(test::roomDistance(level, 0, 2).has_value());
    CHECK(test::roomDistance(level, 0, 2)->distance == doctest::Approx(40.0f));
    CHECK(test::roomDistance(level, 0, 2)->portal_a == 0);
    CHECK(test::roomDistance(level, 0, 2)->portal_b == 2);
    CHECK(level.setRoomDistance({0, 2, 40.0f, 0, 0}) == ARX_LEVEL_BAD_ROOM_DISTANCE);

    std::array<RoomDistance, 1> incomplete = {{{.distance = 10.0f}}};
    CHECK(test::replaceRoomDistances(level, incomplete) == ARX_LEVEL_BAD_ROOM_DISTANCE_COUNT);
    REQUIRE(level.roomDistanceCount() == 3);

    level.clearRoomDistances();
    CHECK(level.roomDistanceCount() == 0);
    CHECK_FALSE(test::roomDistance(level, 0, 1).has_value());
  }

  TEST_CASE("Anchor edits keep connections sorted and remapped") {
    Level level;
    CHECK(test::addAnchor(level, anchor(0.0f)) == 0);
    CHECK(test::addAnchor(level, anchor(1.0f)) == 1);
    CHECK(test::addAnchor(level, anchor(2.0f)) == 2);
    CHECK(test::addAnchorConnection(level, {1, 2}) == 0);
    CHECK(test::addAnchorConnection(level, {0, 2}) == 0);
    CHECK(test::addAnchorConnection(level, {0, 2}) == 0);
    REQUIRE(level.anchorConnectionCount() == 2);
    CHECK(test::anchorConnection(level, 0).first == 0);
    CHECK(test::anchorConnection(level, 0).second == 2);
    CHECK(test::anchorConnection(level, 1).first == 1);
    CHECK(test::anchorConnection(level, 1).second == 2);

    CHECK(level.removeAnchor(1) == ARX_OK);
    REQUIRE(level.anchorCount() == 2);
    REQUIRE(level.anchorConnectionCount() == 1);
    CHECK(test::anchorConnection(level, 0).first == 0);
    CHECK(test::anchorConnection(level, 0).second == 1);

    Anchor invalid = anchor(3.0f);
    invalid.radius = -1.0f;
    CHECK(test::addAnchor(level, invalid) == kInvalidAnchorIndex);
    invalid = test::anchor(level, 0);
    invalid.height = 1.0f;
    CHECK(test::setAnchor(level, 0, invalid) == ARX_LEVEL_BAD_ANCHOR_HEIGHT);
    CHECK(test::anchor(level, 0).height == doctest::Approx(kDefaultAnchorHeight));

    invalid = test::anchor(level, 0);
    invalid.name = "anchor__one";
    CHECK(test::setAnchor(level, 0, invalid) == ARX_LEVEL_BAD_ANCHOR_NAME);

    Anchor named = anchor(3.0f);
    named.name = "marker";
    CHECK(test::addAnchor(level, named) == 2);
    CHECK(test::addAnchor(level, named) == kInvalidAnchorIndex);
    named.name.clear();
    CHECK(test::addAnchor(level, named) == 3);
    CHECK(test::addAnchor(level, named) == 4);
  }

  TEST_CASE("Bulk anchor replacement rejects broken connection packages") {
    Level level;
    test::AnchorsSnapshot snapshot;
    snapshot.anchors = {anchor(0.0f), anchor(1.0f), anchor(2.0f)};
    snapshot.connections = {{0, 2}, {1, 2}};

    CHECK(test::replaceAnchors(level, snapshot) == ARX_OK);
    CHECK(level.anchorCount() == 3);

    REQUIRE(test::copyAnchors(level, snapshot) == ARX_OK);

    snapshot.connections = {{1, 2}, {0, 2}};
    CHECK(test::replaceAnchors(level, snapshot) == ARX_LEVEL_BAD_ANCHOR_CONNECTION_ORDER);
    CHECK(snapshot.connections.size() == 2);
    CHECK(test::anchorConnection(level, 0).first == 0);

    snapshot.connections = {{0, 3}};
    CHECK(test::replaceAnchors(level, snapshot) == ARX_LEVEL_BAD_ANCHOR_CONNECTION_INDEX);

    snapshot.connections = {{0, 2}, {1, 2}};
    snapshot.anchors[0].flags = kAnchorFlagsAll | 0x40;
    CHECK(test::replaceAnchors(level, snapshot) == ARX_LEVEL_BAD_ANCHOR_FLAGS);
    CHECK(level.anchorCount() == 3);
  }

  TEST_CASE("Nav surface pointer reflects optional surface state") {
    Level level;
    CHECK_FALSE(test::navSurface(level).has_value());

    NavSurface surface = navSurface();
    CHECK(test::setNavSurface(level, surface) == ARX_OK);
    REQUIRE(test::navSurface(level).has_value());
    CHECK(test::navSurface(level)->triangles.size() == 1);

    surface = navSurface();
    surface.triangles[0].vertices[2] = 99;
    CHECK(test::setNavSurface(level, surface) == ARX_LEVEL_BAD_NAV_SURFACE_TRIANGLE);
    CHECK(surface.triangles.size() == 1);
    REQUIRE(test::navSurface(level).has_value());
    CHECK(test::navSurface(level)->triangles[0].vertices[2] == 2);

    level.clearNavSurface();
    CHECK_FALSE(test::navSurface(level).has_value());
  }

  TEST_CASE("Corner color edits use current mesh shape") {
    Level level = makeLevelWithRoomAndTriangle();
    CHECK(test::cornerColors(level).empty());
    CHECK(test::cornerColor(level, 0, 0).r == doctest::Approx(0.5f));

    CHECK(level.setCornerColor(0, 1, {0.1f, 0.2f, 0.3f}) == ARX_OK);
    REQUIRE(test::cornerColors(level).size() == 3);
    CHECK(test::cornerColor(level, 0, 1).g == doctest::Approx(0.2f));
    CHECK(test::cornerColor(level, 0, 0).r == doctest::Approx(0.5f));

    CHECK(level.setCornerColor(1, 0, {0.1f, 0.2f, 0.3f}) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(level.setCornerColor(0, 3, {0.1f, 0.2f, 0.3f}) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(level.setCornerColor(0, 0, {1.1f, 0.0f, 0.0f}) == ARX_LEVEL_BAD_CORNER_COLOR);
    CHECK(test::cornerColor(level, 0, 1).g == doctest::Approx(0.2f));

    level.clearCornerColors();
    CHECK(test::cornerColors(level).empty());
  }

  TEST_CASE("Light edits validate individual lights") {
    Level level;
    CHECK(test::addLight(level, light()) == 0);
    CHECK(test::addLight(level, light()) == kInvalidLightIndex);
    REQUIRE(level.lightCount() == 1);
    CHECK(test::light(level, 0).name == "light");

    Light replacement = light("torch");
    CHECK(test::setLight(level, 0, replacement) == ARX_OK);
    CHECK(test::light(level, 0).name == "torch");

    replacement.color.r = -0.1f;
    CHECK(test::setLight(level, 0, replacement) == ARX_LEVEL_BAD_LIGHT_COLOR);
    CHECK(test::light(level, 0).name == "torch");

    replacement = light("torch__one");
    CHECK(test::setLight(level, 0, replacement) == ARX_LEVEL_BAD_LIGHT_NAME);

    CHECK(test::addLight(level, Light{}) == kInvalidLightIndex);
    CHECK(test::addLight(level, light("light__one")) == kInvalidLightIndex);
    CHECK(level.removeLight(2) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(level.removeLight(0) == ARX_OK);
    CHECK(level.lightCount() == 0);
  }

  TEST_CASE("Player spawn is mandatory in the Level API") {
    Level level;
    bool has_usable_spawn = true;
    CHECK(test::playerSpawn(level, &has_usable_spawn).position.x == doctest::Approx(0.0f));
    CHECK_FALSE(has_usable_spawn);
    CHECK(level.validatePlayerSpawn() == ARX_OK);

    PlayerSpawn spawn;
    spawn.position = {1.0f, 2.0f, 3.0f};
    spawn.rotation = {2.0f, 0.0f, 0.0f, 0.0f};
    CHECK(test::setPlayerSpawn(level, spawn) == ARX_OK);
    CHECK(test::playerSpawn(level, &has_usable_spawn).position.y == doctest::Approx(2.0f));
    CHECK(has_usable_spawn);
    CHECK(test::playerSpawn(level).rotation.w == doctest::Approx(1.0f));

    spawn.rotation.x = std::numeric_limits<float>::infinity();
    CHECK(test::setPlayerSpawn(level, spawn) == ARX_LEVEL_BAD_PLAYER_SPAWN);
    CHECK(test::playerSpawn(level, &has_usable_spawn).position.y == doctest::Approx(2.0f));
    CHECK(has_usable_spawn);

    level.clearPlayerSpawn();
    CHECK(test::playerSpawn(level, &has_usable_spawn).position.x == doctest::Approx(0.0f));
    CHECK_FALSE(has_usable_spawn);
    CHECK(test::playerSpawn(level).rotation.w == doctest::Approx(1.0f));

    spawn.position = {};
    spawn.rotation = {};
    CHECK(test::setPlayerSpawn(level, spawn) == ARX_OK);
    test::playerSpawn(level, &has_usable_spawn);
    CHECK(has_usable_spawn);
  }

  TEST_CASE("Entity edits derive stable unique authoring names") {
    Level level;
    Entity spider = entity("graph/obj3d/interactive/npc/spider_base/spider_base");
    REQUIRE(test::addEntity(level, spider) == 0);
    CHECK(test::entity(level, 0).name == "spider");

    Entity reserved = entity("graph/obj3d/interactive/fix_inter/marker/marker");
    reserved.name = "spider_1";
    REQUIRE(test::addEntity(level, reserved) == 1);
    REQUIRE(test::addEntity(level, spider) == 2);
    CHECK(test::entity(level, 1).name == "spider_1");
    CHECK(test::entity(level, 2).name == "spider_2");

    Entity replacement = entity("graph/obj3d/interactive/npc/human_base/human_base");
    REQUIRE(test::setEntity(level, 0, replacement) == ARX_OK);
    CHECK(test::entity(level, 0).name == "human");
    replacement.name = "human";
    REQUIRE(test::setEntity(level, 1, replacement) == ARX_OK);
    CHECK(test::entity(level, 1).name == "human_1");

    Entity authored = entity("graph/obj3d/interactive/fix_inter/marker/marker");
    authored.name = "human_base";
    REQUIRE(test::addEntity(level, authored) == 3);
    CHECK(test::entity(level, 3).name == "human_base");
    CHECK(level.validateEntities() == ARX_OK);
  }

  TEST_CASE("Scene collection edits validate module invariants") {
    Level level;
    Entity normalized_entity = entity();
    normalized_entity.rotation.w = 2.0f;
    CHECK(test::addEntity(level, normalized_entity) == 0);
    REQUIRE(level.entityCount() == 1);
    CHECK(test::entity(level, 0).class_path == "graph/obj3d/interactive/items/torch");
    CHECK(test::entity(level, 0).name == "torch");
    CHECK(test::entity(level, 0).rotation.w == doctest::Approx(1.0f));

    Entity bad_entity = entity("Graph\\Obj3D\\Interactive\\Items\\Torch.teo");
    CHECK(test::setEntity(level, 0, bad_entity) == ARX_LEVEL_BAD_ENTITY_CLASS_PATH);
    CHECK(test::entity(level, 0).class_path == "graph/obj3d/interactive/items/torch");
    CHECK(test::addEntity(level, bad_entity) == kInvalidEntityIndex);
    CHECK(test::addEntity(level, entity("graph/obj3d/interactive/items/torch__lit")) == kInvalidEntityIndex);
    bad_entity = entity();
    bad_entity.name = "entity__one";
    CHECK(test::setEntity(level, 0, bad_entity) == ARX_LEVEL_BAD_ENTITY_NAME);
    CHECK(level.removeEntity(2) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(level.removeEntity(0) == ARX_OK);

    Fog fog;
    fog.position = {1.0f, 2.0f, 3.0f};
    fog.rotation.w = 2.0f;
    CHECK(test::addFog(level, fog) == 0);
    CHECK(test::addFog(level, fog) == 1);
    CHECK(test::fog(level, 0).rotation.w == doctest::Approx(1.0f));
    fog.position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(test::setFog(level, 0, fog) == ARX_LEVEL_BAD_FOG_POSITION);
    fog.position.x = 1.0f;
    fog.name = "fog__one";
    CHECK(test::setFog(level, 0, fog) == ARX_LEVEL_BAD_FOG_NAME);
    fog.name = "mist";
    CHECK(test::setFog(level, 0, fog) == ARX_OK);
    CHECK(test::setFog(level, 1, fog) == ARX_LEVEL_DUPLICATE_FOG_NAME);
    CHECK(level.removeFog(1) == ARX_OK);
    CHECK(level.removeFog(0) == ARX_OK);

    CHECK(test::addZone(level, zone()) == 0);
    CHECK(test::addZone(level, zone("ZONE")) == kInvalidZoneIndex);
    Zone bad_zone = zone("");
    CHECK(test::setZone(level, 0, bad_zone) == ARX_LEVEL_BAD_ZONE_NAME);
    CHECK(test::addZone(level, zone("zone__one")) == kInvalidZoneIndex);
    bad_zone = zone("zone_two");
    bad_zone.ambiance = ZoneAmbiance{"ambient__cave", 100.0f};
    CHECK(test::setZone(level, 0, bad_zone) == ARX_LEVEL_BAD_ZONE_AMBIANCE);
    CHECK(test::zone(level, 0).name == "zone");
    CHECK(level.removeZone(0) == ARX_OK);

    CHECK(test::addPath(level, path()) == 0);
    CHECK(test::addPath(level, path()) == kInvalidPathIndex);
    CHECK(test::addPath(level, path("PATH")) == kInvalidPathIndex);
    Path bad_path = path("");
    CHECK(test::setPath(level, 0, bad_path) == ARX_LEVEL_BAD_PATH_NAME);
    CHECK(test::addPath(level, path("path__one")) == kInvalidPathIndex);
    CHECK(test::addPath(level, path("path_two")) == 1);
    CHECK(test::setPath(level, 1, path()) == ARX_LEVEL_DUPLICATE_PATH_NAME);
    CHECK(test::setPath(level, 0, path()) == ARX_OK);
    CHECK(test::path(level, 0).name == "path");
    CHECK(test::path(level, 1).name == "path_two");
    CHECK(level.removePath(1) == ARX_OK);
    CHECK(level.removePath(0) == ARX_OK);
  }

  TEST_CASE("Shared projection enums reject values outside their declared domains") {
    Level level;
    RoomIndex room_index = kInvalidRoomIndex;
    REQUIRE(level.addRoom({{"first", 5}}, room_index) == ARX_OK);
    REQUIRE(level.addRoom({{"second", 6}}, room_index) == ARX_OK);

    ArxLevelPortal projected_portal{};
    projected_portal.name = {"portal", 6};
    projected_portal.room_1 = 0;
    projected_portal.room_2 = 1;
    projected_portal.shape = ARX_PORTAL_QUAD;
    projected_portal.vertices[0] = {0.0f, 0.0f, 0.0f};
    projected_portal.vertices[1] = {1.0f, 0.0f, 0.0f};
    projected_portal.vertices[2] = {1.0f, 1.0f, 0.0f};
    projected_portal.vertices[3] = {0.0f, 1.0f, 0.0f};
    PortalIndex portal_index = kInvalidPortalIndex;
    REQUIRE(level.addPortal(projected_portal, portal_index) == ARX_OK);
    projected_portal.shape = ARX_PORTAL_TRIANGLE + 256U;
    CHECK(level.setPortal(portal_index, projected_portal) == ARX_LEVEL_BAD_PORTAL_SHAPE);
    CHECK(level.addPortal(projected_portal, portal_index) == ARX_LEVEL_BAD_PORTAL_SHAPE);
    CHECK(portal_index == kInvalidPortalIndex);

    const std::array<ArxVector2, 3> perimeter = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}}};
    ArxLevelZoneInput projected_zone{};
    projected_zone.value.name = {"zone", 4};
    projected_zone.value.perimeter_count = perimeter.size();
    projected_zone.value.height_mode = ARX_ZONE_HEIGHT_FINITE;
    projected_zone.value.height = 1.0f;
    projected_zone.perimeter_xz = perimeter.data();
    ZoneIndex zone_index = kInvalidZoneIndex;
    REQUIRE(level.addZone(projected_zone, zone_index) == ARX_OK);
    projected_zone.value.height_mode = ARX_ZONE_HEIGHT_FINITE + 256U;
    CHECK(level.setZone(zone_index, projected_zone) == ARX_LEVEL_BAD_ZONE_HEIGHT_MODE);
    CHECK(level.addZone(projected_zone, zone_index) == ARX_LEVEL_BAD_ZONE_HEIGHT_MODE);
    CHECK(zone_index == kInvalidZoneIndex);

    std::array<ArxLevelPathNode, 1> nodes{};
    ArxLevelPathInput projected_path{{"path", 4}, {}, nodes.data(), nodes.size()};
    PathIndex path_index = kInvalidPathIndex;
    REQUIRE(level.addPath(projected_path, path_index) == ARX_OK);
    nodes[0].type = ARX_PATH_NODE_STANDARD + 256U;
    CHECK(level.setPath(path_index, projected_path) == ARX_LEVEL_BAD_PATH_NODE_TYPE);
    CHECK(level.addPath(projected_path, path_index) == ARX_LEVEL_BAD_PATH_NODE_TYPE);
    CHECK(path_index == kInvalidPathIndex);
  }
}
