// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level_diagnostics.hpp"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris.hpp"

#include "level/data.h"
#include "level/validation.h"
#include "level_add_helpers.h"
#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/collision/internal.h"
#include "modules/rooms.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using pistoris::ArxVector3;

struct LogCapture {
  int warnings = 0;
  int debug = 0;
  std::string last;
  std::string last_debug;
};

void captureLog(ArxLogLevel level, const char* msg, void* userdata) {
  auto* capture = static_cast<LogCapture*>(userdata);
  if (level == ARX_LOG_DEBUG) {
    ++capture->debug;
    capture->last_debug = msg ? msg : "";
  }
  if (level == ARX_LOG_WARN) {
    ++capture->warnings;
    capture->last = msg ? msg : "";
  }
}

pistoris::Face makeFace(std::uint32_t a, std::uint32_t b, std::uint32_t c, const ArxVector3& normal,
                        pistoris::FaceType flags = 0) {
  return {
      {{{a, normal, 0.0f, 0.0f}, {b, normal, 0.0f, 0.0f}, {c, normal, 0.0f, 0.0f}}}, pistoris::kNoTexture, flags, 0.0f};
}

void addQuad(pistoris::LevelModules& level, const ArxVector3& a, const ArxVector3& b, const ArxVector3& c,
             const ArxVector3& d, const ArxVector3& normal, pistoris::FaceType flags = 0, std::uint32_t room = 0) {
  std::uint32_t base = static_cast<std::uint32_t>(level.geometry.vertices.size());
  level.geometry.vertices.push_back({a});
  level.geometry.vertices.push_back({b});
  level.geometry.vertices.push_back({c});
  level.geometry.vertices.push_back({d});
  level.geometry.faces.push_back(makeFace(base + 0, base + 1, base + 2, normal, flags));
  level.rooms.face_rooms.push_back(room);
  level.geometry.faces.push_back(makeFace(base + 0, base + 2, base + 3, normal, flags));
  level.rooms.face_rooms.push_back(room);
}

pistoris::LevelModules baseLevel() {
  pistoris::LevelModules level;
  level.rooms.definitions.push_back({"room"});
  return level;
}

void addFloor(pistoris::LevelModules& level, float min_x, float max_x, float y, float min_z, float max_z) {
  addQuad(level, {min_x, y, min_z}, {max_x, y, min_z}, {max_x, y, max_z}, {min_x, y, max_z}, {0.0f, -1.0f, 0.0f});
}

void addRoomFloor(pistoris::LevelModules& level, float min_x, float max_x, float y, float min_z, float max_z,
                  std::uint32_t room) {
  addQuad(
      level, {min_x, y, min_z}, {max_x, y, min_z}, {max_x, y, max_z}, {min_x, y, max_z}, {0.0f, -1.0f, 0.0f}, 0, room);
}

void addNavSurface(pistoris::LevelModules& level, float min_x, float max_x, float y, float min_z, float max_z) {
  level.navigation.surface = pistoris::NavSurface{
      {{{min_x, y, min_z}}, {{max_x, y, min_z}}, {{max_x, y, max_z}}, {{min_x, y, max_z}}},
      {{{{0, 1, 2}}}, {{{0, 2, 3}}}},
  };
}

void addTiledNavSurface(pistoris::LevelModules& level, float min_x, float max_x, float y, float min_z, float max_z,
                        float tile_size) {
  pistoris::NavSurface surface;
  // NOLINTBEGIN(bugprone-float-loop-counter): fixture deliberately samples caller-provided tile boundaries
  for (float z = min_z; z < max_z; z += tile_size) {
    for (float x = min_x; x < max_x; x += tile_size) {
      float next_x = std::min(x + tile_size, max_x);
      float next_z = std::min(z + tile_size, max_z);
      std::uint32_t base = static_cast<std::uint32_t>(surface.vertices.size());
      surface.vertices.push_back({{x, y, z}});
      surface.vertices.push_back({{next_x, y, z}});
      surface.vertices.push_back({{next_x, y, next_z}});
      surface.vertices.push_back({{x, y, next_z}});
      surface.triangles.push_back({{{base + 0, base + 1, base + 2}}});
      surface.triangles.push_back({{{base + 0, base + 2, base + 3}}});
    }
  }
  // NOLINTEND(bugprone-float-loop-counter)
  level.navigation.surface = std::move(surface);
}

void addObstacle(pistoris::LevelModules& level, float x, float y_bottom, float y_top, float min_z, float max_z) {
  addQuad(level, {x, y_bottom, min_z}, {x, y_top, min_z}, {x, y_top, max_z}, {x, y_bottom, max_z}, {-1.0f, 0.0f, 0.0f});
}

void addDefaultAnchors(pistoris::LevelModules& level) {
  level.navigation.anchors.push_back({{50.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}});
  level.navigation.anchors.push_back({{150.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}});
}

pistoris::navigation::NavSurfaceGenOptions toModuleOptions(const pistoris::Level::NavSurfaceGenOptions& options) {
  pistoris::navigation::NavSurfaceGenOptions result;
  result.radius = options.radius;
  result.height = options.height;
  result.clearance = options.clearance;
  result.max_step_up = options.max_step_up;
  result.support_min_up_cos = options.support_min_up_cos;
  result.support_ignore_flags = options.support_ignore_flags;
  return result;
}

