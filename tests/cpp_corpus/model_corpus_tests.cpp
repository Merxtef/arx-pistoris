// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/glb.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "support/animation_equivalence.h"
#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/fixture_catalog.h"
#include "support/fixture_resources.h"
#include "support/model_equivalence.h"
#include "support/native_equivalence.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct ModelBounds {
  pistoris::ArxVector3 minimum;
  pistoris::ArxVector3 maximum;
};

ModelBounds modelBounds(const pistoris::Model& model) {
  REQUIRE(model.vertexCount() > 0);
  std::vector<ArxModelVertex> vertices(model.vertexCount());
  REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);

  ModelBounds result{vertices.front().position, vertices.front().position};
  for (const ArxModelVertex& vertex : vertices) {
    result.minimum.x = std::min(result.minimum.x, vertex.position.x);
    result.minimum.y = std::min(result.minimum.y, vertex.position.y);
    result.minimum.z = std::min(result.minimum.z, vertex.position.z);
    result.maximum.x = std::max(result.maximum.x, vertex.position.x);
    result.maximum.y = std::max(result.maximum.y, vertex.position.y);
    result.maximum.z = std::max(result.maximum.z, vertex.position.z);
  }
  return result;
}

void checkBoundsEquivalent(const ModelBounds& left, const ModelBounds& right) {
  CHECK(left.minimum.x == doctest::Approx(right.minimum.x).epsilon(1e-5).scale(1.0));
  CHECK(left.minimum.y == doctest::Approx(right.minimum.y).epsilon(1e-5).scale(1.0));
  CHECK(left.minimum.z == doctest::Approx(right.minimum.z).epsilon(1e-5).scale(1.0));
  CHECK(left.maximum.x == doctest::Approx(right.maximum.x).epsilon(1e-5).scale(1.0));
  CHECK(left.maximum.y == doctest::Approx(right.maximum.y).epsilon(1e-5).scale(1.0));
  CHECK(left.maximum.z == doctest::Approx(right.maximum.z).epsilon(1e-5).scale(1.0));
}

}  // namespace

