// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/ftl.h"
#include "helpers.h"
#include "utils/cursor.h"

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

// Group header (kFtlGroupHeaderSize): name[256] + origin(4) + numIndices(4) + reserved(4) +
// blob_shadow_size(4); group indices follow all group headers
static std::vector<uint8_t> makeFtlWithGroupIndex(int32_t idx_val) {
  std::vector<uint8_t> buf = makeMinimalFtl();
  int32_t two = 2, one = 1;
  std::memcpy(buf.data() + kFtlNVertsOff, &two, 4);
  std::memcpy(buf.data() + kFtlNGroupsOff, &one, 4);
  buf.resize(kFtlDataOff + 2 * kFtlVertexSize + kFtlGroupHeaderSize + 4, 0);
  std::size_t group_base = kFtlDataOff + 2 * kFtlVertexSize;
  std::memcpy(buf.data() + group_base + kFtlGroupOffIdxCount, &one, 4);
  std::memcpy(buf.data() + group_base + kFtlGroupHeaderSize, &idx_val, 4);
  return buf;
}

// Selection header (kFtlSelHeaderSize): name[64] + numSelected(4) + reserved(4);
// selection indices follow all selection headers
static std::vector<uint8_t> makeFtlWithSelIndex(int32_t idx_val) {
  std::vector<uint8_t> buf = makeMinimalFtl();
  int32_t two = 2, one = 1;
  std::memcpy(buf.data() + kFtlNVertsOff, &two, 4);
  std::memcpy(buf.data() + kFtlNSelsOff, &one, 4);
  buf.resize(kFtlDataOff + 2 * kFtlVertexSize + kFtlSelHeaderSize + 4, 0);
  std::size_t sel_base = kFtlDataOff + 2 * kFtlVertexSize;
  std::memcpy(buf.data() + sel_base + kFtlSelOffIdxCount, &one, 4);
  std::memcpy(buf.data() + sel_base + kFtlSelHeaderSize, &idx_val, 4);
  return buf;
}

static ArxReturnCode load(const std::vector<uint8_t>& buf, pistoris::ftl::Data& d) {
  pistoris::ReadCursor c(buf.data(), buf.size());
  return pistoris::loadFtl(&d, c);
}

