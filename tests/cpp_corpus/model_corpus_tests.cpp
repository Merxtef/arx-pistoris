// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/math.h"
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
  const auto vertices = model.vertices();
  REQUIRE_FALSE(vertices.size() == 0);

  ModelBounds result{vertices[0].position, vertices[0].position};
  for (const ArxModelVertex vertex : vertices) {
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
      auto source_result = pistoris::readFtl(source_bytes);
      if (!test_support::checkCorpusStatus(path, "read FTL", source_result)) continue;
      pistoris::Ftl source = std::move(*source_result);

      std::vector<std::string> texture_source_paths;
      auto imported = pistoris::Model::importNative(source, &texture_source_paths);
      if (!test_support::checkCorpusStatus(path, "import FTL into Model", imported)) continue;
      pistoris::Model model = std::move(*imported);
      if (!test_support::checkCorpusStatus(path, "validate Model", model.validate())) continue;
      CHECK(model.resourcePath().empty());
      const std::optional<test_support::HydrationResult> hydration =
          test_support::hydrateTextures(model, test_support::nativeMount(path), texture_source_paths, true);
      if (!hydration) continue;
      if (test_support::isCommittedFixture(path)) fixture_hydrations += hydration->hydrated;

      auto baked_result = model.bakeNativeBundle({.include_texture_files = true});
      if (!test_support::checkCorpusStatus(path, "bake Model to native bundle", baked_result)) continue;
      pistoris::NativeModelBundle baked = std::move(*baked_result);
      if (!test_support::validateTextureFiles(model, std::span<const pistoris::NativeTextureFile>(baked.texture_files)))
        continue;

      std::vector<std::string> roundtrip_sources;
      auto roundtrip_result = pistoris::Model::importNative(baked.ftl, &roundtrip_sources);
      if (!test_support::checkCorpusStatus(path, "import baked FTL into Model", roundtrip_result)) continue;
      pistoris::Model roundtrip = std::move(*roundtrip_result);
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
      std::vector<std::string> texture_source_paths;
      auto imported = pistoris::Model::importObj(input->obj, material_libraries, &texture_source_paths);
      REQUIRE(imported);
      pistoris::Model model = std::move(*imported);
      REQUIRE(model.validate());
      const std::optional<test_support::HydrationResult> hydration =
          test_support::hydrateTextures(model, fixture.obj.parent_path(), texture_source_paths);
      if (!hydration) continue;
      fixture_hydrations += hydration->hydrated;

      auto encoded_result = model.exportObj(fixture.obj.stem().string(), {.include_files = true});
      REQUIRE(encoded_result);
      pistoris::ObjBundle encoded = std::move(*encoded_result);
      if (!test_support::validateTextureFiles(model, std::span<const pistoris::ObjTextureFile>(encoded.texture_files)))
        continue;

      std::vector<std::string> roundtrip_sources;
      auto roundtrip_result = pistoris::Model::importObj(encoded.text, encoded.mtl, &roundtrip_sources);
      REQUIRE(roundtrip_result);
      pistoris::Model roundtrip = std::move(*roundtrip_result);
      const std::optional<test_support::HydrationResult> roundtrip_hydration = test_support::hydrateTexturesFromFiles(
          roundtrip, roundtrip_sources, std::span<const pistoris::ObjTextureFile>(encoded.texture_files));
      if (!roundtrip_hydration) continue;
      REQUIRE(roundtrip.validate());
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

      std::vector<std::string> texture_source_paths;
      auto imported =
          pistoris::Model::importGlb(test_support::readBytes(fixture.glb.path), import_options, &texture_source_paths);
      REQUIRE(imported);
      pistoris::Model model = std::move(*imported);
      REQUIRE(model.validate());
      if (!test_support::hydrateTextures(model, fixture.glb.path.parent_path(), texture_source_paths)) continue;

      auto encoded = model.exportGlb(export_options);
      REQUIRE(encoded);
      auto roundtrip_result = pistoris::Model::importGlb(*encoded, import_options);
      REQUIRE(roundtrip_result);
      pistoris::Model roundtrip = std::move(*roundtrip_result);
      CHECK(roundtrip.validate());
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
      auto authored_result = pistoris::Model::importGlb(test_support::readBytes(fixture.glb.path), import_options);
      REQUIRE(authored_result);
      pistoris::Model authored = std::move(*authored_result);

      auto native = pistoris::readFtl(test_support::readBytes(fixture.ftl));
      REQUIRE(native);
      auto generated_result = pistoris::Model::importNative(*native);
      REQUIRE(generated_result);
      pistoris::Model generated = std::move(*generated_result);

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
      std::vector<std::string> texture_source_paths;
      std::vector<pistoris::AnimationSoundSourceReference> sound_sources;
      auto imported = pistoris::Model::importGlbWithAnimations(
          test_support::readBytes(fixture.glb.path), import_options, nullptr, &texture_source_paths, &sound_sources);
      REQUIRE(imported);
      pistoris::Model model = std::move(imported->model);
      std::vector<pistoris::Animation> animations = std::move(imported->animations);
      REQUIRE(model.validate());
      if (!test_support::hydrateTextures(model, fixture.glb.path.parent_path(), texture_source_paths)) continue;
      for (const pistoris::Animation& animation : animations) {
        REQUIRE(animation.validate());
      }
      const std::optional<test_support::HydrationResult> hydration =
          test_support::hydrateAnimationSounds(animations, fixture.glb.path.parent_path(), sound_sources);
      if (!hydration) continue;
      fixture_hydrations += hydration->hydrated;

      for (const test_support::AnimationFixture* animation_fixture : animation_fixtures) {
        CAPTURE(animation_fixture->name);
        const auto found = std::ranges::find_if(animations, [&](const pistoris::Animation& animation) {
          return animation.name() == animation_fixture->name;
        });
        REQUIRE(found != animations.end());

        auto baked_result = found->bakeNativeBundle({.include_sound_files = true});
        REQUIRE(baked_result);
        const pistoris::NativeAnimationBundle& baked = *baked_result;
        if (!test_support::validateSoundFiles(*found, std::span<const pistoris::SoundFile>(baked.sound_files)))
          continue;

        auto native = pistoris::readTea(test_support::readBytes(animation_fixture->tea));
        REQUIRE(native);
        test_support::checkEquivalent(*native, baked.tea, {.comparison_epsilon = 1e-5f});
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

      auto native_model = pistoris::readFtl(test_support::readBytes(fixture.ftl));
      REQUIRE(native_model);
      auto model_result = pistoris::Model::importNative(*native_model);
      REQUIRE(model_result);
      pistoris::Model model = std::move(*model_result);
      REQUIRE(model.setResourcePath(fixture.selector));

      std::vector<pistoris::Animation> animations;
      animations.reserve(animation_fixtures.size());
      std::size_t source_hydrations = 0;
      bool hydration_failed = false;
      for (const test_support::AnimationFixture* animation_fixture : animation_fixtures) {
        auto native_animation = pistoris::readTea(test_support::readBytes(animation_fixture->tea));
        REQUIRE(native_animation);
        std::vector<pistoris::SoundSourceReference> sound_sources;
        auto animation_result = pistoris::Animation::importNative(*native_animation, &sound_sources);
        REQUIRE(animation_result);
        pistoris::Animation animation = std::move(*animation_result);
        REQUIRE(animation.setResourcePath(animation_fixture->selector));
        const std::optional<test_support::HydrationResult> hydration =
            test_support::hydrateSounds(animation,
                                        test_support::nativeMount(animation_fixture->tea),
                                        sound_sources,
                                        test_support::SoundSourceLayout::kNativeAnimation);
        if (!hydration) {
          hydration_failed = true;
          break;
        }
        source_hydrations += hydration->hydrated;
        fixture_hydrations += hydration->hydrated;
        animations.push_back(std::move(animation));
      }
      if (hydration_failed) continue;

      std::vector<const pistoris::Animation*> views;
      views.reserve(animations.size());
      for (const pistoris::Animation& animation : animations) views.push_back(&animation);
      pistoris::Model::GlbExportOptions export_options;
      export_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      auto bundle_result = model.exportGlbBundle(views, export_options, nullptr);
      REQUIRE(bundle_result);
      pistoris::ModelGlbBundle bundle = std::move(*bundle_result);
      if (!test_support::validateAnimationSoundFiles(views, bundle.sound_files)) continue;

      std::vector<std::string> texture_source_paths;
      std::vector<pistoris::AnimationSoundSourceReference> sound_sources;
      pistoris::Model::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      auto roundtrip_result = pistoris::Model::importGlbWithAnimations(
          bundle.glb, import_options, nullptr, &texture_source_paths, &sound_sources);
      REQUIRE(roundtrip_result);
      pistoris::ModelGlbImport roundtrip = std::move(*roundtrip_result);
      std::vector<pistoris::Animation>& roundtrip_animations = roundtrip.animations;
      REQUIRE(roundtrip_animations.size() == animations.size());
      std::size_t roundtrip_hydrations = 0;
      for (const pistoris::AnimationSoundSourceReference& source : sound_sources) {
        REQUIRE(source.animation_index < roundtrip_animations.size());
        const ArxSoundView sound = roundtrip_animations[source.animation_index].sounds()[source.reference.sound];
        if (sound.encoded_audio.size != 0) continue;
        const auto found = std::ranges::find_if(bundle.sound_files, [&](const pistoris::AnimationSoundFile& file) {
          return file.animation_index == source.animation_index &&
                 test_support::resourceKey(file.file.path) == test_support::resourceKey(source.reference.path);
        });
        if (found == bundle.sound_files.end()) continue;
        REQUIRE(roundtrip_animations[source.animation_index].setSoundData(
            source.reference.sound, {found->file.encoded_audio.data(), found->file.encoded_audio.size()}));
        ++roundtrip_hydrations;
      }
      CHECK(roundtrip_hydrations >= source_hydrations);
      for (std::size_t index = 0; index < roundtrip_animations.size(); ++index) {
        REQUIRE(roundtrip_animations[index].validate());
        test_support::AnimationEquivalenceOptions equivalence;
        equivalence.comparison_epsilon = 1e-4f;
        test_support::checkAnimationsEquivalent(animations[index], roundtrip_animations[index], equivalence);
      }
    }
    CHECK(fixture_hydrations > 0);
  }
}