TEST_SUITE("model_corpus") {
  TEST_CASE("Native FTL converts through Model") {
    std::size_t fixture_hydrations = 0;
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kFtl)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      pistoris::Ftl source;
      if (!test_support::checkCorpusStatus(path, "read FTL", pistoris::readFtl(source_bytes, source))) continue;

      pistoris::Model model;
      std::vector<std::string> texture_source_paths;
      if (!test_support::checkCorpusStatus(
              path, "import FTL into Model", pistoris::Model::importNative(model, source, &texture_source_paths)))
        continue;
      if (!test_support::checkCorpusStatus(path, "validate Model", model.validate())) continue;
      CHECK(model.resourcePath().empty());
      const std::optional<test_support::HydrationResult> hydration =
          test_support::hydrateTextures(model, test_support::nativeMount(path), texture_source_paths, true);
      if (!hydration) continue;
      if (test_support::isCommittedFixture(path)) fixture_hydrations += hydration->hydrated;

      pistoris::NativeModelBundle baked;
      if (!test_support::checkCorpusStatus(
              path, "bake Model to native bundle", model.bakeNativeBundle({.include_texture_files = true}, baked)))
        continue;
      if (!test_support::validateTextureFiles(model, std::span<const pistoris::NativeTextureFile>(baked.texture_files)))
        continue;

      pistoris::Model roundtrip;
      std::vector<std::string> roundtrip_sources;
      if (!test_support::checkCorpusStatus(path,
                                           "import baked FTL into Model",
                                           pistoris::Model::importNative(roundtrip, baked.ftl, &roundtrip_sources)))
        continue;
      const std::optional<test_support::HydrationResult> roundtrip_hydration = test_support::hydrateTexturesFromFiles(
          roundtrip, roundtrip_sources, std::span<const pistoris::NativeTextureFile>(baked.texture_files));
      if (!roundtrip_hydration) continue;
      if (!test_support::checkCorpusStatus(path, "validate roundtrip Model", roundtrip.validate())) continue;
      CHECK(roundtrip_hydration->hydrated >= hydration->hydrated);
      test_support::checkModelsEquivalent(model,
                                          roundtrip,
                                          {.compare_external_texture_extensions = false,
                                           .compare_encoded_images = false,
                                           .compare_vertex_multiplicity = false,
                                           .comparison_epsilon = 1e-5f});
    }
    CHECK(fixture_hydrations > 0);
  }

  TEST_CASE("OBJ fixtures convert through Model") {
    std::size_t fixture_count = 0;
    std::size_t fixture_hydrations = 0;
    for (const test_support::ModelFixture& fixture : test_support::fixtureCatalog().models) {
      if (fixture.obj.empty()) continue;
      ++fixture_count;
      CAPTURE(fixture.obj.string());

      const std::optional<test_support::ObjFixtureInput> input = test_support::readObjFixture(fixture.obj);
      if (!input) continue;
      const std::vector<pistoris::ObjMaterialLibraryView> material_libraries =
          test_support::objMaterialLibraryViews(*input);
      pistoris::Model model;
      std::vector<std::string> texture_source_paths;
      REQUIRE(pistoris::Model::importObj(model, input->obj, material_libraries, &texture_source_paths) == ARX_OK);
      REQUIRE(model.validate() == ARX_OK);
      const std::optional<test_support::HydrationResult> hydration =
          test_support::hydrateTextures(model, fixture.obj.parent_path(), texture_source_paths);
      if (!hydration) continue;
      fixture_hydrations += hydration->hydrated;

      pistoris::ObjBundle encoded;
      REQUIRE(model.exportObj(fixture.obj.stem().string(), {.include_files = true}, encoded) == ARX_OK);
      if (!test_support::validateTextureFiles(model, std::span<const pistoris::ObjTextureFile>(encoded.texture_files)))
        continue;

      pistoris::Model roundtrip;
      std::vector<std::string> roundtrip_sources;
      REQUIRE(pistoris::Model::importObj(roundtrip, encoded.text, encoded.mtl, &roundtrip_sources) == ARX_OK);
      const std::optional<test_support::HydrationResult> roundtrip_hydration = test_support::hydrateTexturesFromFiles(
          roundtrip, roundtrip_sources, std::span<const pistoris::ObjTextureFile>(encoded.texture_files));
      if (!roundtrip_hydration) continue;
      REQUIRE(roundtrip.validate() == ARX_OK);
      CHECK(roundtrip_hydration->hydrated >= hydration->hydrated);
      test_support::checkModelsEquivalent(model, roundtrip, {.compare_external_texture_extensions = false});
    }
    CHECK(fixture_count > 0);
    CHECK(fixture_hydrations > 0);
  }

  TEST_CASE("GLB fixtures convert through Model") {
    for (const test_support::ModelFixture& fixture : test_support::fixtureCatalog().models) {
      if (fixture.glb.empty()) continue;
      CAPTURE(fixture.glb.path.string());

      pistoris::Model::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      pistoris::Model::GlbExportOptions export_options;
      export_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;

      pistoris::Model model;
      std::vector<std::string> texture_source_paths;
      REQUIRE(pistoris::Model::importGlb(
                  model, test_support::readBytes(fixture.glb.path), import_options, &texture_source_paths) == ARX_OK);
      REQUIRE(model.validate() == ARX_OK);
      if (!test_support::hydrateTextures(model, fixture.glb.path.parent_path(), texture_source_paths)) continue;

      std::vector<std::uint8_t> encoded;
      REQUIRE(model.exportGlb(encoded, export_options) == ARX_OK);
      pistoris::Model roundtrip;
      REQUIRE(pistoris::Model::importGlb(roundtrip, encoded, import_options) == ARX_OK);
      CHECK(roundtrip.validate() == ARX_OK);
      test_support::checkModelsEquivalent(model,
                                          roundtrip,
                                          {.compare_external_texture_extensions = false,
                                           .compare_normals = false,
                                           .compare_geometry_order = false,
                                           .comparison_epsilon = 1e-5f});
    }
  }

  TEST_CASE("Authored GLB scale matches generated native Model") {
    for (const test_support::ModelFixture& fixture : test_support::fixtureCatalog().models) {
      if (fixture.glb.empty() || fixture.ftl.empty()) continue;
      CAPTURE(fixture.name);

      pistoris::Model::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      pistoris::Model authored;
      REQUIRE(pistoris::Model::importGlb(authored, test_support::readBytes(fixture.glb.path), import_options) ==
              ARX_OK);

      pistoris::Ftl native;
      REQUIRE(pistoris::readFtl(test_support::readBytes(fixture.ftl), native) == ARX_OK);
      pistoris::Model generated;
      REQUIRE(pistoris::Model::importNative(generated, native) == ARX_OK);

      checkBoundsEquivalent(modelBounds(authored), modelBounds(generated));
    }
  }

  TEST_CASE("Authored GLB fixtures convert through Model and Animations") {
    const test_support::FixtureCatalog& catalog = test_support::fixtureCatalog();
    std::size_t fixture_count = 0;
    std::size_t fixture_hydrations = 0;
    for (const test_support::ModelFixture& fixture : catalog.models) {
      std::vector<const test_support::AnimationFixture*> animation_fixtures;
      for (const test_support::AnimationFixture& animation : catalog.animations) {
        if (animation.model == fixture.name) animation_fixtures.push_back(&animation);
      }
      if (animation_fixtures.empty()) continue;
      REQUIRE_FALSE(fixture.glb.empty());
      ++fixture_count;
      CAPTURE(fixture.name);

      pistoris::Model::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      pistoris::Model model;
      std::vector<std::unique_ptr<pistoris::Animation>> animations;
      std::vector<std::string> texture_source_paths;
      std::vector<pistoris::AnimationSoundSourceReference> sound_sources;
      REQUIRE(pistoris::Model::importGlb(model,
                                         animations,
                                         test_support::readBytes(fixture.glb.path),
                                         import_options,
                                         nullptr,
                                         &texture_source_paths,
                                         &sound_sources) == ARX_OK);
      REQUIRE(model.validate() == ARX_OK);
      if (!test_support::hydrateTextures(model, fixture.glb.path.parent_path(), texture_source_paths)) continue;
      for (const std::unique_ptr<pistoris::Animation>& animation : animations) {
        REQUIRE(animation != nullptr);
        REQUIRE(animation->validate() == ARX_OK);
      }
      const std::optional<test_support::HydrationResult> hydration =
          test_support::hydrateAnimationSounds(animations, fixture.glb.path.parent_path(), sound_sources);
      if (!hydration) continue;
      fixture_hydrations += hydration->hydrated;

      for (const test_support::AnimationFixture* animation_fixture : animation_fixtures) {
        CAPTURE(animation_fixture->name);
        const auto found = std::ranges::find_if(animations, [&](const std::unique_ptr<pistoris::Animation>& animation) {
          return animation != nullptr && animation->name() == animation_fixture->name;
        });
        REQUIRE(found != animations.end());

        pistoris::NativeAnimationBundle baked;
        REQUIRE((*found)->bakeNativeBundle({.include_sound_files = true}, baked) == ARX_OK);
        if (!test_support::validateSoundFiles(**found, std::span<const pistoris::SoundFile>(baked.sound_files)))
          continue;

        pistoris::Tea native;
        REQUIRE(pistoris::readTea(test_support::readBytes(animation_fixture->tea), native) == ARX_OK);
        test_support::checkEquivalent(native, baked.tea, {.comparison_epsilon = 1e-5f});
      }
    }
    CHECK(fixture_count > 0);
    CHECK(fixture_hydrations > 0);
  }

  TEST_CASE("Native Models and Animations convert through GLB") {
    const test_support::FixtureCatalog& catalog = test_support::fixtureCatalog();
    std::size_t fixture_hydrations = 0;
    for (const test_support::ModelFixture& fixture : catalog.models) {
      std::vector<const test_support::AnimationFixture*> animation_fixtures;
      for (const test_support::AnimationFixture& animation : catalog.animations) {
        if (animation.model == fixture.name) animation_fixtures.push_back(&animation);
      }
      if (animation_fixtures.empty()) continue;
      REQUIRE_FALSE(fixture.glb.empty());

      pistoris::Ftl native_model;
      REQUIRE(pistoris::readFtl(test_support::readBytes(fixture.ftl), native_model) == ARX_OK);
      pistoris::Model model;
      REQUIRE(pistoris::Model::importNative(model, native_model) == ARX_OK);
      REQUIRE(model.setResourcePath(fixture.selector) == ARX_OK);

      std::vector<std::unique_ptr<pistoris::Animation>> animations;
      std::vector<const pistoris::Animation*> views;
      std::size_t source_hydrations = 0;
      bool hydration_failed = false;
      for (const test_support::AnimationFixture* animation_fixture : animation_fixtures) {
        pistoris::Tea native_animation;
        REQUIRE(pistoris::readTea(test_support::readBytes(animation_fixture->tea), native_animation) == ARX_OK);
        auto animation = std::make_unique<pistoris::Animation>();
        std::vector<pistoris::SoundSourceReference> sound_sources;
        REQUIRE(pistoris::Animation::importNative(*animation, native_animation, &sound_sources) == ARX_OK);
        REQUIRE(animation->setResourcePath(animation_fixture->selector) == ARX_OK);
        const std::optional<test_support::HydrationResult> hydration =
            test_support::hydrateSounds(*animation,
                                        test_support::nativeMount(animation_fixture->tea),
                                        sound_sources,
                                        test_support::SoundSourceLayout::kNativeAnimation);
        if (!hydration) {
          hydration_failed = true;
          break;
        }
        source_hydrations += hydration->hydrated;
        fixture_hydrations += hydration->hydrated;
        views.push_back(animation.get());
        animations.push_back(std::move(animation));
      }
      if (hydration_failed) continue;

      pistoris::ModelGlbBundle bundle;
      pistoris::Model::GlbExportOptions export_options;
      export_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      REQUIRE(model.exportGlbBundle(views, export_options, nullptr, bundle) == ARX_OK);
      if (!test_support::validateAnimationSoundFiles(views, bundle.sound_files)) continue;

      pistoris::Model roundtrip;
      std::vector<std::unique_ptr<pistoris::Animation>> roundtrip_animations;
      std::vector<std::string> texture_source_paths;
      std::vector<pistoris::AnimationSoundSourceReference> sound_sources;
      pistoris::Model::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      REQUIRE(pistoris::Model::importGlb(roundtrip,
                                         roundtrip_animations,
                                         bundle.glb,
                                         import_options,
                                         nullptr,
                                         &texture_source_paths,
                                         &sound_sources) == ARX_OK);
      REQUIRE(roundtrip_animations.size() == animations.size());
      std::size_t roundtrip_hydrations = 0;
      for (const pistoris::AnimationSoundSourceReference& source : sound_sources) {
        REQUIRE(source.animation_index < roundtrip_animations.size());
        ArxSoundView sound{};
        REQUIRE(roundtrip_animations[source.animation_index]->copySoundViews(source.reference.sound, 1, &sound) ==
                ARX_OK);
        if (sound.encoded_audio.size != 0) continue;
        const auto found = std::ranges::find_if(bundle.sound_files, [&](const pistoris::AnimationSoundFile& file) {
          return file.animation_index == source.animation_index &&
                 test_support::resourceKey(file.file.path) == test_support::resourceKey(source.reference.path);
        });
        if (found == bundle.sound_files.end()) continue;
        REQUIRE(roundtrip_animations[source.animation_index]->setSoundData(
                    source.reference.sound, {found->file.encoded_audio.data(), found->file.encoded_audio.size()}) ==
                ARX_OK);
        ++roundtrip_hydrations;
      }
      CHECK(roundtrip_hydrations >= source_hydrations);
      for (std::size_t index = 0; index < roundtrip_animations.size(); ++index) {
        REQUIRE(roundtrip_animations[index]->validate() == ARX_OK);
        test_support::AnimationEquivalenceOptions equivalence;
        equivalence.comparison_epsilon = 1e-4f;
        test_support::checkAnimationsEquivalent(*animations[index], *roundtrip_animations[index], equivalence);
      }
    }
    CHECK(fixture_hydrations > 0);
  }
}