pistoris::navigation::AnchorGenOptions toModuleOptions(const pistoris::Level::AnchorGenOptions& options) {
  return {
      .sample_spacing = options.sample_spacing,
      .radius = options.radius,
      .height = options.height,
  };
}

pistoris::navigation::AnchorConnectionGenOptions toModuleOptions(
    const pistoris::Level::AnchorConnectionGenOptions& options) {
  return {
      .max_distance = options.max_distance,
      .max_step_distance = options.max_step_distance,
      .max_step_up = options.max_step_up,
      .radius_scale = options.radius_scale,
      .max_steps = options.max_steps,
  };
}

ArxReturnCode generateNavSurface(pistoris::LevelModules& level,
                                 const pistoris::Level::NavSurfaceGenOptions& options = {},
                                 pistoris::GeometryDerived* derived = nullptr,
                                 pistoris::navigation::NavigationDiagnostics* diagnostics = nullptr) {
  pistoris::GeometryDerived next_derived;
  ArxReturnCode rc =
      pistoris::level_validation::geometryError(pistoris::geometry::validate(level.geometry, &next_derived));
  if (rc != ARX_OK) return rc;
  pistoris::NavSurface surface;
  rc = pistoris::level_validation::navigationError(pistoris::navigation::generateSurface(
      surface, level.geometry, toModuleOptions(options), diagnostics ? &diagnostics->surface : nullptr));
  if (rc != ARX_OK) return rc;
  level.navigation.surface = std::move(surface);
  if (derived) *derived = next_derived;
  return ARX_OK;
}

ArxReturnCode generateAnchors(pistoris::LevelModules& level, const pistoris::Level::AnchorGenOptions& options = {},
                              pistoris::GeometryDerived* derived = nullptr,
                              pistoris::navigation::NavigationDiagnostics* diagnostics = nullptr) {
  pistoris::GeometryDerived next_derived;
  ArxReturnCode rc =
      pistoris::level_validation::geometryError(pistoris::geometry::validate(level.geometry, &next_derived));
  if (rc != ARX_OK) return rc;
  if (!level.navigation.surface.has_value()) return ARX_LEVEL_NAV_SURFACE_REQUIRED;
  rc = pistoris::level_validation::navigationError(pistoris::navigation::validateSurface(*level.navigation.surface));
  if (rc != ARX_OK) return rc;
  std::vector<pistoris::Anchor> anchors;
  rc = pistoris::level_validation::navigationError(
      pistoris::navigation::generateAnchors(anchors,
                                            level.geometry,
                                            *level.navigation.surface,
                                            next_derived.referenced_bounds,
                                            toModuleOptions(options),
                                            diagnostics ? &diagnostics->anchors : nullptr));
  if (rc != ARX_OK) return rc;
  level.navigation.anchors = std::move(anchors);
  level.navigation.connections.clear();
  if (derived) *derived = next_derived;
  return ARX_OK;
}

ArxReturnCode generateAnchorConnections(pistoris::LevelModules& level,
                                        const pistoris::Level::AnchorConnectionGenOptions& options = {},
                                        pistoris::GeometryDerived* derived = nullptr,
                                        pistoris::navigation::NavigationDiagnostics* diagnostics = nullptr) {
  pistoris::GeometryDerived next_derived;
  ArxReturnCode rc =
      pistoris::level_validation::geometryError(pistoris::geometry::validate(level.geometry, &next_derived));
  if (rc != ARX_OK) return rc;
  rc = pistoris::level_validation::navigationError(
      pistoris::navigation::validateAnchorDefinitions(level.navigation.anchors));
  if (rc != ARX_OK) return rc;
  std::vector<pistoris::AnchorConnection> connections;
  rc = pistoris::level_validation::navigationError(
      pistoris::navigation::generateAnchorConnections(connections,
                                                      level.geometry,
                                                      level.navigation.anchors,
                                                      toModuleOptions(options),
                                                      diagnostics ? &diagnostics->connections : nullptr));
  if (rc != ARX_OK) return rc;
  level.navigation.connections = std::move(connections);
  if (derived) *derived = next_derived;
  return ARX_OK;
}

pistoris::Level makePublicLevel(const pistoris::LevelModules& src) {
  pistoris::Level level;
  for (const pistoris::Room& room : src.rooms.definitions) {
    const pistoris::RoomIndex expected = static_cast<pistoris::RoomIndex>(level.roomCount());
    REQUIRE(test::addRoom(level, room) == expected);
  }

  test::MeshSnapshot mesh;
  mesh.vertices = src.geometry.vertices;
  mesh.faces = src.geometry.faces;
  mesh.textures = src.geometry.textures;
  mesh.face_rooms = src.rooms.face_rooms;
  mesh.corner_colors = src.lighting.corner_colors;
  REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);

  for (const pistoris::Portal& portal : src.rooms.portals) {
    const pistoris::PortalIndex expected = static_cast<pistoris::PortalIndex>(level.portalCount());
    REQUIRE(test::addPortal(level, portal) == expected);
  }

  return pistoris::Level(level);
}

double projectedArea2(const ArxVector3& a, const ArxVector3& b, const ArxVector3& c) {
  return (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.z) - a.z) -
         (static_cast<double>(b.z) - a.z) * (static_cast<double>(c.x) - a.x);
}

}  // namespace

