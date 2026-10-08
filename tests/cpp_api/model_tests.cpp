// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/obj_location.hpp"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/texture.h"

#include "image_helpers.h"
#include "model_helpers.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

static_assert((pistoris::kModelFaceBitsAll & pistoris::kFaceBitQuad) == 0);

std::string_view stringView(ArxStringView value) { return {value.data, value.size}; }

template <class T, class Location>
T take(pistoris::Result<T, Location>&& result) {
  REQUIRE(result);
  return std::move(*result);
}

std::vector<pistoris::VertexIndex> selectionVertices(const pistoris::Model& model, pistoris::SelectionId id) {
  const auto result = model.selectionVertices(id);
  CHECK(result);
  if (!result) return {};
  return {result->begin(), result->end()};
}

std::vector<pistoris::BoneIndex> selectionBones(const pistoris::Model& model, pistoris::SelectionId id) {
  const auto result = model.selectionBones(id);
  CHECK(result);
  if (!result) return {};
  return {result->begin(), result->end()};
}

std::vector<pistoris::ActionPointIndex> selectionActionPoints(const pistoris::Model& model, pistoris::SelectionId id) {
  const auto result = model.selectionActionPoints(id);
  CHECK(result);
  if (!result) return {};
  return {result->begin(), result->end()};
}

pistoris::SelectionId selectionId(const pistoris::Model& model, std::string_view name) {
  for (pistoris::SelectionId id : model.selectionIds()) {
    const auto selection = model.selection(id);
    CHECK(selection);
    if (selection && stringView(selection->name) == name) return id;
  }
  return pistoris::kInvalidSelectionId;
}

bool selectionIncludesOrigin(const pistoris::Model& model, pistoris::SelectionId id) {
  const auto result = model.selectionIncludesOrigin(id);
  CHECK(result);
  return result && *result;
}

struct LogCapture {
  std::vector<std::string> messages;

  LogCapture() {
    pistoris::setLogCallback(
        [](ArxLogLevel level, const char* message, void* userdata) {
          if (level == ARX_LOG_INFO && message) static_cast<LogCapture*>(userdata)->messages.emplace_back(message);
        },
        this);
  }

  ~LogCapture() { pistoris::setLogCallback(nullptr, nullptr); }
};

struct WarningCapture {
  std::vector<std::string> messages;

  WarningCapture() {
    pistoris::setLogCallback(
        [](ArxLogLevel level, const char* message, void* userdata) {
          if (level == ARX_LOG_WARN && message) static_cast<WarningCapture*>(userdata)->messages.emplace_back(message);
        },
        this);
  }

  ~WarningCapture() { pistoris::setLogCallback(nullptr, nullptr); }
};

}  // namespace

TEST_SUITE("C++ Model API") {
  TEST_CASE("Native import locates an invalid FTL face corner") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.faces[0].vertex_idx.y = static_cast<std::int32_t>(native.vertices.size());

    const pistoris::FtlResult<pistoris::Model> result = pistoris::Model::importNative(native);

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_FTL_BAD_FACE_VERT_IDX);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->element == pistoris::FtlElement::kFace);
    CHECK(result.error()->location()->index == 0);
    CHECK(result.error()->location()->subindex == 1);
  }

  TEST_CASE("Clearing vertices cascades to faces and textures clear independently") {
    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));
    REQUIRE(model.textureCount() != 0);

    model.clearVertices();

    CHECK(model.vertexCount() == 0);
    CHECK(model.faceCount() == 0);
    CHECK(model.textureCount() != 0);
    model.clearTextures();
    CHECK(model.textureCount() == 0);
  }

  TEST_CASE("Copies grouped Model geometry and affiliation arrays") {
    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));
    const std::size_t face_count = model.faceCount();
    const std::size_t vertex_count = model.vertexCount();
    std::vector<std::uint32_t> vertex_indices(face_count * 3U);
    std::vector<float> uvs(face_count * 6U);
    std::vector<float> corner_normals(face_count * 9U);
    std::vector<pistoris::TextureIndex> textures(face_count);
    std::vector<float> transvals(face_count);
    std::vector<float> face_normals(face_count * 3U);
    std::vector<pistoris::FaceType> flags(face_count);

    REQUIRE(model.copyFaces({.vertex_indices = std::span<std::uint32_t>(vertex_indices),
                             .uvs = std::span<float>(uvs),
                             .corner_normals = std::span<float>(corner_normals),
                             .textures = std::span<pistoris::TextureIndex>(textures),
                             .transvals = std::span<float>(transvals),
                             .face_normals = std::span<float>(face_normals),
                             .flags = std::span<pistoris::FaceType>(flags)}));
    for (std::size_t face_index = 0; face_index < face_count; ++face_index) {
      const ArxModelFace face = model.faces()[face_index];
      CHECK(textures[face_index] == face.texture);
      CHECK(transvals[face_index] == face.transval);
      CHECK(flags[face_index] == face.flags);
      CHECK(face_normals[face_index * 3U] == face.normal.x);
      CHECK(face_normals[face_index * 3U + 1U] == face.normal.y);
      CHECK(face_normals[face_index * 3U + 2U] == face.normal.z);
      for (std::size_t corner = 0; corner < 3U; ++corner) {
        const std::size_t item = face_index * 3U + corner;
        CHECK(vertex_indices[item] == face.corners[corner].vertex);
        CHECK(uvs[item * 2U] == face.corners[corner].u);
        CHECK(uvs[item * 2U + 1U] == face.corners[corner].v);
        CHECK(corner_normals[item * 3U] == face.corners[corner].normal.x);
        CHECK(corner_normals[item * 3U + 1U] == face.corners[corner].normal.y);
        CHECK(corner_normals[item * 3U + 2U] == face.corners[corner].normal.z);
      }
    }

    std::vector<float> positions(vertex_count * 3U);
    REQUIRE(model.copyVertexPositions(positions));
    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
      const ArxModelVertex value = model.vertices()[vertex];
      CHECK(positions[vertex * 3U] == value.position.x);
      CHECK(positions[vertex * 3U + 1U] == value.position.y);
      CHECK(positions[vertex * 3U + 2U] == value.position.z);
    }

    std::vector<pistoris::TextureIndex> face_textures(face_count);
    REQUIRE(model.copyFaceTextures(face_textures));
    CHECK(face_textures == textures);
    std::vector<pistoris::BoneIndex> vertex_bones(vertex_count);
    REQUIRE(model.copyVertexBones(vertex_bones));
    for (std::size_t index = 0; index < vertex_count; ++index)
      CHECK(vertex_bones[index] == model.vertices()[index].bone);
    std::vector<pistoris::BoneIndex> action_point_bones(model.actionPointCount());
    REQUIRE(model.copyActionPointBones(action_point_bones));
    for (std::size_t index = 0; index < action_point_bones.size(); ++index)
      CHECK(action_point_bones[index] == model.actionPoints()[index].bone);

    std::vector<std::uint64_t> vertex_masks(vertex_count);
    std::vector<std::uint64_t> bone_masks(model.boneCount());
    std::vector<std::uint64_t> action_point_masks(model.actionPointCount());
    REQUIRE(model.copyVertexSelectionMasks(vertex_masks));
    REQUIRE(model.copyBoneSelectionMasks(bone_masks));
    REQUIRE(model.copyActionPointSelectionMasks(action_point_masks));
    for (std::size_t index = 0; index < vertex_masks.size(); ++index) {
      std::uint64_t expected = 0;
      for (pistoris::SelectionId id : model.selectionIds()) {
        const auto members = model.selectionVertices(id);
        REQUIRE(members);
        if (std::find(members->begin(), members->end(), static_cast<pistoris::VertexIndex>(index)) != members->end())
          expected |= std::uint64_t{1} << id;
      }
      CHECK(vertex_masks[index] == expected);
    }

    CHECK(model.copyFaces({}).code() == ARX_INVALID_OPTIONS);
    std::uint32_t empty_destination = 0;
    CHECK(model.copyFaces({.vertex_indices = std::span<std::uint32_t>(&empty_destination, 0)}).code() ==
          ARX_BUFFER_TOO_SMALL);
    std::array<float, 15> overlapping{};
    CHECK(model
              .copyFaces({.uvs = std::span<float>(overlapping.data(), 6),
                          .corner_normals = std::span<float>(overlapping.data() + 3, 9)})
              .code() == ARX_INVALID_OPTIONS);
    CHECK(std::all_of(overlapping.begin(), overlapping.end(), [](float value) { return value == 0.0f; }));
    std::array<std::uint32_t, 2> too_small{77U, 77U};
    CHECK(model.copyFaces({.vertex_indices = std::span<std::uint32_t>(too_small)}).code() == ARX_BUFFER_TOO_SMALL);
    CHECK(too_small == std::array<std::uint32_t, 2>{77U, 77U});
    std::vector<std::uint32_t> too_large(face_count * 3U + 1U, 88U);
    CHECK(model.copyFaces({.vertex_indices = std::span<std::uint32_t>(too_large)}).code() == ARX_INVALID_OPTIONS);
    CHECK(std::all_of(too_large.begin(), too_large.end(), [](std::uint32_t value) { return value == 88U; }));
  }

  TEST_CASE("Returns the first format path for each normalized Model texture") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.texture_containers.push_back(native.texture_containers.front());
    setFtlName("graph/obj3d/textures/my_tex",
               native.texture_containers.back().filename,
               sizeof(native.texture_containers.back().filename));

    std::vector<std::string> sources;
    pistoris::Model model = take(pistoris::Model::importNative(native, &sources));
    CHECK(model.textureCount() == 1);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == "graph/obj3d/textures/my_tex");
  }

  TEST_CASE("Treats empty native texture slots as no texture") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.texture_containers.emplace_back();
    pistoris::ftl::Face untextured = native.faces[0];
    untextured.texture_id = 1;
    native.faces.push_back(untextured);

    std::vector<std::string> sources;
    pistoris::Model model = take(pistoris::Model::importNative(native, &sources));
    REQUIRE(model.validate());
    CHECK(model.textureCount() == 1);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == "graph/obj3d/textures/my_tex");

    const auto faces = model.faces();
    REQUIRE(faces.size() == 2);
    CHECK(faces[0].texture == 0);
    CHECK(faces[1].texture == pistoris::kNoTexture);
  }

  TEST_CASE("Returns OBJ image references rather than normalized logical paths") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
usemtl wall
f 1 2 3
)";
    constexpr std::string_view kMtl = R"(newmtl wall
map_Kd Imported/Textures/WALL.BMP
)";

    std::vector<std::string> sources;
    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl, &sources));
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == "Imported/Textures/WALL.BMP");
    const ArxTextureView texture = model.textures()[0];
    CHECK((stringView(texture.path) == "imported/textures/wall"));
    CHECK((stringView(texture.external_image_extension) == ".bmp"));
  }

  TEST_CASE("Uses filenames as Model identities for absolute OBJ image references") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
usemtl wall
f 1 2 3
)";

    for (const std::string_view source : {"/Imported/Textures/WALL.BMP", "C:/Imported/Textures/WALL.BMP"}) {
      const std::string mtl = "newmtl wall\nmap_Kd " + std::string(source) + "\n";
      std::vector<std::string> sources;
      pistoris::Model model = take(pistoris::Model::importObj(kObj, mtl, &sources));
      REQUIRE(sources.size() == 1);
      CHECK(sources[0] == std::string(source));
      const ArxTextureView texture = model.textures()[0];
      CHECK((stringView(texture.path) == "wall"));
      CHECK((stringView(texture.external_image_extension) == ".bmp"));
    }
  }

  TEST_CASE("Distinguishes dotted OBJ fallbacks from physical image paths") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
v 0 0 1
usemtl stone.old
f 1 2 3
usemtl mapped
f 1 4 2
)";
    constexpr std::string_view kMtl = R"(newmtl mapped
