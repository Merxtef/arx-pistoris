// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/types.h"
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
#include <vector>

namespace {

static_assert((pistoris::kModelFaceBitsAll & pistoris::kFaceBitQuad) == 0);

std::string_view stringView(ArxStringView value) { return {value.data, value.size}; }

std::vector<pistoris::VertexIndex> selectionVertices(const pistoris::Model& model, pistoris::SelectionId id) {
  std::size_t count = 0;
  CHECK(model.selectionVertexCount(id, count) == ARX_OK);
  std::vector<pistoris::VertexIndex> result(count);
  CHECK(model.copySelectionVertices(id, 0, count, result.data()) == ARX_OK);
  return result;
}

std::vector<pistoris::BoneIndex> selectionBones(const pistoris::Model& model, pistoris::SelectionId id) {
  std::size_t count = 0;
  CHECK(model.selectionBoneCount(id, count) == ARX_OK);
  std::vector<pistoris::BoneIndex> result(count);
  CHECK(model.copySelectionBones(id, 0, count, result.data()) == ARX_OK);
  return result;
}

std::vector<pistoris::ActionPointIndex> selectionActionPoints(const pistoris::Model& model, pistoris::SelectionId id) {
  std::size_t count = 0;
  CHECK(model.selectionActionPointCount(id, count) == ARX_OK);
  std::vector<pistoris::ActionPointIndex> result(count);
  CHECK(model.copySelectionActionPoints(id, 0, count, result.data()) == ARX_OK);
  return result;
}

pistoris::SelectionId selectionId(const pistoris::Model& model, std::string_view name) {
  std::vector<pistoris::SelectionId> ids(model.selectionCount());
  CHECK(model.copySelectionIds(0, ids.size(), ids.data()) == ARX_OK);
  for (pistoris::SelectionId id : ids) {
    ArxModelSelection selection{};
    CHECK(model.selection(id, selection) == ARX_OK);
    if (stringView(selection.name) == name) return id;
  }
  return pistoris::kInvalidSelectionId;
}

bool selectionIncludesOrigin(const pistoris::Model& model, pistoris::SelectionId id) {
  bool result = false;
  CHECK(model.selectionIncludesOrigin(id, result) == ARX_OK);
  return result;
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
  TEST_CASE("Clears mesh textures with geometry") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
    REQUIRE(model.textureCount() != 0);

    model.clearMesh();

    CHECK(model.vertexCount() == 0);
    CHECK(model.faceCount() == 0);
    CHECK(model.textureCount() == 0);
  }

  TEST_CASE("Returns the first format path for each normalized Model texture") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.texture_containers.push_back(native.texture_containers.front());
    setFtlName("graph/obj3d/textures/my_tex",
               native.texture_containers.back().filename,
               sizeof(native.texture_containers.back().filename));

    pistoris::Model model;
    std::vector<std::string> sources;
    REQUIRE(pistoris::Model::importNative(model, native, &sources) == ARX_OK);
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

    pistoris::Model model;
    std::vector<std::string> sources;
    REQUIRE(pistoris::Model::importNative(model, native, &sources) == ARX_OK);
    REQUIRE(model.validate() == ARX_OK);
    CHECK(model.textureCount() == 1);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == "graph/obj3d/textures/my_tex");

    std::array<ArxModelFace, 2> faces{};
    REQUIRE(model.copyFaces(0, faces.size(), faces.data()) == ARX_OK);
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

    pistoris::Model model;
    std::vector<std::string> sources;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl, &sources) == ARX_OK);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == "Imported/Textures/WALL.BMP");
    ArxTextureView texture{};
    REQUIRE(model.copyTextureViews(0, 1, &texture) == ARX_OK);
    CHECK((stringView(texture.path) == "imported/textures/wall"));
    CHECK((stringView(texture.external_image_extension) == ".bmp"));
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

    pistoris::Model model;
    std::vector<std::string> sources;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl, &sources) == ARX_OK);
    REQUIRE(model.textureCount() == 2);
    std::array<ArxTextureView, 2> textures{};
    REQUIRE(model.copyTextureViews(0, textures.size(), textures.data()) == ARX_OK);
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
    std::vector<std::string> paths;
    REQUIRE(pistoris::objMaterialLibraryPaths(kObj, paths) == ARX_OK);
    REQUIRE(paths.size() == 2);
    CHECK(paths[0] == "materials/main.mtl");
    CHECK(paths[1] == "details.mtl");

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

    pistoris::Model model;
    std::vector<std::string> sources;
    REQUIRE(pistoris::Model::importObj(model, kObj, libraries, &sources) == ARX_OK);
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

    pistoris::Model model;
    std::vector<std::string> sources;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl, &sources) == ARX_OK);
    REQUIRE(model.textureCount() == 2);
    REQUIRE(sources.size() == 2);
    CHECK(sources[0] == "textures/wall?.bmp");
    CHECK(sources[1] == "textures/wall*.bmp");
    std::array<ArxTextureView, 2> textures{};
    REQUIRE(model.copyTextureViews(0, textures.size(), textures.data()) == ARX_OK);
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl) == ARX_OK);
    REQUIRE(model.validate() == ARX_OK);
    CHECK(model.vertexCount() == 5);
    CHECK(model.faceCount() == 3);
    CHECK(model.textureCount() == 1);
    CHECK(model.actionPointCount() == 2);
    CHECK(model.boneCount() == 0);
    CHECK(model.selectionCount() == 0);

    std::array<ArxModelVertex, 5> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(vertices[0].position == ArxVector3{1.0f, -2.0f, -3.0f});
    CHECK(vertices[0].bone == pistoris::kInvalidBoneIndex);

    std::array<ArxModelActionPoint, 2> actions{};
    REQUIRE(model.copyActionPoints(0, actions.size(), actions.data()) == ARX_OK);
    CHECK((stringView(actions[0].name) == "hit_30"));
    CHECK(actions[0].position == ArxVector3{4.0f, -5.0f, -6.0f});
    CHECK((stringView(actions[1].name) == "hit_30"));
    CHECK(actions[1].position == ArxVector3{7.0f, -8.0f, -9.0f});

    ArxTextureView texture{};
    REQUIRE(model.copyTextureViews(0, 1, &texture) == ARX_OK);
    CHECK((stringView(texture.path) == "textures/wall"));
    CHECK((stringView(texture.external_image_extension) == ".bmp"));

    pistoris::ObjBundle encoded;
    REQUIRE(model.exportObj("static_model", encoded) == ARX_OK);
    CHECK(encoded.text.find("# origin") == std::string::npos);
    const std::size_t first_action = encoded.text.find("# arx_action hit_30 ");
    REQUIRE(first_action != std::string::npos);
    CHECK(encoded.text.find("# arx_action hit_30 ", first_action + 1) != std::string::npos);
    CHECK(encoded.text.find("v 1 2 3") != std::string::npos);
    CHECK(encoded.mtl.find("map_Kd textures/wall.bmp") != std::string::npos);
    CHECK(encoded.mtl.find("arx_path") == std::string::npos);

    pistoris::Model roundtrip;
    REQUIRE(pistoris::Model::importObj(roundtrip, encoded.text, encoded.mtl) == ARX_OK);
    CHECK(roundtrip.validate() == ARX_OK);
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj) == ARX_OK);

    ArxModelFace face{};
    REQUIRE(model.copyFaces(0, 1, &face) == ARX_OK);
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

    pistoris::NativeModelBundle native;
    REQUIRE(model.bakeNativeBundle({}, native) == ARX_OK);
    REQUIRE(native.ftl.faces.size() == 1);
    const pistoris::ftl::Face& native_face = native.ftl.faces[0];
    const std::array native_u = {native_face.u.x, native_face.u.y, native_face.u.z};
    const std::array native_v = {native_face.v.x, native_face.v.y, native_face.v.z};
    for (std::size_t corner = 0; corner < native_u.size(); ++corner) {
      CHECK(native_u[corner] == doctest::Approx(face.corners[corner].u));
      CHECK(native_v[corner] == doctest::Approx(face.corners[corner].v));
    }

    pistoris::ObjBundle encoded;
    REQUIRE(model.exportObj("texture_origin", encoded) == ARX_OK);
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
    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl) == ARX_OK);
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(model.setTextureImage(0, {image.data(), image.size()}) == ARX_OK);

    pistoris::ObjBundle encoded;
    REQUIRE(model.exportObj("static_model", encoded) == ARX_OK);
    CHECK(encoded.mtl.find("map_Kd textures/wall.bmp") != std::string::npos);
    REQUIRE(encoded.texture_files.size() == 1);
    CHECK(encoded.texture_files[0].source_texture == 0);
    CHECK(encoded.texture_files[0].path == "textures/wall.bmp");
    CHECK(encoded.texture_files[0].encoded_image == image);

    const pistoris::ObjExportOptions options{.include_files = false};
    REQUIRE(model.exportObj("static_model", options, encoded) == ARX_OK);
    CHECK(encoded.texture_files.empty());
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
    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl) == ARX_OK);
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl) == ARX_OK);
    CHECK(model.textureCount() == 0);
  }

  TEST_CASE("Respects OBJ smoothing groups for generated normals") {
    const auto import = [](std::string_view first_smoothing, std::string_view second_smoothing) {
      std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n";
      obj.append(first_smoothing).append("f 1 2 3\n");
      obj.append(second_smoothing).append("f 1 4 2\n");
      pistoris::Model model;
      REQUIRE(pistoris::Model::importObj(model, obj) == ARX_OK);
      std::array<ArxModelFace, 2> faces{};
      REQUIRE(model.copyFaces(0, faces.size(), faces.data()) == ARX_OK);
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj) == ARX_OK);
    std::array<ArxModelFace, 2> faces{};
    REQUIRE(model.copyFaces(0, faces.size(), faces.data()) == ARX_OK);
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj) == ARX_OK);
    std::array<ArxModelFace, 2> faces{};
    REQUIRE(model.copyFaces(0, faces.size(), faces.data()) == ARX_OK);
    std::array<pistoris::VertexIndex, 2> shared{};
    std::size_t shared_count = 0;
    for (const ArxModelCorner& first : faces[0].corners)
      for (const ArxModelCorner& second : faces[1].corners)
        if (first.vertex == second.vertex) {
          REQUIRE(shared_count < shared.size());
          shared[shared_count++] = first.vertex;
        }
    REQUIRE(shared_count == 2);
    std::array<ArxModelVertex, 4> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj) == ARX_OK);
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl) == ARX_OK);
    std::array<ArxModelFace, 2> faces{};
    REQUIRE(model.copyFaces(0, faces.size(), faces.data()) == ARX_OK);
    CHECK(faces[0].transval == doctest::Approx(0.25f));
    CHECK(faces[1].transval == doctest::Approx(0.75f));

    pistoris::ObjBundle encoded;
    REQUIRE(model.exportObj("transparent", encoded) == ARX_OK);
    CHECK(encoded.mtl.find("newmtl no_tex__TRANS__TRANSVAL_0.25\n") != std::string::npos);
    CHECK(encoded.mtl.find("newmtl no_tex__TRANS__TRANSVAL_0.75\n") != std::string::npos);
    CHECK(encoded.mtl.find("d 0.75") != std::string::npos);
    CHECK(encoded.mtl.find("d 0.25") != std::string::npos);

    pistoris::Model roundtrip;
    REQUIRE(pistoris::Model::importObj(roundtrip, encoded.text, encoded.mtl) == ARX_OK);
    REQUIRE(roundtrip.copyFaces(0, faces.size(), faces.data()) == ARX_OK);
    CHECK(faces[0].transval == doctest::Approx(0.25f));
    CHECK(faces[1].transval == doctest::Approx(0.75f));
  }

  TEST_CASE("Infers OBJ transparency from MTL opacity") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