TEST_CASE("SurfaceSupportIndexUsesTransientRoomPredicateContext") {
  pistoris::LevelModules level;
  level.rooms.definitions = {{"room_1"}, {"room_2"}};
  addRoomFloor(level, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 0);
  addRoomFloor(level, 200.0f, 300.0f, -10.0f, 0.0f, 100.0f, 1);

  struct RoomPredicateContext {
    const pistoris::RoomsData* rooms = nullptr;
    pistoris::RoomIndex room = pistoris::kInvalidRoomIndex;
  };
  const auto belongs_to_room = [](pistoris::FaceIndex face, const void* user_data) noexcept {
    if (user_data == nullptr) return false;
    const auto& context = *static_cast<const RoomPredicateContext*>(user_data);
    return context.rooms != nullptr && face < context.rooms->face_rooms.size() &&
           context.rooms->face_rooms[face] == context.room;
  };

  const RoomPredicateContext room_1_context{&level.rooms, 0};
  const RoomPredicateContext room_2_context{&level.rooms, 1};
  pistoris::geometry::SurfaceSupportIndex room_1 =
      pistoris::geometry::buildSurfaceSupportIndex(level.geometry, {belongs_to_room, &room_1_context});
  pistoris::geometry::SurfaceSupportIndex room_2 =
      pistoris::geometry::buildSurfaceSupportIndex(level.geometry, {belongs_to_room, &room_2_context});

  CHECK(room_1.triangles().size() == 2);
  CHECK(room_2.triangles().size() == 2);
  CHECK(room_1.hitsAt(50.0f, 50.0f).size() == 2);
  CHECK(room_1.hitsAt(250.0f, 50.0f).empty());
  CHECK(room_2.hitsAt(50.0f, 50.0f).empty());
  CHECK(room_2.hitsAt(250.0f, 50.0f).size() == 2);
}

TEST_CASE("NavigationCollisionBroadphaseUsesCylinderRadiusWithEpsilon") {
  pistoris::navigation::collision::Cylinder cylinder{{50.0f, 0.0f, 50.0f}, 5.0f, -80.0f};

  CHECK(pistoris::navigation::collision::broadphaseRadius(cylinder) == doctest::Approx(6.0f));
}

TEST_CASE("NavigationCollisionBroadphaseDoesNotScanOldCompatibilityPadding") {
  pistoris::LevelModules level = baseLevel();
  addQuad(level,
          {110.0f, 0.0f, 40.0f},
          {120.0f, 0.0f, 40.0f},
          {120.0f, 0.0f, 60.0f},
          {110.0f, 0.0f, 60.0f},
          {0.0f, -1.0f, 0.0f});

  pistoris::navigation::collision::StaticCollisionIndex index(level.geometry);
  pistoris::navigation::collision::Cylinder cylinder{{50.0f, 0.0f, 50.0f}, 5.0f, -80.0f};

  CHECK(index.candidates(cylinder).empty());
}

TEST_CASE("NavigationCollisionBroadphaseKeepsNearbySmallRadiusFacesAcrossCells") {
  pistoris::LevelModules level = baseLevel();
  addQuad(level,
          {101.0f, 0.0f, 40.0f},
          {104.0f, 0.0f, 40.0f},
          {104.0f, 0.0f, 60.0f},
          {101.0f, 0.0f, 60.0f},
          {0.0f, -1.0f, 0.0f});

  pistoris::navigation::collision::StaticCollisionIndex index(level.geometry);
  pistoris::navigation::collision::Cylinder cylinder{{99.0f, 0.0f, 50.0f}, 5.0f, -80.0f};

  CHECK(!index.candidates(cylinder).empty());
}

TEST_CASE("NavigationCollisionBroadphaseBoundsHugeFiniteQueries") {
  pistoris::LevelModules level = baseLevel();
  addQuad(level,
          {100.0f, 0.0f, 100.0f},
          {200.0f, 0.0f, 100.0f},
          {200.0f, 0.0f, 200.0f},
          {100.0f, 0.0f, 200.0f},
          {0.0f, -1.0f, 0.0f});

  pistoris::navigation::collision::StaticCollisionIndex index(level.geometry);
  pistoris::navigation::collision::Cylinder cylinder{{8000.0f, 0.0f, 8000.0f}, 1.0e20f, -80.0f};

  CHECK(index.candidates(cylinder).size() == level.geometry.faces.size());
}

TEST_CASE("LevelNavSurfaceGenerationSamplesWalkableGeometryWithClearance") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  addDefaultAnchors(level);
  level.navigation.connections.push_back({0, 1});

  REQUIRE(generateNavSurface(level, {.radius = 50.0f, .height = -165.0f}) == ARX_OK);

  REQUIRE(level.navigation.surface.has_value());
  CHECK(!level.navigation.surface->vertices.empty());
  CHECK(!level.navigation.surface->triangles.empty());
  for (const pistoris::Vertex& vertex : level.navigation.surface->vertices) {
    CHECK(vertex.position.y == doctest::Approx(-5.0f));
  }
  CHECK(level.navigation.anchors.size() == 2);
  CHECK(level.navigation.connections.size() == 1);
}

