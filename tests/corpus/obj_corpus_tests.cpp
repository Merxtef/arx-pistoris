// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "support/fixture_catalog.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string readText(const fs::path& path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}

TEST_SUITE("obj") {
  TEST_CASE("Model OBJ corpus roundtrips through the public boundary") {
    for (const test_support::ModelFixture& fixture : test_support::fixtureCatalog().models) {
      if (fixture.obj.empty()) continue;
      const fs::path& obj_path = fixture.obj;
      CAPTURE(obj_path.string());

      std::string obj_text = readText(obj_path);
      const std::string stem = fixture.name;

      ArxObjMaterialLibraryPaths* library_paths = nullptr;
      REQUIRE(arx_pistoris_obj_material_library_paths(
                  reinterpret_cast<const std::uint8_t*>(obj_text.data()), obj_text.size(), &library_paths) == ARX_OK);
      std::size_t library_count = 0;
      REQUIRE(arx_pistoris_obj_material_library_paths_count(library_paths, &library_count) == ARX_OK);
      std::vector<std::string> library_texts;
      library_texts.reserve(library_count);
      std::vector<ArxStringView> path_views;
      path_views.reserve(library_count);
      for (std::size_t index = 0; index < library_count; ++index) {
        ArxStringView path_view{};
        REQUIRE(arx_pistoris_obj_material_library_paths_get(library_paths, index, &path_view) == ARX_OK);
        path_views.push_back(path_view);
        library_texts.push_back(readText(obj_path.parent_path() / std::string(path_view.data, path_view.size)));
      }
      std::vector<ArxObjMaterialLibraryView> libraries;
      libraries.reserve(library_count);
      for (std::size_t index = 0; index < library_count; ++index) {
        const std::string& text = library_texts[index];
        libraries.push_back({path_views[index], reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
      }

      ArxModel* initial_model = nullptr;
      REQUIRE(arx_pistoris_model_import_obj(reinterpret_cast<const std::uint8_t*>(obj_text.data()),
                                            obj_text.size(),
                                            libraries.data(),
                                            libraries.size(),
                                            &initial_model,
                                            nullptr) == ARX_OK);
      arx_pistoris_obj_material_library_paths_destroy(library_paths);

      const auto roundtrip =
          [&](const std::string& obj_in, const std::string& mtl_in, std::string& obj_out, std::string& mtl_out) {
            ArxModel* model = nullptr;
            const std::string material_path = stem + ".mtl";
            const ArxObjMaterialLibraryView library{
                {material_path.data(), material_path.size()},
                reinterpret_cast<const std::uint8_t*>(mtl_in.data()),
                mtl_in.size(),
            };
            const ArxReturnCode import_rc =
                arx_pistoris_model_import_obj(reinterpret_cast<const std::uint8_t*>(obj_in.data()),
                                              obj_in.size(),
                                              mtl_in.empty() ? nullptr : &library,
                                              mtl_in.empty() ? 0 : 1,
                                              &model,
                                              nullptr);
            if (import_rc != ARX_OK) return import_rc;

            char* obj_raw = nullptr;
            char* mtl_raw = nullptr;
            const ArxReturnCode export_rc =
                arx_pistoris_model_export_obj(model, {stem.data(), stem.size()}, nullptr, &obj_raw, &mtl_raw, nullptr);
            arx_pistoris_model_destroy(model);
            if (obj_raw) {
              obj_out = obj_raw;
              arx_pistoris_free_string(obj_raw);
            }
            if (mtl_raw) {
              mtl_out = mtl_raw;
              arx_pistoris_free_string(mtl_raw);
            }
            return export_rc;
          };

      std::string first_obj;
      std::string first_mtl;
      char* first_obj_raw = nullptr;
      char* first_mtl_raw = nullptr;
      REQUIRE(arx_pistoris_model_export_obj(
                  initial_model, {stem.data(), stem.size()}, nullptr, &first_obj_raw, &first_mtl_raw, nullptr) ==
              ARX_OK);
      arx_pistoris_model_destroy(initial_model);
      REQUIRE(first_obj_raw != nullptr);
      first_obj = first_obj_raw;
      arx_pistoris_free_string(first_obj_raw);
      if (first_mtl_raw) {
        first_mtl = first_mtl_raw;
        arx_pistoris_free_string(first_mtl_raw);
      }
      REQUIRE_FALSE(first_obj.empty());

      std::string second_obj;
      std::string second_mtl;
      CHECK(roundtrip(first_obj, first_mtl, second_obj, second_mtl) == ARX_OK);
      CHECK_FALSE(second_obj.empty());
    }
  }
}
