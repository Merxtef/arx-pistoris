// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/utils/texture.h"
#include "external/glb/writer.h"

#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

bool near(float left, float right) { return std::abs(left - right) <= 1.0e-5f; }

}  // namespace

TEST_SUITE("Shared GLB contracts") {
  TEST_CASE("Decodes the effective texture coordinate transform") {
    cgltf_image image{};
    cgltf_texture texture{};
    texture.image = &image;
    cgltf_texture_view view{};
    view.texture = &texture;
    view.texcoord = 1;
    view.has_transform = 1;
    view.transform.offset[0] = 0.25f;
    view.transform.offset[1] = -0.5f;
    view.transform.scale[0] = 2.0f;
    view.transform.scale[1] = 3.0f;
    view.transform.rotation = 1.57079632679f;
    view.transform.has_texcoord = 1;
    view.transform.texcoord = 4;

    pistoris::glb::TextureBinding binding;
    REQUIRE(pistoris::glb::decodeTextureBinding(view, binding) == pistoris::glb::TextureBindingError::kNone);
    CHECK(binding.image == &image);
    CHECK(binding.texcoord == 4);
    CHECK(binding.transformed);
    const pistoris::glb::Vec2 transformed = pistoris::glb::transformTexcoord(binding, {1.0f, 2.0f});
    CHECK(near(transformed.x, -5.75f));
    CHECK(near(transformed.y, 1.5f));
  }

  TEST_CASE("Validates only required extensions against the route allowlist") {
    char supported_name[] = "KHR_texture_transform";
    char unsupported_name[] = "EXT_unknown";
    char* required[] = {supported_name};
    cgltf_data data{};
    data.extensions_required = required;
    data.extensions_required_count = 1;
    constexpr std::string_view kSupported[] = {"KHR_texture_transform"};
    CHECK(pistoris::glb::validateRequiredExtensions(data, kSupported) == ARX_OK);

    required[0] = unsupported_name;
    CHECK(pistoris::glb::validateRequiredExtensions(data, kSupported) == ARX_GLB_UNSUPPORTED_FEATURE);
    required[0] = nullptr;
    CHECK(pistoris::glb::validateRequiredExtensions(data, kSupported) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("Writes unlit materials through neutral material options") {
    pistoris::glb::Builder builder;
    builder.addMaterial("illustration", -1, {.double_sided = true, .unlit = true});
    builder.addRoot(builder.addNode("root"));
    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    REQUIRE(asset.data()->materials_count == 1);
    CHECK(asset.data()->materials[0].double_sided);
    CHECK(asset.data()->materials[0].unlit);
  }
}
