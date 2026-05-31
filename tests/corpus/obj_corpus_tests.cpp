// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/ftl_data.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

static std::string readText(const fs::path& p) {
  std::ifstream f(p);
  return {std::istreambuf_iterator<char>(f), {}};
}

TEST_SUITE("obj") {
  // import -> export -> import -> export; both exports must match (pipeline stable after pass 1)
  TEST_CASE("ModelObjImportStable") {
    std::size_t count = 0;
    for (auto& e : fs::directory_iterator("data/fixtures/model/native")) {
      if (e.path().extension() != ".ftl") continue;
      fs::path obj_path = fs::path("data/fixtures/model/obj") / e.path().stem();
      obj_path += ".obj";
      if (!fs::exists(obj_path)) continue;
      count++;
      fs::path mtl_path = fs::path("data/fixtures/model/obj") / e.path().stem();
      mtl_path += ".mtl";
      CAPTURE(obj_path.string());

      std::string obj_text = readText(obj_path);
      std::string mtl_text = fs::exists(mtl_path) ? readText(mtl_path) : "";
      std::string stem     = e.path().stem().string();

      auto import_and_export = [&](const std::string& obj_in, const std::string& mtl_in, std::string& obj_out,
                                   std::string& mtl_out) -> bool {
        ArxFtlHandle h = nullptr;
        if (arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj_in.data()), obj_in.size(),
                                   mtl_in.empty() ? nullptr : reinterpret_cast<const uint8_t*>(mtl_in.data()),
                                   mtl_in.size(), stem.c_str(), &h) != ARX_OK)
          return false;
        char* obj_raw = nullptr;
        char* mtl_raw = nullptr;
        bool ok       = arx_pistoris_ftl_to_obj(h, stem.c_str(), &obj_raw) == ARX_OK;
        ok            = ok && arx_pistoris_ftl_to_mtl(h, &mtl_raw) == ARX_OK;
        arx_pistoris_ftl_free(h);
        if (obj_raw) {
          obj_out = obj_raw;
          arx_pistoris_free_string(obj_raw);
        }
        if (mtl_raw) {
          mtl_out = mtl_raw;
          arx_pistoris_free_string(mtl_raw);
        }
        return ok;
      };

      std::string pass1_obj, pass1_mtl, pass2_obj, pass2_mtl;
      CHECK(import_and_export(obj_text, mtl_text, pass1_obj, pass1_mtl));
      if (pass1_obj.empty()) continue;
      CHECK(import_and_export(pass1_obj, pass1_mtl, pass2_obj, pass2_mtl));

      // size-check first to avoid dumping full content on mismatch
      CHECK(pass1_obj.size() == pass2_obj.size());
      if (pass1_obj.size() == pass2_obj.size()) CHECK(pass1_obj == pass2_obj);
    }
    CHECK(count >= 1);
  }

  // > 65535 unique vertices -> ARX_OBJ_TOO_MANY_VERTICES
  TEST_CASE("ImportTooManyVertices") {
    // each corner uses unique position with no vn -> unique (vi, SIZE_MAX) key;
    // faces 1..21845 produce 65535 verts, face 21846 corner 1 trips the limit
    std::string obj;
    obj.reserve(1300000);
    for (int i = 1; i <= 65538; ++i) {
      obj += "v ";
      obj += std::to_string(i);
      obj += " 0 0\n";
    }
    for (int i = 1; i <= 21846; ++i) {
      obj += "f ";
      obj += std::to_string(3 * i - 2);
      obj += " ";
      obj += std::to_string(3 * i - 1);
      obj += " ";
      obj += std::to_string(3 * i);
      obj += "\n";
    }

    ArxFtlHandle h = nullptr;
    ArxReturnCode rc =
        arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj.data()), obj.size(), nullptr, 0, nullptr, &h);
    CHECK(rc == ARX_OBJ_TOO_MANY_VERTICES);
    CHECK(h == nullptr);
  }

  // > kFtlMaxVertices vn lines -> ARX_OBJ_TOO_MANY_NORMALS
  TEST_CASE("ImportTooManyNormals") {
    std::string obj;
    obj.reserve((pistoris::kFtlMaxVertices + 1) * 9);
    for (std::size_t i = 0; i < pistoris::kFtlMaxVertices + 1; ++i) obj += "vn 0 0 1\n";

    ArxFtlHandle h = nullptr;
    ArxReturnCode rc =
        arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj.data()), obj.size(), nullptr, 0, nullptr, &h);
    CHECK(rc == ARX_OBJ_TOO_MANY_NORMALS);
    CHECK(h == nullptr);
  }

  // > kFtlMaxVertices*4 vt lines -> ARX_OBJ_TOO_MANY_TEXCOORDS
  TEST_CASE("ImportTooManyTexcoords") {
    std::string obj;
    obj.reserve((pistoris::kFtlMaxVertices * 4 + 1) * 7);
    for (std::size_t i = 0; i < pistoris::kFtlMaxVertices * 4 + 1; ++i) obj += "vt 0 0\n";

    ArxFtlHandle h = nullptr;
    ArxReturnCode rc =
        arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj.data()), obj.size(), nullptr, 0, nullptr, &h);
    CHECK(rc == ARX_OBJ_TOO_MANY_TEXCOORDS);
    CHECK(h == nullptr);
  }

  // > kFtlMaxFaces triangulated faces -> ARX_OBJ_TOO_MANY_FACES
  TEST_CASE("ImportTooManyFaces") {
    // shared 3 positions: vertex dedup is O(1), only face count matters
    std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
    obj.reserve(obj.size() + (pistoris::kFtlMaxFaces + 1) * 8);
    for (std::size_t i = 0; i < pistoris::kFtlMaxFaces + 1; ++i) obj += "f 1 2 3\n";

    ArxFtlHandle h = nullptr;
    ArxReturnCode rc =
        arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj.data()), obj.size(), nullptr, 0, nullptr, &h);
    CHECK(rc == ARX_OBJ_TOO_MANY_FACES);
    CHECK(h == nullptr);
  }

  // > 65534 unique texture stems -> ARX_OBJ_TOO_MANY_TEXTURES
  TEST_CASE("ImportTooManyTextures") {
    // each unique usemtl stem creates a new TextureContainer
    std::string obj;
    obj.reserve(1100000);
    for (int i = 0; i < 65536; ++i) {
      obj += "usemtl TEX";
      obj += std::to_string(i);
      obj += "\n";
    }

    ArxFtlHandle h = nullptr;
    ArxReturnCode rc =
        arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj.data()), obj.size(), nullptr, 0, nullptr, &h);
    CHECK(rc == ARX_OBJ_TOO_MANY_TEXTURES);
    CHECK(h == nullptr);
  }

  // > kFtlMaxFaces materials (per-face ceiling) -> ARX_OBJ_TOO_MANY_MATERIALS
  TEST_CASE("ImportTooManyMaterials") {
    // MTL is parsed first and fails before OBJ processing starts
    const auto* dummy_obj = reinterpret_cast<const uint8_t*>("");
    std::string mtl;
    mtl.reserve((pistoris::kFtlMaxFaces + 1) * 15);
    for (std::size_t i = 0; i < pistoris::kFtlMaxFaces + 1; ++i) {
      mtl += "newmtl M";
      mtl += std::to_string(i);
      mtl += "\n";
    }

    ArxFtlHandle h = nullptr;
    ArxReturnCode rc =
        arx_pistoris_obj_parse(dummy_obj, 0, reinterpret_cast<const uint8_t*>(mtl.data()), mtl.size(), nullptr, &h);
    CHECK(rc == ARX_OBJ_TOO_MANY_MATERIALS);
    CHECK(h == nullptr);
  }

}  // TEST_SUITE("obj")
