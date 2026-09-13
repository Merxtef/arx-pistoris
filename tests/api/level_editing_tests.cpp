// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/level/types.h"

#include "image_helpers.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>  // IWYU pragma: keep
#include <string_view>
#include <vector>

namespace {

ArxStringView levelStringView(std::string_view value) { return {value.data(), value.size()}; }

std::string_view levelString(ArxStringView value) { return {value.data, value.size}; }

ArxLevelFace floorFace(ArxVertexIndex a, ArxVertexIndex b, ArxVertexIndex c, ArxTextureIndex texture) {
  ArxLevelFace result{};
  result.texture = texture;
  result.room = 0;
  result.corners[0] = {a, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f, {}};
  result.corners[1] = {b, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f, {}};
  result.corners[2] = {c, {0.0f, -1.0f, 0.0f}, 1.0f, 1.0f, {}};
  return result;
}

ArxLevel* makePopulatedLevel() {
  ArxLevel* level = nullptr;
  REQUIRE(arx_pistoris_level_create(&level) == ARX_OK);
  const ArxLevelRoom room{levelStringView("room")};
  ArxRoomIndex room_index = ARX_INVALID_INDEX;
  REQUIRE(arx_pistoris_level_add_room(level, &room, &room_index) == ARX_OK);
  REQUIRE(room_index == 0);

  const ArxTextureView texture{levelStringView("graph/obj3d/textures/floor.png"), {}};
  const std::array<ArxLevelVertex, 4> vertices = {
      ArxLevelVertex{{0.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{400.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{400.0f, 0.0f, 400.0f}},
      ArxLevelVertex{{0.0f, 0.0f, 400.0f}},
  };
  const std::array<ArxLevelFace, 2> faces = {
      floorFace(0, 1, 2, 0),
      floorFace(0, 2, 3, 0),
  };
  const ArxLevelMeshInput mesh{vertices.data(), vertices.size(), faces.data(), faces.size(), &texture, 1};
  REQUIRE(arx_pistoris_level_replace_mesh(level, &mesh) == ARX_OK);
  return level;
}

void setFlatNavSurface(ArxLevel* level) {
  const std::array<ArxLevelVertex, 4> vertices = {
      ArxLevelVertex{{0.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{400.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{400.0f, 0.0f, 400.0f}},
      ArxLevelVertex{{0.0f, 0.0f, 400.0f}},
  };
  const std::array<ArxLevelNavSurfaceTriangle, 2> triangles = {{{0, 1, 2}, {0, 2, 3}}};
  const ArxLevelNavSurfaceInput surface{vertices.data(), vertices.size(), triangles.data(), triangles.size()};
  REQUIRE(arx_pistoris_level_set_nav_surface(level, &surface) == ARX_OK);
}

void setDisconnectedNavSurface(ArxLevel* level) {
  const std::array<ArxLevelVertex, 6> vertices = {
      ArxLevelVertex{{0.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{10.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{0.0f, 0.0f, 10.0f}},
      ArxLevelVertex{{20.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{25.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{20.0f, 0.0f, 2.0f}},
  };
  const std::array<ArxLevelNavSurfaceTriangle, 2> triangles = {{{0, 1, 2}, {3, 4, 5}}};
  const ArxLevelNavSurfaceInput surface{vertices.data(), vertices.size(), triangles.data(), triangles.size()};
  REQUIRE(arx_pistoris_level_set_nav_surface(level, &surface) == ARX_OK);
}

}  // namespace

TEST_SUITE("C Level editing") {
  TEST_CASE("Geometry and textures expose complete edit and inspection lifecycles") {
    ArxLevel* level = makePopulatedLevel();
    const std::vector<std::uint8_t> image = makeTestBmp();

    ArxLevelVertex vertex{{450.0f, 0.0f, 450.0f}};
    ArxVertexIndex added_vertex = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_vertex(level, vertex, &added_vertex) == ARX_OK);
    CHECK(added_vertex == 4);
    vertex.position = {425.0f, 0.0f, 425.0f};
    REQUIRE(arx_pistoris_level_set_vertex(level, added_vertex, vertex) == ARX_OK);
    const ArxLevelVertex bulk_vertex{{410.0f, 0.0f, 410.0f}};
    ArxVertexIndex first_bulk_vertex = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_vertices(level, &bulk_vertex, 1, &first_bulk_vertex) == ARX_OK);
    CHECK(first_bulk_vertex == 5);

    ArxTextureView texture{levelStringView("graph/obj3d/textures/stone.png"), {}};
    REQUIRE(arx_pistoris_level_set_texture(level, 0, &texture) == ARX_OK);
    ArxTextureView copied_texture{};
    REQUIRE(arx_pistoris_level_copy_texture_views(level, 0, 1, &copied_texture) == ARX_OK);
    CHECK(levelString(copied_texture.path) == "graph/obj3d/textures/stone.png");
    REQUIRE(arx_pistoris_level_set_texture_image(level, 0, image.data(), image.size()) == ARX_OK);
    REQUIRE(arx_pistoris_level_copy_texture_views(level, 0, 1, &copied_texture) == ARX_OK);
    REQUIRE(copied_texture.encoded_image.size == image.size());
    CHECK(std::equal(image.begin(), image.end(), copied_texture.encoded_image.data));
    REQUIRE(arx_pistoris_level_clear_texture_image(level, 0) == ARX_OK);
    REQUIRE(arx_pistoris_level_copy_texture_views(level, 0, 1, &copied_texture) == ARX_OK);
    CHECK(copied_texture.encoded_image.size == 0);
    REQUIRE(arx_pistoris_level_set_corner_color(level, 0, 0, {0.2f, 0.4f, 0.6f}) == ARX_OK);
    const ArxLevelFace added_face = floorFace(0, 1, 2, 0);
    ArxFaceIndex added_face_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_face(level, &added_face, &added_face_index) == ARX_OK);
    CHECK(added_face_index == 2);

    REQUIRE(arx_pistoris_level_validate_mesh(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_vertices(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_textures(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_faces(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_face_rooms(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_corner_colors(level) == ARX_OK);

    ArxAabb bounds{};
    REQUIRE(arx_pistoris_level_bounds(level, &bounds) == ARX_OK);
    CHECK(bounds.max.x == doctest::Approx(425.0f));
    REQUIRE(arx_pistoris_level_referenced_bounds(level, &bounds) == ARX_OK);
    CHECK(bounds.max.x == doctest::Approx(400.0f));

    std::size_t count = 0;
    REQUIRE(arx_pistoris_level_vertex_count(level, &count) == ARX_OK);
    CHECK(count == 6);
    REQUIRE(arx_pistoris_level_face_count(level, &count) == ARX_OK);
    CHECK(count == 3);
    REQUIRE(arx_pistoris_level_texture_count(level, &count) == ARX_OK);
    CHECK(count == 1);

    std::array<ArxLevelVertex, 2> copied_vertices{};
    REQUIRE(arx_pistoris_level_copy_vertices(level, 4, copied_vertices.size(), copied_vertices.data()) == ARX_OK);
    CHECK(copied_vertices[0].position.x == doctest::Approx(425.0f));
    CHECK(copied_vertices[1].position.x == doctest::Approx(410.0f));
    std::array<ArxLevelFace, 3> copied_faces{};
    REQUIRE(arx_pistoris_level_copy_faces(level, 0, copied_faces.size(), copied_faces.data()) == ARX_OK);
    CHECK(copied_faces[0].has_corner_colors != 0);
    CHECK(copied_faces[0].corners[0].color.g == doctest::Approx(0.4f));
    CHECK(copied_faces[2].corners[2].vertex == 2);
    REQUIRE(arx_pistoris_level_copy_texture_views(level, 0, 1, &copied_texture) == ARX_OK);
    CHECK(levelString(copied_texture.path) == "graph/obj3d/textures/stone.png");
    CHECK(copied_texture.encoded_image.size == 0);

    REQUIRE(arx_pistoris_level_clear_corner_colors(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_copy_faces(level, 0, 1, copied_faces.data()) == ARX_OK);
    CHECK(copied_faces[0].has_corner_colors == 0);
    REQUIRE(arx_pistoris_level_remove_face(level, added_face_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_face_count(level, &count) == ARX_OK);
    CHECK(count == 2);
    std::size_t removed = 0;
    REQUIRE(arx_pistoris_level_compact_vertices(level, &removed) == ARX_OK);
    CHECK(removed == 2);
    REQUIRE(arx_pistoris_level_clear_mesh(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_vertex_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_face_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_texture_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_reset(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_room_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Rooms and navigation expose complete edit and inspection lifecycles") {
    ArxLevel* level = makePopulatedLevel();

    ArxLevelRoom second_room{levelStringView("second")};
    ArxRoomIndex second_room_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_room(level, &second_room, &second_room_index) == ARX_OK);
    CHECK(second_room_index == 1);
    second_room.name = levelStringView("renamed");
    REQUIRE(arx_pistoris_level_set_room(level, second_room_index, &second_room) == ARX_OK);

    ArxLevelPortal portal{};
    portal.name = levelStringView("door");
    portal.room_1 = 0;
    portal.room_2 = second_room_index;
    portal.shape = ARX_PORTAL_TRIANGLE;
    portal.vertices[0] = {100.0f, 0.0f, 100.0f};
    portal.vertices[1] = {100.0f, -100.0f, 100.0f};
    portal.vertices[2] = {200.0f, 0.0f, 100.0f};
    ArxPortalIndex portal_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_portal(level, &portal, &portal_index) == ARX_OK);
    CHECK(portal_index == 0);
    portal.name = levelStringView("doorway");
    REQUIRE(arx_pistoris_level_set_portal(level, portal_index, &portal) == ARX_OK);

    const ArxLevelRoomDistance distance{0, second_room_index, -1.0f, portal_index, portal_index};
    REQUIRE(arx_pistoris_level_replace_room_distances(level, &distance, 1) == ARX_OK);

    std::array<ArxLevelAnchor, 2> anchors{};
    anchors[0] = {{100.0f, 0.0f, 100.0f}, 25.0f, -80.0f, 0, levelStringView("first")};
    anchors[1] = {{200.0f, 0.0f, 100.0f}, 25.0f, -80.0f, 0, levelStringView("second")};
    const ArxLevelAnchorConnection connection{0, 1};
    const ArxLevelAnchorsInput anchor_input{anchors.data(), anchors.size(), &connection, 1};
    REQUIRE(arx_pistoris_level_replace_anchors(level, &anchor_input) == ARX_OK);
    anchors[0].radius = 30.0f;
    REQUIRE(arx_pistoris_level_set_anchor(level, 0, &anchors[0]) == ARX_OK);
    const ArxLevelAnchor third_anchor{{300.0f, 0.0f, 100.0f}, 25.0f, -80.0f, 0, levelStringView("third")};
    ArxAnchorIndex third_anchor_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_anchor(level, &third_anchor, &third_anchor_index) == ARX_OK);
    CHECK(third_anchor_index == 2);
    REQUIRE(arx_pistoris_level_set_anchor_connection(level, 0, {0, third_anchor_index}) == ARX_OK);
    ArxAnchorConnectionIndex second_connection_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_anchor_connection(level, {1, third_anchor_index}, &second_connection_index) ==
            ARX_OK);
    CHECK(second_connection_index == 1);
    setFlatNavSurface(level);

    REQUIRE(arx_pistoris_level_validate_rooms(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_portals(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_room_distances(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_nav_surface(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_anchors(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_anchor_connections(level) == ARX_OK);

    std::size_t count = 0;
    REQUIRE(arx_pistoris_level_room_count(level, &count) == ARX_OK);
    CHECK(count == 2);
    std::array<ArxLevelRoom, 2> copied_rooms{};
    REQUIRE(arx_pistoris_level_copy_rooms(level, 0, copied_rooms.size(), copied_rooms.data()) == ARX_OK);
    CHECK(levelString(copied_rooms[1].name) == "renamed");
    REQUIRE(arx_pistoris_level_portal_count(level, &count) == ARX_OK);
    CHECK(count == 1);
    ArxLevelPortal copied_portal{};
    REQUIRE(arx_pistoris_level_copy_portals(level, 0, 1, &copied_portal) == ARX_OK);
    CHECK(levelString(copied_portal.name) == "doorway");
    CHECK(copied_portal.room_2 == second_room_index);
    CHECK(copied_portal.shape == ARX_PORTAL_TRIANGLE);
    CHECK(copied_portal.vertices[2].x == doctest::Approx(200.0f));
    REQUIRE(arx_pistoris_level_room_distance_count(level, &count) == ARX_OK);
    CHECK(count == 1);
    ArxLevelRoomDistance copied_distance{};
    REQUIRE(arx_pistoris_level_copy_room_distances(level, 0, 1, &copied_distance) == ARX_OK);
    CHECK(copied_distance.room_b == second_room_index);
    CHECK(copied_distance.distance == doctest::Approx(-1.0f));
    CHECK(copied_distance.portal_a == portal_index);

    REQUIRE(arx_pistoris_level_anchor_count(level, &count) == ARX_OK);
    CHECK(count == 3);
    std::array<ArxLevelAnchor, 3> copied_anchors{};
    REQUIRE(arx_pistoris_level_copy_anchors(level, 0, copied_anchors.size(), copied_anchors.data()) == ARX_OK);
    CHECK(copied_anchors[0].radius == doctest::Approx(30.0f));
    CHECK(levelString(copied_anchors[2].name) == "third");
    REQUIRE(arx_pistoris_level_anchor_connection_count(level, &count) == ARX_OK);
    CHECK(count == 2);
    std::array<ArxLevelAnchorConnection, 2> copied_connections{};
    REQUIRE(arx_pistoris_level_copy_anchor_connections(
                level, 0, copied_connections.size(), copied_connections.data()) == ARX_OK);
    CHECK(copied_connections[0].first == 0);
    CHECK(copied_connections[0].second == third_anchor_index);
    CHECK(copied_connections[1].first == 1);
    CHECK(copied_connections[1].second == third_anchor_index);

    ArxLevelNavSurfaceInfo surface_info{};
    REQUIRE(arx_pistoris_level_nav_surface_info(level, &surface_info) == ARX_OK);
    CHECK(surface_info.has_surface != 0);
    CHECK(surface_info.vertex_count == 4);
    CHECK(surface_info.triangle_count == 2);
    std::array<ArxLevelVertex, 4> copied_nav_vertices{};
    std::array<ArxLevelNavSurfaceTriangle, 2> copied_nav_triangles{};
    REQUIRE(arx_pistoris_level_copy_nav_surface_vertices(
                level, 0, copied_nav_vertices.size(), copied_nav_vertices.data()) == ARX_OK);
    REQUIRE(arx_pistoris_level_copy_nav_surface_triangles(
                level, 0, copied_nav_triangles.size(), copied_nav_triangles.data()) == ARX_OK);
    CHECK(copied_nav_vertices[2].position.x == doctest::Approx(400.0f));
    CHECK(copied_nav_vertices[2].position.z == doctest::Approx(400.0f));
    CHECK(copied_nav_triangles[1].vertices[1] == 2);

    REQUIRE(arx_pistoris_level_clear_nav_surface(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_nav_surface_info(level, &surface_info) == ARX_OK);
    CHECK(surface_info.has_surface == 0);
    REQUIRE(arx_pistoris_level_remove_anchor_connection(level, 1) == ARX_OK);
    REQUIRE(arx_pistoris_level_anchor_connection_count(level, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_level_remove_anchor_connection(level, 0) == ARX_OK);
    REQUIRE(arx_pistoris_level_anchor_connection_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_remove_anchor(level, 1) == ARX_OK);
    REQUIRE(arx_pistoris_level_anchor_count(level, &count) == ARX_OK);
    CHECK(count == 2);
    REQUIRE(arx_pistoris_level_clear_anchors(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_anchor_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_clear_room_distances(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_room_distance_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_remove_portal(level, portal_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_portal_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_remove_room(level, second_room_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_room_count(level, &count) == ARX_OK);
    CHECK(count == 1);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Scene objects expose complete edit and inspection lifecycles") {
    ArxLevel* level = makePopulatedLevel();

    ArxLevelLight light{};
    light.name = levelStringView("torch");
    light.position = {100.0f, -100.0f, 100.0f};
    light.color = {1.0f, 0.5f, 0.25f};
    light.fallend = 300.0f;
    light.intensity = 1.0f;
    ArxLightIndex light_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_light(level, &light, &light_index) == ARX_OK);
    light.intensity = 0.5f;
    REQUIRE(arx_pistoris_level_set_light(level, light_index, &light) == ARX_OK);

    const ArxLevelPlayerSpawn spawn{{50.0f, 0.0f, 50.0f}, ARX_QUAT_IDENTITY_INIT, 1};
    REQUIRE(arx_pistoris_level_set_player_spawn(level, &spawn) == ARX_OK);

    ArxLevelEntity entity{};
    entity.class_path = levelStringView("graph/obj3d/interactive/items/weapons/sword/sword");
    entity.ident = 1;
    entity.position = {50.0f, 0.0f, 50.0f};
    entity.name = levelStringView("sword");
    ArxEntityIndex entity_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_entity(level, &entity, &entity_index) == ARX_OK);
    entity.position.x = 60.0f;
    REQUIRE(arx_pistoris_level_set_entity(level, entity_index, &entity) == ARX_OK);

    ArxLevelFog fog{};
    fog.position = {100.0f, -20.0f, 100.0f};
    fog.color = {0.2f, 0.3f, 0.4f};
    fog.size = 10.0f;
    fog.scale = 1.0f;
    fog.lifetime_ms = 1000;
    fog.name = levelStringView("mist");
    ArxFogIndex fog_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_fog(level, &fog, &fog_index) == ARX_OK);
    fog.frequency = 2.0f;
    REQUIRE(arx_pistoris_level_set_fog(level, fog_index, &fog) == ARX_OK);

    const std::array<ArxVector2, 3> perimeter = {{{50.0f, 50.0f}, {150.0f, 50.0f}, {50.0f, 150.0f}}};
    ArxLevelZoneInput zone{};
    zone.value.name = levelStringView("zone");
    zone.value.perimeter_count = perimeter.size();
    zone.value.reference_y = 0.0f;
    zone.value.height_mode = ARX_ZONE_HEIGHT_FINITE;
    zone.value.height = 100.0f;
    zone.perimeter_xz = perimeter.data();
    ArxZoneIndex zone_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_zone(level, &zone, &zone_index) == ARX_OK);
    zone.value.height = 120.0f;
    REQUIRE(arx_pistoris_level_set_zone(level, zone_index, &zone) == ARX_OK);

    std::array<ArxLevelPathNode, 2> nodes{};
    nodes[0] = {{}, ARX_PATH_NODE_STANDARD, 0};
    nodes[1] = {{100.0f, 0.0f, 0.0f}, ARX_PATH_NODE_BEZIER, 1000};
    ArxLevelPathInput path{levelStringView("patrol"), {10.0f, 0.0f, 10.0f}, nodes.data(), nodes.size()};
    ArxPathIndex path_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_path(level, &path, &path_index) == ARX_OK);
    path.position.x = 20.0f;
    REQUIRE(arx_pistoris_level_set_path(level, path_index, &path) == ARX_OK);

    REQUIRE(arx_pistoris_level_validate_lights(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_player_spawn(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_entities(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_fogs(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_zones(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_paths(level) == ARX_OK);

    std::size_t count = 0;
    ArxLevelLight copied_light{};
    REQUIRE(arx_pistoris_level_light_count(level, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_level_copy_lights(level, 0, 1, &copied_light) == ARX_OK);
    CHECK(levelString(copied_light.name) == "torch");
    CHECK(copied_light.intensity == doctest::Approx(0.5f));
    ArxLevelPlayerSpawn copied_spawn{};
    REQUIRE(arx_pistoris_level_player_spawn(level, &copied_spawn) == ARX_OK);
    CHECK(copied_spawn.is_usable != 0);
    CHECK(copied_spawn.position.x == doctest::Approx(50.0f));
    ArxLevelEntity copied_entity{};
    REQUIRE(arx_pistoris_level_entity_count(level, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_level_copy_entities(level, 0, 1, &copied_entity) == ARX_OK);
    CHECK(levelString(copied_entity.name) == "sword");
    CHECK(copied_entity.position.x == doctest::Approx(60.0f));
    ArxLevelFog copied_fog{};
    REQUIRE(arx_pistoris_level_fog_count(level, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_level_copy_fogs(level, 0, 1, &copied_fog) == ARX_OK);
    CHECK(levelString(copied_fog.name) == "mist");
    CHECK(copied_fog.frequency == doctest::Approx(2.0f));
    ArxLevelZone copied_zone{};
    REQUIRE(arx_pistoris_level_zone_count(level, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_level_copy_zones(level, 0, 1, &copied_zone) == ARX_OK);
    CHECK(levelString(copied_zone.name) == "zone");
    CHECK(copied_zone.height == doctest::Approx(120.0f));
    std::array<ArxVector2, 3> copied_perimeter{};
    REQUIRE(arx_pistoris_level_copy_zone_perimeter(level, 0, 0, copied_perimeter.size(), copied_perimeter.data()) ==
            ARX_OK);
    CHECK(copied_perimeter[1].x == doctest::Approx(150.0f));
    ArxLevelPath copied_path{};
    REQUIRE(arx_pistoris_level_path_count(level, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_level_copy_paths(level, 0, 1, &copied_path) == ARX_OK);
    CHECK(levelString(copied_path.name) == "patrol");
    CHECK(copied_path.position.x == doctest::Approx(20.0f));
    CHECK(copied_path.node_count == 2);
    std::array<ArxLevelPathNode, 2> copied_nodes{};
    REQUIRE(arx_pistoris_level_copy_path_nodes(level, 0, 0, copied_nodes.size(), copied_nodes.data()) == ARX_OK);
    CHECK(copied_nodes[1].type == ARX_PATH_NODE_BEZIER);
    CHECK(copied_nodes[1].time_ms == 1000);

    ArxLevelDlfBakeOptions dlf_options = ARX_LEVEL_DLF_BAKE_OPTIONS_INIT;
    dlf_options.level_name = levelStringView("level7");
    ArxDlf* dlf = nullptr;
    REQUIRE(arx_pistoris_level_bake_dlf(level, &dlf_options, &dlf) == ARX_OK);
    CHECK(arx_pistoris_dlf_validate(dlf) == ARX_OK);
    arx_pistoris_dlf_destroy(dlf);

    REQUIRE(arx_pistoris_level_remove_path(level, path_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_remove_zone(level, zone_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_remove_fog(level, fog_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_remove_entity(level, entity_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_clear_player_spawn(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_remove_light(level, light_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_path_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_zone_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_fog_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_entity_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_light_count(level, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_level_player_spawn(level, &copied_spawn) == ARX_OK);
    CHECK(copied_spawn.is_usable == 0);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Level generators publish inspectable results") {
    ArxLevel* level = makePopulatedLevel();
    const ArxLevelRoom second_room{levelStringView("second")};
    ArxRoomIndex second_room_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_room(level, &second_room, &second_room_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_set_face_room(level, 1, second_room_index) == ARX_OK);
    ArxLevelFace room_face{};
    REQUIRE(arx_pistoris_level_copy_faces(level, 1, 1, &room_face) == ARX_OK);
    CHECK(room_face.room == second_room_index);
    ArxLevelPortal portal{};
    portal.name = levelStringView("door");
    portal.room_1 = 0;
    portal.room_2 = second_room_index;
    portal.vertices[0] = {200.0f, 0.0f, 100.0f};
    portal.vertices[1] = {200.0f, -100.0f, 100.0f};
    portal.vertices[2] = {200.0f, -100.0f, 300.0f};
    portal.vertices[3] = {200.0f, 0.0f, 300.0f};
    ArxPortalIndex portal_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_portal(level, &portal, &portal_index) == ARX_OK);

    REQUIRE(arx_pistoris_level_generate_static_lighting(level, nullptr) == ARX_OK);
    ArxLevelFace lit_face{};
    REQUIRE(arx_pistoris_level_copy_faces(level, 0, 1, &lit_face) == ARX_OK);
    CHECK(lit_face.has_corner_colors != 0);
    CHECK(lit_face.corners[0].color.r > 0.0f);

    REQUIRE(arx_pistoris_level_generate_nav_surface(level, nullptr) == ARX_OK);
    ArxLevelNavSurfaceInfo surface{};
    REQUIRE(arx_pistoris_level_nav_surface_info(level, &surface) == ARX_OK);
    CHECK(surface.has_surface != 0);
    CHECK(surface.vertex_count > 0);
    REQUIRE(arx_pistoris_level_clear_nav_surface(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_set_nav_surface_from_floor(level, nullptr) == ARX_OK);
    REQUIRE(arx_pistoris_level_nav_surface_info(level, &surface) == ARX_OK);
    CHECK(surface.has_surface != 0);
    CHECK(surface.vertex_count == 4);
    CHECK(surface.triangle_count == 2);
    setDisconnectedNavSurface(level);
    ArxLevelNavSurfacePruneOptions nav_prune = ARX_LEVEL_NAV_SURFACE_PRUNE_OPTIONS_INIT;
    nav_prune.min_component_area_ratio = 0.0f;
    nav_prune.min_component_area = 5.01;
    REQUIRE(arx_pistoris_level_prune_nav_surface_islands(level, &nav_prune) == ARX_OK);
    REQUIRE(arx_pistoris_level_nav_surface_info(level, &surface) == ARX_OK);
    CHECK(surface.vertex_count == 3);
    CHECK(surface.triangle_count == 1);
    setFlatNavSurface(level);

    REQUIRE(arx_pistoris_level_generate_anchors(level, nullptr) == ARX_OK);
    std::size_t anchor_count = 0;
    REQUIRE(arx_pistoris_level_anchor_count(level, &anchor_count) == ARX_OK);
    CHECK(anchor_count > 0);
    REQUIRE(arx_pistoris_level_generate_anchor_connections(level, nullptr) == ARX_OK);
    std::size_t connection_count = 0;
    REQUIRE(arx_pistoris_level_anchor_connection_count(level, &connection_count) == ARX_OK);
    CHECK(connection_count > 0);
    const ArxLevelAnchor isolated_anchor{{1000.0f, 0.0f, 1000.0f}, 25.0f, -80.0f, 0, levelStringView("isolated")};
    ArxAnchorIndex isolated_anchor_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_level_add_anchor(level, &isolated_anchor, &isolated_anchor_index) == ARX_OK);
    REQUIRE(arx_pistoris_level_anchor_count(level, &anchor_count) == ARX_OK);
    const std::size_t unpruned_anchor_count = anchor_count;
    ArxLevelAnchorPruneOptions anchor_prune = ARX_LEVEL_ANCHOR_PRUNE_OPTIONS_INIT;
    anchor_prune.min_component_anchor_ratio = 0.75f;
    REQUIRE(arx_pistoris_level_prune_anchor_islands(level, &anchor_prune) == ARX_OK);
    REQUIRE(arx_pistoris_level_anchor_count(level, &anchor_count) == ARX_OK);
    CHECK(anchor_count > 0);
    CHECK(anchor_count < unpruned_anchor_count);
    REQUIRE(arx_pistoris_level_generate_room_distances(level, nullptr) == ARX_OK);
    std::size_t distance_count = 0;
    REQUIRE(arx_pistoris_level_room_distance_count(level, &distance_count) == ARX_OK);
    CHECK(distance_count == 1);
    ArxLevelRoomDistance distance{};
    REQUIRE(arx_pistoris_level_copy_room_distances(level, 0, 1, &distance) == ARX_OK);
    CHECK(distance.room_a == 0);
    CHECK(distance.room_b == second_room_index);
    CHECK(distance.distance == doctest::Approx(-1.0f));
    REQUIRE(arx_pistoris_level_validate_corner_colors(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_nav_surface(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_anchors(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_anchor_connections(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_room_distances(level) == ARX_OK);
    arx_pistoris_level_destroy(level);
  }

  TEST_CASE("Remaining Level image projections return owned PNG data") {
    ArxLevel* level = makePopulatedLevel();
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(arx_pistoris_level_set_minimap(level, {image.data(), image.size()}, {{0.0f, 0.0f}, {400.0f, 400.0f}}) ==
            ARX_OK);
    REQUIRE(arx_pistoris_level_set_loading_screen(level, {image.data(), image.size()}) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_minimap(level) == ARX_OK);
    REQUIRE(arx_pistoris_level_validate_loading_screen(level) == ARX_OK);

    std::uint8_t* encoded = nullptr;
    std::size_t encoded_size = 0;
    ArxVector2 projection_offset{};
    ArxImageInfo info{};
    REQUIRE(arx_pistoris_level_render_compact_minimap_png(level, &projection_offset, &encoded, &encoded_size) ==
            ARX_OK);
    REQUIRE(encoded != nullptr);
    REQUIRE(arx_pistoris_binary_inspect_encoded_image({encoded, encoded_size}, &info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width > 0);
    CHECK(info.height > 0);
    CHECK(projection_offset.x == doctest::Approx(0.0f));
    CHECK(projection_offset.y == doctest::Approx(0.0f));
    arx_pistoris_free_bytes(encoded);
    encoded = nullptr;
    encoded_size = 0;

    REQUIRE(arx_pistoris_level_render_fullscreen_loading_screen_png(level, &encoded, &encoded_size) == ARX_OK);
    REQUIRE(encoded != nullptr);
    REQUIRE(arx_pistoris_binary_inspect_encoded_image({encoded, encoded_size}, &info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width == 640);
    CHECK(info.height == 480);
    arx_pistoris_free_bytes(encoded);
    encoded = nullptr;
    encoded_size = 0;

    REQUIRE(arx_pistoris_level_transcode_loading_screen_png(level, &encoded, &encoded_size) == ARX_OK);
    REQUIRE(encoded != nullptr);
    REQUIRE(arx_pistoris_binary_inspect_encoded_image({encoded, encoded_size}, &info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    arx_pistoris_free_bytes(encoded);
    arx_pistoris_level_destroy(level);
  }
}