TEST_CASE("PublicLevelNavSurfaceGenerationModesPreserveAnchorGraph") {
  pistoris::LevelModules src = baseLevel();
  addFloor(src, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  pistoris::Level level = makePublicLevel(src);
  REQUIRE(test::addAnchor(level, {{50.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}}) == 0);
  REQUIRE(test::addAnchor(level, {{150.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}}) == 1);
  REQUIRE(test::addAnchorConnection(level, {0, 1}) == 0);
  REQUIRE(level.generateNavSurface({.radius = 50.0f, .height = -165.0f}) == ARX_OK);
  CHECK(level.anchorCount() == 2);
  CHECK(level.anchorConnectionCount() == 1);

  REQUIRE(level.setNavSurfaceFromFloor() == ARX_OK);
  const std::optional<pistoris::NavSurface> surface = test::navSurface(level);
  REQUIRE(surface.has_value());
  CHECK(surface->vertices.size() == 4);
  CHECK(surface->triangles.size() == 2);
  for (const pistoris::Vertex& vertex : surface->vertices) {
    CHECK(vertex.position.y == doctest::Approx(-5.0f));
  }
  CHECK(level.anchorCount() == 2);
  CHECK(level.anchorConnectionCount() == 1);
}

TEST_CASE("PublicLevelNavSurfacePruningPreservesNoOpData") {
  pistoris::LevelModules src = baseLevel();
  addFloor(src, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f);
  pistoris::Level level = makePublicLevel(src);
  pistoris::NavSurface surface{
      {{{0.0f, 0.0f, 0.0f}},
       {{10.0f, 0.0f, 0.0f}},
       {{0.0f, 0.0f, 10.0f}},
       {{20.0f, 0.0f, 0.0f}},
       {{25.0f, 0.0f, 0.0f}},
       {{20.0f, 0.0f, 2.0f}}},
      {{{{0, 1, 2}}}, {{{3, 4, 5}}}},
  };
  REQUIRE(test::setNavSurface(level, surface) == ARX_OK);

  REQUIRE(level.pruneNavSurfaceIslands({.min_component_area_ratio = 0.0f, .min_component_area = 5.0}) == ARX_OK);
  REQUIRE(test::navSurface(level).has_value());
  CHECK(test::navSurface(level)->vertices.size() == 6);
  CHECK(test::navSurface(level)->triangles.size() == 2);

  pistoris::level_debug::NavigationDiagnostics diagnostics;
  REQUIRE(pistoris::level_debug::pruneNavSurfaceIslands(
              level, {.min_component_area_ratio = 0.0f, .min_component_area = 5.01}, diagnostics) == ARX_OK);
  REQUIRE(test::navSurface(level).has_value());
  CHECK(test::navSurface(level)->vertices.size() == 3);
  CHECK(test::navSurface(level)->triangles.size() == 1);
  CHECK(diagnostics.surface_pruning.pruned.size() == 1);

  std::vector<std::uint8_t> glb;
  REQUIRE(pistoris::level_debug::exportNavigationDebugGlb(level, glb, &diagnostics) == ARX_OK);
  const std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
  CHECK(text.find("navigation_debug_surface_pruning") != std::string::npos);
  CHECK(text.find("navigation_debug_surface_pruned") != std::string::npos);
}

TEST_CASE("PublicLevelAnchorComponentPruningPreservesNoOpDataAndRemapsLinks") {
  pistoris::LevelModules src = baseLevel();
  addFloor(src, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f);
  pistoris::Level level = makePublicLevel(src);
  for (std::uint32_t i = 0; i < 6; ++i) {
    REQUIRE(test::addAnchor(level, {{static_cast<float>((i + 1) * 10), 0.0f, 10.0f}, 5.0f, -20.0f, 0, {}}) == i);
  }
  REQUIRE(test::addAnchorConnection(level, {0, 1}) == 0);
  REQUIRE(test::addAnchorConnection(level, {3, 4}) == 1);
  REQUIRE(test::addAnchorConnection(level, {4, 5}) == 2);
  REQUIRE(level.pruneAnchorIslands({.min_component_anchor_ratio = 0.0f, .min_component_anchor_count = 1}) == ARX_OK);
  CHECK(level.anchorCount() == 6);
  CHECK(level.anchorConnectionCount() == 3);

  pistoris::level_debug::NavigationDiagnostics diagnostics;
  REQUIRE(pistoris::level_debug::pruneAnchorIslands(
              level, {.min_component_anchor_ratio = 0.75f, .min_component_anchor_count = 1}, diagnostics) == ARX_OK);
  REQUIRE(level.anchorCount() == 3);
  CHECK(test::anchor(level, 0).position.x == doctest::Approx(40.0f));
  REQUIRE(level.anchorConnectionCount() == 2);
  CHECK(test::anchorConnection(level, 0).first == 0);
  CHECK(test::anchorConnection(level, 0).second == 1);
  CHECK(test::anchorConnection(level, 1).first == 1);
  CHECK(test::anchorConnection(level, 1).second == 2);
  CHECK(diagnostics.anchor_pruning.pruned.size() == 3);

  std::vector<std::uint8_t> glb;
  REQUIRE(pistoris::level_debug::exportNavigationDebugGlb(level, glb, &diagnostics) == ARX_OK);
  const std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
  CHECK(text.find("navigation_debug_anchor_pruning") != std::string::npos);
  CHECK(text.find("navigation_debug_anchor_pruned") != std::string::npos);
}

TEST_CASE("LevelNavigationDebugGlbExportsGenerationDiagnostics") {
  pistoris::LevelModules src = baseLevel();
  addFloor(src, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  pistoris::Level level = makePublicLevel(src);

  pistoris::level_debug::NavigationDiagnostics diagnostics;
  REQUIRE(pistoris::level_debug::generateNavSurface(level, {.radius = 50.0f, .height = -165.0f}, diagnostics) ==
          ARX_OK);
  REQUIRE(!diagnostics.surface.support.empty());
  REQUIRE(!diagnostics.surface.base.empty());

  std::vector<std::uint8_t> glb;
  REQUIRE(pistoris::level_debug::exportNavigationDebugGlb(level, glb, &diagnostics) == ARX_OK);

  const std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
  CHECK(text.find("arx_navigation_support") != std::string::npos);
  CHECK(text.find("navigation_debug_floor_context") != std::string::npos);
  CHECK(text.find("navigation_debug_generation") != std::string::npos);
  CHECK(text.find("navigation_debug_surface_generation") != std::string::npos);
  CHECK(text.find("navigation_debug_surface_base") != std::string::npos);
}

TEST_CASE("LevelNavSurfaceGenerationUsesConsistentNativeUpWinding") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 300.0f, 0.0f, 0.0f, 300.0f);

  REQUIRE(generateNavSurface(level, {.radius = 50.0f, .height = -165.0f}) == ARX_OK);

  REQUIRE(level.navigation.surface.has_value());
  for (const pistoris::NavSurfaceTriangle& triangle : level.navigation.surface->triangles) {
    const ArxVector3& a = level.navigation.surface->vertices[triangle.vertices[0]].position;
    const ArxVector3& b = level.navigation.surface->vertices[triangle.vertices[1]].position;
    const ArxVector3& c = level.navigation.surface->vertices[triangle.vertices[2]].position;
    CHECK(projectedArea2(a, b, c) > 0.0);
  }
}

