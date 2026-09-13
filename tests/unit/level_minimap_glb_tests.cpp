// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/minimap.h"
#include "external/glb/writer.h"
#include "image_helpers.h"
#include "modules/minimap.h"
#include "stb/stb_image.h"
#include "utils/encoded_image.h"
#include "utils/math/mat4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::vector<std::uint8_t> makeMinimapGlb(std::span<const pistoris::glb::Vec2> texcoords,
                                         std::span<const std::uint32_t> indices) {
  const std::array<pistoris::glb::Vec3, 4> positions = {
      pistoris::glb::Vec3{0.0f, 0.0f, -1.0f},
      pistoris::glb::Vec3{2.0f, 0.0f, -1.0f},
      pistoris::glb::Vec3{2.0f, 0.0f, 0.0f},
      pistoris::glb::Vec3{0.0f, 0.0f, 0.0f},
  };
  std::vector<std::uint8_t> source = makeSolidTestBmp(2, 1, 255, 0, 0);
  source[57] = 0;
  source[58] = 255;
  source[59] = 0;
  std::vector<std::uint8_t> image;
  REQUIRE(pistoris::image::transcodeToPng(source, image) == pistoris::image::Error::kNone);

  pistoris::glb::Builder builder;
  const int texture = builder.addEmbeddedTexture("minimap", "image/png", image);
  const int material = builder.addMaterial("minimap", texture, pistoris::kFaceBitDoublesided, 1.0f);
  pistoris::glb::Primitive primitive;
  primitive.indices = builder.addAccessor(indices, cgltf_component_type_r_32u, cgltf_type_scalar);
  primitive.material = material;
  primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(positions));
  if (!texcoords.empty())
    primitive.attributes.emplace_back("TEXCOORD_0",
                                      builder.addAccessor(texcoords, cgltf_component_type_r_32f, cgltf_type_vec2));
  const int mesh = builder.addMesh("minimap", {std::move(primitive)});
  builder.addRoot(builder.addNode("arx_minimap__map", mesh));

  std::vector<std::uint8_t> encoded;
  REQUIRE(builder.write(encoded) == ARX_OK);
  return encoded;
}

std::array<std::uint8_t, 4> pixel(std::span<const std::uint8_t> encoded, int x, int y, int& width, int& height) {
  int components = 0;
  stbi_uc* decoded =
      stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height, &components, 4);
  REQUIRE(decoded != nullptr);
  const std::size_t offset =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
  const std::array<std::uint8_t, 4> result = {
      decoded[offset], decoded[offset + 1], decoded[offset + 2], decoded[offset + 3]};
  stbi_image_free(decoded);
  return result;
}

struct WarningCapture {
  std::vector<std::string> messages;

  WarningCapture() {
    pistoris::setLogCallback(
        [](ArxLogLevel level, const char* message, void* userdata) {
          if (level == ARX_LOG_WARN && message != nullptr)
            static_cast<WarningCapture*>(userdata)->messages.emplace_back(message);
        },
        this);
  }

  ~WarningCapture() { pistoris::setLogCallback(nullptr, nullptr); }

  bool contains(std::string_view text) const {
    for (const std::string& message : messages)
      if (message.find(text) != std::string::npos) return true;
    return false;
  }
};

pistoris::MinimapData importMinimap(std::span<const std::uint8_t> encoded) {
  pistoris::glb::Asset asset;
  REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
  REQUIRE(asset.data()->nodes_count == 1);
  pistoris::MinimapData minimap;
  REQUIRE(pistoris::glb_level::importMinimap(
              asset, asset.data()->nodes[0], pistoris::math::kIdentityMat4, {.arx_per_glb_unit = 1.0}, minimap) ==
          pistoris::glb_level::MinimapImportError::kNone);
  return minimap;
}

}  // namespace

TEST_SUITE("Level minimap GLB") {
  TEST_CASE("Imports unusual minimap topology through its horizontal bounds") {
    constexpr std::array<pistoris::glb::Vec2, 4> kTexcoords = {
        pistoris::glb::Vec2{0.0f, 0.0f},
        pistoris::glb::Vec2{1.0f, 0.0f},
        pistoris::glb::Vec2{1.0f, 1.0f},
        pistoris::glb::Vec2{0.0f, 1.0f},
    };
    constexpr std::array<std::uint32_t, 6> kOverlapping = {0, 1, 2, 0, 1, 3};
    WarningCapture warnings;
    const pistoris::MinimapData minimap = importMinimap(makeMinimapGlb(kTexcoords, kOverlapping));
    CHECK(minimap.world_xz_bounds.min.x == 0.0f);
    CHECK(minimap.world_xz_bounds.min.y == 0.0f);
    CHECK(minimap.world_xz_bounds.max.x == 2.0f);
    CHECK(minimap.world_xz_bounds.max.y == 1.0f);
    CHECK(warnings.contains("rectangular topology"));
  }

  TEST_CASE("Canonicalizes exact quarter-turn minimap UVs") {
    constexpr std::array<pistoris::glb::Vec2, 4> kClockwise = {
        pistoris::glb::Vec2{0.0f, 1.0f},
        pistoris::glb::Vec2{0.0f, 0.0f},
        pistoris::glb::Vec2{1.0f, 0.0f},
        pistoris::glb::Vec2{1.0f, 1.0f},
    };
    constexpr std::array<std::uint32_t, 6> kIndices = {0, 1, 2, 0, 2, 3};
    WarningCapture warnings;
    const pistoris::MinimapData minimap = importMinimap(makeMinimapGlb(kClockwise, kIndices));
    CHECK(warnings.messages.empty());

    int width = 0;
    int height = 0;
    CHECK(pixel(minimap.encoded_image, 0, 0, width, height) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
    CHECK(width == 1);
    CHECK(height == 2);
    CHECK(pixel(minimap.encoded_image, 0, 1, width, height) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
  }

  TEST_CASE("Imports minimaps without UVs using canonical image orientation") {
    constexpr std::array<std::uint32_t, 6> kIndices = {0, 1, 2, 0, 2, 3};
    WarningCapture warnings;
    const pistoris::MinimapData minimap = importMinimap(makeMinimapGlb({}, kIndices));
    CHECK_FALSE(minimap.encoded_image.empty());
    CHECK(warnings.contains("no TEXCOORD_0"));
  }
}
