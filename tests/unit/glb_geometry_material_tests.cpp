// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"

#include "external/glb/geometry_material.h"
#include "external/glb/utils/texture.h"

#include <array>
#include <string>

TEST_SUITE("glb::geometry_material") {
  TEST_CASE("GLB and material-name double-sided declarations are additive") {
    using namespace pistoris;

    struct Case {
      const char* name;
      cgltf_bool glb_double_sided;
      bool expected;
    };
    constexpr std::array kCases = {
        Case{"no_tex", 0, false},
        Case{"no_tex__DOUBLESIDED", 0, true},
        Case{"no_tex", 1, true},
        Case{"no_tex__DOUBLESIDED", 1, true},
    };
    for (const Case& test : kCases) {
      cgltf_material source{};
      source.name = const_cast<char*>(test.name);
      source.double_sided = test.glb_double_sided;
      source.alpha_mode = cgltf_alpha_mode_opaque;
      glb::GeometryMaterial material;
      REQUIRE(glb::decodeGeometryMaterial(&source, material) == glb::GeometryMaterialError::kNone);
      CHECK(((material.flags & kFaceBitDoublesided) != 0) == test.expected);
    }
  }

  TEST_CASE("Referenced images override no_tex and MASK settings are normalized") {
    using namespace pistoris;

    char name[] = "no_tex";
    char uri[] = "textures/cutout.png";
    cgltf_image image{};
    image.uri = uri;
    cgltf_texture texture{};
    texture.image = &image;
    cgltf_material source{};
    source.name = name;
    source.alpha_mode = cgltf_alpha_mode_mask;
    source.alpha_cutoff = 0.25f;
    source.has_pbr_metallic_roughness = 1;
    source.pbr_metallic_roughness.base_color_factor[3] = 0.75f;
    source.pbr_metallic_roughness.base_color_texture.texture = &texture;

    glb::GeometryMaterial material;
    glb::GeometryMaterialInfo info;
    REQUIRE(glb::decodeGeometryMaterial(&source, material, &info) == glb::GeometryMaterialError::kNone);
    REQUIRE(material.texture.has_value());
    CHECK(material.texture->image == &image);
    CHECK(std::string(material.texture->fallback_path) == "texture");
    CHECK(info.no_tex_with_image);
    CHECK(info.normalized_mask);
    CHECK((material.flags & kFaceBitTrans) == 0);
  }

  TEST_CASE("Geometry material validation is shared and strict") {
    using namespace pistoris;

    char name[] = "stone";
    cgltf_material source{};
    source.name = name;
    source.alpha_mode = cgltf_alpha_mode_blend;
    source.has_pbr_metallic_roughness = 1;
    source.pbr_metallic_roughness.base_color_factor[3] = 2.0f;
    glb::GeometryMaterial material;
    CHECK(glb::decodeGeometryMaterial(&source, material) == glb::GeometryMaterialError::kBadAlpha);

    source.pbr_metallic_roughness.base_color_factor[3] = 0.5f;
    source.extensions_count = 1;
    CHECK(glb::decodeGeometryMaterial(&source, material) == glb::GeometryMaterialError::kUnsupportedFeature);

    source.extensions_count = 0;
    source.name = const_cast<char*>("stone__TRANSVAL_0.5");
    source.alpha_mode = cgltf_alpha_mode_opaque;
    CHECK(glb::decodeGeometryMaterial(&source, material) == glb::GeometryMaterialError::kBadMaterial);

    source.alpha_mode = cgltf_alpha_mode_mask;
    source.alpha_cutoff = 0.5f;
    CHECK(glb::decodeGeometryMaterial(&source, material) == glb::GeometryMaterialError::kBadMaterial);
  }

  TEST_CASE("BLEND distinguishes scalar transparency from texture alpha") {
    using namespace pistoris;

    struct Case {
      const char* name;
      float alpha;
      bool image;
      bool expected_trans;
      float expected_transval;
      bool expected_normalized_blend;
    };
    constexpr std::array kCases = {
        Case{"stone", 1.0f, true, false, 0.0f, true},
        Case{"stone", 1.0f, false, false, 0.0f, true},
        Case{"no_tex", 1.0f, false, false, 0.0f, true},
        Case{"no_tex", 0.25f, false, true, 0.75f, false},
        Case{"stone__TRANS", 1.0f, true, true, 0.0f, false},
        Case{"stone__TRANSVAL_0.5", 1.0f, true, true, 0.5f, false},
    };

    char uri[] = "textures/stone.png";
    cgltf_image image{};
    image.uri = uri;
    cgltf_texture texture{};
    texture.image = &image;
    for (const Case& test : kCases) {
      CAPTURE(test.name);
      CAPTURE(test.alpha);
      CAPTURE(test.image);
      cgltf_material source{};
      source.name = const_cast<char*>(test.name);
      source.alpha_mode = cgltf_alpha_mode_blend;
      source.has_pbr_metallic_roughness = 1;
      source.pbr_metallic_roughness.base_color_factor[3] = test.alpha;
      if (test.image) source.pbr_metallic_roughness.base_color_texture.texture = &texture;

      glb::GeometryMaterial material;
      glb::GeometryMaterialInfo info;
      REQUIRE(glb::decodeGeometryMaterial(&source, material, &info) == glb::GeometryMaterialError::kNone);
      CHECK(((material.flags & kFaceBitTrans) != 0) == test.expected_trans);
      CHECK(material.transval == doctest::Approx(test.expected_transval));
      CHECK(info.normalized_blend == test.expected_normalized_blend);
    }
  }

  TEST_CASE("Geometry material export preserves native semantics") {
    using namespace pistoris;

    const glb::ExportedGeometryMaterial ordinary =
        glb::exportGeometryMaterial("stone", kFaceBitTrans | kFaceBitDoublesided, 0.25f, glb::TextureAlpha::kAbsent);
    CHECK(ordinary.name == "stone__DOUBLESIDED__TRANS__TRANSVAL_0.25");
    CHECK(ordinary.alpha == doctest::Approx(0.75f));
    CHECK_FALSE(ordinary.nonstandard_transval);

    const glb::ExportedGeometryMaterial native_blend =
        glb::exportGeometryMaterial("stone", kFaceBitTrans, 2.0f, glb::TextureAlpha::kAbsent);
    CHECK(native_blend.alpha == 1.0f);
    CHECK(native_blend.nonstandard_transval);

    const glb::ExportedGeometryMaterial cutout =
        glb::exportGeometryMaterial("stone", 0, 0.0f, glb::TextureAlpha::kPresent);
    CHECK(cutout.alpha_cutout);
    CHECK_FALSE(cutout.unknown_alpha);
  }
}