TEST_SUITE("ftl") {
  TEST_CASE("FtlBadIdentifier") {
    uint8_t data[] = {'X', 'X', 'X', '\0', 0, 0, 0, 0};
    pistoris::ftl::Data d;
    pistoris::ReadCursor c(data, sizeof(data));
    CHECK(pistoris::loadFtl(&d, c) == ARX_INVALID_IDENTIFIER);
  }

  TEST_CASE("FtlBadVersion") {
    std::vector<uint8_t> buf(kFtlDataOff, 0);
    std::memcpy(buf.data(), pistoris::kFtlMagic, 4);
    uint32_t bad = 0;
    std::memcpy(buf.data() + 4, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_VERSION);
  }

  // buffer ends at kFtlHeaderOff, leaving no bytes for Header read
  TEST_CASE("FtlTruncatedHeader") {
    std::vector<uint8_t> buf(kFtlHeaderOff, 0);
    std::memcpy(buf.data(), pistoris::kFtlMagic, 4);
    std::memcpy(buf.data() + 4, &pistoris::kFtlVersion, 4);
    int32_t sec0 = kFtlNVertsOff;
    std::memcpy(buf.data() + kFtlSectionPtrsOff, &sec0, 4);
    int32_t one = 1;
    std::memcpy(buf.data() + kFtlNVertsOff, &one, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // numberOfVertices = 2 but only 1 vertex follows
  TEST_CASE("FtlTruncatedVertices") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t two              = 2;
    std::memcpy(buf.data() + kFtlNVertsOff, &two, sizeof(two));
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // 1 face referencing texture 0, but no texture containers -> invalid texture id
  TEST_CASE("FtlInvalidFaceTexture") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    buf.resize(buf.size() + kFtlVertexSize + kFtlFaceSize, 0);
    int32_t one = 1;
    std::memcpy(buf.data() + kFtlNVertsOff, &one, 4);
    std::memcpy(buf.data() + kFtlNFacesOff, &one, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_FACE_TEX);
  }

  TEST_CASE("FtlBadOffsetNegative") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t bad              = -1;
    std::memcpy(buf.data() + kFtlSectionPtrsOff, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_OFFSET);
  }

  TEST_CASE("FtlBadOffsetTooSmall") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t bad              = 100;
    std::memcpy(buf.data() + kFtlSectionPtrsOff, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_OFFSET);
  }

  TEST_CASE("FtlBadOffsetPastEof") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t bad              = static_cast<int32_t>(buf.size() + 100);
    std::memcpy(buf.data() + kFtlSectionPtrsOff, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_OFFSET);
  }

  // offset_3d_data > 544: gap between secondary header and 3D section is skipped
  TEST_CASE("FtlOffsetGap") {
    constexpr int32_t kOffset   = 600;
    constexpr std::size_t kSize = kOffset + (kFtlDataOff - kFtlNVertsOff) + kFtlVertexSize;
    std::vector<uint8_t> buf(kSize, 0);
    std::memcpy(buf.data(), pistoris::kFtlMagic, 4);
    std::memcpy(buf.data() + 4, &pistoris::kFtlVersion, 4);
    std::memcpy(buf.data() + kFtlSectionPtrsOff, &kOffset, 4);
    for (int i = 1; i < 6; ++i) {
      int32_t sec_n = -1;
      std::memcpy(buf.data() + kFtlSectionPtrsOff + i * 4, &sec_n, 4);
    }
    int32_t one = 1;
    std::memcpy(buf.data() + kOffset, &one, 4);

    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_OK);
    CHECK(d.vertices.size() == 1);
  }

  TEST_CASE("FtlBadGroupIdxOob") {
    auto buf = makeFtlWithGroupIndex(2);  // 2 >= numVerts (2)
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_GROUP_IDX);
  }

  TEST_CASE("FtlBadGroupIdxNegative") {
    auto buf = makeFtlWithGroupIndex(-1);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_GROUP_IDX);
  }

  TEST_CASE("FtlBadSelIdxOob") {
    auto buf = makeFtlWithSelIndex(2);  // 2 >= numVerts (2)
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_SEL_IDX);
  }

  TEST_CASE("FtlBadSelIdxNegative") {
    auto buf = makeFtlWithSelIndex(-1);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_SEL_IDX);
  }

  // numberOfVertices = 0 -> no FTL can have zero vertices
  TEST_CASE("FtlBadVertN") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t zero             = 0;
    std::memcpy(buf.data() + kFtlNVertsOff, &zero, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_VERT_N);
  }

  // header.origin = 1 with only 1 vertex (index 0) -> out of range
  TEST_CASE("FtlBadOrigin") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    uint32_t bad             = 1;
    std::memcpy(buf.data() + kFtlHeaderOff, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_ORIGIN);
  }

  // numberOfFaces = -1
  TEST_CASE("FtlBadFaceN") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t bad              = -1;
    std::memcpy(buf.data() + kFtlNFacesOff, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_FACE_N);
  }

  // face type bits beyond FACE_BITS_ALL = (1<<28)-1
  TEST_CASE("FtlBadFaceType") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t one              = 1;
    std::memcpy(buf.data() + kFtlNFacesOff, &one, 4);
    buf.resize(kFtlDataOff + kFtlVertexSize + kFtlFaceSize, 0);
    uint32_t bad_type = 1U << 28;
    std::memcpy(buf.data() + kFtlDataOff + kFtlVertexSize, &bad_type, 4);
    int16_t no_tex = -1;  // kFtlTextureNone
    std::memcpy(buf.data() + kFtlDataOff + kFtlVertexSize + kFtlFaceOffTexId, &no_tex, 2);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_FACE_TYPE);
  }

  // vertex_idx.x = 1 >= numVertices (1)
  TEST_CASE("FtlBadFaceVertIdx") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t one              = 1;
    std::memcpy(buf.data() + kFtlNFacesOff, &one, 4);
    buf.resize(kFtlDataOff + kFtlVertexSize + kFtlFaceSize, 0);
    uint16_t idx[3] = {1, 0, 0};
    std::memcpy(buf.data() + kFtlDataOff + kFtlVertexSize + kFtlFaceOffVertIdx, idx, sizeof(idx));
    int16_t no_tex = -1;  // kFtlTextureNone
    std::memcpy(buf.data() + kFtlDataOff + kFtlVertexSize + kFtlFaceOffTexId, &no_tex, 2);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_FACE_VERT_IDX);
  }

  // numberOfTextures = -1
  TEST_CASE("FtlBadTexN") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t bad              = -1;
    std::memcpy(buf.data() + kFtlNTexOff, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_TEX_N);
  }

  // numberOfGroups = -1
  TEST_CASE("FtlBadGroupN") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t bad              = -1;
    std::memcpy(buf.data() + kFtlNGroupsOff, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_GROUP_N);
  }

  // group num_indices = -1
  TEST_CASE("FtlBadGroupIdxN") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t one              = 1;
    std::memcpy(buf.data() + kFtlNGroupsOff, &one, 4);
    buf.resize(kFtlDataOff + kFtlVertexSize + kFtlGroupHeaderSize, 0);
    int32_t bad = -1;
    std::memcpy(buf.data() + kFtlDataOff + kFtlVertexSize + kFtlGroupOffIdxCount, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_GROUP_IDX_N);
  }

  // group origin 2 >= numVertices (2)
  TEST_CASE("FtlBadGroupOrigin") {
    constexpr std::size_t kGroupBase = kFtlDataOff + 2 * kFtlVertexSize;
    auto buf                         = makeFtlWithGroupIndex(0);
    uint32_t bad_origin              = 2;
    std::memcpy(buf.data() + kGroupBase + kFtlGroupOffOrigin, &bad_origin, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_GROUP_ORIGIN);
  }

  // numberOfActions = -1
  TEST_CASE("FtlBadActionN") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t bad              = -1;
    std::memcpy(buf.data() + kFtlNActionsOff, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_ACTION_N);
  }

  // action vertex_idx = -1 -> out of range
  TEST_CASE("FtlBadActionVertIdx") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t one              = 1;
    std::memcpy(buf.data() + kFtlNActionsOff, &one, 4);
    buf.resize(kFtlDataOff + kFtlVertexSize + kFtlActionSize, 0);
    int32_t bad = -1;
    std::memcpy(buf.data() + kFtlDataOff + kFtlVertexSize + kFtlActionOffVertIdx, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_ACTION_VERT_IDX);
  }

  // numberOfSelections = -1
  TEST_CASE("FtlBadSelN") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t bad              = -1;
    std::memcpy(buf.data() + kFtlNSelsOff, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_SEL_N);
  }

  // selection num_selected = -1
  TEST_CASE("FtlBadSelIdxN") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    int32_t one              = 1;
    std::memcpy(buf.data() + kFtlNSelsOff, &one, 4);
    buf.resize(kFtlDataOff + kFtlVertexSize + kFtlSelHeaderSize, 0);
    int32_t bad = -1;
    std::memcpy(buf.data() + kFtlDataOff + kFtlVertexSize + kFtlSelOffIdxCount, &bad, 4);
    pistoris::ftl::Data d;
    CHECK(load(buf, d) == ARX_FTL_BAD_SEL_IDX_N);
  }

  // --- ClampStr paths: name fills all bytes without null terminator ---

  // header.name at kFtlHeaderOff+4 (after origin uint32_t); 256 bytes
  TEST_CASE("FtlHeaderNameNoNullTerm") {
    auto buf = makeMinimalFtl();
    std::memset(buf.data() + kFtlHeaderOff + 4, 'A', 256);
    pistoris::ftl::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.header.name[255] == '\0');
  }

  // group.name is first 256 bytes of group header
  TEST_CASE("FtlGroupNameNoNullTerm") {
    auto buf                         = makeFtlWithGroupIndex(0);
    constexpr std::size_t kGroupBase = kFtlDataOff + 2 * kFtlVertexSize;
    std::memset(buf.data() + kGroupBase, 'A', 256);
    pistoris::ftl::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.groups[0].name[255] == '\0');
  }

  // texture.filename is the full 256-byte TextureContainer
  TEST_CASE("FtlTextureFilenameNoNullTerm") {
    auto buf                       = makeTriangleFtlWithTexture();
    constexpr std::size_t kTexBase = kFtlDataOff + 3 * kFtlVertexSize + kFtlFaceSize;
    std::memset(buf.data() + kTexBase, 'A', 256);
    pistoris::ftl::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.texture_containers[0].filename[255] == '\0');
  }

  // action.name is first 256 bytes of action; vertex_idx at +256 stays 0 (valid)
  TEST_CASE("FtlActionNameNoNullTerm") {
    auto buf    = makeMinimalFtl();
    int32_t one = 1;
    std::memcpy(buf.data() + kFtlNActionsOff, &one, 4);
    buf.resize(kFtlDataOff + kFtlVertexSize + kFtlActionSize, 0);
    std::memset(buf.data() + kFtlDataOff + kFtlVertexSize, 'A', 256);
    pistoris::ftl::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.actions[0].name[255] == '\0');
  }

  // selection.name is first 64 bytes of selection header
  TEST_CASE("FtlSelectionNameNoNullTerm") {
    auto buf                       = makeFtlWithSelIndex(0);
    constexpr std::size_t kSelBase = kFtlDataOff + 2 * kFtlVertexSize;
    std::memset(buf.data() + kSelBase, 'A', 64);
    pistoris::ftl::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.selections[0].name[63] == '\0');
  }

  // group 1 origin (vertex 3) not in any earlier group's indices -> parent_bone[1] = -1
  TEST_CASE("FtlOrphanBone") {
    pistoris::ftl::Data d = makeData(4);
    pistoris::ftl::Group g0{};
    std::memcpy(g0.name, "root", 5);
    g0.origin  = 0;
    g0.indices = {0, 1, 2};
    pistoris::ftl::Group g1{};
    std::memcpy(g1.name, "orphan", 7);
    g1.origin  = 3;  // vertex 3 is not in g0.indices and not in any group before g1
    g1.indices = {3};
    d.groups.push_back(std::move(g0));
    d.groups.push_back(std::move(g1));

    CHECK(pistoris::validateFtl(&d) == ARX_FTL_ORPHAN_BONE);
  }

  // group 1's origin is in group 0's indices -> g0 is its parent
  TEST_CASE("FtlValidTwoBoneChain") {
    pistoris::ftl::Data d = makeData(4);
    pistoris::ftl::Group g0{};
    std::memcpy(g0.name, "root", 5);
    g0.origin  = 0;
    g0.indices = {0, 1, 2, 3};
    pistoris::ftl::Group g1{};
    std::memcpy(g1.name, "child", 6);
    g1.origin  = 1;  // vertex 1 is in g0.indices -> g0 is parent
    g1.indices = {1, 2};
    d.groups.push_back(std::move(g0));
    d.groups.push_back(std::move(g1));

    REQUIRE(pistoris::validateFtl(&d) == ARX_OK);
    CHECK(d.extras.parent_bone[0] == -1);
    CHECK(d.extras.parent_bone[1] == 0);
  }

}  // TEST_SUITE("ftl")