TEST_CASE("LevelNavSurfaceGenerationKeepsInteriorOfLargeTriangles") {
  pistoris::LevelModules level = baseLevel();
  level.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{300.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 300.0f}}};
  level.geometry.faces.push_back(makeFace(0, 1, 2, {0.0f, -1.0f, 0.0f}));
  level.rooms.face_rooms.push_back(0);

  REQUIRE(generateNavSurface(level, {.radius = 10.0f, .height = -80.0f}) == ARX_OK);

  REQUIRE(level.navigation.surface.has_value());
  CHECK(level.navigation.surface->vertices.size() > 50);
  CHECK(level.navigation.surface->triangles.size() > 50);
}

TEST_CASE("LevelNavSurfaceGenerationRepairsSmallBoundaryDents") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 300.0f, 0.0f, 0.0f, 300.0f);

  LogCapture capture;
  pistoris::setLogCallback(captureLog, &capture);
  REQUIRE(generateNavSurface(level, {.radius = 50.0f, .height = -165.0f}) == ARX_OK);
  pistoris::setLogCallback(nullptr, nullptr);

  REQUIRE(level.navigation.surface.has_value());
  CHECK(capture.last_debug.find("boundary repair") != std::string::npos);
  CHECK(capture.last_debug.find("considered") != std::string::npos);
  CHECK(capture.last_debug.find("added") != std::string::npos);
}

TEST_CASE("LevelNavSurfaceGenerationIgnoresNoPathEvenWhenProxyMaskDoesNot") {
  pistoris::LevelModules level = baseLevel();
  addQuad(level,
          {0.0f, 0.0f, 0.0f},
          {200.0f, 0.0f, 0.0f},
          {200.0f, 0.0f, 200.0f},
          {0.0f, 0.0f, 200.0f},
          {0.0f, -1.0f, 0.0f},
          pistoris::kFaceBitNopath);

  pistoris::Level::NavSurfaceGenOptions options;
  options.radius = 50.0f;
  options.height = -165.0f;
  options.support_ignore_flags = 0;
  CHECK(generateNavSurface(level, options) == ARX_LEVEL_EMPTY_NAVIGATION_RESULT);
}

TEST_CASE("LevelNavSurfaceGenerationRejectsSamplesResolvedOntoNoPathGeometry") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  addQuad(level,
          {0.0f, -80.0f, 0.0f},
          {200.0f, -80.0f, 0.0f},
          {200.0f, -80.0f, 200.0f},
          {0.0f, -80.0f, 200.0f},
          {0.0f, -1.0f, 0.0f},
          pistoris::kFaceBitNopath);

  CHECK(generateNavSurface(level, {.radius = 50.0f, .height = -165.0f}) == ARX_LEVEL_EMPTY_NAVIGATION_RESULT);
}

TEST_CASE("LevelNavSurfaceGenerationDoesNotBridgeTallWall") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addObstacle(level, 100.0f, 0.0f, -120.0f, 0.0f, 100.0f);

  REQUIRE(generateNavSurface(level, {.radius = 40.0f, .height = -165.0f}) == ARX_OK);
  REQUIRE(level.navigation.surface.has_value());

  for (const pistoris::NavSurfaceTriangle& triangle : level.navigation.surface->triangles) {
    bool left = false;
    bool right = false;
    for (std::uint32_t vertex_index : triangle.vertices) {
      float x = level.navigation.surface->vertices[vertex_index].position.x;
      left |= x < 95.0f;
      right |= x > 105.0f;
    }
    CHECK((left && right) == false);
  }
}

