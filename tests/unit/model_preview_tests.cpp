// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/model.hpp"

#include "external/glb/container.h"
#include "model_helpers.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

const cgltf_node* findNode(const cgltf_data& data, std::string_view name) {
  const std::span<const cgltf_node> nodes(data.nodes, data.nodes_count);
  for (const cgltf_node& node : nodes)
    if (node.name && std::string_view(node.name) == name) return &node;
  return nullptr;
}

pistoris::Model makePreviewModel(bool with_resource_path = true) {
  pistoris::Model model;
  REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
  if (with_resource_path) REQUIRE(model.setResourcePath("model:npc:human_base") == ARX_OK);
  return pistoris::Model(model);
}

pistoris::Level makePreviewLevel() {
  pistoris::Level level;
  pistoris::RoomIndex room = pistoris::kInvalidRoomIndex;
  const ArxLevelRoom submitted_room{view("room")};
  REQUIRE(level.addRoom(submitted_room, room) == ARX_OK);

  const std::array<ArxLevelVertex, 3> vertices = {
      ArxLevelVertex{{0.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{100.0f, 0.0f, 0.0f}},
      ArxLevelVertex{{0.0f, 0.0f, 100.0f}},
  };
  ArxLevelFace face{};
  face.texture = ARX_NO_TEXTURE;
  face.room = room;
  for (std::size_t index = 0; index < 3; ++index) {
    face.corners[index].vertex = static_cast<pistoris::VertexIndex>(index);
    face.corners[index].normal = {0.0f, -1.0f, 0.0f};
  }
  const ArxLevelMeshInput mesh{vertices.data(), vertices.size(), &face, 1, nullptr, 0};
  REQUIRE(level.replaceMesh(mesh) == ARX_OK);

  for (std::string_view name : {"human", "human_1"}) {
    const ArxLevelEntity entity{
        .class_path = view("graph/obj3d/interactive/npc/human_base/human_base"),
        .position = {},
        .rotation = {},
        .name = view(name),
    };
    pistoris::EntityIndex index = pistoris::kInvalidEntityIndex;
    REQUIRE(level.addEntity(entity, index) == ARX_OK);
  }
  REQUIRE(level.validate() == ARX_OK);
  return pistoris::Level(level);
}

}  // namespace

TEST_SUITE("Model Level previews") {
  TEST_CASE("Standalone preview is a static Level entity") {
    pistoris::Model model = makePreviewModel();
    pistoris::Model::LevelPreviewGlbOptions options;
    options.class_path = "model:npc:human_base:human_kultar";
    options.asset_name = "human";

    std::vector<std::uint8_t> encoded;
    REQUIRE(model.exportLevelPreviewGlb(encoded, options) == ARX_OK);

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    const cgltf_data& data = *asset.data();
    const cgltf_node* entity = findNode(data, "arx_entity__000__human");
    const cgltf_node* class_path = findNode(data, "CLASS_model:npc:human_base__human");
    REQUIRE(entity != nullptr);
    REQUIRE(class_path != nullptr);
    CHECK(entity->mesh != nullptr);
    CHECK(class_path->parent == entity);
    CHECK(data.skins_count == 0);
    CHECK(data.animations_count == 0);
    CHECK(findNode(data, "arx_model_origin__origin") == nullptr);
    REQUIRE(entity->mesh->primitives_count == 1);
    const cgltf_primitive& primitive = entity->mesh->primitives[0];
    for (const cgltf_attribute& attribute :
         std::span<const cgltf_attribute>(primitive.attributes, primitive.attributes_count)) {
      CHECK(attribute.type != cgltf_attribute_type_joints);
      CHECK(attribute.type != cgltf_attribute_type_weights);
      CHECK(attribute.type != cgltf_attribute_type_custom);
    }
  }

  TEST_CASE("Standalone preview preserves an unspecified class as a visible placeholder") {
    pistoris::Model model = makePreviewModel();

    std::vector<std::uint8_t> encoded;
    REQUIRE(model.exportLevelPreviewGlb(encoded) == ARX_OK);

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    CHECK(findNode(*asset.data(), "CLASS_<class-path>__asset") != nullptr);
  }

  TEST_CASE("Level entities reuse previews by Model resource identity") {
    pistoris::Level level = makePreviewLevel();
    pistoris::Model preview = makePreviewModel();
    REQUIRE(preview.setResourcePath("model:npc:human_base:human_kultar") == ARX_OK);
    pistoris::Model anonymous = makePreviewModel(false);
    const std::array<const pistoris::Model*, 2> previews = {&preview, &anonymous};

    ArxLevelModelPreviewReport report{};
    std::vector<std::uint8_t> encoded;
    REQUIRE(level.exportGlb(encoded, previews, &report) == ARX_OK);
    CHECK(report.mapped_models == 1);
    CHECK(report.previewed_entities == 2);
    CHECK(report.skipped_anonymous_models == 1);

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    const cgltf_data& data = *asset.data();
    const cgltf_node* first = findNode(data, "arx_entity__000__human");
    const cgltf_node* second = findNode(data, "arx_entity__001__human_1");
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(first->mesh != nullptr);
    CHECK(first->mesh == second->mesh);
  }

  TEST_CASE("Level entity rotation applies to its preview mesh") {
    constexpr float kHalfSqrtTwo = 0.70710678f;
    pistoris::Level level = makePreviewLevel();
    const ArxLevelEntity entity{
        .class_path = view("graph/obj3d/interactive/npc/human_base/human_base"),
        .position = {},
        .rotation = {kHalfSqrtTwo, 0.0f, kHalfSqrtTwo, 0.0f},
        .name = view("human"),
    };
    REQUIRE(level.setEntity(0, entity) == ARX_OK);
    pistoris::Model preview = makePreviewModel();
    const std::array<const pistoris::Model*, 1> previews = {&preview};

    std::vector<std::uint8_t> encoded;
    REQUIRE(level.exportGlb(encoded, previews) == ARX_OK);

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    const cgltf_node* exported = findNode(*asset.data(), "arx_entity__000__human");
    REQUIRE(exported != nullptr);
    REQUIRE(exported->mesh != nullptr);
    REQUIRE(exported->has_rotation);
    CHECK(exported->rotation[0] == doctest::Approx(0.0f));
    CHECK(exported->rotation[1] == doctest::Approx(-kHalfSqrtTwo));
    CHECK(exported->rotation[2] == doctest::Approx(0.0f));
    CHECK(exported->rotation[3] == doctest::Approx(kHalfSqrtTwo));
  }
}
