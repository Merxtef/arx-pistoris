// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/model/types.h"

#include "audio_helpers.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

}  // namespace

TEST_SUITE("C Animation API") {
  TEST_CASE("Owns edited keyframes and bakes an opaque native handle") {
    ArxAnimation* animation = nullptr;
    REQUIRE(arx_pistoris_animation_create(&animation) == ARX_OK);
    REQUIRE(arx_pistoris_animation_set_name(animation, view("walk")) == ARX_OK);
    REQUIRE(arx_pistoris_animation_set_resource_path(animation, view("anim:npc:walk")) == ARX_OK);
    const ArxSoundView sound{view("sfx/step.wav"), {}};
    ArxSoundIndex sound_index = ARX_NO_SOUND;
    REQUIRE(arx_pistoris_animation_add_sound(animation, &sound, &sound_index) == ARX_OK);

    std::array<ArxAnimationGroupTransform, 1> transforms{};
    transforms[0].translation = {1.0f, 2.0f, 3.0f};
    const ArxAnimationKeyframeInput keyframe{{0, {}, {}, 1, sound_index}, transforms.data(), transforms.size()};
    size_t index = SIZE_MAX;
    REQUIRE(arx_pistoris_animation_add_keyframe(animation, &keyframe, &index) == ARX_OK);
    CHECK(index == 0);
    transforms[0].translation.x = 99.0f;

    ArxAnimationGroupTransform copied{};
    REQUIRE(arx_pistoris_animation_copy_group_transforms(animation, 0, 0, 1, &copied) == ARX_OK);
    CHECK(copied.translation.x == 1.0f);
    CHECK(arx_pistoris_animation_validate(animation) == ARX_OK);
    REQUIRE(arx_pistoris_animation_set_frame_length(animation, 3) == ARX_OK);
    CHECK(arx_pistoris_animation_replace_keyframes(animation, 0, nullptr, 0) == ARX_ANIMATION_NO_KEYFRAMES);
    size_t keyframe_count = 0;
    uint32_t frame_length = 0;
    REQUIRE(arx_pistoris_animation_keyframe_count(animation, &keyframe_count) == ARX_OK);
    REQUIRE(arx_pistoris_animation_frame_length(animation, &frame_length) == ARX_OK);
    CHECK(keyframe_count == 1);
    CHECK(frame_length == 3);

    ArxTea* native = nullptr;
    REQUIRE(arx_pistoris_animation_bake_native(animation, nullptr, &native, nullptr) == ARX_OK);
    CHECK(arx_pistoris_tea_validate(native) == ARX_OK);
    arx_pistoris_tea_destroy(native);
    arx_pistoris_animation_destroy(animation);
  }

  TEST_CASE("Validates handles and submitted pointers") {
    CHECK(arx_pistoris_animation_create(nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_animation_validate(nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_animation_scale(nullptr, 1.0f) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_animation_rotate(nullptr, {}) == ARX_INVALID_HANDLE);
    arx_pistoris_animation_destroy(nullptr);

    ArxAnimation* animation = nullptr;
    REQUIRE(arx_pistoris_animation_create(&animation) == ARX_OK);
    CHECK(arx_pistoris_animation_set_name(animation, {nullptr, 1}) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_animation_copy_keyframes(animation, 0, 1, nullptr) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(arx_pistoris_animation_add_keyframe(animation, nullptr, nullptr) == ARX_INVALID_DATA_POINTER);
    size_t index = 42;
    CHECK(arx_pistoris_animation_add_keyframe(animation, nullptr, &index) == ARX_INVALID_DATA_POINTER);
    CHECK(index == 42);

    const ArxAnimationKeyframeInput invalid_keyframe{
        {std::numeric_limits<std::uint32_t>::max(), {}, {}, 0, ARX_NO_SOUND}, nullptr, 0};
    CHECK(arx_pistoris_animation_add_keyframe(animation, &invalid_keyframe, &index) == ARX_ANIMATION_BAD_FRAME);
    CHECK(index == SIZE_MAX);
    arx_pistoris_animation_destroy(animation);
  }

  TEST_CASE("Transforms Animation motion through the C boundary") {
    ArxAnimation* animation = nullptr;
    REQUIRE(arx_pistoris_animation_create(&animation) == ARX_OK);
    REQUIRE(arx_pistoris_animation_set_name(animation, view("walk")) == ARX_OK);
    ArxAnimationGroupTransform group{};
    group.translation = {1.0f, 1.0f, 0.0f};
    const ArxAnimationKeyframeInput keyframe{{0, {1.0f, 0.0f, 0.0f}, {}, 0, ARX_NO_SOUND}, &group, 1};
    size_t index = SIZE_MAX;
    REQUIRE(arx_pistoris_animation_add_keyframe(animation, &keyframe, &index) == ARX_OK);

    REQUIRE(arx_pistoris_animation_scale(animation, 2.0f) == ARX_OK);
    REQUIRE(arx_pistoris_animation_rotate(animation, {2.0f, 0.0f, 0.0f, 2.0f}) == ARX_OK);
    ArxAnimationKeyframe copied{};
    REQUIRE(arx_pistoris_animation_copy_keyframes(animation, 0, 1, &copied) == ARX_OK);
    CHECK(copied.root_translation.x == doctest::Approx(0.0f));
    CHECK(copied.root_translation.y == doctest::Approx(2.0f));
    REQUIRE(arx_pistoris_animation_copy_group_transforms(animation, 0, 0, 1, &group) == ARX_OK);
    CHECK(group.translation.x == doctest::Approx(-2.0f));
    CHECK(group.translation.y == doctest::Approx(2.0f));
    CHECK(group.scale.x == 1.0f);
    CHECK(group.scale.y == 1.0f);
    CHECK(group.scale.z == 1.0f);

    CHECK(arx_pistoris_animation_scale(animation, 0.0f) == ARX_INVALID_OPTIONS);
    CHECK(arx_pistoris_animation_rotate(animation, {0.0f, 0.0f, 0.0f, 0.0f}) == ARX_INVALID_OPTIONS);
    arx_pistoris_animation_destroy(animation);
  }

  TEST_CASE("Edits Animation group state through the C boundary") {
    ArxAnimation* animation = nullptr;
    REQUIRE(arx_pistoris_animation_create(&animation) == ARX_OK);
    REQUIRE(arx_pistoris_animation_set_name(animation, view("guarded")) == ARX_OK);
    ArxAnimationGroupTransform group{};
    const ArxAnimationKeyframeInput keyframe{{0, {}, {}, 0, ARX_NO_SOUND}, &group, 1};
    size_t index = SIZE_MAX;
    REQUIRE(arx_pistoris_animation_add_keyframe(animation, &keyframe, &index) == ARX_OK);

    uint8_t value = 0;
    REQUIRE(arx_pistoris_animation_is_group_void(animation, 0, &value) == ARX_OK);
    CHECK(value == 1);
    REQUIRE(arx_pistoris_animation_claim_group(animation, 0) == ARX_OK);
    REQUIRE(arx_pistoris_animation_is_group_claimed(animation, 0, &value) == ARX_OK);
    CHECK(value == 1);
    REQUIRE(arx_pistoris_animation_void_group(animation, 0) == ARX_OK);
    REQUIRE(arx_pistoris_animation_is_group_claimed(animation, 0, &value) == ARX_OK);
    CHECK(value == 0);
    CHECK(arx_pistoris_animation_is_group_void(animation, 0, nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_animation_unclaim_group(animation, 1) == ARX_INDEX_OUT_OF_RANGE);
    arx_pistoris_animation_destroy(animation);
  }

  TEST_CASE("Transfers Animation sidecars through the Model GLB C boundary") {
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_create(&model) == ARX_OK);
    std::array<ArxModelVertex, 3> vertices{};
    vertices[1].position.x = 1.0f;
    vertices[2].position.y = 1.0f;
    ArxVertexIndex first = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_vertices(model, vertices.data(), vertices.size(), &first) == ARX_OK);
    ArxModelFace face{};
    face.normal = {0.0f, 0.0f, 1.0f};
    face.texture = ARX_NO_TEXTURE;
    for (std::size_t index = 0; index < 3; ++index) {
      face.corners[index].vertex = static_cast<ArxVertexIndex>(index);
      face.corners[index].normal = face.normal;
    }
    ArxFaceIndex face_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_model_add_face(model, &face, &face_index) == ARX_OK);

    ArxAnimation* animation = nullptr;
    REQUIRE(arx_pistoris_animation_create(&animation) == ARX_OK);
    REQUIRE(arx_pistoris_animation_set_name(animation, view("idle")) == ARX_OK);
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    const ArxSoundView sound{view("sfx/test.wav"), {wav.data(), wav.size()}};
    ArxSoundIndex sound_index = ARX_NO_SOUND;
    REQUIRE(arx_pistoris_animation_add_sound(animation, &sound, &sound_index) == ARX_OK);
    const ArxAnimationKeyframeInput keyframe{{0, {}, {}, 0, sound_index}, nullptr, 0};
    size_t keyframe_index = SIZE_MAX;
    REQUIRE(arx_pistoris_animation_add_keyframe(animation, &keyframe, &keyframe_index) == ARX_OK);

    const ArxAnimation* sidecars[] = {animation};
    const ArxModelGlbExportOptions export_options = ARX_MODEL_GLB_EXPORT_OPTIONS_INIT;
    ArxAnimationConversionReport export_report{};
    std::uint8_t* encoded = nullptr;
    size_t encoded_size = 0;
    ArxAnimationSoundFiles* exported_sounds = nullptr;
    REQUIRE(arx_pistoris_model_export_glb(
                model, sidecars, 1, &export_options, &export_report, &encoded, &encoded_size, &exported_sounds) ==
            ARX_OK);
    CHECK(export_report.converted == 1);
    size_t exported_sound_count = 0;
    REQUIRE(arx_pistoris_animation_sound_files_count(exported_sounds, &exported_sound_count) == ARX_OK);
    REQUIRE(exported_sound_count == 1);
    ArxAnimationSoundFile exported_sound{};
    REQUIRE(arx_pistoris_animation_sound_files_get(exported_sounds, 0, &exported_sound) == ARX_OK);
    CHECK(exported_sound.animation_index == 0);
    CHECK((std::string_view(exported_sound.file.path.data, exported_sound.file.path.size) == "sfx/test.wav"));
    exported_sound.animation_index = 17;
    exported_sound.file.source_sound = 23;
    exported_sound.file.path = view("sentinel");
    exported_sound.file.encoded_audio = {wav.data(), wav.size()};
    CHECK(arx_pistoris_animation_sound_files_get(exported_sounds, exported_sound_count, &exported_sound) ==
          ARX_INDEX_OUT_OF_RANGE);
    CHECK(exported_sound.animation_index == 0);
    CHECK(exported_sound.file.source_sound == 0);
    CHECK(exported_sound.file.path.data == nullptr);
    CHECK(exported_sound.file.path.size == 0);
    CHECK(exported_sound.file.encoded_audio.data == nullptr);
    CHECK(exported_sound.file.encoded_audio.size == 0);

    const ArxModelGlbImportOptions import_options = ARX_MODEL_GLB_IMPORT_OPTIONS_INIT;
    ArxModel* imported_model = nullptr;
    ArxAnimationList* imported_animations = nullptr;
    ArxAnimationConversionReport import_report{};
    ArxTextureSourcePaths* sources = nullptr;
    ArxAnimationSoundSourceReferences* sound_sources = nullptr;
    REQUIRE(arx_pistoris_model_import_glb(encoded,
                                          encoded_size,
                                          &import_options,
                                          &imported_model,
                                          &imported_animations,
                                          &import_report,
                                          &sources,
                                          &sound_sources) == ARX_OK);
    REQUIRE(imported_animations != nullptr);
    size_t imported_count = 0;
    REQUIRE(arx_pistoris_animation_list_count(imported_animations, &imported_count) == ARX_OK);
    REQUIRE(imported_count == 1);
    REQUIRE(sources != nullptr);
    std::size_t source_count = 1;
    REQUIRE(arx_pistoris_texture_source_paths_count(sources, &source_count) == ARX_OK);
    CHECK(source_count == 0);
    size_t sound_source_count = 0;
    REQUIRE(arx_pistoris_animation_sound_source_references_count(sound_sources, &sound_source_count) == ARX_OK);
    REQUIRE(sound_source_count == 1);
    ArxAnimationSoundSourceReference sound_source{};
    REQUIRE(arx_pistoris_animation_sound_source_references_get(sound_sources, 0, &sound_source) == ARX_OK);
    CHECK(sound_source.animation_index == 0);
    CHECK((std::string_view(sound_source.reference.path.data, sound_source.reference.path.size) == "sfx/test.wav"));
    sound_source.animation_index = 17;
    sound_source.reference.sound = 23;
    sound_source.reference.path = view("sentinel");
    CHECK(arx_pistoris_animation_sound_source_references_get(sound_sources, sound_source_count, &sound_source) ==
          ARX_INDEX_OUT_OF_RANGE);
    CHECK(sound_source.animation_index == 0);
    CHECK(sound_source.reference.sound == 0);
    CHECK(sound_source.reference.path.data == nullptr);
    CHECK(sound_source.reference.path.size == 0);
    CHECK(import_report.converted == 1);
    ArxAnimation* imported_animation = nullptr;
    REQUIRE(arx_pistoris_animation_list_get(imported_animations, 0, &imported_animation) == ARX_OK);
    ArxStringView name{};
    REQUIRE(arx_pistoris_animation_name(imported_animation, &name) == ARX_OK);
    CHECK((std::string_view(name.data, name.size) == "idle"));
    CHECK(arx_pistoris_animation_list_get(imported_animations, imported_count, &imported_animation) ==
          ARX_INDEX_OUT_OF_RANGE);
    CHECK(imported_animation == nullptr);

    arx_pistoris_texture_source_paths_destroy(sources);
    arx_pistoris_animation_sound_source_references_destroy(sound_sources);
    arx_pistoris_animation_sound_files_destroy(exported_sounds);
    arx_pistoris_animation_list_destroy(imported_animations);
    arx_pistoris_model_destroy(imported_model);
    arx_pistoris_free_bytes(encoded);
    arx_pistoris_animation_destroy(animation);
    arx_pistoris_model_destroy(model);
  }
}