usemtl glass
f 1 2 3
)";
    constexpr std::string_view kMtl = "newmtl glass\nd 0.5\n";

    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl) == ARX_OK);
    ArxModelFace face{};
    REQUIRE(model.copyFaces(0, 1, &face) == ARX_OK);
    CHECK((face.flags & pistoris::kFaceBitTrans) != 0);
    CHECK(face.transval == doctest::Approx(0.5f));
  }

  TEST_CASE("Rejects unsafe OBJ material-library names") {
    constexpr std::string_view kObj = R"(v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";
    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj) == ARX_OK);

    for (std::string_view name : {"", "model name", "model#name", "model\nname", "model\tname"}) {
      pistoris::ObjBundle output;
      output.text = "unchanged";
      output.mtl = "unchanged";
      CHECK(model.exportObj(name, output) == ARX_OBJ_BAD_MATERIAL_LIBRARY_NAME);
      CHECK(output.text == "unchanged");
      CHECK(output.mtl == "unchanged");
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj, kMtl) == ARX_OK);
    WarningCapture warnings;
    pistoris::ObjBundle encoded;
    REQUIRE(model.exportObj("transparent", encoded) == ARX_OK);
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

    pistoris::Model model;
    CHECK(pistoris::Model::importObj(model, kObj) == ARX_OBJ_BAD_MATERIAL_NAME);
  }

  TEST_CASE("Validates reserved OBJ directives") {
    constexpr std::string_view kObj = R"(# arx_unknown ignored
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";
    WarningCapture warnings;
    pistoris::Model model;
    REQUIRE(pistoris::Model::importObj(model, kObj) == ARX_OK);
    REQUIRE(warnings.messages.size() == 1);
    CHECK(warnings.messages[0].find("unknown reserved directive 'arx_unknown' ignored") != std::string::npos);

    constexpr std::string_view kBadMtl = "newmtl wall\nmap_Kd -o\n";
    CHECK(pistoris::Model::importObj(model, kObj, kBadMtl) == ARX_OBJ_BAD_MTL);

    constexpr std::string_view kBadIndex = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n";
    CHECK(pistoris::Model::importObj(model, kBadIndex) == ARX_OBJ_BAD_POSITION_INDEX);
    CHECK(pistoris::Model::importObj(model, "v 0 0 0\n") == ARX_OBJ_NO_GEOMETRY);
  }

  TEST_CASE("Converts Model GLB through the public class") {
    pistoris::Model source;
    REQUIRE(pistoris::Model::importNative(source, makeSemanticModelFtl()) == ARX_OK);
    std::vector<std::uint8_t> encoded;
    REQUIRE(source.exportGlb(encoded) == ARX_OK);
    pistoris::Model imported;
    REQUIRE(pistoris::Model::importGlb(imported, encoded) == ARX_OK);
    CHECK(imported.validate() == ARX_OK);
    CHECK(imported.faceCount() == source.faceCount());
    CHECK(imported.boneCount() == source.boneCount());
    CHECK(imported.actionPointCount() == source.actionPointCount());
    CHECK(imported.selectionCount() == source.selectionCount());
  }

  TEST_CASE("Converts native runtime semantics and bakes a coherent FTL") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.faces[0].type |= pistoris::kFaceBitQuad;
    REQUIRE(pistoris::validate(native) == ARX_OK);

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, native) == ARX_OK);
    CHECK(model.resourcePath().empty());
    REQUIRE(model.setResourcePath("MODEL:NPC:MY__NPC") == ARX_OK);
    CHECK((model.resourcePath() == "game/graph/obj3d/interactive/npc/my__npc/my__npc.ftl"));
    REQUIRE(model.setResourcePath(R"(Graph\MY_FOLDER\My_Model.FTL)") == ARX_OK);
    CHECK((model.resourcePath() == "graph/my_folder/my_model.ftl"));
    CHECK(model.vertexCount() == 3);
    CHECK(model.faceCount() == 1);
    CHECK(model.textureCount() == 1);
    CHECK(model.boneCount() == 2);
    CHECK(model.actionPointCount() == 1);
    CHECK(model.selectionCount() == 6);

    std::array<pistoris::SelectionId, 6> selection_ids{};
    REQUIRE(model.copySelectionIds(0, selection_ids.size(), selection_ids.data()) == ARX_OK);
    CHECK(selection_ids == std::array<pistoris::SelectionId, 6>{0, 1, 2, 3, 4, 5});

    std::array<ArxModelVertex, 3> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(vertices[0].position == ArxVector3{0.0f, 0.0f, 0.0f});
    CHECK(vertices[0].bone == 0);
    CHECK(vertices[1].bone == 1);
    CHECK(selectionVertices(model, 0) == std::vector<pistoris::VertexIndex>{0});
    CHECK(selectionVertices(model, 1) == std::vector<pistoris::VertexIndex>{1});
    CHECK(selectionVertices(model, 2) == std::vector<pistoris::VertexIndex>{0, 1});
    CHECK(selectionVertices(model, 3) == std::vector<pistoris::VertexIndex>{0});
    CHECK(selectionVertices(model, 4) == std::vector<pistoris::VertexIndex>{1});
    CHECK(selectionVertices(model, 5) == std::vector<pistoris::VertexIndex>{2});

    std::array<ArxModelBone, 2> bones{};
    REQUIRE(model.copyBones(0, bones.size(), bones.data()) == ARX_OK);
    CHECK((stringView(bones[0].name) == "root"));
    CHECK(bones[0].parent == pistoris::kInvalidBoneIndex);
    CHECK(selectionBones(model, 0) == std::vector<pistoris::BoneIndex>{0});
    CHECK(selectionBones(model, 3) == std::vector<pistoris::BoneIndex>{0});
    CHECK((stringView(bones[1].name) == "chest"));
    CHECK(bones[1].parent == 0);
    CHECK(selectionBones(model, 1) == std::vector<pistoris::BoneIndex>{1});

    ArxModelActionPoint action{};
    REQUIRE(model.copyActionPoints(0, 1, &action) == ARX_OK);
    CHECK((stringView(action.name) == "view_attach"));
    CHECK(action.position == ArxVector3{1.0f, 0.0f, 0.0f});
    CHECK(action.bone == 1);
    CHECK(selectionActionPoints(model, 0) == std::vector<pistoris::ActionPointIndex>{0});
    CHECK(selectionActionPoints(model, 3) == std::vector<pistoris::ActionPointIndex>{0});

    const ArxModelOrigin origin = model.origin();
    CHECK(origin.bone == 0);
    CHECK(selectionIncludesOrigin(model, 0));
    CHECK(selectionIncludesOrigin(model, 3));

    ArxModelSelection cut_head{};
    REQUIRE(model.selection(3, cut_head) == ARX_OK);
    CHECK((stringView(cut_head.name) == "cut_head"));
    CHECK(cut_head.has_leading_vertex == 1);
    CHECK(cut_head.leading_position == ArxVector3{0.5f, 0.5f, 0.0f});
    CHECK(cut_head.leading_bone == 1);

    ArxTextureView texture{};
    REQUIRE(model.copyTextureViews(0, 1, &texture) == ARX_OK);
    CHECK((stringView(texture.path) == "graph/obj3d/textures/my_tex"));

    ArxModelFace face{};
    REQUIRE(model.copyFaces(0, 1, &face) == ARX_OK);
    CHECK(face.normal == native.faces[0].norm);
    CHECK((face.flags & pistoris::kFaceBitQuad) == 0);

    pistoris::NativeModelBundle bundle;
    REQUIRE(model.bakeNativeBundle({}, bundle) == ARX_OK);
    const pistoris::Ftl& baked = bundle.ftl;
    REQUIRE(pistoris::validate(baked) == ARX_OK);
    CHECK(baked.faces[0].norm == native.faces[0].norm);
    CHECK((baked.faces[0].type & pistoris::kFaceBitQuad) == 0);
    CHECK(baked.vertices[baked.header.origin].position == ArxVector3{});
    CHECK(baked.groups.size() == 2);
    CHECK(baked.actions.size() == 1);
    CHECK(baked.selections.size() == 6);

    pistoris::Model roundtrip;
    REQUIRE(pistoris::Model::importNative(roundtrip, baked) == ARX_OK);
    CHECK(roundtrip.validate() == ARX_OK);
    CHECK(roundtrip.resourcePath().empty());
    CHECK(roundtrip.vertexCount() == model.vertexCount());
    CHECK(roundtrip.faceCount() == model.faceCount());
    CHECK(roundtrip.textureCount() == model.textureCount());
    CHECK(roundtrip.boneCount() == model.boneCount());
    CHECK(roundtrip.actionPointCount() == model.actionPointCount());

    std::array<ArxModelVertex, 3> roundtrip_vertices{};
    REQUIRE(roundtrip.copyVertices(0, roundtrip_vertices.size(), roundtrip_vertices.data()) == ARX_OK);
    for (std::size_t i = 0; i < vertices.size(); ++i) {
      CHECK(roundtrip_vertices[i].position == vertices[i].position);
      CHECK(roundtrip_vertices[i].bone == vertices[i].bone);
    }

    ArxModelFace roundtrip_face{};
    REQUIRE(roundtrip.copyFaces(0, 1, &roundtrip_face) == ARX_OK);
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

    ArxTextureView roundtrip_texture{};
    REQUIRE(roundtrip.copyTextureViews(0, 1, &roundtrip_texture) == ARX_OK);
    CHECK((stringView(roundtrip_texture.path) == "graph/obj3d/textures/my_tex"));
    CHECK(roundtrip_texture.encoded_image.size == texture.encoded_image.size);

    std::array<ArxModelBone, 2> roundtrip_bones{};
    REQUIRE(roundtrip.copyBones(0, roundtrip_bones.size(), roundtrip_bones.data()) == ARX_OK);
    for (std::size_t i = 0; i < bones.size(); ++i) {
      CHECK((stringView(roundtrip_bones[i].name) == stringView(bones[i].name)));
      CHECK(roundtrip_bones[i].position == bones[i].position);
      CHECK(roundtrip_bones[i].parent == bones[i].parent);
      CHECK(roundtrip_bones[i].blob_shadow_size == bones[i].blob_shadow_size);
    }

    ArxModelActionPoint roundtrip_action{};
    REQUIRE(roundtrip.copyActionPoints(0, 1, &roundtrip_action) == ARX_OK);
    CHECK((stringView(roundtrip_action.name) == stringView(action.name)));
    CHECK(roundtrip_action.position == action.position);
    CHECK(roundtrip_action.bone == action.bone);

    const ArxModelOrigin roundtrip_origin = roundtrip.origin();
    CHECK(roundtrip_origin.bone == origin.bone);
    for (pistoris::SelectionId id : selection_ids) {
      CHECK(selectionVertices(roundtrip, id) == selectionVertices(model, id));
      CHECK(selectionBones(roundtrip, id) == selectionBones(model, id));
      CHECK(selectionActionPoints(roundtrip, id) == selectionActionPoints(model, id));
      CHECK(selectionIncludesOrigin(roundtrip, id) == selectionIncludesOrigin(model, id));
    }

    ArxModelSelection roundtrip_cut_head{};
    REQUIRE(roundtrip.selection(3, roundtrip_cut_head) == ARX_OK);
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, native) == ARX_OK);
    CHECK(model.vertexCount() == 4);

    std::array<ArxModelFace, 2> faces{};
    REQUIRE(model.copyFaces(0, faces.size(), faces.data()) == ARX_OK);
    CHECK(faces[0].corners[0].vertex == 0);
    CHECK(faces[1].corners[0].vertex == 3);
    CHECK(faces[0].corners[0].normal == native.vertices[0].normal);
    CHECK(faces[1].corners[0].normal == native.vertices[3].normal);

    pistoris::NativeModelBundle bundle;
    REQUIRE(model.bakeNativeBundle({}, bundle) == ARX_OK);
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
      pistoris::VertexIndex index = pistoris::kInvalidVertexIndex;
      REQUIRE(model.addVertex(vertex, index) == ARX_OK);
    }

    ArxModelSelection selection{};
    selection.name = {"split", 5};
    pistoris::SelectionId selection_id = pistoris::kInvalidSelectionId;
    REQUIRE(model.addSelection(selection, selection_id) == ARX_OK);
    const pistoris::VertexIndex selected_vertex = 0;
    REQUIRE(model.updateSelectionMembers(selection_id, {.vertices = &selected_vertex, .vertex_count = 1}) == ARX_OK);

    ArxModelFace first{};
    first.normal = {0.0f, 0.0f, 1.0f};
    first.texture = pistoris::kNoTexture;
    first.corners[0].vertex = 0;
    first.corners[0].normal = {0.0f, 0.0f, 1.0f};
    first.corners[1].vertex = 1;
    first.corners[1].normal = {0.0f, 0.0f, 1.0f};
    first.corners[2].vertex = 2;
    first.corners[2].normal = {0.0f, 0.0f, 1.0f};
    pistoris::FaceIndex face_index = pistoris::kInvalidFaceIndex;
    REQUIRE(model.addFace(first, face_index) == ARX_OK);

    ArxModelFace second{};
    second.normal = {0.0f, 1.0f, 0.0f};
    second.texture = pistoris::kNoTexture;
    second.corners[0].vertex = 0;
    second.corners[0].normal = {1.0f, 0.0f, 0.0f};
    second.corners[1].vertex = 3;
    second.corners[1].normal = {0.0f, 0.0f, 1.0f};
    second.corners[2].vertex = 1;
    second.corners[2].normal = {0.0f, 0.0f, 1.0f};
    REQUIRE(model.addFace(second, face_index) == ARX_OK);
    REQUIRE(model.validate() == ARX_OK);

    pistoris::NativeModelBundle bundle;
    REQUIRE(model.bakeNativeBundle({}, bundle) == ARX_OK);
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

    pistoris::Model roundtrip;
    REQUIRE(pistoris::Model::importNative(roundtrip, bundle.ftl) == ARX_OK);
    std::array<ArxModelFace, 2> roundtrip_faces{};
    REQUIRE(roundtrip.copyFaces(0, roundtrip_faces.size(), roundtrip_faces.data()) == ARX_OK);
    CHECK(roundtrip_faces[0].corners[0].vertex != roundtrip_faces[1].corners[0].vertex);
    CHECK(roundtrip_faces[0].corners[0].normal == first.corners[0].normal);
    CHECK(roundtrip_faces[1].corners[0].normal == second.corners[0].normal);

    std::array<pistoris::SelectionId, 1> roundtrip_selection_ids{};
    REQUIRE(roundtrip.copySelectionIds(0, 1, roundtrip_selection_ids.data()) == ARX_OK);
    const std::vector<pistoris::VertexIndex> roundtrip_selected =
        selectionVertices(roundtrip, roundtrip_selection_ids[0]);
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
    REQUIRE(pistoris::validate(native) == ARX_OK);

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, native) == ARX_OK);
    CHECK(model.vertexCount() == 3);
    CHECK(model.faceCount() == 1);
    CHECK(model.validate() == ARX_OK);
  }

  TEST_CASE("Repairs native vertex normals when constructing Model corners") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.vertices[1].normal = {0.0f, 0.0f, 0.5f};
    native.vertices[2].normal = {};
    REQUIRE(pistoris::validate(native) == ARX_OK);
    WarningCapture warnings;

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, native) == ARX_OK);

    ArxModelFace face{};
    REQUIRE(model.copyFaces(0, 1, &face) == ARX_OK);
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
    REQUIRE(pistoris::validate(native) == ARX_OK);
    WarningCapture warnings;

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, native) == ARX_OK);

    ArxModelFace face{};
    REQUIRE(model.copyFaces(0, 1, &face) == ARX_OK);
    CHECK(face.corners[0].normal == ArxVector3{0.0f, 0.0f, 1.0f});
    CHECK(face.corners[1].normal == ArxVector3{0.0f, 0.0f, 1.0f});
    REQUIRE(warnings.messages.size() == 1);
    CHECK((warnings.messages[0] == "FTL -> Model repairs: 2 corner normal(s) regenerated"));
  }

  TEST_CASE("Clamps negative native bone blob-shadow sizes") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.groups[1].blob_shadow_size = -2.0f;
    REQUIRE(pistoris::validate(native) == ARX_OK);
    WarningCapture warnings;

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, native) == ARX_OK);

    ArxModelBone bone{};
    REQUIRE(model.copyBones(1, 1, &bone) == ARX_OK);
    CHECK(bone.blob_shadow_size == 0.0f);
    REQUIRE(warnings.messages.size() == 1);
    CHECK((warnings.messages[0] == "FTL -> Model repairs: 1 negative bone blob-shadow size(s) clamped to zero"));
  }

  TEST_CASE("Rejects nonfinite native bone blob-shadow sizes") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.groups[0].blob_shadow_size = std::numeric_limits<float>::infinity();
    REQUIRE(pistoris::validate(native) == ARX_OK);

    pistoris::Model model;
    CHECK(pistoris::Model::importNative(model, native) == ARX_MODEL_BAD_BONE_BLOB_SHADOW_SIZE);
  }

  TEST_CASE("Sets and clears validated texture image data") {
    pistoris::Model model;
    ArxTextureView texture{};
    texture.path = {"wall.bmp", 8};
    pistoris::TextureIndex texture_index = pistoris::kNoTexture;
    REQUIRE(model.addTexture(texture, texture_index) == ARX_OK);

    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(model.setTextureImage(texture_index, {image.data(), image.size()}) == ARX_OK);
    CHECK(model.setTextureImage(texture_index, {}) == ARX_MODEL_BAD_TEXTURE_IMAGE);

    ArxTextureView copied{};
    REQUIRE(model.copyTextureViews(0, 1, &copied) == ARX_OK);
    REQUIRE(copied.encoded_image.size == image.size());
    CHECK(std::equal(image.begin(), image.end(), copied.encoded_image.data));

    REQUIRE(model.clearTextureImage(texture_index) == ARX_OK);
    REQUIRE(model.copyTextureViews(0, 1, &copied) == ARX_OK);
    CHECK(copied.encoded_image.size == 0);
    CHECK(std::string(copied.external_image_extension.data, copied.external_image_extension.size) == ".bmp");
  }

  TEST_CASE("Stores and renders an optional inventory icon") {
    pistoris::Model model;

    CHECK(model.setInventoryIcon({}) == ARX_MODEL_BAD_INVENTORY_ICON);

    pistoris::Model::InventoryIconRenderOptions options;
    std::vector<std::uint8_t> rendered{1};
    REQUIRE(model.renderIconPng(options, rendered) == ARX_OK);
    CHECK(rendered.empty());

    const std::vector<std::uint8_t> icon = makeTestBmp(255, 0, 0);
    REQUIRE(model.setInventoryIcon({icon.data(), icon.size()}) == ARX_OK);

    const pistoris::Model::InventoryIconView borrowed = model.inventoryIcon();
    REQUIRE(borrowed.encoded_image.size == icon.size());
    CHECK(std::equal(icon.begin(), icon.end(), borrowed.encoded_image.data));
    CHECK(borrowed.width_slots == 1);
    CHECK(borrowed.height_slots == 1);

    pistoris::Model copied(model);
    CHECK(model.setInventoryIcon({icon.data(), 1}) == ARX_MODEL_BAD_INVENTORY_ICON);
    CHECK(model.inventoryIcon().encoded_image.size == icon.size());
    pistoris::Model::InventoryIconSetOptions set_options;
    set_options.width_slots = 0;
    CHECK(model.setInventoryIcon({icon.data(), icon.size()}, set_options) == ARX_INVALID_OPTIONS);
    CHECK(model.inventoryIcon().encoded_image.size == icon.size());
    model.clearInventoryIcon();
    CHECK(model.inventoryIcon().encoded_image.data == nullptr);
    CHECK(model.inventoryIcon().encoded_image.size == 0);
    CHECK(model.inventoryIcon().width_slots == 0);
    CHECK(model.inventoryIcon().height_slots == 0);
    CHECK(copied.inventoryIcon().encoded_image.size == icon.size());

    set_options.width_slots = 3;
    set_options.height_slots = 2;
    REQUIRE(copied.setInventoryIcon({icon.data(), icon.size()}, set_options) == ARX_OK);
    REQUIRE(copied.renderIconPng(options, rendered) == ARX_OK);
    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width == 96);
    CHECK(info.height == 64);

    REQUIRE(copied.renderIconBmp(options, rendered) == ARX_OK);
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_BMP);
    CHECK(info.width == 96);
    CHECK(info.height == 64);
    CHECK(info.components == 4);

    options.width_slots = -2;
    const std::vector<std::uint8_t> before = rendered;
    CHECK(copied.renderIconPng(options, rendered) == ARX_INVALID_OPTIONS);
    CHECK(rendered == before);
    CHECK(copied.renderIconBmp(options, rendered) == ARX_INVALID_OPTIONS);
    CHECK(rendered == before);
  }

  TEST_CASE("Derives inventory icon render footprints") {
    pistoris::Model model;
    const std::vector<std::uint8_t> icon = makeSolidTestBmp(128, 65);
    REQUIRE(model.setInventoryIcon({icon.data(), icon.size()}) == ARX_OK);
    CHECK(model.inventoryIcon().width_slots == 3);
    CHECK(model.inventoryIcon().height_slots == 2);

    pistoris::Model::InventoryIconSetOptions set_options;
    set_options.width_slots = 2;
    set_options.height_slots = -1;
    REQUIRE(model.setInventoryIcon({icon.data(), icon.size()}, set_options) == ARX_OK);
    CHECK(model.inventoryIcon().width_slots == 2);
    CHECK(model.inventoryIcon().height_slots == 2);

    pistoris::Model::InventoryIconRenderOptions options;
    options.width_slots = 0;
    options.height_slots = -1;
    options.layout = pistoris::Model::InventoryIconLayout::kStretch;
    std::vector<std::uint8_t> rendered;
    REQUIRE(model.renderIconPng(options, rendered) == ARX_OK);
    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.width == 64);
    CHECK(info.height == 64);

    const std::vector<std::uint8_t> large = makeSolidTestBmp(512, 256);
    REQUIRE(model.setInventoryIcon({large.data(), large.size()}) == ARX_OK);
    options.width_slots = -1;
    options.height_slots = -1;
    REQUIRE(model.renderIconPng(options, rendered) == ARX_OK);
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.width == 96);
    CHECK(info.height == 64);

    options.layout = static_cast<pistoris::Model::InventoryIconLayout>(255);
    const std::vector<std::uint8_t> before = rendered;
    CHECK(model.renderIconPng(options, rendered) == ARX_INVALID_OPTIONS);
    CHECK(rendered == before);
  }

  TEST_CASE("Canonicalizes texture path metadata on mutation") {
    pistoris::Model model;
    const ArxTextureView texture{{"GRAPH/TEXTURES/WALL", 19}, {}, {".BMP", 4}};
    pistoris::TextureIndex texture_index = pistoris::kNoTexture;
    REQUIRE(model.addTexture(texture, texture_index) == ARX_OK);

    ArxTextureView copied{};
    REQUIRE(model.copyTextureViews(texture_index, 1, &copied) == ARX_OK);
    CHECK((stringView(copied.path) == "graph/textures/wall"));
    CHECK((stringView(copied.external_image_extension) == ".bmp"));
  }

  TEST_CASE("Rejects texture identities without a valid resource path") {
    pistoris::Model model;
    pistoris::TextureIndex texture_index = 42;
    const ArxTextureView invalid{{"", 0}, {}, {}};
    CHECK(model.addTexture(invalid, texture_index) == ARX_MODEL_BAD_TEXTURE_PATH);
    CHECK(texture_index == pistoris::kNoTexture);
    CHECK(model.textureCount() == 0);
  }

  TEST_CASE("Native bake emits Model texture sidecars") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(model.setTextureImage(0, {image.data(), image.size()}) == ARX_OK);
    REQUIRE(model.rebaseTexturePaths("graph/obj3d/textures") == ARX_OK);

    pistoris::NativeModelBundle bundle;
    REQUIRE(model.bakeNativeBundle({}, bundle) == ARX_OK);
    REQUIRE(bundle.texture_files.size() == 1);
    CHECK(bundle.texture_files[0].source_texture == 0);
    CHECK(bundle.texture_files[0].resource_path == "graph/obj3d/textures/my_tex.bmp");
    CHECK(bundle.texture_files[0].encoded_image == image);
    CHECK(std::string(bundle.ftl.texture_containers[0].filename) == "graph/obj3d/textures/my_tex");

    REQUIRE(model.bakeNativeBundle({.include_texture_files = false}, bundle) == ARX_OK);
    CHECK(bundle.texture_files.empty());
    CHECK(std::string(bundle.ftl.texture_containers[0].filename) == "graph/obj3d/textures/my_tex");
  }

  TEST_CASE("Native bake applies the FTL texture path limit with its extension") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
    ArxTextureView texture{};
    const std::string resource_name = std::string(18, 'd') + "/" + std::string(236, 'a');
    const std::string path = resource_name + ".bmp";
    texture.path = {path.data(), path.size()};
    REQUIRE(model.setTexture(0, texture) == ARX_OK);

    pistoris::NativeModelBundle bundle;
    CHECK(model.bakeNativeBundle({}, bundle) == ARX_MODEL_BAD_TEXTURE_PATH);
  }

  TEST_CASE("Repairs native names beyond lowercasing") {
    pistoris::Ftl native = makeSemanticModelFtl();
    setFtlName("bad_name", native.selections[0].name, sizeof(native.selections[0].name));
    setFtlName("bad__name", native.selections[4].name, sizeof(native.selections[4].name));
    setFtlName("__", native.selections[5].name, sizeof(native.selections[5].name));
    setFtlName("root", native.groups[1].name, sizeof(native.groups[1].name));
    setFtlName("", native.actions[0].name, sizeof(native.actions[0].name));
    LogCapture logs;

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, native) == ARX_OK);

    ArxModelSelection selection{};
    REQUIRE(model.selection(4, selection) == ARX_OK);
    CHECK((stringView(selection.name) == "bad_name_1"));
    REQUIRE(model.selection(5, selection) == ARX_OK);
    CHECK((stringView(selection.name) == "selection"));
    ArxModelBone bone{};
    REQUIRE(model.copyBones(1, 1, &bone) == ARX_OK);
    CHECK((stringView(bone.name) == "root_1"));
    ArxModelActionPoint action{};
    REQUIRE(model.copyActionPoints(0, 1, &action) == ARX_OK);
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

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, native) == ARX_OK);

    ArxModelSelection selection{};
    REQUIRE(model.selection(0, selection) == ARX_OK);
    CHECK((stringView(selection.name) == "a"));
    REQUIRE(model.selection(1, selection) == ARX_OK);
    CHECK((stringView(selection.name) == "a_2"));
    REQUIRE(model.selection(2, selection) == ARX_OK);
    CHECK((stringView(selection.name) == "a_1"));
    REQUIRE(model.selection(3, selection) == ARX_OK);
    CHECK((stringView(selection.name) == boundary_name));
    REQUIRE(model.selection(4, selection) == ARX_OK);
    CHECK((stringView(selection.name) == std::string(60, 'x') + "_1"));
  }

  TEST_CASE("Native conversion preserves duplicate action point names") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.actions.push_back(native.actions.front());
    setFtlName("hit_30", native.actions[0].name, sizeof(native.actions[0].name));
    setFtlName("hit_30", native.actions[1].name, sizeof(native.actions[1].name));

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, native) == ARX_OK);
    REQUIRE(model.actionPointCount() == 2);
    std::array<ArxModelActionPoint, 2> actions{};
    REQUIRE(model.copyActionPoints(0, actions.size(), actions.data()) == ARX_OK);
    CHECK((stringView(actions[0].name) == "hit_30"));
    CHECK((stringView(actions[1].name) == "hit_30"));

    pistoris::NativeModelBundle bundle;
    REQUIRE(model.bakeNativeBundle({}, bundle) == ARX_OK);
    REQUIRE(bundle.ftl.actions.size() == 2);
    CHECK((std::string_view(bundle.ftl.actions[0].name) == "hit_30"));
    CHECK((std::string_view(bundle.ftl.actions[1].name) == "hit_30"));
  }

  TEST_CASE("Edits generic selections and cut leading vertices") {
    pistoris::Model model;
    REQUIRE(model.setResourcePath("model:armor:chain_shirt") == ARX_OK);

    ArxModelBone root{};
    root.name = {"ROOT", 4};
    root.position = {};
    pistoris::BoneIndex root_index = pistoris::kInvalidBoneIndex;
    REQUIRE(model.addBone(root, root_index) == ARX_OK);
    CHECK(root_index == 0);

    ArxModelSelection cut_head{};
    cut_head.name = {"CUT_HEAD", 8};
    cut_head.has_leading_vertex = 2;
    cut_head.leading_position = {0.25f, 0.25f, 0.0f};
    cut_head.leading_bone = root_index;
    pistoris::SelectionId cut_head_id = pistoris::kInvalidSelectionId;
    REQUIRE(model.addSelection(cut_head, cut_head_id) == ARX_OK);
    CHECK(cut_head_id == 0);
    ArxModelSelection copied_cut_head{};
    REQUIRE(model.selection(cut_head_id, copied_cut_head) == ARX_OK);
    CHECK(copied_cut_head.has_leading_vertex == 1);

    ArxModelSelection reserved_name{};
    reserved_name.name = {"bad__name", 9};
    pistoris::SelectionId rejected_id = 42;
    CHECK(model.addSelection(reserved_name, rejected_id) == ARX_MODEL_BAD_SELECTION_NAME);
    CHECK(rejected_id == pistoris::kInvalidSelectionId);
    for (std::string_view name : {"_leading", "trailing_"}) {
      reserved_name.name = {name.data(), name.size()};
      CHECK(model.addSelection(reserved_name, rejected_id) == ARX_MODEL_BAD_SELECTION_NAME);
      CHECK(rejected_id == pistoris::kInvalidSelectionId);
    }
    reserved_name.name = {"valid-name", 10};
    REQUIRE(model.addSelection(reserved_name, rejected_id) == ARX_OK);

    std::array<ArxModelVertex, 3> vertices{};
    vertices[0].position = {0.0f, 0.0f, 0.0f};
    vertices[1].position = {1.0f, 0.0f, 0.0f};
    vertices[2].position = {0.0f, 1.0f, 0.0f};
    for (ArxModelVertex& vertex : vertices) vertex.bone = root_index;

    for (const ArxModelVertex& vertex : vertices) {
      pistoris::VertexIndex index = pistoris::kInvalidVertexIndex;
      REQUIRE(model.addVertex(vertex, index) == ARX_OK);
    }
    const pistoris::VertexIndex cut_vertex = 0;
    REQUIRE(model.updateSelectionMembers(cut_head_id, {.vertices = &cut_vertex, .vertex_count = 1}) == ARX_OK);
    REQUIRE(model.updateSelectionMembers(cut_head_id, {}) == ARX_OK);
    CHECK(selectionVertices(model, cut_head_id) == std::vector<pistoris::VertexIndex>{0});
    REQUIRE(model.updateSelectionMembers(cut_head_id, {.vertices = &cut_vertex, .vertex_count = 0}) == ARX_OK);
    CHECK(selectionVertices(model, cut_head_id).empty());
    REQUIRE(model.updateSelectionMembers(cut_head_id, {.vertices = &cut_vertex, .vertex_count = 1}) == ARX_OK);

    ArxModelFace face{};
    face.normal = {0.0f, 0.0f, 1.0f};
    face.texture = pistoris::kNoTexture;
    face.flags = pistoris::kFaceBitQuad;
    for (std::size_t corner = 0; corner < 3; ++corner) {
      face.corners[corner].vertex = static_cast<pistoris::VertexIndex>(corner);
      face.corners[corner].normal = {0.0f, 0.0f, 1.0f};
    }
    pistoris::FaceIndex face_index = pistoris::kInvalidFaceIndex;
    REQUIRE(model.addFace(face, face_index) == ARX_OK);
    ArxModelFace normalized_face{};
    REQUIRE(model.copyFaces(face_index, 1, &normalized_face) == ARX_OK);
    CHECK((normalized_face.flags & pistoris::kFaceBitQuad) == 0);

    ArxModelFace invalid_face_normal = face;
    invalid_face_normal.normal.x = std::numeric_limits<float>::infinity();
    CHECK(model.setFace(face_index, invalid_face_normal) == ARX_MODEL_BAD_FACE_NORMAL);
    ArxModelFace invalid_corner_normal = face;
    invalid_corner_normal.corners[0].normal = {};
    CHECK(model.setFace(face_index, invalid_corner_normal) == ARX_MODEL_BAD_CORNER_NORMAL);
    CHECK(model.validate() == ARX_OK);

    ArxModelBone copied{};
    REQUIRE(model.copyBones(0, 1, &copied) == ARX_OK);
    CHECK((stringView(copied.name) == "root"));
    CHECK(model.removeBone(root_index) == ARX_MODEL_BONE_IN_USE);

    REQUIRE(model.removeSelection(cut_head_id) == ARX_OK);
    CHECK(model.validateSelections() == ARX_OK);
  }

  TEST_CASE("Selection membership updates remain category-specific") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);

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
                                          .action_point_count = 1}) == ARX_OK);
    REQUIRE(model.setSelectionIncludesOrigin(selection, false) == ARX_OK);
    CHECK(selectionVertices(model, selection) == std::vector<pistoris::VertexIndex>{2});
    CHECK(selectionBones(model, selection) == std::vector<pistoris::BoneIndex>{1});
    CHECK(selectionActionPoints(model, selection) == std::vector<pistoris::ActionPointIndex>{0});
    CHECK_FALSE(selectionIncludesOrigin(model, selection));

    REQUIRE(model.clearSelectionVertices(selection) == ARX_OK);
    REQUIRE(model.clearSelectionBones(selection) == ARX_OK);
    REQUIRE(model.clearSelectionActionPoints(selection) == ARX_OK);
    REQUIRE(model.setSelectionIncludesOrigin(selection, true) == ARX_OK);
    CHECK(selectionVertices(model, selection).empty());
    CHECK(selectionBones(model, selection).empty());
    CHECK(selectionActionPoints(model, selection).empty());
    CHECK(selectionIncludesOrigin(model, selection));
  }

  TEST_CASE("Replacing rig categories preserves unrelated selection membership") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);

    const pistoris::SelectionId selection = 0;
    const std::vector<pistoris::VertexIndex> vertices = selectionVertices(model, selection);
    REQUIRE_FALSE(vertices.empty());
    REQUIRE_FALSE(selectionBones(model, selection).empty());
    REQUIRE_FALSE(selectionActionPoints(model, selection).empty());
    REQUIRE(selectionIncludesOrigin(model, selection));

    std::vector<ArxModelBone> bones(model.boneCount());
    REQUIRE(model.copyBones(0, bones.size(), bones.data()) == ARX_OK);
    REQUIRE(model.replaceSkeleton({bones.data(), bones.size(), model.origin()}) == ARX_OK);
    CHECK(selectionVertices(model, selection) == vertices);
    CHECK(selectionBones(model, selection).empty());
    CHECK_FALSE(selectionActionPoints(model, selection).empty());
    CHECK_FALSE(selectionIncludesOrigin(model, selection));

    std::vector<ArxModelActionPoint> action_points(model.actionPointCount());
    REQUIRE(model.copyActionPoints(0, action_points.size(), action_points.data()) == ARX_OK);
    REQUIRE(model.replaceActionPoints({action_points.data(), action_points.size()}) == ARX_OK);
    CHECK(selectionVertices(model, selection) == vertices);
    CHECK(selectionBones(model, selection).empty());
    CHECK(selectionActionPoints(model, selection).empty());
    CHECK_FALSE(selectionIncludesOrigin(model, selection));
  }

  TEST_CASE("Vertex compaction preserves aligned semantic data") {
    pistoris::Model model;
    REQUIRE(model.setResourcePath("model:npc:compact_test") == ARX_OK);
    std::array<ArxModelVertex, 4> vertices{};
    vertices[0].position = {0.0f, 0.0f, 0.0f};
    vertices[1].position = {9.0f, 9.0f, 9.0f};
    vertices[2].position = {1.0f, 0.0f, 0.0f};
    vertices[3].position = {0.0f, 1.0f, 0.0f};
    ArxModelSelection head{};
    head.name = {"head", 4};
    pistoris::SelectionId head_id = pistoris::kInvalidSelectionId;
    REQUIRE(model.addSelection(head, head_id) == ARX_OK);
    ArxModelSelection torso{};
    torso.name = {"torso", 5};
    pistoris::SelectionId torso_id = pistoris::kInvalidSelectionId;
    REQUIRE(model.addSelection(torso, torso_id) == ARX_OK);
    for (const ArxModelVertex& vertex : vertices) {
      pistoris::VertexIndex index = pistoris::kInvalidVertexIndex;
      REQUIRE(model.addVertex(vertex, index) == ARX_OK);
    }
    const std::array<pistoris::VertexIndex, 2> head_vertices = {0, 3};
    const std::array<pistoris::VertexIndex, 3> torso_vertices = {1, 2, 3};
    REQUIRE(model.updateSelectionMembers(
                head_id, {.vertices = head_vertices.data(), .vertex_count = head_vertices.size()}) == ARX_OK);
    REQUIRE(model.updateSelectionMembers(
                torso_id, {.vertices = torso_vertices.data(), .vertex_count = torso_vertices.size()}) == ARX_OK);

    ArxModelFace face{};
    face.normal = {0.0f, 0.0f, 1.0f};
    face.texture = pistoris::kNoTexture;
    face.corners[0].vertex = 2;
    face.corners[1].vertex = 0;
    face.corners[2].vertex = 3;
    for (ArxModelCorner& corner : face.corners) corner.normal = {0.0f, 0.0f, 1.0f};
    pistoris::FaceIndex face_index = pistoris::kInvalidFaceIndex;
    REQUIRE(model.addFace(face, face_index) == ARX_OK);

    std::size_t removed = 0;
    REQUIRE(model.compactVertices(&removed) == ARX_OK);
    CHECK(removed == 1);
    CHECK(model.vertexCount() == 3);
    CHECK(model.validate() == ARX_OK);

    std::array<ArxModelVertex, 3> compact{};
    REQUIRE(model.copyVertices(0, compact.size(), compact.data()) == ARX_OK);
    CHECK(compact[0].position == ArxVector3{0.0f, 0.0f, 0.0f});
    CHECK(compact[1].position == ArxVector3{1.0f, 0.0f, 0.0f});
    CHECK(compact[2].position == ArxVector3{0.0f, 1.0f, 0.0f});
    CHECK(selectionVertices(model, head_id) == std::vector<pistoris::VertexIndex>{0, 2});
    CHECK(selectionVertices(model, torso_id) == std::vector<pistoris::VertexIndex>{1, 2});
  }

  TEST_CASE("Intermediate counts may exceed native FTL limits") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);

    std::vector<ArxModelActionPoint> action_points(1025);
    for (ArxModelActionPoint& point : action_points) {
      point.name = {"hit_30", 6};
      point.bone = 0;
    }
    REQUIRE(model.replaceActionPoints({action_points.data(), action_points.size()}) == ARX_OK);
    CHECK(model.validate() == ARX_OK);

    std::vector<std::uint8_t> glb;
    REQUIRE(model.exportGlb(glb) == ARX_OK);
    pistoris::Model glb_roundtrip;
    REQUIRE(pistoris::Model::importGlb(glb_roundtrip, glb) == ARX_OK);
    CHECK(glb_roundtrip.actionPointCount() == action_points.size());

    pistoris::ObjBundle obj;
    REQUIRE(model.exportObj("intermediate_counts", obj) == ARX_OK);
    pistoris::Model obj_roundtrip;
    REQUIRE(pistoris::Model::importObj(obj_roundtrip, obj.text, obj.mtl) == ARX_OK);
    CHECK(obj_roundtrip.actionPointCount() == action_points.size());

    pistoris::NativeModelBundle native;
    CHECK(model.bakeNativeBundle({}, native) == ARX_MODEL_TOO_MANY_ACTION_POINTS);
  }

  TEST_CASE("Native import rejects Skeletons beyond the intermediate limit") {
    pistoris::Ftl native = makeSemanticModelFtl();
    native.groups.resize(1025, native.groups.front());
    pistoris::Model model;
    CHECK(pistoris::Model::importNative(model, native) == ARX_MODEL_TOO_MANY_BONES);
  }

  TEST_CASE("Edits report focused semantic errors") {
    pistoris::Model model;
    CHECK(model.setResourcePath("not/a/model") == ARX_MODEL_BAD_RESOURCE_PATH);

    ArxModelMeshInput oversized{};
    oversized.vertex_count = std::numeric_limits<std::size_t>::max();
    CHECK(model.replaceMesh(oversized) == ARX_MODEL_TOO_MANY_VERTICES);

    ArxModelSkeletonInput oversized_skeleton{};
    oversized_skeleton.bone_count = 1025;
    CHECK(model.replaceSkeleton(oversized_skeleton) == ARX_MODEL_TOO_MANY_BONES);

    ArxModelActionPointsInput oversized_action_points{};
    oversized_action_points.action_point_count = std::numeric_limits<std::size_t>::max();
    CHECK(model.replaceActionPoints(oversized_action_points) == ARX_MODEL_TOO_MANY_ACTION_POINTS);

    ArxModelVertex vertex{};
    pistoris::VertexIndex vertex_index = 42;
    vertex.bone = 0;
    CHECK(model.addVertex(vertex, vertex_index) == ARX_MODEL_BAD_VERTEX_BONE);
    CHECK(vertex_index == pistoris::kInvalidVertexIndex);

    vertex.bone = pistoris::kInvalidBoneIndex;
    REQUIRE(model.addVertex(vertex, vertex_index) == ARX_OK);
    ArxModelSelection selection{};
    selection.name = {"test", 4};
    pistoris::SelectionId selection_id = pistoris::kInvalidSelectionId;
    REQUIRE(model.addSelection(selection, selection_id) == ARX_OK);
    const pistoris::VertexIndex invalid_vertex = 1;
    CHECK(model.updateSelectionMembers(selection_id, {.vertices = &invalid_vertex, .vertex_count = 1}) ==
          ARX_MODEL_BAD_SELECTION_VERTEX);

    ArxModelOrigin origin{};
    origin.bone = 0;
    CHECK(model.setOrigin(origin) == ARX_MODEL_BAD_ORIGIN_BONE);

    ArxModelBone bone{};
    bone.name = {nullptr, 1};
    pistoris::BoneIndex bone_index = 42;
    CHECK(model.addBone(bone, bone_index) == ARX_INVALID_DATA_POINTER);
    CHECK(bone_index == pistoris::kInvalidBoneIndex);
    bone.name = {nullptr, 0};
    CHECK(model.addBone(bone, bone_index) == ARX_MODEL_BAD_BONE_NAME);
    bone.name = {"root", 4};
    bone_index = 42;
    REQUIRE(model.addBone(bone, bone_index) == ARX_OK);

    ArxModelActionPoint point{};
    point.name = {nullptr, 1};
    pistoris::ActionPointIndex point_index = 42;
    CHECK(model.addActionPoint(point, point_index) == ARX_INVALID_DATA_POINTER);
    CHECK(point_index == pistoris::kInvalidActionPointIndex);
    point.name = {nullptr, 0};
    CHECK(model.addActionPoint(point, point_index) == ARX_MODEL_BAD_ACTION_POINT_NAME);
    point.name = {"view_attach", 11};
    point.bone = 1;
    point_index = 42;
    CHECK(model.addActionPoint(point, point_index) == ARX_MODEL_BAD_ACTION_POINT_BONE);
    CHECK(point_index == pistoris::kInvalidActionPointIndex);
  }

  TEST_CASE("Batch vertex insertion validates before changing aligned state") {
    pistoris::Model model;
    const std::array<ArxModelVertex, 3> vertices = {
        ArxModelVertex{{0.0f, 0.0f, 0.0f}},
        ArxModelVertex{{1.0f, 0.0f, 0.0f}},
        ArxModelVertex{{0.0f, 1.0f, 0.0f}},
    };
    pistoris::VertexIndex first = 42;
    REQUIRE(model.addVertices(vertices.data(), vertices.size(), first) == ARX_OK);
    CHECK(first == 0);
    CHECK(model.vertexCount() == vertices.size());

    std::array<ArxModelVertex, 2> invalid = {vertices[0], vertices[1]};
    invalid[1].bone = 0;
    first = 42;
    CHECK(model.addVertices(invalid.data(), invalid.size(), first) == ARX_MODEL_BAD_VERTEX_BONE);
    CHECK(first == pistoris::kInvalidVertexIndex);
    CHECK(model.vertexCount() == vertices.size());
    CHECK(model.addVertices(nullptr, 0, first) == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Face edits replace corner normals directly") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
    ArxModelFace face{};
    REQUIRE(model.copyFaces(0, 1, &face) == ARX_OK);
    face.corners[0].normal = {1.0f, 0.0f, 0.0f};
    REQUIRE(model.setFace(0, face) == ARX_OK);
    face = {};
    REQUIRE(model.copyFaces(0, 1, &face) == ARX_OK);
    CHECK(face.corners[0].normal == ArxVector3{1.0f, 0.0f, 0.0f});
  }

  TEST_CASE("Transforms all positional Model state atomically") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);

    ArxModelVertex unchanged{};
    REQUIRE(model.copyVertices(1, 1, &unchanged) == ARX_OK);
    CHECK(model.scale(std::numeric_limits<float>::max()) == ARX_MODEL_BAD_BONE_BLOB_SHADOW_SIZE);
    ArxModelVertex after_failed_scale{};
    REQUIRE(model.copyVertices(1, 1, &after_failed_scale) == ARX_OK);
    CHECK(after_failed_scale.position == unchanged.position);

    REQUIRE(model.scale(2.0f) == ARX_OK);
    REQUIRE(model.rotate({2.0f, 0.0f, 0.0f, 2.0f}) == ARX_OK);
    REQUIRE(model.translate({10.0f, 20.0f, 30.0f}) == ARX_OK);

    std::array<ArxModelVertex, 3> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(vertices[0].position == ArxVector3{10.0f, 20.0f, 30.0f});
    CHECK(vertices[1].position.x == doctest::Approx(10.0f));
    CHECK(vertices[1].position.y == doctest::Approx(22.0f));
    CHECK(vertices[2].position.x == doctest::Approx(8.0f));
    CHECK(vertices[2].position.y == doctest::Approx(20.0f));

    ArxModelFace face{};
    REQUIRE(model.copyFaces(0, 1, &face) == ARX_OK);
    CHECK(face.normal.x == doctest::Approx(0.0f));
    CHECK(face.normal.y == doctest::Approx(1.0f));
    CHECK(face.corners[0].normal == ArxVector3{0.0f, 0.0f, 1.0f});

    std::array<ArxModelBone, 2> bones{};
    REQUIRE(model.copyBones(0, bones.size(), bones.data()) == ARX_OK);
    CHECK(bones[0].blob_shadow_size == doctest::Approx(4.0f));
    CHECK(bones[1].position.x == doctest::Approx(8.0f));
    CHECK(bones[1].position.y == doctest::Approx(20.0f));

    ArxModelActionPoint action{};
    REQUIRE(model.copyActionPoints(0, 1, &action) == ARX_OK);
    CHECK(action.position.x == doctest::Approx(10.0f));
    CHECK(action.position.y == doctest::Approx(22.0f));

    ArxModelSelection selection{};
    REQUIRE(model.selection(3, selection) == ARX_OK);
    CHECK(selection.leading_position.x == doctest::Approx(9.0f));
    CHECK(selection.leading_position.y == doctest::Approx(21.0f));

    CHECK(model.rotate({0.0f, 0.0f, 0.0f, 0.0f}) == ARX_INVALID_OPTIONS);
    CHECK(model.translate({std::numeric_limits<float>::infinity(), 0.0f, 0.0f}) == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Snaps bone origins in engine-centered Model space") {
    pistoris::Ftl reference_native = makeSemanticModelFtl();
    pistoris::Ftl target_native = reference_native;
    target_native.vertices[target_native.groups[0].origin].position = {40.0f, 50.0f, 60.0f};
    target_native.vertices[target_native.groups[1].origin].position = {70.0f, 80.0f, 90.0f};

    pistoris::Model reference;
    pistoris::Model target;
    REQUIRE(pistoris::Model::importNative(reference, reference_native) == ARX_OK);
    REQUIRE(pistoris::Model::importNative(target, target_native) == ARX_OK);
    REQUIRE(target.applyReference(reference, {.snap_bone_origins = true}) == ARX_OK);

    pistoris::NativeModelBundle baked;
    REQUIRE(target.bakeNativeBundle({}, baked) == ARX_OK);
    REQUIRE(baked.ftl.groups.size() == reference_native.groups.size());
    for (std::size_t index = 0; index < reference_native.groups.size(); ++index) {
      const ArxVector3 expected = reference_native.vertices[reference_native.groups[index].origin].position -
                                  reference_native.vertices[reference_native.header.origin].position;
      const ArxVector3 actual = baked.ftl.vertices[baked.ftl.groups[index].origin].position -
                                baked.ftl.vertices[baked.ftl.header.origin].position;
      CHECK(actual == expected);
    }

    pistoris::Model no_bones;
    CHECK(target.applyReference(no_bones, {.snap_bone_origins = true}) == ARX_MODEL_REFERENCE_BONE_COUNT_MISMATCH);

    ArxModelBone target_extra{{"target-extra", 12}, {1.0f, 2.0f, 3.0f}, 1, 0.0f};
    ArxModelBone reference_extra{{"reference-extra", 15}, {4.0f, 5.0f, 6.0f}, 0, 0.0f};
    pistoris::BoneIndex bone = pistoris::kInvalidBoneIndex;
    REQUIRE(target.addBone(target_extra, bone) == ARX_OK);
    REQUIRE(reference.addBone(reference_extra, bone) == ARX_OK);
    const ArxVector3 unchanged = target_extra.position;
    CHECK(target.applyReference(reference, {.snap_bone_origins = true}) == ARX_MODEL_REFERENCE_BONE_TOPOLOGY_MISMATCH);
    std::array<ArxModelBone, 3> target_bones{};
    REQUIRE(target.copyBones(0, target_bones.size(), target_bones.data()) == ARX_OK);
    CHECK(target_bones.back().position == unchanged);

    REQUIRE(target.removeBone(2) == ARX_OK);
    REQUIRE(reference.removeBone(2) == ARX_OK);
    ArxModelBone renamed{};
    REQUIRE(reference.copyBones(0, 1, &renamed) == ARX_OK);
    renamed.name = {"renamed-root", 12};
    REQUIRE(reference.setBone(0, renamed) == ARX_OK);
    WarningCapture warnings;
    CHECK(target.applyReference(reference, {.snap_bone_origins = true}) == ARX_OK);
    REQUIRE(warnings.messages.size() == 1);
    CHECK(warnings.messages[0].find("Model reference: bone 0 name mismatch") != std::string::npos);
  }

  TEST_CASE("Applies reference selection memberships transactionally by semantic identity") {
    pistoris::Model target;
    pistoris::Model reference;

    const auto add_bones = [](pistoris::Model& model, float offset) {
      pistoris::BoneIndex index = pistoris::kInvalidBoneIndex;
      const ArxModelBone root{{"root", 4}, {offset, 0.0f, 0.0f}, pistoris::kInvalidBoneIndex, 0.0f};
      const ArxModelBone child{{"child", 5}, {offset, 1.0f, 0.0f}, 0, 0.0f};
      REQUIRE(model.addBone(root, index) == ARX_OK);
      REQUIRE(model.addBone(child, index) == ARX_OK);
    };
    add_bones(target, 10.0f);
    add_bones(reference, 20.0f);

    const auto add_action = [](pistoris::Model& model, std::string_view name) {
      ArxModelActionPoint point{};
      point.name = {name.data(), name.size()};
      point.bone = 0;
      pistoris::ActionPointIndex index = pistoris::kInvalidActionPointIndex;
      REQUIRE(model.addActionPoint(point, index) == ARX_OK);
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
      pistoris::SelectionId id = pistoris::kInvalidSelectionId;
      REQUIRE(model.addSelection(selection, id) == ARX_OK);
      return id;
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
      REQUIRE(model.updateSelectionMembers(id, members) == ARX_OK);
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
        .snap_bone_origins = true,
        .copy_bone_origin_selections = true,
        .copy_action_point_selections = true,
    };
    CHECK(target.applyReference(reference, {}) == ARX_INVALID_OPTIONS);

    pistoris::Model incompatible(reference);
    pistoris::BoneIndex added = pistoris::kInvalidBoneIndex;
    const ArxModelBone extra{{"extra", 5}, {}, pistoris::kInvalidBoneIndex, 0.0f};
    REQUIRE(incompatible.addBone(extra, added) == ARX_OK);
    CHECK(target.applyReference(incompatible, options) == ARX_MODEL_REFERENCE_BONE_COUNT_MISMATCH);
    CHECK(selectionBones(target, target_shared_a) == std::vector<pistoris::BoneIndex>{1});
    CHECK(selectionActionPoints(target, target_shared_a) == std::vector<pistoris::ActionPointIndex>{1});
    ArxModelBone unchanged_root{};
    REQUIRE(target.copyBones(0, 1, &unchanged_root) == ARX_OK);
    CHECK(unchanged_root.position == ArxVector3{10.0f, 0.0f, 0.0f});

    WarningCapture warnings;
    REQUIRE(target.applyReference(reference, options) == ARX_OK);
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
    ArxModelBone copied_root{};
    REQUIRE(target.copyBones(0, 1, &copied_root) == ARX_OK);
    CHECK(copied_root.position == ArxVector3{20.0f, 0.0f, 0.0f});
  }

  TEST_CASE("Infers bone-origin selections from directly owned vertices") {
    pistoris::Model model;
    pistoris::BoneIndex bone = pistoris::kInvalidBoneIndex;
    REQUIRE(model.addBone({{"root", 4}, {}, pistoris::kInvalidBoneIndex, 0.0f}, bone) == ARX_OK);
    REQUIRE(model.addBone({{"child", 5}, {}, 0, 0.0f}, bone) == ARX_OK);
    REQUIRE(model.addBone({{"empty", 5}, {}, 0, 0.0f}, bone) == ARX_OK);

    std::vector<ArxModelVertex> vertices(22);
    for (std::size_t index = 0; index < 10; ++index) vertices[index].bone = 0;
    for (std::size_t index = 10; index < 21; ++index) vertices[index].bone = 1;
    vertices[21].bone = pistoris::kInvalidBoneIndex;
    pistoris::VertexIndex first = pistoris::kInvalidVertexIndex;
    REQUIRE(model.addVertices(vertices.data(), vertices.size(), first) == ARX_OK);

    pistoris::ActionPointIndex action = pistoris::kInvalidActionPointIndex;
    REQUIRE(model.addActionPoint({{"attach", 6}, {}, 0}, action) == ARX_OK);
    pistoris::SelectionId selection = pistoris::kInvalidSelectionId;
    ArxModelSelection armor{};
    armor.name = {"armor", 5};
    REQUIRE(model.addSelection(armor, selection) == ARX_OK);

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
                                          .action_point_count = selected_actions.size()}) == ARX_OK);
    REQUIRE(model.setSelectionIncludesOrigin(selection, true) == ARX_OK);

    REQUIRE(model.inferBoneOriginSelections() == ARX_OK);
    CHECK(selectionBones(model, selection) == std::vector<pistoris::BoneIndex>{0});
    CHECK(selectionVertices(model, selection) == selected);
    CHECK(selectionActionPoints(model, selection) == std::vector<pistoris::ActionPointIndex>{0});
    CHECK(selectionIncludesOrigin(model, selection));
  }
}