map_Kd stone.old
)";

    std::vector<std::string> sources;
    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl, &sources));
    REQUIRE(model.textureCount() == 2);
    const auto textures = model.textures();
    CHECK((stringView(textures[0].path) == "stone.old"));
    CHECK(stringView(textures[0].external_image_extension).empty());
    CHECK((stringView(textures[1].path) == "stone"));
    CHECK((stringView(textures[1].external_image_extension) == ".old"));
    REQUIRE(sources.size() == 2);
    CHECK(sources[0].empty());
    CHECK(sources[1] == "stone.old");
  }

  TEST_CASE("Discovers and imports named OBJ material libraries") {
    constexpr std::string_view kObj = R"(mtllib materials/main.mtl details.mtl
v 0 0 0
v 1 0 0
v 0 1 0
v 0 0 1
usemtl polished wall
f 1 2 3
usemtl trim
f 1 4 2
)";
    auto paths = pistoris::objMaterialLibraryPaths(kObj);
    REQUIRE(paths);
    REQUIRE(paths->size() == 2);
    CHECK((*paths)[0] == "materials/main.mtl");
    CHECK((*paths)[1] == "details.mtl");

    constexpr std::string_view kMainMtl = R"(newmtl polished wall
map_Kd -s 1 1 1 imported/wall panel.bmp
)";
    constexpr std::string_view kDetailsMtl = R"(newmtl trim
map_Kd imported/trim.tga
)";
    const std::array libraries = {
        pistoris::ObjMaterialLibraryView{"materials/main.mtl", kMainMtl},
        pistoris::ObjMaterialLibraryView{"details.mtl", kDetailsMtl},
    };

    std::vector<std::string> sources;
    pistoris::Model model = take(pistoris::Model::importObj(kObj, libraries, &sources));
    REQUIRE(model.textureCount() == 2);
    REQUIRE(sources.size() == 2);
    CHECK(sources[0] == "imported/wall panel.bmp");
    CHECK(sources[1] == "imported/trim.tga");
  }

  TEST_CASE("Keeps distinct OBJ sources when their normalized texture paths collide") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
v 0 0 1
usemtl first
f 1 2 3
usemtl second
f 1 4 2
)";
    constexpr std::string_view kMtl = R"(newmtl first
map_Kd textures/wall?.bmp
newmtl second
map_Kd textures/wall*.bmp
)";

    std::vector<std::string> sources;
    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl, &sources));
    REQUIRE(model.textureCount() == 2);
    REQUIRE(sources.size() == 2);
    CHECK(sources[0] == "textures/wall?.bmp");
    CHECK(sources[1] == "textures/wall*.bmp");
    const auto textures = model.textures();
    CHECK((stringView(textures[0].path) == "textures/wall-"));
    CHECK((stringView(textures[1].path) == "textures/wall-_1"));
  }

  TEST_CASE("Converts static OBJ directly through Model") {
    constexpr std::string_view kObj = R"(#arx_action HIT_30 4 5 6
v 1 2 3
v 2 2 3
v 2 3 3
v 1.5 2.5 3
v 1 3 3
vn 0 0 1
vt 0 0
vt 1 0
vt 1 1
vt 0.5 0.5
vt 0 1
usemtl wall__METAL
f 1/1/1 2/2/1 3/3/1 4/4/1 5/5/1
# arx_action HIT_30 7 8 9
)";
    constexpr std::string_view kMtl = R"(newmtl wall__METAL
map_Kd textures/wall.bmp
)";

    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl));
    REQUIRE(model.validate());
    CHECK(model.vertexCount() == 5);
    CHECK(model.faceCount() == 3);
    CHECK(model.textureCount() == 1);
    CHECK(model.actionPointCount() == 2);
    CHECK(model.boneCount() == 0);
    CHECK(model.selectionCount() == 0);

    const auto vertices = model.vertices();
    REQUIRE(vertices.size() == 5);
    CHECK(vertices[0].position == ArxVector3{1.0f, -2.0f, -3.0f});
    CHECK(vertices[0].bone == pistoris::kInvalidBoneIndex);

    const auto actions = model.actionPoints();
    REQUIRE(actions.size() == 2);
    CHECK((stringView(actions[0].name) == "hit_30"));
    CHECK(actions[0].position == ArxVector3{4.0f, -5.0f, -6.0f});
    CHECK((stringView(actions[1].name) == "hit_30"));
    CHECK(actions[1].position == ArxVector3{7.0f, -8.0f, -9.0f});

    const ArxTextureView texture = model.textures()[0];
    CHECK((stringView(texture.path) == "textures/wall"));
    CHECK((stringView(texture.external_image_extension) == ".bmp"));

    const pistoris::ObjBundle encoded = take(model.exportObj("static_model"));
    CHECK(encoded.text.find("# origin") == std::string::npos);
    const std::size_t first_action = encoded.text.find("# arx_action hit_30 ");
    REQUIRE(first_action != std::string::npos);
    CHECK(encoded.text.find("# arx_action hit_30 ", first_action + 1) != std::string::npos);
    CHECK(encoded.text.find("v 1 2 3") != std::string::npos);
    CHECK(encoded.mtl.find("map_Kd textures/wall.bmp") != std::string::npos);
    CHECK(encoded.mtl.find("arx_path") == std::string::npos);

    pistoris::Model roundtrip = take(pistoris::Model::importObj(encoded.text, encoded.mtl));
    CHECK(roundtrip.validate());
    CHECK(roundtrip.faceCount() == model.faceCount());
    CHECK(roundtrip.actionPointCount() == model.actionPointCount());
  }

  TEST_CASE("Converts OBJ texture coordinates to the Model origin") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
vt 0.25 0.125
vt 0.5 -0.25
vt 0.75 1.5
f 1/1 2/2 3/3
)";

    pistoris::Model model = take(pistoris::Model::importObj(kObj));

    const ArxModelFace face = model.faces()[0];
    constexpr std::array<ArxVector2, 3> kExpectedByVertex = {
        ArxVector2{0.25f, 0.875f},
        ArxVector2{0.5f, 1.25f},
        ArxVector2{0.75f, -0.5f},
    };
    for (const ArxModelCorner& corner : face.corners) {
      REQUIRE(corner.vertex < kExpectedByVertex.size());
      CHECK(corner.u == doctest::Approx(kExpectedByVertex[corner.vertex].x));
      CHECK(corner.v == doctest::Approx(kExpectedByVertex[corner.vertex].y));
    }

    const pistoris::NativeModelBundle native = take(model.bakeNativeBundle({}));
    REQUIRE(native.ftl.faces.size() == 1);
    const pistoris::ftl::Face& native_face = native.ftl.faces[0];
    const std::array native_u = {native_face.u.x, native_face.u.y, native_face.u.z};
    const std::array native_v = {native_face.v.x, native_face.v.y, native_face.v.z};
    for (std::size_t corner = 0; corner < native_u.size(); ++corner) {
      CHECK(native_u[corner] == doctest::Approx(face.corners[corner].u));
      CHECK(native_v[corner] == doctest::Approx(face.corners[corner].v));
    }

    const pistoris::ObjBundle encoded = take(model.exportObj("texture_origin"));
    CHECK(encoded.text.find("vt 0.25 0.125\n") != std::string::npos);
    CHECK(encoded.text.find("vt 0.5 -0.25\n") != std::string::npos);
    CHECK(encoded.text.find("vt 0.75 1.5\n") != std::string::npos);
  }

  TEST_CASE("Exports referenced OBJ texture files with their MTL paths") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
usemtl wall
f 1 2 3
)";
    constexpr std::string_view kMtl = "newmtl wall\nmap_Kd textures/wall.bmp\n";
    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl));
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(model.setTextureImage(0, {image.data(), image.size()}));

    const pistoris::ObjBundle encoded = take(model.exportObj("static_model"));
    CHECK(encoded.mtl.find("map_Kd textures/wall.bmp") != std::string::npos);
    REQUIRE(encoded.texture_files.size() == 1);
    CHECK(encoded.texture_files[0].source_texture == 0);
    CHECK(encoded.texture_files[0].path == "textures/wall.bmp");
    CHECK(encoded.texture_files[0].encoded_image == image);

    const pistoris::ObjExportOptions options{.include_files = false};
    const pistoris::ObjBundle without_files = take(model.exportObj("static_model", options));
    CHECK(without_files.texture_files.empty());
  }

  TEST_CASE("Uses a real OBJ texture source over the no_tex fallback") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
usemtl no_tex
f 1 2 3
)";
    constexpr std::string_view kMtl = R"(newmtl no_tex
map_Kd wall.png
)";

    WarningCapture warnings;
    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl));
    CHECK(model.textureCount() == 1);
    REQUIRE(warnings.messages.size() == 1);
    CHECK(warnings.messages[0].find("no_tex material 'no_tex' uses its map_Kd texture") != std::string::npos);
  }

  TEST_CASE("Ignores unused OBJ materials") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
usemtl unused
usemtl no_tex
f 1 2 3
)";
    constexpr std::string_view kMtl = R"(newmtl unused
map_Kd wall.png
)";

    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl));
    CHECK(model.textureCount() == 0);
  }

  TEST_CASE("Respects OBJ smoothing groups for generated normals") {
    const auto import = [](std::string_view first_smoothing, std::string_view second_smoothing) {
      std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n";
      obj.append(first_smoothing).append("f 1 2 3\n");
      obj.append(second_smoothing).append("f 1 4 2\n");
      pistoris::Model model = take(pistoris::Model::importObj(obj));
      std::array<ArxModelFace, 2> faces{};
      REQUIRE(model.faces().size() == faces.size());
      std::ranges::copy(model.faces(), faces.begin());
      return faces;
    };

    const std::array<ArxModelFace, 2> flat = import({}, "s off\n");
    CHECK(flat[0].corners[0].normal == flat[0].normal);
    CHECK(flat[1].corners[0].normal == flat[1].normal);
    CHECK(flat[0].corners[0].normal != flat[1].corners[0].normal);

    const std::array<ArxModelFace, 2> smooth = import("s on\n", "s 1\n");
    CHECK(smooth[0].corners[0].normal == smooth[1].corners[0].normal);
    CHECK(smooth[0].corners[0].normal != smooth[0].normal);
    CHECK(smooth[1].corners[0].normal != smooth[1].normal);

    const std::array<ArxModelFace, 2> named = import("s shell\n", "s shell\n");
    CHECK(named[0].corners[0].normal == named[1].corners[0].normal);
  }

  TEST_CASE("Includes explicitly-normaled faces when generating smoothing normals") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
v 0 0 1
vn 0 0 1
s 1
f 1//1 2//1 3//1
f 1 4 2
)";

    pistoris::Model model = take(pistoris::Model::importObj(kObj));
    std::array<ArxModelFace, 2> faces{};
    REQUIRE(model.faces().size() == faces.size());
    std::ranges::copy(model.faces(), faces.begin());
    for (const ArxModelCorner& corner : faces[0].corners) CHECK(corner.normal == ArxVector3{0.0f, 0.0f, -1.0f});
    CHECK(faces[1].corners[0].normal != faces[1].normal);
    CHECK(faces[1].corners[2].normal != faces[1].normal);
  }

  TEST_CASE("Uses the interior diagonal for concave OBJ quads") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 0.5 0.5 0