TEST_CASE("LevelNavSurfaceComponentPruningRemovesSmallComponents") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 160.0f, 0.0f, 0.0f, 160.0f);
  addFloor(level, 400.0f, 460.0f, 0.0f, 0.0f, 60.0f);

  REQUIRE(generateNavSurface(level, {.radius = 20.0f, .height = -165.0f}) == ARX_OK);
  REQUIRE(level.navigation.surface.has_value());
  CHECK(std::any_of(level.navigation.surface->vertices.begin(),
                    level.navigation.surface->vertices.end(),
                    [](const pistoris::Vertex& vertex) { return vertex.position.x > 300.0f; }));
  REQUIRE(pistoris::navigation::pruneSurfaceComponents(*level.navigation.surface,
                                                       {.min_component_area_ratio = 0.5f, .min_component_area = 0.0}) ==
          pistoris::navigation::Error::kNone);

  for (const pistoris::Vertex& vertex : level.navigation.surface->vertices) {
    CHECK(vertex.position.x < 300.0f);
  }
}

TEST_CASE("LevelNavSurfaceGenerationFailurePreservesExistingSurface") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  addNavSurface(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);

  REQUIRE(generateNavSurface(level, {.radius = 0.0f, .height = -165.0f}) == ARX_INVALID_OPTIONS);

  REQUIRE(level.navigation.surface.has_value());
  CHECK(level.navigation.surface->vertices.size() == 4);
  CHECK(level.navigation.surface->triangles.size() == 2);
}

TEST_CASE("LevelAnchorGenerationSamplesCellCenters") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  addNavSurface(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);

  REQUIRE(generateAnchors(level, {.sample_spacing = 100.0f, .radius = 50.0f, .height = -165.0f}) == ARX_OK);

  REQUIRE(level.navigation.anchors.size() == 4);
  CHECK(level.navigation.anchors[0].position.x == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[0].position.z == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[1].position.x == doctest::Approx(150.0f));
  CHECK(level.navigation.anchors[1].position.z == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[2].position.x == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[2].position.z == doctest::Approx(150.0f));
  CHECK(level.navigation.anchors[3].position.x == doctest::Approx(150.0f));
  CHECK(level.navigation.anchors[3].position.z == doctest::Approx(150.0f));
}

TEST_CASE("LevelAnchorGenerationSamplesWholeSurfaceIndependentOfTriangleTiling") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  addTiledNavSurface(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f, 50.0f);

  REQUIRE(generateAnchors(level, {.sample_spacing = 100.0f, .radius = 50.0f, .height = -165.0f}) == ARX_OK);

  REQUIRE(level.navigation.anchors.size() == 4);
  CHECK(level.navigation.anchors[0].position.x == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[0].position.z == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[1].position.x == doctest::Approx(150.0f));
  CHECK(level.navigation.anchors[1].position.z == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[2].position.x == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[2].position.z == doctest::Approx(150.0f));
  CHECK(level.navigation.anchors[3].position.x == doctest::Approx(150.0f));
  CHECK(level.navigation.anchors[3].position.z == doctest::Approx(150.0f));
}

TEST_CASE("LevelAnchorGenerationKeepsOneAnchorPerSampleBandAtMinimumSpacing") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 20.0f, 0.0f, 0.0f, 20.0f);
  addNavSurface(level, 0.0f, 20.0f, 0.0f, 0.0f, 20.0f);

  REQUIRE(generateAnchors(level, {.sample_spacing = 10.0f, .radius = 5.0f, .height = -165.0f}) == ARX_OK);

  REQUIRE(level.navigation.anchors.size() == 4);
  CHECK(level.navigation.anchors[0].position.x == doctest::Approx(5.0f));
  CHECK(level.navigation.anchors[0].position.z == doctest::Approx(5.0f));
  CHECK(level.navigation.anchors[1].position.x == doctest::Approx(15.0f));
  CHECK(level.navigation.anchors[1].position.z == doctest::Approx(5.0f));
  CHECK(level.navigation.anchors[2].position.x == doctest::Approx(5.0f));
  CHECK(level.navigation.anchors[2].position.z == doctest::Approx(15.0f));
  CHECK(level.navigation.anchors[3].position.x == doctest::Approx(15.0f));
  CHECK(level.navigation.anchors[3].position.z == doctest::Approx(15.0f));
}

TEST_CASE("LevelAnchorGenerationRepairsUnsupportedCandidateOntoNearbyNavSurface") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 60.0f, 160.0f, 0.0f, 0.0f, 100.0f);
  addNavSurface(level, 0.0f, 160.0f, 0.0f, 0.0f, 100.0f);

  REQUIRE(generateAnchors(level, {.sample_spacing = 100.0f, .radius = 50.0f, .height = -165.0f}) == ARX_OK);

  bool found_repaired = false;
  for (const pistoris::Anchor& anchor : level.navigation.anchors) {
    if (anchor.position.x == doctest::Approx(62.5f) && anchor.position.z == doctest::Approx(50.0f)) {
      found_repaired = true;
      break;
    }
  }
  CHECK(found_repaired);
}

TEST_CASE("LevelAnchorGenerationRepairsCandidateIntersectingStaticWall") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addObstacle(level, 75.0f, 0.0f, -100.0f, 0.0f, 100.0f);
  addNavSurface(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);

  REQUIRE(generateAnchors(level, {.sample_spacing = 100.0f, .radius = 50.0f, .height = -165.0f}) == ARX_OK);

  for (const pistoris::Anchor& anchor : level.navigation.anchors) {
    CHECK(anchor.position.x != doctest::Approx(50.0f));
  }
}

