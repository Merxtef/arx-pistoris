// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/arx_pistoris.h"

#include "audio_helpers.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

ArxStringView stringView(std::string_view value) { return {value.data(), value.size()}; }

}  // namespace

TEST_SUITE("C Animation editing") {
  TEST_CASE("Animation lifecycle exposes native import and sound editing") {
    ArxAnimation* animation = nullptr;
    REQUIRE(arx_pistoris_animation_create(&animation) == ARX_OK);
    REQUIRE(arx_pistoris_animation_set_name(animation, stringView("walk")) == ARX_OK);
    REQUIRE(arx_pistoris_animation_set_resource_path(animation, stringView("anim:npc:walk")) == ARX_OK);

    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    ArxSoundView first_sound{stringView("sfx/step.wav"), {wav.data(), wav.size()}};
    ArxSoundView unused_sound{stringView("sfx/unused.wav"), {wav.data(), wav.size()}};
    ArxSoundIndex first_sound_index = ARX_NO_SOUND;
    ArxSoundIndex unused_sound_index = ARX_NO_SOUND;
    REQUIRE(arx_pistoris_animation_add_sound(animation, &first_sound, &first_sound_index) == ARX_OK);
    REQUIRE(arx_pistoris_animation_add_sound(animation, &unused_sound, &unused_sound_index) == ARX_OK);

    first_sound.path = stringView("sfx/updated.wav");
    first_sound.encoded_audio = {};
    REQUIRE(arx_pistoris_animation_set_sound(animation, first_sound_index, &first_sound) == ARX_OK);
    ArxSoundView copied_sound{};
    REQUIRE(arx_pistoris_animation_copy_sound_views(animation, first_sound_index, 1, &copied_sound) == ARX_OK);
    CHECK((std::string_view(copied_sound.path.data, copied_sound.path.size) == "sfx/updated.wav"));
    REQUIRE(arx_pistoris_animation_set_sound_data(animation, first_sound_index, {wav.data(), wav.size()}) == ARX_OK);
    REQUIRE(arx_pistoris_animation_copy_sound_views(animation, first_sound_index, 1, &copied_sound) == ARX_OK);
    REQUIRE(copied_sound.encoded_audio.size == wav.size());
    CHECK(std::equal(wav.begin(), wav.end(), copied_sound.encoded_audio.data));
    REQUIRE(arx_pistoris_animation_clear_sound_data(animation, first_sound_index) == ARX_OK);
    REQUIRE(arx_pistoris_animation_copy_sound_views(animation, first_sound_index, 1, &copied_sound) == ARX_OK);
    CHECK(copied_sound.encoded_audio.size == 0);
    REQUIRE(arx_pistoris_animation_set_sound_data(animation, first_sound_index, {wav.data(), wav.size()}) == ARX_OK);
    REQUIRE(arx_pistoris_animation_copy_sound_views(animation, first_sound_index, 1, &copied_sound) == ARX_OK);
    REQUIRE(copied_sound.encoded_audio.size == wav.size());
    CHECK(std::equal(wav.begin(), wav.end(), copied_sound.encoded_audio.data));

    std::array<ArxAnimationGroupTransform, 2> transforms{};
    transforms[0].translation = {1.0f, 2.0f, 3.0f};
    ArxAnimationKeyframeInput keyframe{{0, {}, {}, 0, first_sound_index}, transforms.data(), transforms.size()};
    std::size_t keyframe_index = SIZE_MAX;
    REQUIRE(arx_pistoris_animation_add_keyframe(animation, &keyframe, &keyframe_index) == ARX_OK);
    REQUIRE(arx_pistoris_animation_set_frame_length(animation, 3) == ARX_OK);
    keyframe.keyframe.frame = 2;
    keyframe.keyframe.footstep = 1;
    REQUIRE(arx_pistoris_animation_set_keyframe(animation, keyframe_index, &keyframe) == ARX_OK);
    ArxAnimationKeyframe copied_keyframe{};
    REQUIRE(arx_pistoris_animation_copy_keyframes(animation, keyframe_index, 1, &copied_keyframe) == ARX_OK);
    CHECK(copied_keyframe.frame == 2);
    CHECK(copied_keyframe.footstep == 1);
    CHECK(copied_keyframe.sound == first_sound_index);
    ArxAnimationGroupTransform copied_transform{};
    REQUIRE(arx_pistoris_animation_copy_group_transforms(animation, keyframe_index, 0, 1, &copied_transform) == ARX_OK);
    CHECK(copied_transform.translation.x == doctest::Approx(1.0f));
    CHECK(copied_transform.translation.y == doctest::Approx(2.0f));
    CHECK(copied_transform.translation.z == doctest::Approx(3.0f));

    std::size_t count = 0;
    REQUIRE(arx_pistoris_animation_group_count(animation, &count) == ARX_OK);
    CHECK(count == transforms.size());
    REQUIRE(arx_pistoris_animation_sound_count(animation, &count) == ARX_OK);
    CHECK(count == 2);
    std::uint32_t frame_length = 0;
    REQUIRE(arx_pistoris_animation_frame_length(animation, &frame_length) == ARX_OK);
    CHECK(frame_length == 3);
    std::array<ArxSoundView, 2> copied_sounds{};
    REQUIRE(arx_pistoris_animation_copy_sound_views(animation, 0, copied_sounds.size(), copied_sounds.data()) ==
            ARX_OK);
    CHECK((std::string_view(copied_sounds[0].path.data, copied_sounds[0].path.size) == "sfx/updated.wav"));
    ArxStringView resource_path{};
    REQUIRE(arx_pistoris_animation_resource_path(animation, &resource_path) == ARX_OK);
    CHECK((std::string_view(resource_path.data, resource_path.size) == "graph/obj3d/anims/npc/walk.tea"));

    std::size_t removed = 0;
    REQUIRE(arx_pistoris_animation_compact_sounds(animation, &removed) == ARX_OK);
    CHECK(removed == 1);
    REQUIRE(arx_pistoris_animation_rebase_sound_paths(animation, stringView("custom")) == ARX_OK);
    REQUIRE(arx_pistoris_animation_copy_sound_views(animation, 0, 1, copied_sounds.data()) == ARX_OK);
    CHECK((std::string_view(copied_sounds[0].path.data, copied_sounds[0].path.size) == "custom/updated.wav"));

    ArxTea* native = nullptr;
    REQUIRE(arx_pistoris_animation_bake_native(animation, nullptr, &native, nullptr) == ARX_OK);
    ArxAnimation* imported = nullptr;
    ArxSoundSourceReferences* sources = nullptr;
    REQUIRE(arx_pistoris_animation_import_native(native, &imported, &sources) == ARX_OK);
    REQUIRE(imported != nullptr);
    REQUIRE(sources != nullptr);
    CHECK(arx_pistoris_animation_validate(imported) == ARX_OK);
    ArxStringView imported_name{};
    REQUIRE(arx_pistoris_animation_name(imported, &imported_name) == ARX_OK);
    CHECK((std::string_view(imported_name.data, imported_name.size) == "walk"));
    REQUIRE(arx_pistoris_animation_frame_length(imported, &frame_length) == ARX_OK);
    CHECK(frame_length == 3);
    REQUIRE(arx_pistoris_animation_keyframe_count(imported, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_animation_group_count(imported, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_animation_sound_count(imported, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_animation_copy_keyframes(imported, 0, 1, &copied_keyframe) == ARX_OK);
    CHECK(copied_keyframe.frame == 2);
    CHECK(copied_keyframe.footstep == 1);
    CHECK(copied_keyframe.sound == 0);
    REQUIRE(arx_pistoris_animation_copy_group_transforms(imported, 0, 0, 1, &copied_transform) == ARX_OK);
    CHECK(copied_transform.translation.x == doctest::Approx(1.0f));
    REQUIRE(arx_pistoris_animation_copy_sound_views(imported, 0, 1, &copied_sound) == ARX_OK);
    CHECK((std::string_view(copied_sound.path.data, copied_sound.path.size) == "sfx/custom/updated.wav"));
    std::size_t source_count = 0;
    REQUIRE(arx_pistoris_sound_source_references_count(sources, &source_count) == ARX_OK);
    CHECK(source_count == 1);
    ArxSoundSourceReference source{};
    REQUIRE(arx_pistoris_sound_source_references_get(sources, 0, &source) == ARX_OK);
    CHECK(source.sound == 0);
    CHECK((std::string_view(source.path.data, source.path.size) == "custom/updated"));

    ArxAnimation* clone = nullptr;
    REQUIRE(arx_pistoris_animation_clone(imported, &clone) == ARX_OK);
    REQUIRE(arx_pistoris_animation_reset(imported) == ARX_OK);
    REQUIRE(arx_pistoris_animation_keyframe_count(imported, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_animation_group_count(imported, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_animation_sound_count(imported, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_animation_frame_length(imported, &frame_length) == ARX_OK);
    CHECK(frame_length == 0);
    REQUIRE(arx_pistoris_animation_name(imported, &imported_name) == ARX_OK);
    CHECK(imported_name.size == 0);
    CHECK(arx_pistoris_animation_validate(clone) == ARX_OK);

    REQUIRE(arx_pistoris_animation_remove_keyframe(animation, 0) == ARX_OK);
    REQUIRE(arx_pistoris_animation_keyframe_count(animation, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_animation_remove_sound(animation, 0) == ARX_OK);
    REQUIRE(arx_pistoris_animation_sound_count(animation, &count) == ARX_OK);
    CHECK(count == 0);
    keyframe.keyframe.sound = ARX_NO_SOUND;
    keyframe_index = SIZE_MAX;
    REQUIRE(arx_pistoris_animation_add_keyframe(animation, &keyframe, &keyframe_index) == ARX_OK);
    REQUIRE(arx_pistoris_animation_keyframe_count(animation, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_animation_clear_keyframes(animation) == ARX_OK);
    REQUIRE(arx_pistoris_animation_keyframe_count(animation, &count) == ARX_OK);
    CHECK(count == 0);

    arx_pistoris_animation_destroy(clone);
    arx_pistoris_sound_source_references_destroy(sources);
    arx_pistoris_animation_destroy(imported);
    arx_pistoris_tea_destroy(native);
    arx_pistoris_animation_destroy(animation);
  }
}