v 2 0 0
v 0 2 0
f 1 2 3 4
)";

    pistoris::Model model = take(pistoris::Model::importObj(kObj));
    std::array<ArxModelFace, 2> faces{};
    REQUIRE(model.faces().size() == faces.size());
    std::ranges::copy(model.faces(), faces.begin());
    std::array<pistoris::VertexIndex, 2> shared{};
    std::size_t shared_count = 0;
    for (const ArxModelCorner& first : faces[0].corners)
      for (const ArxModelCorner& second : faces[1].corners)
        if (first.vertex == second.vertex) {
          REQUIRE(shared_count < shared.size());
          shared[shared_count++] = first.vertex;
        }
    REQUIRE(shared_count == 2);
    const auto vertices = model.vertices();
    REQUIRE(vertices.size() == 4);
    const ArxVector3 first = vertices[shared[0]].position;
    const ArxVector3 second = vertices[shared[1]].position;
    CHECK(((first == ArxVector3{0.5f, -0.5f, 0.0f} && second == ArxVector3{0.0f, -2.0f, 0.0f}) ||
           (second == ArxVector3{0.5f, -0.5f, 0.0f} && first == ArxVector3{0.0f, -2.0f, 0.0f})));
  }

  TEST_CASE("Triangulates warped OBJ quads") {
    constexpr std::string_view kObj = R"(v 1.468437 -1.158562 0.100560
v 0 -1.306799 0.034982
v 0 -1.314666 0.056717
v 1.459824 -1.156057 0.070345
f 1 2 3 4
)";

    pistoris::Model model = take(pistoris::Model::importObj(kObj));
    CHECK(model.faceCount() == 2);
  }

  TEST_CASE("Preserves representable OBJ transparency per material") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
v 0 0 1
usemtl no_tex__TRANS__TRANSVAL_0.25
f 1 2 3
usemtl no_tex__TRANS__TRANSVAL_0.75
f 1 4 2
)";
    constexpr std::string_view kMtl = R"(newmtl no_tex__TRANS__TRANSVAL_0.25
d 0.75
newmtl no_tex__TRANS__TRANSVAL_0.75
d 0.25
)";

    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl));
    auto faces = model.faces();
    CHECK(faces[0].transval == doctest::Approx(0.25f));
    CHECK(faces[1].transval == doctest::Approx(0.75f));

    const pistoris::ObjBundle encoded = take(model.exportObj("transparent"));
    CHECK(encoded.mtl.find("newmtl no_tex__TRANS__TRANSVAL_0.25\n") != std::string::npos);
    CHECK(encoded.mtl.find("newmtl no_tex__TRANS__TRANSVAL_0.75\n") != std::string::npos);
    CHECK(encoded.mtl.find("d 0.75") != std::string::npos);
    CHECK(encoded.mtl.find("d 0.25") != std::string::npos);

    pistoris::Model roundtrip = take(pistoris::Model::importObj(encoded.text, encoded.mtl));
    CHECK(roundtrip.faces()[0].transval == doctest::Approx(0.25f));
    CHECK(roundtrip.faces()[1].transval == doctest::Approx(0.75f));
  }

  TEST_CASE("Infers OBJ transparency from MTL opacity") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
usemtl glass
f 1 2 3
)";
    constexpr std::string_view kMtl = "newmtl glass\nd 0.5\n";

    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl));
    const ArxModelFace face = model.faces()[0];
    CHECK((face.flags & pistoris::kFaceBitTrans) != 0);
    CHECK(face.transval == doctest::Approx(0.5f));
  }

  TEST_CASE("Rejects unsafe OBJ material-library names") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";
    pistoris::Model model = take(pistoris::Model::importObj(kObj));

    for (std::string_view name : {"", "model name", "model#name", "model\nname", "model\tname"}) {
      CHECK(model.exportObj(name).code() == ARX_OBJ_BAD_MATERIAL_LIBRARY_NAME);
    }
  }

  TEST_CASE("Preserves out-of-range OBJ transparency in the material name") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
usemtl no_tex__TRANS
f 1 2 3
)";
    constexpr std::string_view kMtl = R"(newmtl no_tex__TRANS
d -1
)";

    pistoris::Model model = take(pistoris::Model::importObj(kObj, kMtl));
    WarningCapture warnings;
    const pistoris::ObjBundle encoded = take(model.exportObj("transparent"));
    CHECK(encoded.mtl.find("\nd ") == std::string::npos);
    CHECK(encoded.mtl.find("newmtl no_tex__TRANS__TRANSVAL_2") != std::string::npos);
    CHECK(warnings.messages.empty());
  }

  TEST_CASE("Rejects TRANSVAL without effective transparency") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