TEST_CASE("LevelAnchorGenerationRejectsCandidatesResolvedOntoNoPathGeometry") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  addQuad(level,
          {0.0f, -80.0f, 0.0f},
          {200.0f, -80.0f, 0.0f},
          {200.0f, -80.0f, 200.0f},
          {0.0f, -80.0f, 200.0f},
          {0.0f, -1.0f, 0.0f},
          pistoris::kFaceBitNopath);
  addNavSurface(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);

  addDefaultAnchors(level);
  REQUIRE(generateAnchors(level, {.sample_spacing = 100.0f, .radius = 50.0f, .height = -165.0f}) ==
          ARX_LEVEL_EMPTY_NAVIGATION_RESULT);

  REQUIRE(level.navigation.anchors.size() == 2);
  CHECK(level.navigation.anchors[0].position.x == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[1].position.x == doctest::Approx(150.0f));
}

TEST_CASE("LevelAnchorGenerationFailurePreservesExistingNavigation") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  addNavSurface(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  addDefaultAnchors(level);
  level.navigation.connections.push_back({0, 1});

  REQUIRE(generateAnchors(level, {.sample_spacing = 100.0f, .radius = 0.0f, .height = -165.0f}) == ARX_INVALID_OPTIONS);

  REQUIRE(level.navigation.anchors.size() == 2);
  CHECK(level.navigation.anchors[0].position.x == doctest::Approx(50.0f));
  CHECK(level.navigation.anchors[1].position.x == doctest::Approx(150.0f));
  REQUIRE(level.navigation.connections.size() == 1);
  CHECK(level.navigation.connections[0].first == 0);
  CHECK(level.navigation.connections[0].second == 1);
}

TEST_CASE("LevelAnchorConnectionsUseStaticCylinderTraversalOnFlatFloor") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addDefaultAnchors(level);

  REQUIRE(generateAnchorConnections(level) == ARX_OK);
  REQUIRE(level.navigation.connections.size() == 1);
  CHECK(level.navigation.connections[0].first == 0);
  CHECK(level.navigation.connections[0].second == 1);
}

TEST_CASE("LevelAnchorConnectionsReplaceStaleInvalidLinks") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addDefaultAnchors(level);
  level.navigation.connections.push_back({0, 0});

  REQUIRE(generateAnchorConnections(level) == ARX_OK);
  REQUIRE(level.navigation.connections.size() == 1);
  CHECK(level.navigation.connections[0].first == 0);
  CHECK(level.navigation.connections[0].second == 1);
}

TEST_CASE("LevelAnchorConnectionsRespectMaxDistance") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addDefaultAnchors(level);

  REQUIRE(generateAnchorConnections(level, {.max_distance = 75.0f}) == ARX_OK);
  CHECK(level.navigation.connections.empty());
}

TEST_CASE("LevelAnchorConnectionsBlockOnTallStaticObstacle") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addObstacle(level, 100.0f, 0.0f, -100.0f, 0.0f, 100.0f);
  addDefaultAnchors(level);

  pistoris::navigation::NavigationDiagnostics diagnostics;
  REQUIRE(generateAnchorConnections(level, {}, nullptr, &diagnostics) == ARX_OK);
  CHECK(level.navigation.connections.empty());
  REQUIRE(diagnostics.connections.rejected_connections.size() == 1);
  CHECK(diagnostics.connections.rejected_connections[0].reason !=
        pistoris::navigation::AnchorConnectionRejectedDebugReason::kInvalid);
}

TEST_CASE("LevelAnchorConnectionsBlockWallAwayFromVertices") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 900.0f);
  addObstacle(level, 100.0f, 0.0f, -100.0f, 0.0f, 900.0f);
  level.navigation.anchors.push_back({{50.0f, 0.0f, 450.0f}, 50.0f, -80.0f, 0, {}});
  level.navigation.anchors.push_back({{150.0f, 0.0f, 450.0f}, 50.0f, -80.0f, 0, {}});

  REQUIRE(generateAnchorConnections(level) == ARX_OK);
  CHECK(level.navigation.connections.empty());
}

TEST_CASE("LevelAnchorConnectionsScaleEndpointRadiusForConnectionPrefilter") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 200.0f);
  addObstacle(level, 95.0f, 0.0f, -100.0f, 0.0f, 200.0f);
  level.navigation.anchors.push_back({{50.0f, 0.0f, 150.0f}, 50.0f, -80.0f, 0, {}});
  level.navigation.anchors.push_back({{150.0f, 0.0f, 150.0f}, 50.0f, -80.0f, 0, {}});

  pistoris::LevelModules full_radius = level;
  pistoris::navigation::NavigationDiagnostics full_diagnostics;
  REQUIRE(generateAnchorConnections(
              full_radius, {.max_distance = 150.0f, .radius_scale = 1.0f}, nullptr, &full_diagnostics) == ARX_OK);
  CHECK(!full_diagnostics.connections.skipped_endpoints.empty());

  pistoris::navigation::NavigationDiagnostics scaled_diagnostics;
  REQUIRE(generateAnchorConnections(
              level, {.max_distance = 150.0f, .radius_scale = 0.5f}, nullptr, &scaled_diagnostics) == ARX_OK);
  CHECK(scaled_diagnostics.connections.skipped_endpoints.empty());
}

TEST_CASE("LevelAnchorConnectionsAllowStepWithinConfiguredLimit") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addObstacle(level, 100.0f, 0.0f, -40.0f, 0.0f, 100.0f);
  addDefaultAnchors(level);

  REQUIRE(generateAnchorConnections(level) == ARX_OK);
  REQUIRE(level.navigation.connections.size() == 1);
}

TEST_CASE("LevelAnchorConnectionsRejectStepAboveConfiguredLimit") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addObstacle(level, 100.0f, 0.0f, -70.0f, 0.0f, 100.0f);
  addDefaultAnchors(level);

  REQUIRE(generateAnchorConnections(level) == ARX_OK);
  CHECK(level.navigation.connections.empty());
}

TEST_CASE("LevelAnchorConnectionsDoNotLetStackedFloorsBlockSameLayer") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addFloor(level, 0.0f, 200.0f, -200.0f, 0.0f, 100.0f);
  addDefaultAnchors(level);

  REQUIRE(generateAnchorConnections(level) == ARX_OK);
  REQUIRE(level.navigation.connections.size() == 1);
}

TEST_CASE("LevelAnchorConnectionsAllowAnchorsSlightlyAboveGround") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addQuad(level,
          {0.0f, -30.0f, 0.0f},
          {200.0f, -30.0f, 0.0f},
          {200.0f, -30.0f, 100.0f},
          {0.0f, -30.0f, 100.0f},
          {0.0f, -1.0f, 0.0f},
          pistoris::kFaceBitNocol);
  level.navigation.anchors.push_back({{50.0f, -20.0f, 50.0f}, 50.0f, -80.0f, 0, {}});
  level.navigation.anchors.push_back({{150.0f, -20.0f, 50.0f}, 50.0f, -80.0f, 0, {}});

  REQUIRE(generateAnchorConnections(level) == ARX_OK);
  REQUIRE(level.navigation.connections.size() == 1);
}

TEST_CASE("LevelAnchorConnectionsAllowAnchorsSlightlyBelowGround") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, -2.0f, 0.0f, 100.0f);
  addQuad(level,
          {0.0f, 0.0f, 0.0f},
          {200.0f, 0.0f, 0.0f},
          {200.0f, 0.0f, 100.0f},
          {0.0f, 0.0f, 100.0f},
          {0.0f, -1.0f, 0.0f},
          pistoris::kFaceBitNocol);
  level.navigation.anchors.push_back({{50.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}});
  level.navigation.anchors.push_back({{150.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}});

  REQUIRE(generateAnchorConnections(level) == ARX_OK);
  REQUIRE(level.navigation.connections.size() == 1);
}

TEST_CASE("LevelAnchorConnectionsDetectSupportInsideLargeTriangle") {
  pistoris::LevelModules level = baseLevel();
  level.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1000.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1000.0f}}};
  level.geometry.faces.push_back(makeFace(0, 1, 2, {0.0f, -1.0f, 0.0f}));
  level.rooms.face_rooms.push_back(0);
  level.navigation.anchors.push_back({{100.0f, 0.0f, 700.0f}, 10.0f, -80.0f, 0, {}});
  level.navigation.anchors.push_back({{200.0f, 0.0f, 650.0f}, 10.0f, -80.0f, 0, {}});

  REQUIRE(generateAnchorConnections(level, {.max_distance = 150.0f}) == ARX_OK);
  REQUIRE(level.navigation.connections.size() == 1);
}

TEST_CASE("LevelAnchorConnectionsSkipAnchorsTooFarAboveGround") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 200.0f, 0.0f, 0.0f, 100.0f);
  addQuad(level,
          {0.0f, -40.0f, 0.0f},
          {200.0f, -40.0f, 0.0f},
          {200.0f, -40.0f, 100.0f},
          {0.0f, -40.0f, 100.0f},
          {0.0f, -1.0f, 0.0f},
          pistoris::kFaceBitNocol);
  level.navigation.anchors.push_back({{50.0f, -35.0f, 50.0f}, 50.0f, -50.0f, 0, {}});
  level.navigation.anchors.push_back({{150.0f, -35.0f, 50.0f}, 50.0f, -50.0f, 0, {}});

  LogCapture capture;
  pistoris::setLogCallback(captureLog, &capture);
  REQUIRE(generateAnchorConnections(level) == ARX_OK);
  pistoris::setLogCallback(nullptr, nullptr);

  CHECK(level.navigation.connections.empty());
  CHECK(capture.warnings == 1);
  CHECK(capture.last.find("skipped 2 anchor") != std::string::npos);
}

TEST_CASE("LevelAnchorConnectionsSkipUnsupportedAnchorsAndWarnOnce") {
  pistoris::LevelModules level = baseLevel();
  addFloor(level, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f);
  addObstacle(level, 300.0f, 0.0f, -100.0f, 0.0f, 100.0f);
  level.navigation.anchors.push_back({{50.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}});
  level.navigation.anchors.push_back({{250.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}});

  LogCapture capture;
  pistoris::setLogCallback(captureLog, &capture);
  REQUIRE(generateAnchorConnections(level, {.max_distance = 300.0f}) == ARX_OK);
  pistoris::setLogCallback(nullptr, nullptr);

  CHECK(level.navigation.anchors.size() == 2);
  CHECK(level.navigation.connections.empty());
  CHECK(capture.warnings == 1);
  CHECK(capture.last.find("skipped 1 anchor") != std::string::npos);
}