usemtl no_tex__TRANSVAL_2
f 1 2 3
)";

    CHECK(pistoris::Model::importObj(kObj).code() == ARX_OBJ_BAD_MATERIAL_NAME);
  }

  TEST_CASE("Validates reserved OBJ directives") {
    constexpr std::string_view kObj = R"(# arx_unknown ignored
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";
    WarningCapture warnings;
    pistoris::Model model = take(pistoris::Model::importObj(kObj));
    REQUIRE(warnings.messages.size() == 1);
    CHECK(warnings.messages[0].find("unknown reserved directive 'arx_unknown' ignored") != std::string::npos);

    constexpr std::string_view kBadMtl = "newmtl wall\nmap_Kd -o\n";
    const pistoris::ObjResult<pistoris::Model> bad_mtl = pistoris::Model::importObj(kObj, kBadMtl);
    CHECK(bad_mtl.code() == ARX_OBJ_BAD_MTL);
    REQUIRE(bad_mtl.error() != nullptr);
    REQUIRE(bad_mtl.error()->location().has_value());
    CHECK(bad_mtl.error()->location()->source == pistoris::ObjSource::kMaterialLibrary);
    CHECK(bad_mtl.error()->location()->source_index == 0);
    CHECK(bad_mtl.error()->location()->line == 2);
    CHECK(bad_mtl.error()->location()->source_path == "<inline>");

    constexpr std::string_view kBadIndex = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n";
    const pistoris::ObjResult<pistoris::Model> bad_index = pistoris::Model::importObj(kBadIndex);
    CHECK(bad_index.code() == ARX_OBJ_BAD_POSITION_INDEX);
    REQUIRE(bad_index.error() != nullptr);
    REQUIRE(bad_index.error()->location().has_value());
    CHECK(bad_index.error()->location()->source == pistoris::ObjSource::kObj);
    CHECK(bad_index.error()->location()->line == 4);
    CHECK(pistoris::Model::importObj("v 0 0 0\n").code() == ARX_OBJ_NO_GEOMETRY);
  }

  TEST_CASE("Converts Model GLB through the public class") {
    pistoris::Model source = take(pistoris::Model::importNative(makeSemanticModelFtl()));
    const std::vector<std::uint8_t> encoded = take(source.exportGlb());
    pistoris::Model imported = take(pistoris::Model::importGlb(encoded));
    CHECK(imported.validate());
    CHECK(imported.faceCount() == source.faceCount());
    CHECK(imported.boneCount() == source.boneCount());
    CHECK(imported.actionPointCount() == source.actionPointCount());
    CHECK(imported.selectionCount() == source.selectionCount());
  }

  TEST_CASE("Model failures preserve resource and composite input identity") {
    pistoris::Model empty;
    REQUIRE(empty.setResourcePath("model:npc:empty"));
    const pistoris::ModelResult<void> validation = empty.validate();
    REQUIRE_FALSE(validation);
    REQUIRE(validation.error() != nullptr);
    REQUIRE(validation.error()->location());
    CHECK(validation.error()->location()->resource_path == "game/graph/obj3d/interactive/npc/empty/empty.ftl");

    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));
    const std::array<const pistoris::Animation*, 1> animations = {nullptr};
    const auto export_result = model.exportGlb(animations);
    REQUIRE_FALSE(export_result);
    REQUIRE(export_result.error() != nullptr);
    REQUIRE(export_result.error()->location());
    const auto* location = std::get_if<pistoris::AnimationLocation>(&*export_result.error()->location());
    REQUIRE(location != nullptr);
    CHECK(location->element == pistoris::AnimationElement::kResource);
    CHECK(location->input_index == 0);
  }

  TEST_CASE("Converts native runtime semantics and bakes a coherent FTL") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.faces[0].type |= pistoris::kFaceBitQuad;
    REQUIRE(pistoris::validate(native));

    pistoris::Model model = take(pistoris::Model::importNative(native));
    CHECK(model.resourcePath().empty());
    REQUIRE(model.setResourcePath("MODEL:NPC:MY__NPC"));
    CHECK((model.resourcePath() == "game/graph/obj3d/interactive/npc/my__npc/my__npc.ftl"));
    REQUIRE(model.setResourcePath(R"(Graph\MY_FOLDER\My_Model.FTL)"));
    CHECK((model.resourcePath() == "graph/my_folder/my_model.ftl"));
    CHECK(model.vertexCount() == 3);
    CHECK(model.faceCount() == 1);
    CHECK(model.textureCount() == 1);
    CHECK(model.boneCount() == 2);
    CHECK(model.actionPointCount() == 1);
    CHECK(model.selectionCount() == 6);

    CHECK(std::ranges::equal(model.selectionIds(), std::array<pistoris::SelectionId, 6>{0, 1, 2, 3, 4, 5}));

    const auto vertices = model.vertices();
    CHECK(vertices[0].position == ArxVector3{0.0f, 0.0f, 0.0f});
    CHECK(vertices[0].bone == 0);
    CHECK(vertices[1].bone == 1);
    CHECK(selectionVertices(model, 0) == std::vector<pistoris::VertexIndex>{0});
    CHECK(selectionVertices(model, 1) == std::vector<pistoris::VertexIndex>{1});
    CHECK(selectionVertices(model, 2) == std::vector<pistoris::VertexIndex>{0, 1});
    CHECK(selectionVertices(model, 3) == std::vector<pistoris::VertexIndex>{0});
    CHECK(selectionVertices(model, 4) == std::vector<pistoris::VertexIndex>{1});
    CHECK(selectionVertices(model, 5) == std::vector<pistoris::VertexIndex>{2});

    const auto bones = model.bones();
    CHECK((stringView(bones[0].name) == "root"));
    CHECK(bones[0].parent == pistoris::kInvalidBoneIndex);
    CHECK(selectionBones(model, 0) == std::vector<pistoris::BoneIndex>{0});
    CHECK(selectionBones(model, 3) == std::vector<pistoris::BoneIndex>{0});
    CHECK((stringView(bones[1].name) == "chest"));
    CHECK(bones[1].parent == 0);
    CHECK(selectionBones(model, 1) == std::vector<pistoris::BoneIndex>{1});

    const ArxModelActionPoint action = model.actionPoints()[0];
    CHECK((stringView(action.name) == "view_attach"));
    CHECK(action.position == ArxVector3{1.0f, 0.0f, 0.0f});
    CHECK(action.bone == 1);
    CHECK(selectionActionPoints(model, 0) == std::vector<pistoris::ActionPointIndex>{0});
    CHECK(selectionActionPoints(model, 3) == std::vector<pistoris::ActionPointIndex>{0});

    const ArxModelOrigin origin = model.origin();
    CHECK(origin.bone == 0);
    CHECK(selectionIncludesOrigin(model, 0));
    CHECK(selectionIncludesOrigin(model, 3));

    const ArxModelSelection cut_head = take(model.selection(3));
    CHECK((stringView(cut_head.name) == "cut_head"));
    CHECK(cut_head.has_leading_vertex == 1);
    CHECK(cut_head.leading_position == ArxVector3{0.5f, 0.5f, 0.0f});
    CHECK(cut_head.leading_bone == 1);

    const ArxTextureView texture = model.textures()[0];
    CHECK((stringView(texture.path) == "graph/obj3d/textures/my_tex"));

    const ArxModelFace face = model.faces()[0];
    CHECK(face.normal == native.faces[0].norm);
    CHECK((face.flags & pistoris::kFaceBitQuad) == 0);

    const pistoris::NativeModelBundle bundle = take(model.bakeNativeBundle({}));
    const pistoris::Ftl& baked = bundle.ftl;
    REQUIRE(pistoris::validate(baked));
    CHECK(baked.faces[0].norm == native.faces[0].norm);
    CHECK((baked.faces[0].type & pistoris::kFaceBitQuad) == 0);
    CHECK(baked.vertices[baked.header.origin].position == ArxVector3{});
    CHECK(baked.groups.size() == 2);
    CHECK(baked.actions.size() == 1);
    CHECK(baked.selections.size() == 6);

    pistoris::Model roundtrip = take(pistoris::Model::importNative(baked));
    CHECK(roundtrip.validate());
    CHECK(roundtrip.resourcePath().empty());
    CHECK(roundtrip.vertexCount() == model.vertexCount());
    CHECK(roundtrip.faceCount() == model.faceCount());
    CHECK(roundtrip.textureCount() == model.textureCount());
    CHECK(roundtrip.boneCount() == model.boneCount());
    CHECK(roundtrip.actionPointCount() == model.actionPointCount());

    const auto roundtrip_vertices = roundtrip.vertices();
    for (std::size_t i = 0; i < vertices.size(); ++i) {
      CHECK(roundtrip_vertices[i].position == vertices[i].position);
      CHECK(roundtrip_vertices[i].bone == vertices[i].bone);
    }

    const ArxModelFace roundtrip_face = roundtrip.faces()[0];
    CHECK(roundtrip_face.normal == face.normal);
    for (std::size_t i = 0; i < std::size(face.corners); ++i) {
      CHECK(roundtrip_face.corners[i].vertex == face.corners[i].vertex);
      CHECK(roundtrip_face.corners[i].normal == face.corners[i].normal);
      CHECK(roundtrip_face.corners[i].u == face.corners[i].u);
      CHECK(roundtrip_face.corners[i].v == face.corners[i].v);
    }
    CHECK(roundtrip_face.texture == face.texture);
    CHECK(roundtrip_face.flags == face.flags);
    CHECK(roundtrip_face.transval == face.transval);

    const ArxTextureView roundtrip_texture = roundtrip.textures()[0];
    CHECK((stringView(roundtrip_texture.path) == "graph/obj3d/textures/my_tex"));
    CHECK(roundtrip_texture.encoded_image.size == texture.encoded_image.size);

    const auto roundtrip_bones = roundtrip.bones();
    for (std::size_t i = 0; i < bones.size(); ++i) {
      CHECK((stringView(roundtrip_bones[i].name) == stringView(bones[i].name)));
      CHECK(roundtrip_bones[i].position == bones[i].position);
      CHECK(roundtrip_bones[i].parent == bones[i].parent);
      CHECK(roundtrip_bones[i].blob_shadow_size == bones[i].blob_shadow_size);
    }

    const ArxModelActionPoint roundtrip_action = roundtrip.actionPoints()[0];
    CHECK((stringView(roundtrip_action.name) == stringView(action.name)));
    CHECK(roundtrip_action.position == action.position);
    CHECK(roundtrip_action.bone == action.bone);

    const ArxModelOrigin roundtrip_origin = roundtrip.origin();
    CHECK(roundtrip_origin.bone == origin.bone);
    for (pistoris::SelectionId id : model.selectionIds()) {
      CHECK(selectionVertices(roundtrip, id) == selectionVertices(model, id));
      CHECK(selectionBones(roundtrip, id) == selectionBones(model, id));
      CHECK(selectionActionPoints(roundtrip, id) == selectionActionPoints(model, id));
      CHECK(selectionIncludesOrigin(roundtrip, id) == selectionIncludesOrigin(model, id));
    }

    const ArxModelSelection roundtrip_cut_head = take(roundtrip.selection(3));
    CHECK((stringView(roundtrip_cut_head.name) == stringView(cut_head.name)));
    CHECK(roundtrip_cut_head.has_leading_vertex == cut_head.has_leading_vertex);
    CHECK(roundtrip_cut_head.leading_position == cut_head.leading_position);
    CHECK(roundtrip_cut_head.leading_bone == cut_head.leading_bone);
  }

  TEST_CASE("Native import preserves distinct vertex identities") {
    pistoris::Ftl native;
    native.header.origin = 0;
    native.vertices = {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    };
    pistoris::ftl::Face first;
    first.vertex_idx = {0, 1, 2};
    first.texture_id = pistoris::kFtlTextureNone;
    first.norm = {0.0f, 0.0f, 1.0f};
    pistoris::ftl::Face second = first;
    second.vertex_idx.x = 3;
    native.faces = {first, second};

    pistoris::Model model = take(pistoris::Model::importNative(native));
    CHECK(model.vertexCount() == 4);

    const auto faces = model.faces();
    CHECK(faces[0].corners[0].vertex == 0);
    CHECK(faces[1].corners[0].vertex == 3);
    CHECK(faces[0].corners[0].normal == native.vertices[0].normal);
    CHECK(faces[1].corners[0].normal == native.vertices[3].normal);

    const pistoris::NativeModelBundle bundle = take(model.bakeNativeBundle({}));
    CHECK(bundle.ftl.faces[0].vertex_idx.x != bundle.ftl.faces[1].vertex_idx.x);
  }

  TEST_CASE("Native bake splits selected vertices by corner normal") {
    pistoris::Model model;
    const std::array<ArxModelVertex, 4> vertices = {
        ArxModelVertex{{0.0f, 0.0f, 0.0f}},
        ArxModelVertex{{1.0f, 0.0f, 0.0f}},
        ArxModelVertex{{0.0f, 1.0f, 0.0f}},
        ArxModelVertex{{0.0f, 0.0f, 1.0f}},
    };
    for (const ArxModelVertex& vertex : vertices) {
      (void)take(model.addVertex(vertex));
    }

    ArxModelSelection selection{};
    selection.name = {"split", 5};
    const pistoris::SelectionId selection_id = take(model.addSelection(selection));
    const pistoris::VertexIndex selected_vertex = 0;
    REQUIRE(model.updateSelectionMembers(selection_id, {.vertices = &selected_vertex, .vertex_count = 1}));

    ArxModelFace first{};
    first.normal = {0.0f, 0.0f, 1.0f};
    first.texture = pistoris::kNoTexture;
    first.corners[0].vertex = 0;
    first.corners[0].normal = {0.0f, 0.0f, 1.0f};
    first.corners[1].vertex = 1;
    first.corners[1].normal = {0.0f, 0.0f, 1.0f};
    first.corners[2].vertex = 2;
    first.corners[2].normal = {0.0f, 0.0f, 1.0f};
    (void)take(model.addFace(first));

    ArxModelFace second{};
    second.normal = {0.0f, 1.0f, 0.0f};
    second.texture = pistoris::kNoTexture;
    second.corners[0].vertex = 0;
    second.corners[0].normal = {1.0f, 0.0f, 0.0f};
    second.corners[1].vertex = 3;
    second.corners[1].normal = {0.0f, 0.0f, 1.0f};
    second.corners[2].vertex = 1;
    second.corners[2].normal = {0.0f, 0.0f, 1.0f};
    (void)take(model.addFace(second));
    REQUIRE(model.validate());

    const pistoris::NativeModelBundle bundle = take(model.bakeNativeBundle({}));
    REQUIRE(bundle.ftl.faces.size() == 2);
    const std::uint16_t first_variant = bundle.ftl.faces[0].vertex_idx.x;
    const std::uint16_t second_variant = bundle.ftl.faces[1].vertex_idx.x;
    CHECK(first_variant != second_variant);
    CHECK(bundle.ftl.vertices[first_variant].position == vertices[0].position);
    CHECK(bundle.ftl.vertices[first_variant].normal == first.corners[0].normal);
    CHECK(bundle.ftl.vertices[second_variant].position == vertices[0].position);
    CHECK(bundle.ftl.vertices[second_variant].normal == second.corners[0].normal);

    REQUIRE(bundle.ftl.selections.size() == 1);
    const std::vector<std::int32_t>& selected = bundle.ftl.selections[0].selected;
    REQUIRE(selected.size() == 2);
    CHECK(std::find(selected.begin(), selected.end(), first_variant) != selected.end());
    CHECK(std::find(selected.begin(), selected.end(), second_variant) != selected.end());

    pistoris::Model roundtrip = take(pistoris::Model::importNative(bundle.ftl));
    const auto roundtrip_faces = roundtrip.faces();
    CHECK(roundtrip_faces[0].corners[0].vertex != roundtrip_faces[1].corners[0].vertex);
    CHECK(roundtrip_faces[0].corners[0].normal == first.corners[0].normal);
    CHECK(roundtrip_faces[1].corners[0].normal == second.corners[0].normal);

    const std::vector<pistoris::VertexIndex> roundtrip_selected =
        selectionVertices(roundtrip, roundtrip.selectionIds()[0]);
    REQUIRE(roundtrip_selected.size() == 2);
    CHECK(std::find(roundtrip_selected.begin(), roundtrip_selected.end(), roundtrip_faces[0].corners[0].vertex) !=
          roundtrip_selected.end());
    CHECK(std::find(roundtrip_selected.begin(), roundtrip_selected.end(), roundtrip_faces[1].corners[0].vertex) !=
          roundtrip_selected.end());
  }

  TEST_CASE("Discards degenerate native faces and their unreferenced vertices") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.vertices.push_back({{12.0f, 20.0f, 30.0f}, {0.0f, 0.0f, 1.0f}});
    pistoris::ftl::Face degenerate = native.faces.front();
    degenerate.vertex_idx = {1, 2, 8};
    native.faces.push_back(degenerate);
    REQUIRE(pistoris::validate(native));

    pistoris::Model model = take(pistoris::Model::importNative(native));
    CHECK(model.vertexCount() == 3);
    CHECK(model.faceCount() == 1);
    CHECK(model.validate());
  }

  TEST_CASE("Repairs native vertex normals when constructing Model corners") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.vertices[1].normal = {0.0f, 0.0f, 0.5f};
    native.vertices[2].normal = {};
    REQUIRE(pistoris::validate(native));
    WarningCapture warnings;

    pistoris::Model model = take(pistoris::Model::importNative(native));

    const ArxModelFace face = model.faces()[0];
    CHECK(face.corners[0].normal == ArxVector3{0.0f, 0.0f, 1.0f});
    CHECK(face.corners[1].normal == ArxVector3{0.0f, 0.0f, 1.0f});
    CHECK(face.corners[2].normal == ArxVector3{0.0f, 0.0f, 1.0f});
    REQUIRE(warnings.messages.size() == 1);
    CHECK((warnings.messages[0] ==
           "FTL -> Model repairs: 1 corner normal(s) regenerated; 1 corner normal(s) normalized"));
  }

  TEST_CASE("Repairs nonfinite native vertex normals used by faces") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.vertices[1].normal.x = std::numeric_limits<float>::infinity();
    native.vertices[2].normal = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(),
    };
    REQUIRE(pistoris::validate(native));
    WarningCapture warnings;

    pistoris::Model model = take(pistoris::Model::importNative(native));

    const ArxModelFace face = model.faces()[0];
    CHECK(face.corners[0].normal == ArxVector3{0.0f, 0.0f, 1.0f});
    CHECK(face.corners[1].normal == ArxVector3{0.0f, 0.0f, 1.0f});
    REQUIRE(warnings.messages.size() == 1);
    CHECK((warnings.messages[0] == "FTL -> Model repairs: 2 corner normal(s) regenerated"));
  }

  TEST_CASE("Clamps negative native bone blob-shadow sizes") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.groups[1].blob_shadow_size = -2.0f;
    REQUIRE(pistoris::validate(native));
    WarningCapture warnings;

    pistoris::Model model = take(pistoris::Model::importNative(native));

    const ArxModelBone bone = model.bones()[1];
    CHECK(bone.blob_shadow_size == 0.0f);
    REQUIRE(warnings.messages.size() == 1);
    CHECK((warnings.messages[0] == "FTL -> Model repairs: 1 negative bone blob-shadow size(s) clamped to zero"));
  }

  TEST_CASE("Rejects nonfinite native bone blob-shadow sizes") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.groups[0].blob_shadow_size = std::numeric_limits<float>::infinity();
    REQUIRE(pistoris::validate(native));

    CHECK(pistoris::Model::importNative(native).code() == ARX_MODEL_BAD_BONE_BLOB_SHADOW_SIZE);
  }

  TEST_CASE("Sets and clears validated texture image data") {
    pistoris::Model model;
    ArxTextureView texture{};
    texture.path = {"wall.bmp", 8};
    const pistoris::TextureIndex texture_index = take(model.addTexture(texture));

    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(model.setTextureImage(texture_index, {image.data(), image.size()}));
    REQUIRE(model.setTexturePath(texture_index, "custom/wall"));
    CHECK(model.setTextureExternalImageExtension(texture_index, ".png").code() == ARX_MODEL_BAD_TEXTURE_IMAGE);
    CHECK(model.setTextureImage(texture_index, {}).code() == ARX_MODEL_BAD_TEXTURE_IMAGE);

    ArxTextureView copied = model.textures()[0];
    CHECK((std::string_view(copied.path.data, copied.path.size) == "custom/wall"));
    REQUIRE(copied.encoded_image.size == image.size());
    CHECK(std::equal(image.begin(), image.end(), copied.encoded_image.data));

    REQUIRE(model.clearTextureImage(texture_index));
    copied = model.textures()[0];
    CHECK(copied.encoded_image.size == 0);
    CHECK(std::string(copied.external_image_extension.data, copied.external_image_extension.size) == ".bmp");
    REQUIRE(model.setTextureExternalImageExtension(texture_index, ".png"));
    copied = model.textures()[0];
    CHECK((std::string_view(copied.external_image_extension.data, copied.external_image_extension.size) == ".png"));
  }

  TEST_CASE("Stores and renders an optional inventory icon") {
    pistoris::Model model;

    CHECK(model.setInventoryIcon({}).code() == ARX_MODEL_BAD_INVENTORY_ICON);

    pistoris::Model::InventoryIconRenderOptions options;
    std::vector<std::uint8_t> rendered = take(model.renderIcon(options));
    CHECK(rendered.empty());

    const std::vector<std::uint8_t> icon = makeTestBmp(255, 0, 0);
    REQUIRE(model.setInventoryIcon({icon.data(), icon.size()}));

    const pistoris::Model::InventoryIconView borrowed = model.inventoryIcon();
    REQUIRE(borrowed.encoded_image.size == icon.size());
    CHECK(std::equal(icon.begin(), icon.end(), borrowed.encoded_image.data));
    CHECK(borrowed.width_slots == 1);
    CHECK(borrowed.height_slots == 1);

    pistoris::Model copied(model);
    CHECK(model.setInventoryIcon({icon.data(), 1}).code() == ARX_MODEL_BAD_INVENTORY_ICON);
    CHECK(model.inventoryIcon().encoded_image.size == icon.size());
    pistoris::Model::InventoryIconSetOptions set_options;
    set_options.width_slots = 0;
    CHECK(model.setInventoryIcon({icon.data(), icon.size()}, set_options).code() == ARX_INVALID_OPTIONS);
    CHECK(model.inventoryIcon().encoded_image.size == icon.size());
    model.clearInventoryIcon();
    CHECK(model.inventoryIcon().encoded_image.data == nullptr);
    CHECK(model.inventoryIcon().encoded_image.size == 0);
    CHECK(model.inventoryIcon().width_slots == 0);
    CHECK(model.inventoryIcon().height_slots == 0);
    CHECK(copied.inventoryIcon().encoded_image.size == icon.size());

    set_options.width_slots = 3;
    set_options.height_slots = 2;
    REQUIRE(copied.setInventoryIcon({icon.data(), icon.size()}, set_options));
    rendered = take(copied.renderIcon(options));
    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width == 96);
    CHECK(info.height == 64);

    options.format = pistoris::ImageFormat::kBmp;
    rendered = take(copied.renderIcon(options));
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_BMP);
    CHECK(info.width == 96);
    CHECK(info.height == 64);
    CHECK(info.components == 4);

    options.format = pistoris::ImageFormat::kTga;
    rendered = take(copied.renderIcon(options));
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_TGA);

    options.format = pistoris::ImageFormat::kJpeg;
    CHECK(copied.renderIcon(options).code() == ARX_MODEL_UNSUPPORTED_INVENTORY_ICON_FORMAT);
    options.format = pistoris::ImageFormat::kPng;
    options.width_slots = 0;
    CHECK(copied.renderIcon(options).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Derives inventory icon render footprints") {
    pistoris::Model model;
    const std::vector<std::uint8_t> icon = makeSolidTestBmp(128, 65);
    REQUIRE(model.setInventoryIcon({icon.data(), icon.size()}));
    CHECK(model.inventoryIcon().width_slots == 3);
    CHECK(model.inventoryIcon().height_slots == 2);

    pistoris::Model::InventoryIconSetOptions set_options;
    set_options.width_slots = 2;
    set_options.height_slots.reset();
    REQUIRE(model.setInventoryIcon({icon.data(), icon.size()}, set_options));
    CHECK(model.inventoryIcon().width_slots == 2);
    CHECK(model.inventoryIcon().height_slots == 2);

    pistoris::Model::InventoryIconRenderOptions options;
    options.width_slots = 2;
    options.height_slots.reset();
    options.layout = pistoris::Model::InventoryIconLayout::kStretch;
    std::vector<std::uint8_t> rendered = take(model.renderIcon(options));
    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.width == 64);
    CHECK(info.height == 64);

    const std::vector<std::uint8_t> large = makeSolidTestBmp(512, 256);
    REQUIRE(model.setInventoryIcon({large.data(), large.size()}));
    options.width_slots.reset();
    options.height_slots.reset();
    rendered = take(model.renderIcon(options));
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.width == 96);
    CHECK(info.height == 64);

    options.layout = static_cast<pistoris::Model::InventoryIconLayout>(255);
    CHECK(model.renderIcon(options).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Canonicalizes texture path metadata on mutation") {
    pistoris::Model model;
    const ArxTextureView texture{{"GRAPH/TEXTURES/WALL", 19}, {}, {".BMP", 4}};
    const pistoris::TextureIndex texture_index = take(model.addTexture(texture));

    const ArxTextureView copied = model.textures()[texture_index];
    CHECK((stringView(copied.path) == "graph/textures/wall"));
    CHECK((stringView(copied.external_image_extension) == ".bmp"));
  }

  TEST_CASE("Rejects texture identities without a valid resource path") {
    pistoris::Model model;
    const ArxTextureView invalid{{"", 0}, {}, {}};
    CHECK(model.addTexture(invalid).code() == ARX_MODEL_BAD_TEXTURE_PATH);
    CHECK(model.textureCount() == 0);
  }

  TEST_CASE("Native bake emits Model texture sidecars") {
    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(model.setTextureImage(0, {image.data(), image.size()}));
    REQUIRE(model.rebaseTexturePaths("graph/obj3d/textures"));

    pistoris::NativeModelBundle bundle = take(model.bakeNativeBundle({}));
    REQUIRE(bundle.texture_files.size() == 1);
    CHECK(bundle.texture_files[0].source_texture == 0);
    CHECK(bundle.texture_files[0].resource_path == "graph/obj3d/textures/my_tex.bmp");
    CHECK(bundle.texture_files[0].encoded_image == image);
    CHECK(std::string(bundle.ftl.texture_containers[0].filename) == "graph/obj3d/textures/my_tex");

    bundle = take(model.bakeNativeBundle({.include_texture_files = false}));
    CHECK(bundle.texture_files.empty());
    CHECK(std::string(bundle.ftl.texture_containers[0].filename) == "graph/obj3d/textures/my_tex");
  }

  TEST_CASE("Native bake applies the FTL texture path limit with its extension") {
    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));
    ArxTextureView texture{};
    const std::string resource_name = std::string(18, 'd') + "/" + std::string(236, 'a');
    const std::string path = resource_name + ".bmp";
    texture.path = {path.data(), path.size()};
    REQUIRE(model.setTexture(0, texture));

    CHECK(model.bakeNativeBundle({}).code() == ARX_MODEL_BAD_TEXTURE_PATH);
  }

  TEST_CASE("Repairs native names beyond lowercasing") {
    pistoris::Ftl native = makeSemanticModelFtl();
    setFtlName("bad_name", native.selections[0].name, sizeof(native.selections[0].name));
    setFtlName("bad__name", native.selections[4].name, sizeof(native.selections[4].name));
    setFtlName("__", native.selections[5].name, sizeof(native.selections[5].name));
    setFtlName("root", native.groups[1].name, sizeof(native.groups[1].name));
    setFtlName("", native.actions[0].name, sizeof(native.actions[0].name));
    LogCapture logs;

    pistoris::Model model = take(pistoris::Model::importNative(native));

    ArxModelSelection selection = take(model.selection(4));
    CHECK((stringView(selection.name) == "bad_name_1"));
    selection = take(model.selection(5));
    CHECK((stringView(selection.name) == "selection"));
    const ArxModelBone bone = model.bones()[1];
    CHECK((stringView(bone.name) == "root_1"));
    const ArxModelActionPoint action = model.actionPoints()[0];
    CHECK((stringView(action.name) == "unnamed"));
    REQUIRE(logs.messages.size() == 4);
    CHECK((logs.messages[0] == "FTL -> Model: selection 'bad__name' normalized to 'bad_name_1'"));
    CHECK((logs.messages[1] == "FTL -> Model: selection '__' normalized to 'selection'"));
    CHECK((logs.messages[2] == "FTL -> Model: bone 'root' normalized to 'root_1'"));
    CHECK((logs.messages[3] == "FTL -> Model: action point '' normalized to 'unnamed'"));
  }

  TEST_CASE("Native name repair preserves existing suffixed names") {
    pistoris::Ftl native = makeSemanticModelFtl();
    setFtlName("a", native.selections[0].name, sizeof(native.selections[0].name));
    setFtlName("a", native.selections[1].name, sizeof(native.selections[1].name));
    setFtlName("a_1", native.selections[2].name, sizeof(native.selections[2].name));
    const std::string boundary_name = std::string(60, 'x') + "_bb";
    setFtlName(boundary_name, native.selections[3].name, sizeof(native.selections[3].name));
    setFtlName(boundary_name, native.selections[4].name, sizeof(native.selections[4].name));

    pistoris::Model model = take(pistoris::Model::importNative(native));

    ArxModelSelection selection = take(model.selection(0));
    CHECK((stringView(selection.name) == "a"));
    selection = take(model.selection(1));
    CHECK((stringView(selection.name) == "a_2"));
    selection = take(model.selection(2));
    CHECK((stringView(selection.name) == "a_1"));
    selection = take(model.selection(3));
    CHECK((stringView(selection.name) == boundary_name));
    selection = take(model.selection(4));
    CHECK((stringView(selection.name) == std::string(60, 'x') + "_1"));
  }

  TEST_CASE("Native conversion preserves duplicate action point names") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.actions.push_back(native.actions.front());
    setFtlName("hit_30", native.actions[0].name, sizeof(native.actions[0].name));
    setFtlName("hit_30", native.actions[1].name, sizeof(native.actions[1].name));

    pistoris::Model model = take(pistoris::Model::importNative(native));
    REQUIRE(model.actionPointCount() == 2);
    const auto actions = model.actionPoints();
    CHECK((stringView(actions[0].name) == "hit_30"));
    CHECK((stringView(actions[1].name) == "hit_30"));

    const pistoris::NativeModelBundle bundle = take(model.bakeNativeBundle({}));
    REQUIRE(bundle.ftl.actions.size() == 2);
    CHECK((std::string_view(bundle.ftl.actions[0].name) == "hit_30"));
    CHECK((std::string_view(bundle.ftl.actions[1].name) == "hit_30"));
  }

  TEST_CASE("Edits generic selections and cut leading vertices") {
    pistoris::Model model;
    REQUIRE(model.setResourcePath("model:armor:chain_shirt"));

    ArxModelBone root{};
    root.name = {"ROOT", 4};
    root.position = {};
    const pistoris::BoneIndex root_index = take(model.addBone(root));
    CHECK(root_index == 0);

    ArxModelSelection cut_head{};
    cut_head.name = {"CUT_HEAD", 8};
    cut_head.has_leading_vertex = 2;
    cut_head.leading_position = {0.25f, 0.25f, 0.0f};
    cut_head.leading_bone = root_index;
    const pistoris::SelectionId cut_head_id = take(model.addSelection(cut_head));
    CHECK(cut_head_id == 0);
    const ArxModelSelection copied_cut_head = take(model.selection(cut_head_id));
    CHECK(copied_cut_head.has_leading_vertex == 1);

    ArxModelSelection reserved_name{};
    reserved_name.name = {"bad__name", 9};
    CHECK(model.addSelection(reserved_name).code() == ARX_MODEL_BAD_SELECTION_NAME);
    for (std::string_view name : {"_leading", "trailing_"}) {
      reserved_name.name = {name.data(), name.size()};
      CHECK(model.addSelection(reserved_name).code() == ARX_MODEL_BAD_SELECTION_NAME);
    }
    reserved_name.name = {"valid-name", 10};
    (void)take(model.addSelection(reserved_name));

    std::array<ArxModelVertex, 3> vertices{};
    vertices[0].position = {0.0f, 0.0f, 0.0f};
    vertices[1].position = {1.0f, 0.0f, 0.0f};
    vertices[2].position = {0.0f, 1.0f, 0.0f};
    for (ArxModelVertex& vertex : vertices) vertex.bone = root_index;

    for (const ArxModelVertex& vertex : vertices) {
      (void)take(model.addVertex(vertex));
    }
    const pistoris::VertexIndex cut_vertex = 0;
    REQUIRE(model.updateSelectionMembers(cut_head_id, {.vertices = &cut_vertex, .vertex_count = 1}));
    REQUIRE(model.updateSelectionMembers(cut_head_id, {}));
    CHECK(selectionVertices(model, cut_head_id) == std::vector<pistoris::VertexIndex>{0});
    REQUIRE(model.updateSelectionMembers(cut_head_id, {.vertices = &cut_vertex, .vertex_count = 0}));
    CHECK(selectionVertices(model, cut_head_id).empty());
    REQUIRE(model.updateSelectionMembers(cut_head_id, {.vertices = &cut_vertex, .vertex_count = 1}));

    ArxModelFace face{};
    face.normal = {0.0f, 0.0f, 1.0f};
    face.texture = pistoris::kNoTexture;
    face.flags = pistoris::kFaceBitQuad;
    for (std::size_t corner = 0; corner < 3; ++corner) {
      face.corners[corner].vertex = static_cast<pistoris::VertexIndex>(corner);
      face.corners[corner].normal = {0.0f, 0.0f, 1.0f};
    }
    const pistoris::FaceIndex face_index = take(model.addFace(face));
    const ArxModelFace normalized_face = model.faces()[face_index];
    CHECK((normalized_face.flags & pistoris::kFaceBitQuad) == 0);

    ArxModelFace invalid_face_normal = face;
    invalid_face_normal.normal.x = std::numeric_limits<float>::infinity();
    CHECK(model.setFace(face_index, invalid_face_normal).code() == ARX_MODEL_BAD_FACE_NORMAL);
    ArxModelFace invalid_corner_normal = face;
    invalid_corner_normal.corners[0].normal = {};
    CHECK(model.setFace(face_index, invalid_corner_normal).code() == ARX_MODEL_BAD_CORNER_NORMAL);
    CHECK(model.validate());

    const ArxModelBone copied = model.bones()[0];
    CHECK((stringView(copied.name) == "root"));
    REQUIRE(model.removeBone(root_index));
    CHECK(model.vertexCount() == 3);
    CHECK(model.vertices()[0].bone == pistoris::kInvalidBoneIndex);
    CHECK(model.origin().bone == pistoris::kInvalidBoneIndex);
    CHECK(take(model.selection(cut_head_id)).leading_bone == pistoris::kInvalidBoneIndex);
    CHECK(model.validate());

    REQUIRE(model.removeSelection(cut_head_id));
    CHECK(model.validateSelections());
  }

  TEST_CASE("Selection membership updates remain category-specific") {
    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));

    const pistoris::SelectionId selection = 0;
    const pistoris::VertexIndex vertex = 2;
    const pistoris::BoneIndex bone = 1;
    const pistoris::ActionPointIndex action_point = 0;
    REQUIRE(model.updateSelectionMembers(selection,
                                         {.vertices = &vertex,
                                          .vertex_count = 1,
                                          .bones = &bone,
                                          .bone_count = 1,
                                          .action_points = &action_point,
                                          .action_point_count = 1}));
    REQUIRE(model.setSelectionIncludesOrigin(selection, false));
    CHECK(selectionVertices(model, selection) == std::vector<pistoris::VertexIndex>{2});
    CHECK(selectionBones(model, selection) == std::vector<pistoris::BoneIndex>{1});
    CHECK(selectionActionPoints(model, selection) == std::vector<pistoris::ActionPointIndex>{0});
    CHECK_FALSE(selectionIncludesOrigin(model, selection));

    REQUIRE(model.clearSelectionVertices(selection));
    REQUIRE(model.clearSelectionBones(selection));
    REQUIRE(model.clearSelectionActionPoints(selection));
    REQUIRE(model.setSelectionIncludesOrigin(selection, true));
    CHECK(selectionVertices(model, selection).empty());
    CHECK(selectionBones(model, selection).empty());
    CHECK(selectionActionPoints(model, selection).empty());
    CHECK(selectionIncludesOrigin(model, selection));
  }

  TEST_CASE("Bulk selection masks expose stable bits including holes and bit 63") {
    pistoris::Model model;
    std::array<pistoris::SelectionId, 64> ids{};
    for (std::size_t i = 0; i < ids.size(); ++i) {
      const std::string name = "selection_" + std::to_string(i);
      ArxModelSelection selection{};
      selection.name = {name.data(), name.size()};
      ids[i] = take(model.addSelection(selection));
    }
    CHECK(model.activeSelectionMask() == std::numeric_limits<std::uint64_t>::max());
    CHECK(take(model.selectionMask(ids[63])) == (std::uint64_t{1} << 63U));

    REQUIRE(model.removeSelection(ids[1]));
    CHECK(model.activeSelectionMask() == (std::numeric_limits<std::uint64_t>::max() & ~(std::uint64_t{1} << 1U)));
    REQUIRE(model.addVertex({}));
    const std::array<std::uint64_t, 1> high_bit = {std::uint64_t{1} << 63U};
    REQUIRE(model.replaceVertexSelectionMasks(high_bit));
    CHECK(selectionVertices(model, ids[63]) == std::vector<pistoris::VertexIndex>{0});

    const std::array<std::uint64_t, 1> stale_bit = {std::uint64_t{1} << 1U};
    CHECK_FALSE(model.replaceVertexSelectionMasks(stale_bit));
    CHECK(selectionVertices(model, ids[63]) == std::vector<pistoris::VertexIndex>{0});

    const std::array<float, 3> replacement_position = {0.0f, 0.0f, 0.0f};
    REQUIRE(model.replaceVertices(replacement_position));
    CHECK(selectionVertices(model, ids[63]).empty());
  }

  TEST_CASE("Raw Model geometry replacement validates spans and resets dependent data") {
    pistoris::Model model;
    const std::array<float, 9> positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    REQUIRE(model.replaceVertices(positions));

    ArxModelSelection selection{};
    selection.name = {"selected", 8};
    const pistoris::SelectionId selection_id = take(model.addSelection(selection));
    const std::array<std::uint64_t, 3> masks = {1, 0, 1};
    REQUIRE(model.replaceVertexSelectionMasks(masks));
    CHECK(selectionVertices(model, selection_id) == std::vector<pistoris::VertexIndex>{0, 2});

    const std::array<std::uint32_t, 3> indices = {0, 1, 2};
    const std::array<float, 6> uvs = {0, 0, 1, 0, 0, 1};
    const std::array<float, 9> corner_normals = {0, 0, 2, 0, 0, 2, 0, 0, 2};
    const std::array<pistoris::TextureIndex, 1> textures = {pistoris::kNoTexture};
    const std::array<float, 1> transvals = {0};
    const std::array<float, 3> face_normals = {0, 0, 3};
    REQUIRE(model.replaceFaces(indices, uvs, corner_normals, textures, transvals, face_normals));
    CHECK(model.faces()[0].normal.z == doctest::Approx(1.0f));
    CHECK(model.faces()[0].corners[0].normal.z == doctest::Approx(1.0f));

    const std::array<float, 5> malformed_uvs{};
    CHECK(model.replaceFaces(indices, malformed_uvs, corner_normals, textures, transvals, face_normals).code() ==
          ARX_MODEL_BAD_FACE_COUNT);
    CHECK(model.faceCount() == 1);
    CHECK(model.faces()[0].corners[2].vertex == 2);

    const std::array<std::uint32_t, 3> invalid_indices = {0, 1, 3};
    CHECK_FALSE(model.replaceFaces(invalid_indices, uvs, corner_normals, textures, transvals, face_normals));
    CHECK(model.faces()[0].corners[2].vertex == 2);

    REQUIRE(model.replaceVertices(positions));
    CHECK(model.faceCount() == 0);
    CHECK(selectionVertices(model, selection_id).empty());
  }

  TEST_CASE("Clearing rig categories clears only their selection memberships") {
    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));

    const pistoris::SelectionId selection = 0;
    const std::vector<pistoris::VertexIndex> vertices = selectionVertices(model, selection);
    REQUIRE_FALSE(vertices.empty());
    REQUIRE_FALSE(selectionBones(model, selection).empty());
    REQUIRE_FALSE(selectionActionPoints(model, selection).empty());
    REQUIRE(selectionIncludesOrigin(model, selection));

    model.clearBones();
    CHECK(selectionVertices(model, selection) == vertices);
    CHECK(selectionBones(model, selection).empty());
    CHECK_FALSE(selectionActionPoints(model, selection).empty());
    CHECK(selectionIncludesOrigin(model, selection));
    CHECK(model.vertices()[0].bone == pistoris::kInvalidBoneIndex);
    CHECK(model.origin().bone == pistoris::kInvalidBoneIndex);
    CHECK(model.actionPoints()[0].bone == pistoris::kInvalidBoneIndex);
    CHECK(take(model.selection(3)).leading_bone == pistoris::kInvalidBoneIndex);

    model.clearActionPoints();
    const ArxModelActionPoint replacement_action_point{{"replacement", 11}, {}, pistoris::kInvalidBoneIndex};
    REQUIRE(model.addActionPoint(replacement_action_point));
    CHECK(selectionVertices(model, selection) == vertices);
    CHECK(selectionBones(model, selection).empty());
    CHECK(selectionActionPoints(model, selection).empty());
    CHECK(selectionIncludesOrigin(model, selection));
  }

  TEST_CASE("Vertex compaction preserves aligned semantic data") {
    pistoris::Model model;
    REQUIRE(model.setResourcePath("model:npc:compact_test"));
    std::array<ArxModelVertex, 4> vertices{};
    vertices[0].position = {0.0f, 0.0f, 0.0f};
    vertices[1].position = {9.0f, 9.0f, 9.0f};
    vertices[2].position = {1.0f, 0.0f, 0.0f};
    vertices[3].position = {0.0f, 1.0f, 0.0f};
    ArxModelSelection head{};
    head.name = {"head", 4};
    const pistoris::SelectionId head_id = take(model.addSelection(head));
    ArxModelSelection torso{};
    torso.name = {"torso", 5};
    const pistoris::SelectionId torso_id = take(model.addSelection(torso));
    for (const ArxModelVertex& vertex : vertices) {
      (void)take(model.addVertex(vertex));
    }
    const std::array<pistoris::VertexIndex, 2> head_vertices = {0, 3};
    const std::array<pistoris::VertexIndex, 3> torso_vertices = {1, 2, 3};
    REQUIRE(model.updateSelectionMembers(head_id,
                                         {.vertices = head_vertices.data(), .vertex_count = head_vertices.size()}));
    REQUIRE(model.updateSelectionMembers(torso_id,
                                         {.vertices = torso_vertices.data(), .vertex_count = torso_vertices.size()}));

    ArxModelFace face{};
    face.normal = {0.0f, 0.0f, 1.0f};
    face.texture = pistoris::kNoTexture;
    face.corners[0].vertex = 2;
    face.corners[1].vertex = 0;
    face.corners[2].vertex = 3;
    for (ArxModelCorner& corner : face.corners) corner.normal = {0.0f, 0.0f, 1.0f};
    (void)take(model.addFace(face));

    const std::size_t removed = take(model.compactVertices());
    CHECK(removed == 1);
    CHECK(model.vertexCount() == 3);
    CHECK(model.validate());

    const auto compact = model.vertices();
    CHECK(compact[0].position == ArxVector3{0.0f, 0.0f, 0.0f});
    CHECK(compact[1].position == ArxVector3{1.0f, 0.0f, 0.0f});
    CHECK(compact[2].position == ArxVector3{0.0f, 1.0f, 0.0f});
    CHECK(selectionVertices(model, head_id) == std::vector<pistoris::VertexIndex>{0, 2});
    CHECK(selectionVertices(model, torso_id) == std::vector<pistoris::VertexIndex>{1, 2});
  }

  TEST_CASE("Vertex welding preserves bone and selection identity") {
    pistoris::Model model;
    (void)take(model.addBone({{"root", 4}, {}, pistoris::kInvalidBoneIndex, 0.0f}));
    (void)take(model.addBone({{"child", 5}, {}, 0, 0.0f}));
    ArxModelSelection selected{};
    selected.name = {"selected", 8};
    const pistoris::SelectionId selection = take(model.addSelection(selected));

    const std::array<ArxModelVertex, 6> vertices = {
        ArxModelVertex{{0.0f, 0.0f, 0.0f}, 0},
        ArxModelVertex{{1.0f, 0.0f, 0.0f}, 0},
        ArxModelVertex{{0.1f, 0.0f, 0.0f}, 0},
        ArxModelVertex{{0.0f, 1.0f, 0.0f}, 0},
        ArxModelVertex{{0.2f, 0.0f, 0.0f}, 0},
        ArxModelVertex{{0.1f, 0.0f, 0.0f}, 1},
    };
    for (const ArxModelVertex& vertex : vertices) (void)take(model.addVertex(vertex));
    const std::array<pistoris::VertexIndex, 4> selected_vertices = {0, 2, 4, 5};
    REQUIRE(model.updateSelectionMembers(
        selection, {.vertices = selected_vertices.data(), .vertex_count = selected_vertices.size()}));

    ArxModelFace face{};
    face.normal = {0.0f, 0.0f, 1.0f};
    face.texture = pistoris::kNoTexture;
    face.corners[0].vertex = 0;
    face.corners[1].vertex = 1;
    face.corners[2].vertex = 3;
    for (ArxModelCorner& corner : face.corners) corner.normal = face.normal;
    (void)take(model.addFace(face));

    REQUIRE(model.weldVertices({.radius = 0.11f}));
    CHECK(model.vertexCount() == 4);
    CHECK(selectionVertices(model, selection) == std::vector<pistoris::VertexIndex>{1, 3});
    CHECK(model.vertices()[2].bone == 0);
    CHECK(model.vertices()[3].bone == 1);
    CHECK(model.validate());

    CHECK(model.weldVertices({.radius = 0.0f}).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Vertex welding applies the selected degenerate-face policy") {
    const auto thin_triangle = [] {
      pistoris::Model model;
      const std::array<ArxModelVertex, 3> vertices = {
          ArxModelVertex{{0.0f, 0.0f, 0.0f}},
          ArxModelVertex{{0.01f, 0.0f, 0.0f}},
          ArxModelVertex{{0.0f, 1.0f, 0.0f}},
      };
      for (const ArxModelVertex& vertex : vertices) (void)take(model.addVertex(vertex));
      ArxModelFace face{};
      face.normal = {0.0f, 0.0f, 1.0f};
      face.texture = pistoris::kNoTexture;
      face.corners[0].vertex = 0;
      face.corners[1].vertex = 1;
      face.corners[2].vertex = 2;
      for (ArxModelCorner& corner : face.corners) corner.normal = face.normal;
      (void)take(model.addFace(face));
      return model;
    };

    pistoris::Model preserved = thin_triangle();
    REQUIRE(preserved.weldVertices({.radius = 0.1f}));
    CHECK(preserved.vertexCount() == 3);
    CHECK(preserved.faceCount() == 1);

    pistoris::Model rejected = thin_triangle();
    CHECK(rejected.weldVertices({.radius = 0.1f, .degenerate_faces = pistoris::Model::DegenerateFacePolicy::kReject})
              .code() == ARX_MODEL_DEGENERATE_FACE);
    CHECK(rejected.vertexCount() == 3);
    CHECK(rejected.faceCount() == 1);

    pistoris::Model discarded = thin_triangle();
    REQUIRE(
        discarded.weldVertices({.radius = 0.1f, .degenerate_faces = pistoris::Model::DegenerateFacePolicy::kDiscard}));
    CHECK(discarded.vertexCount() == 2);
    CHECK(discarded.faceCount() == 0);
  }

  TEST_CASE("Intermediate counts may exceed native FTL limits") {
    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));

    std::vector<ArxModelActionPoint> action_points(1025);
    for (ArxModelActionPoint& point : action_points) {
      point.name = {"hit_30", 6};
      point.bone = 0;
    }
    model.clearActionPoints();
    for (const ArxModelActionPoint& point : action_points) REQUIRE(model.addActionPoint(point));
    CHECK(model.validate());

    const std::vector<std::uint8_t> glb = take(model.exportGlb());
    pistoris::Model glb_roundtrip = take(pistoris::Model::importGlb(glb));
    CHECK(glb_roundtrip.actionPointCount() == action_points.size());

    const pistoris::ObjBundle obj = take(model.exportObj("intermediate_counts"));
    pistoris::Model obj_roundtrip = take(pistoris::Model::importObj(obj.text, obj.mtl));
    CHECK(obj_roundtrip.actionPointCount() == action_points.size());

    CHECK(model.bakeNativeBundle({}).code() == ARX_MODEL_TOO_MANY_ACTION_POINTS);
  }

  TEST_CASE("Native import rejects Skeletons beyond the intermediate limit") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.groups.resize(1025, native.groups.front());
    CHECK(pistoris::Model::importNative(native).code() == ARX_MODEL_TOO_MANY_BONES);
  }

  TEST_CASE("Edits report focused semantic errors") {
    pistoris::Model model;
    CHECK(model.setResourcePath("not/a/model").code() == ARX_MODEL_BAD_RESOURCE_PATH);

    CHECK(model.replaceVertices(std::array<float, 2>{}).code() == ARX_MODEL_BAD_VERTEX_COUNT);

    ArxModelVertex vertex{};
    vertex.bone = 0;
    CHECK(model.addVertex(vertex).code() == ARX_MODEL_BAD_VERTEX_BONE);

    vertex.bone = pistoris::kInvalidBoneIndex;
    (void)take(model.addVertex(vertex));
    ArxModelSelection selection{};
    selection.name = {"test", 4};
    const pistoris::SelectionId selection_id = take(model.addSelection(selection));
    const pistoris::VertexIndex invalid_vertex = 1;
    CHECK(model.updateSelectionMembers(selection_id, {.vertices = &invalid_vertex, .vertex_count = 1}).code() ==
          ARX_MODEL_BAD_SELECTION_VERTEX);

    ArxModelOrigin origin{};
    origin.bone = 0;
    CHECK(model.setOrigin(origin).code() == ARX_MODEL_BAD_ORIGIN_BONE);

    ArxModelBone bone{};
    bone.name = {nullptr, 1};
    CHECK(model.addBone(bone).code() == ARX_INVALID_DATA_POINTER);
    bone.name = {nullptr, 0};
    CHECK(model.addBone(bone).code() == ARX_MODEL_BAD_BONE_NAME);
    bone.name = {"root", 4};
    (void)take(model.addBone(bone));

    ArxModelActionPoint point{};
    point.name = {nullptr, 1};
    CHECK(model.addActionPoint(point).code() == ARX_INVALID_DATA_POINTER);
    point.name = {nullptr, 0};
    CHECK(model.addActionPoint(point).code() == ARX_MODEL_BAD_ACTION_POINT_NAME);
    point.name = {"view_attach", 11};
    point.bone = 1;
    CHECK(model.addActionPoint(point).code() == ARX_MODEL_BAD_ACTION_POINT_BONE);
  }

  TEST_CASE("Batch vertex insertion validates before changing aligned state") {
    pistoris::Model model;
    const std::array<ArxModelVertex, 3> vertices = {
        ArxModelVertex{{0.0f, 0.0f, 0.0f}},
        ArxModelVertex{{1.0f, 0.0f, 0.0f}},
        ArxModelVertex{{0.0f, 1.0f, 0.0f}},
    };
    const pistoris::VertexIndex first = take(model.addVertices(vertices));
    CHECK(first == 0);
    CHECK(model.vertexCount() == vertices.size());

    std::array<ArxModelVertex, 2> invalid = {vertices[0], vertices[1]};
    invalid[1].bone = 0;
    CHECK(model.addVertices(invalid).code() == ARX_MODEL_BAD_VERTEX_BONE);
    CHECK(model.vertexCount() == vertices.size());
    CHECK(model.addVertices({}).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Face edits replace corner normals directly") {
    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));
    ArxModelFace face = model.faces()[0];
    face.corners[0].normal = {1.0f, 0.0f, 0.0f};
    REQUIRE(model.setFace(0, face));
    face = model.faces()[0];
    CHECK(face.corners[0].normal == ArxVector3{1.0f, 0.0f, 0.0f});
  }

  TEST_CASE("Transforms all positional Model state atomically") {
    pistoris::Model model = take(pistoris::Model::importNative(makeSemanticModelFtl()));

    const ArxModelVertex unchanged = model.vertices()[1];
    CHECK(model.scale(std::numeric_limits<float>::max()).code() == ARX_MODEL_BAD_BONE_BLOB_SHADOW_SIZE);
    const ArxModelVertex after_failed_scale = model.vertices()[1];
    CHECK(after_failed_scale.position == unchanged.position);

    REQUIRE(model.scale(2.0f));
    REQUIRE(model.rotate({2.0f, 0.0f, 0.0f, 2.0f}));
    REQUIRE(model.translate({10.0f, 20.0f, 30.0f}));

    const auto vertices = model.vertices();
    CHECK(vertices[0].position == ArxVector3{10.0f, 20.0f, 30.0f});
    CHECK(vertices[1].position.x == doctest::Approx(10.0f));
    CHECK(vertices[1].position.y == doctest::Approx(22.0f));
    CHECK(vertices[2].position.x == doctest::Approx(8.0f));
    CHECK(vertices[2].position.y == doctest::Approx(20.0f));

    const ArxModelFace face = model.faces()[0];
    CHECK(face.normal.x == doctest::Approx(0.0f));
    CHECK(face.normal.y == doctest::Approx(1.0f));
    CHECK(face.corners[0].normal == ArxVector3{0.0f, 0.0f, 1.0f});

    const auto bones = model.bones();
    CHECK(bones[0].blob_shadow_size == doctest::Approx(4.0f));
    CHECK(bones[1].position.x == doctest::Approx(8.0f));
    CHECK(bones[1].position.y == doctest::Approx(20.0f));

    const ArxModelActionPoint action = model.actionPoints()[0];
    CHECK(action.position.x == doctest::Approx(10.0f));
    CHECK(action.position.y == doctest::Approx(22.0f));

    const ArxModelSelection selection = take(model.selection(3));
    CHECK(selection.leading_position.x == doctest::Approx(9.0f));
    CHECK(selection.leading_position.y == doctest::Approx(21.0f));

    CHECK(model.rotate({0.0f, 0.0f, 0.0f, 0.0f}).code() == ARX_INVALID_OPTIONS);
    CHECK(model.translate({std::numeric_limits<float>::infinity(), 0.0f, 0.0f}).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Snaps bone positions in engine-centered Model space") {
    pistoris::Ftl reference_native = makeSemanticModelFtl();
    pistoris::Ftl target_native = reference_native;
    target_native.vertices[target_native.groups[0].origin].position = {40.0f, 50.0f, 60.0f};
    target_native.vertices[target_native.groups[1].origin].position = {70.0f, 80.0f, 90.0f};

    pistoris::Model reference = take(pistoris::Model::importNative(reference_native));
    pistoris::Model target = take(pistoris::Model::importNative(target_native));
    REQUIRE(target.applyReference(reference, {.snap_bone_positions = true}));

    const pistoris::NativeModelBundle baked = take(target.bakeNativeBundle({}));
    REQUIRE(baked.ftl.groups.size() == reference_native.groups.size());
    for (std::size_t index = 0; index < reference_native.groups.size(); ++index) {
      const ArxVector3 expected = reference_native.vertices[reference_native.groups[index].origin].position -
                                  reference_native.vertices[reference_native.header.origin].position;
      const ArxVector3 actual = baked.ftl.vertices[baked.ftl.groups[index].origin].position -
                                baked.ftl.vertices[baked.ftl.header.origin].position;
      CHECK(actual == expected);
    }

    pistoris::Model no_bones;
    CHECK(target.applyReference(no_bones, {.snap_bone_positions = true}).code() ==
          ARX_MODEL_REFERENCE_BONE_COUNT_MISMATCH);

    ArxModelBone target_extra{{"target-extra", 12}, {1.0f, 2.0f, 3.0f}, 1, 0.0f};
    ArxModelBone reference_extra{{"reference-extra", 15}, {4.0f, 5.0f, 6.0f}, 0, 0.0f};
    (void)take(target.addBone(target_extra));
    (void)take(reference.addBone(reference_extra));
    const ArxVector3 unchanged = target_extra.position;
    CHECK(target.applyReference(reference, {.snap_bone_positions = true}).code() ==
          ARX_MODEL_REFERENCE_BONE_TOPOLOGY_MISMATCH);
    const auto target_bones = target.bones();
    CHECK(target_bones.back().position == unchanged);

    REQUIRE(target.removeBone(2));
    REQUIRE(reference.removeBone(2));
    ArxModelBone renamed = reference.bones()[0];
    renamed.name = {"renamed-root", 12};
    REQUIRE(reference.setBone(0, renamed));
    WarningCapture warnings;
    CHECK(target.applyReference(reference, {.snap_bone_positions = true}));
    REQUIRE(warnings.messages.size() == 1);
    CHECK(warnings.messages[0].find("Model reference: bone 0 name mismatch") != std::string::npos);
  }

  TEST_CASE("Applies reference selection memberships transactionally by semantic identity") {
    pistoris::Model target;
    pistoris::Model reference;

    const auto add_bones = [](pistoris::Model& model, float offset) {
      const ArxModelBone root{{"root", 4}, {offset, 0.0f, 0.0f}, pistoris::kInvalidBoneIndex, 0.0f};
      const ArxModelBone child{{"child", 5}, {offset, 1.0f, 0.0f}, 0, 0.0f};
      (void)take(model.addBone(root));
      (void)take(model.addBone(child));
    };
    add_bones(target, 10.0f);
    add_bones(reference, 20.0f);

    const auto add_action = [](pistoris::Model& model, std::string_view name) {
      ArxModelActionPoint point{};
      point.name = {name.data(), name.size()};
      point.bone = 0;
      (void)take(model.addActionPoint(point));
    };
    add_action(target, "hit_30");
    add_action(target, "hit_30");
    add_action(target, "target_only_action");
    add_action(reference, "hit_30");
    add_action(reference, "reference_only_action");
    add_action(reference, "hit_30");

    const auto add_selection = [](pistoris::Model& model, std::string_view name) {
      ArxModelSelection selection{};
      selection.name = {name.data(), name.size()};
      return take(model.addSelection(selection));
    };
    const pistoris::SelectionId target_only = add_selection(target, "target_only");
    const pistoris::SelectionId target_shared_b = add_selection(target, "shared_b");
    const pistoris::SelectionId target_shared_a = add_selection(target, "shared_a");
    const pistoris::SelectionId reference_shared_a = add_selection(reference, "shared_a");
    const pistoris::SelectionId reference_only = add_selection(reference, "reference_only");
    const pistoris::SelectionId reference_shared_b = add_selection(reference, "shared_b");

    const auto set_members = [](pistoris::Model& model,
                                pistoris::SelectionId id,
                                std::span<const pistoris::BoneIndex>
                                    bones,
                                std::span<const pistoris::ActionPointIndex>
                                    actions) {
      const ArxModelSelectionMembersInput members{
          .bones = bones.data(),
          .bone_count = bones.size(),
          .action_points = actions.data(),
          .action_point_count = actions.size(),
      };
      REQUIRE(model.updateSelectionMembers(id, members));
    };
    const std::array<pistoris::BoneIndex, 2> both_bones = {0, 1};
    const std::array<pistoris::BoneIndex, 1> bone_0 = {0};
    const std::array<pistoris::BoneIndex, 1> bone_1 = {1};
    const std::array<pistoris::ActionPointIndex, 2> target_actions = {0, 2};
    const std::array<pistoris::ActionPointIndex, 1> action_0 = {0};
    const std::array<pistoris::ActionPointIndex, 1> action_1 = {1};
    const std::array<pistoris::ActionPointIndex, 1> action_2 = {2};
    const std::array<pistoris::ActionPointIndex, 2> reference_shared_actions = {0, 1};
    set_members(target, target_only, both_bones, target_actions);
    set_members(target, target_shared_a, bone_1, action_1);
    set_members(target, target_shared_b, bone_0, action_0);
    set_members(reference, reference_shared_a, bone_0, reference_shared_actions);
    set_members(reference, reference_only, bone_0, action_0);
    set_members(reference, reference_shared_b, bone_1, action_2);

    const pistoris::Model::ReferenceOptions options{
        .snap_bone_positions = true,
        .copy_bone_selection_memberships = true,
        .copy_action_point_selections = true,
    };
    CHECK(target.applyReference(reference, {}).code() == ARX_INVALID_OPTIONS);

    pistoris::Model incompatible(reference);
    const ArxModelBone extra{{"extra", 5}, {}, pistoris::kInvalidBoneIndex, 0.0f};
    (void)take(incompatible.addBone(extra));
    CHECK(target.applyReference(incompatible, options).code() == ARX_MODEL_REFERENCE_BONE_COUNT_MISMATCH);
    CHECK(selectionBones(target, target_shared_a) == std::vector<pistoris::BoneIndex>{1});
    CHECK(selectionActionPoints(target, target_shared_a) == std::vector<pistoris::ActionPointIndex>{1});
    const ArxModelBone unchanged_root = target.bones()[0];
    CHECK(unchanged_root.position == ArxVector3{10.0f, 0.0f, 0.0f});

    WarningCapture warnings;
    REQUIRE(target.applyReference(reference, options));
    REQUIRE(warnings.messages.size() == 2);
    CHECK(warnings.messages[0].find("selection 'reference_only' is absent from target, omitted 2 membership(s)") !=
          std::string::npos);
    CHECK(warnings.messages[1].find("omitted 1 selection membership(s) from 1 unmatched reference action point(s)") !=
          std::string::npos);
    CHECK(selectionBones(target, target_shared_a) == std::vector<pistoris::BoneIndex>{0});
    CHECK(selectionBones(target, target_shared_b) == std::vector<pistoris::BoneIndex>{1});
    CHECK(selectionBones(target, target_only).empty());
    CHECK(selectionActionPoints(target, target_shared_a) == std::vector<pistoris::ActionPointIndex>{0});
    CHECK(selectionActionPoints(target, target_shared_b) == std::vector<pistoris::ActionPointIndex>{1});
    CHECK(selectionActionPoints(target, target_only).empty());
    CHECK(selectionId(target, "reference_only") == pistoris::kInvalidSelectionId);
    const ArxModelBone copied_root = target.bones()[0];
    CHECK(copied_root.position == ArxVector3{20.0f, 0.0f, 0.0f});
  }

  TEST_CASE("Infers bone selection memberships from directly owned vertices") {
    pistoris::Model model;
    (void)take(model.addBone({{"root", 4}, {}, pistoris::kInvalidBoneIndex, 0.0f}));
    (void)take(model.addBone({{"child", 5}, {}, 0, 0.0f}));
    (void)take(model.addBone({{"empty", 5}, {}, 0, 0.0f}));

    std::vector<ArxModelVertex> vertices(22);
    for (std::size_t index = 0; index < 10; ++index) vertices[index].bone = 0;
    for (std::size_t index = 10; index < 21; ++index) vertices[index].bone = 1;
    vertices[21].bone = pistoris::kInvalidBoneIndex;
    (void)take(model.addVertices(vertices));

    (void)take(model.addActionPoint({{"attach", 6}, {}, 0}));
    ArxModelSelection armor{};
    armor.name = {"armor", 5};
    const pistoris::SelectionId selection = take(model.addSelection(armor));

    std::vector<pistoris::VertexIndex> selected;
    selected.reserve(19);
    for (pistoris::VertexIndex index = 0; index < 9; ++index) selected.push_back(index);
    for (pistoris::VertexIndex index = 10; index < 19; ++index) selected.push_back(index);
    selected.push_back(21);
    const std::array<pistoris::BoneIndex, 2> initial_bones = {1, 2};
    const std::array<pistoris::ActionPointIndex, 1> selected_actions = {0};
    REQUIRE(model.updateSelectionMembers(selection,
                                         {.vertices = selected.data(),
                                          .vertex_count = selected.size(),
                                          .bones = initial_bones.data(),
                                          .bone_count = initial_bones.size(),
                                          .action_points = selected_actions.data(),
                                          .action_point_count = selected_actions.size()}));
    REQUIRE(model.setSelectionIncludesOrigin(selection, true));

    REQUIRE(model.inferBoneSelectionMemberships());
    CHECK(selectionBones(model, selection) == std::vector<pistoris::BoneIndex>{0});
    CHECK(selectionVertices(model, selection) == selected);
    CHECK(selectionActionPoints(model, selection) == std::vector<pistoris::ActionPointIndex>{0});
    CHECK(selectionIncludesOrigin(model, selection));
  }
}
