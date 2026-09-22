// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/native.hpp"

#include "support/equivalence.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string_view>
#include <vector>

namespace test_support {

constexpr pistoris::amb::TrackFlags kAmbTrackFlagMask = pistoris::amb::kTrackPosition | pistoris::amb::kTrackMaster;
constexpr pistoris::amb::SettingFlags kAmbSettingFlagMask =
    pistoris::amb::kSettingRandom | pistoris::amb::kSettingInterpolate;

template <std::size_t N>
inline std::string_view nativeStringView(const char (&value)[N]) {
  const void* terminator = std::memchr(value, '\0', N);
  return {value, terminator ? static_cast<const char*>(terminator) - value : N};
}

inline bool equivalent(float lhs, float rhs) {
  return std::bit_cast<std::uint32_t>(lhs) == std::bit_cast<std::uint32_t>(rhs);
}

inline bool equivalent(const pistoris::ArxVector3& lhs, const pistoris::ArxVector3& rhs) {
  return equivalent(lhs.x, rhs.x) && equivalent(lhs.y, rhs.y) && equivalent(lhs.z, rhs.z);
}

inline bool equivalent(const pistoris::ArxAngle& lhs, const pistoris::ArxAngle& rhs) {
  return equivalent(lhs.pitch, rhs.pitch) && equivalent(lhs.yaw, rhs.yaw) && equivalent(lhs.roll, rhs.roll);
}

inline bool equivalent(const pistoris::ArxColor3& lhs, const pistoris::ArxColor3& rhs) {
  return equivalent(lhs.r, rhs.r) && equivalent(lhs.g, rhs.g) && equivalent(lhs.b, rhs.b);
}

inline bool equivalent(const pistoris::amb::Setting& lhs, const pistoris::amb::Setting& rhs) {
  return equivalent(lhs.min, rhs.min) && equivalent(lhs.max, rhs.max) && lhs.interval_ms == rhs.interval_ms &&
         (lhs.flags & kAmbSettingFlagMask) == (rhs.flags & kAmbSettingFlagMask);
}

inline void checkEquivalent(const pistoris::Amb& lhs, const pistoris::Amb& rhs) {
  CHECK(lhs.tracks.size() == rhs.tracks.size());
  for (std::size_t track_index = 0; track_index < std::min(lhs.tracks.size(), rhs.tracks.size()); ++track_index) {
    CAPTURE(track_index);
    const pistoris::amb::Track& lhs_track = lhs.tracks[track_index];
    const pistoris::amb::Track& rhs_track = rhs.tracks[track_index];
    CHECK(lhs_track.sample_path == rhs_track.sample_path);
    CHECK((lhs_track.flags & kAmbTrackFlagMask) == (rhs_track.flags & kAmbTrackFlagMask));
    CHECK(lhs_track.keys.size() == rhs_track.keys.size());

    for (std::size_t key_index = 0; key_index < std::min(lhs_track.keys.size(), rhs_track.keys.size()); ++key_index) {
      CAPTURE(key_index);
      const pistoris::amb::Key& lhs_key = lhs_track.keys[key_index];
      const pistoris::amb::Key& rhs_key = rhs_track.keys[key_index];
      CHECK(lhs_key.start_ms == rhs_key.start_ms);
      CHECK(lhs_key.loop_minus_one == rhs_key.loop_minus_one);
      CHECK(lhs_key.delay_min_ms == rhs_key.delay_min_ms);
      CHECK(lhs_key.delay_max_ms == rhs_key.delay_max_ms);
      CHECK(equivalent(lhs_key.volume, rhs_key.volume));
      CHECK(equivalent(lhs_key.pitch, rhs_key.pitch));
      CHECK(equivalent(lhs_key.pan, rhs_key.pan));
      CHECK(equivalent(lhs_key.x, rhs_key.x));
      CHECK(equivalent(lhs_key.y, rhs_key.y));
      CHECK(equivalent(lhs_key.z, rhs_key.z));
    }
  }
}

struct CinEquivalenceOptions {
  float comparison_epsilon = 0.0f;
};

inline void checkEquivalent(const pistoris::Cin& lhs, const pistoris::Cin& rhs,
                            const CinEquivalenceOptions& options = {}) {
  CHECK(lhs.bitmaps.size() == rhs.bitmaps.size());
  CHECK(lhs.sounds.size() == rhs.sounds.size());
  CHECK(lhs.end_frame == rhs.end_frame);
  CHECK(equivalence::floatEquivalent(lhs.fps, rhs.fps, options.comparison_epsilon));
  CHECK(lhs.keyframes.size() == rhs.keyframes.size());

  for (std::size_t index = 0; index < std::min(lhs.bitmaps.size(), rhs.bitmaps.size()); ++index) {
    CAPTURE(index);
    CHECK(lhs.bitmaps[index].subdivision_scale == rhs.bitmaps[index].subdivision_scale);
    CHECK(lhs.bitmaps[index].path == rhs.bitmaps[index].path);
  }
  for (std::size_t index = 0; index < std::min(lhs.sounds.size(), rhs.sounds.size()); ++index) {
    CAPTURE(index);
    CHECK(lhs.sounds[index].path == rhs.sounds[index].path);
    CHECK(lhs.sounds[index].speech == rhs.sounds[index].speech);
  }
  for (std::size_t index = 0; index < std::min(lhs.keyframes.size(), rhs.keyframes.size()); ++index) {
    CAPTURE(index);
    const pistoris::cin::Keyframe& left = lhs.keyframes[index];
    const pistoris::cin::Keyframe& right = rhs.keyframes[index];
    CHECK(left.frame == right.frame);
    CHECK(left.bitmap == right.bitmap);
    CHECK(left.effects == right.effects);
    CHECK(left.interpolation == right.interpolation);
    CHECK(left.crossfade == right.crossfade);
    CHECK(equivalence::vectorEquivalent(left.camera_position, right.camera_position, options.comparison_epsilon));
    CHECK(equivalence::floatEquivalent(left.camera_roll, right.camera_roll, options.comparison_epsilon));
    CHECK(left.color == right.color);
    CHECK(left.secondary_color == right.secondary_color);
    CHECK(left.flash_color == right.flash_color);
    CHECK(equivalence::floatEquivalent(left.flash_decay, right.flash_decay, options.comparison_epsilon));
    CHECK(equivalence::vectorEquivalent(left.light.position, right.light.position, options.comparison_epsilon));
    CHECK(equivalence::floatEquivalent(left.light.fall_in, right.light.fall_in, options.comparison_epsilon));
    CHECK(equivalence::floatEquivalent(left.light.fall_out, right.light.fall_out, options.comparison_epsilon));
    CHECK(equivalence::colorEquivalent(left.light.color, right.light.color, options.comparison_epsilon));
    CHECK(equivalence::floatEquivalent(left.light.intensity, right.light.intensity, options.comparison_epsilon));
    CHECK(equivalence::floatEquivalent(
        left.light.random_intensity, right.light.random_intensity, options.comparison_epsilon));
    CHECK(equivalence::vectorEquivalent(left.bitmap_position, right.bitmap_position, options.comparison_epsilon));
    CHECK(equivalence::floatEquivalent(left.bitmap_roll, right.bitmap_roll, options.comparison_epsilon));
    CHECK(equivalence::floatEquivalent(left.outgoing_speed, right.outgoing_speed, options.comparison_epsilon));
    CHECK(left.sound == right.sound);
  }
}

inline void checkEquivalent(const pistoris::Llf& lhs, const pistoris::Llf& rhs) {
  CHECK(equivalent(lhs.version, rhs.version));
  CHECK(lhs.lights.size() == rhs.lights.size());
  CHECK(lhs.colors.size() == rhs.colors.size());

  for (std::size_t light_index = 0; light_index < std::min(lhs.lights.size(), rhs.lights.size()); ++light_index) {
    CAPTURE(light_index);
    const pistoris::llf::Light& lhs_light = lhs.lights[light_index];
    const pistoris::llf::Light& rhs_light = rhs.lights[light_index];
    CHECK(equivalent(lhs_light.position, rhs_light.position));
    CHECK(equivalent(lhs_light.color, rhs_light.color));
    CHECK(equivalent(lhs_light.fallstart, rhs_light.fallstart));
    CHECK(equivalent(lhs_light.fallend, rhs_light.fallend));
    CHECK(equivalent(lhs_light.intensity, rhs_light.intensity));
    CHECK(equivalent(lhs_light.flicker, rhs_light.flicker));
    CHECK(equivalent(lhs_light.effect_radius, rhs_light.effect_radius));
    CHECK(equivalent(lhs_light.effect_frequency, rhs_light.effect_frequency));
    CHECK(equivalent(lhs_light.effect_size, rhs_light.effect_size));
    CHECK(equivalent(lhs_light.effect_speed, rhs_light.effect_speed));
    CHECK(equivalent(lhs_light.flare_size, rhs_light.flare_size));
    CHECK(lhs_light.flags == rhs_light.flags);
  }

  for (std::size_t color_index = 0; color_index < std::min(lhs.colors.size(), rhs.colors.size()); ++color_index) {
    CAPTURE(color_index);
    CHECK(equivalent(lhs.colors[color_index], rhs.colors[color_index]));
  }
}

inline void checkEquivalent(const pistoris::Dlf& lhs, const pistoris::Dlf& rhs) {
  CHECK(equivalent(lhs.version, rhs.version));
  CHECK(equivalent(lhs.player_spawn.position, rhs.player_spawn.position));
  CHECK(equivalent(lhs.player_spawn.angle, rhs.player_spawn.angle));
  CHECK(nativeStringView(lhs.scene_path).compare(nativeStringView(rhs.scene_path)) == 0);
  CHECK(lhs.entities.size() == rhs.entities.size());
  CHECK(lhs.fogs.size() == rhs.fogs.size());
  CHECK(lhs.zones.size() == rhs.zones.size());
  CHECK(lhs.paths.size() == rhs.paths.size());

  for (std::size_t entity_index = 0; entity_index < std::min(lhs.entities.size(), rhs.entities.size());
       ++entity_index) {
    CAPTURE(entity_index);
    const pistoris::dlf::Entity& lhs_entity = lhs.entities[entity_index];
    const pistoris::dlf::Entity& rhs_entity = rhs.entities[entity_index];
    CHECK(nativeStringView(lhs_entity.class_path).compare(nativeStringView(rhs_entity.class_path)) == 0);
    CHECK(lhs_entity.ident == rhs_entity.ident);
    CHECK(equivalent(lhs_entity.position, rhs_entity.position));
    CHECK(equivalent(lhs_entity.angle, rhs_entity.angle));
  }

  for (std::size_t fog_index = 0; fog_index < std::min(lhs.fogs.size(), rhs.fogs.size()); ++fog_index) {
    CAPTURE(fog_index);
    const pistoris::dlf::Fog& lhs_fog = lhs.fogs[fog_index];
    const pistoris::dlf::Fog& rhs_fog = rhs.fogs[fog_index];
    CHECK(equivalent(lhs_fog.position, rhs_fog.position));
    CHECK(equivalent(lhs_fog.color, rhs_fog.color));
    CHECK(equivalent(lhs_fog.size, rhs_fog.size));
    CHECK(lhs_fog.directional == rhs_fog.directional);
    CHECK(equivalent(lhs_fog.scale, rhs_fog.scale));
    CHECK(equivalent(lhs_fog.angle, rhs_fog.angle));
    CHECK(equivalent(lhs_fog.speed, rhs_fog.speed));
    CHECK(equivalent(lhs_fog.rotate_speed, rhs_fog.rotate_speed));
    CHECK(lhs_fog.lifetime_ms == rhs_fog.lifetime_ms);
    CHECK(equivalent(lhs_fog.frequency, rhs_fog.frequency));
  }

  for (std::size_t zone_index = 0; zone_index < std::min(lhs.zones.size(), rhs.zones.size()); ++zone_index) {
    CAPTURE(zone_index);
    const pistoris::dlf::Zone& lhs_zone = lhs.zones[zone_index];
    const pistoris::dlf::Zone& rhs_zone = rhs.zones[zone_index];
    CHECK(nativeStringView(lhs_zone.name).compare(nativeStringView(rhs_zone.name)) == 0);
    CHECK(equivalent(lhs_zone.position, rhs_zone.position));
    CHECK(lhs_zone.height == rhs_zone.height);
    const auto& lhs_color = lhs_zone.color;
    const auto& rhs_color = rhs_zone.color;
    CHECK(lhs_color.has_value() == rhs_color.has_value());
    if (lhs_color && rhs_color) CHECK(equivalent(*lhs_color, *rhs_color));
    const auto& lhs_farclip = lhs_zone.farclip;
    const auto& rhs_farclip = rhs_zone.farclip;
    CHECK(lhs_farclip.has_value() == rhs_farclip.has_value());
    if (lhs_farclip && rhs_farclip) CHECK(equivalent(*lhs_farclip, *rhs_farclip));
    const auto& lhs_ambiance = lhs_zone.ambiance;
    const auto& rhs_ambiance = rhs_zone.ambiance;
    CHECK(lhs_ambiance.has_value() == rhs_ambiance.has_value());
    if (lhs_ambiance && rhs_ambiance) {
      CHECK(nativeStringView(lhs_ambiance->name).compare(nativeStringView(rhs_ambiance->name)) == 0);
      CHECK(equivalent(lhs_ambiance->volume, rhs_ambiance->volume));
    }

    CHECK(lhs_zone.points.size() == rhs_zone.points.size());
    for (std::size_t point_index = 0; point_index < std::min(lhs_zone.points.size(), rhs_zone.points.size());
         ++point_index) {
      CAPTURE(point_index);
      CHECK(equivalent(lhs_zone.points[point_index], rhs_zone.points[point_index]));
    }
  }

  for (std::size_t path_index = 0; path_index < std::min(lhs.paths.size(), rhs.paths.size()); ++path_index) {
    CAPTURE(path_index);
    const pistoris::dlf::Path& lhs_path = lhs.paths[path_index];
    const pistoris::dlf::Path& rhs_path = rhs.paths[path_index];
    CHECK(nativeStringView(lhs_path.name).compare(nativeStringView(rhs_path.name)) == 0);
    CHECK(equivalent(lhs_path.position, rhs_path.position));
    CHECK(lhs_path.nodes.size() == rhs_path.nodes.size());

    for (std::size_t node_index = 0; node_index < std::min(lhs_path.nodes.size(), rhs_path.nodes.size());
         ++node_index) {
      CAPTURE(node_index);
      const pistoris::dlf::PathNode& lhs_node = lhs_path.nodes[node_index];
      const pistoris::dlf::PathNode& rhs_node = rhs_path.nodes[node_index];
      CHECK(equivalent(lhs_node.relative_position, rhs_node.relative_position));
      CHECK(lhs_node.type == rhs_node.type);
      CHECK(lhs_node.time_ms == rhs_node.time_ms);
    }
  }
}

template <class T, std::size_t N>
inline void checkEquivalentArray(const T (&lhs)[N], const T (&rhs)[N]) {
  CHECK(std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs), std::end(rhs)));
}

inline void checkEquivalent(const pistoris::Ftl& lhs, const pistoris::Ftl& rhs) {
  CHECK(lhs.header.origin == rhs.header.origin);
  checkEquivalentArray(lhs.header.name, rhs.header.name);
  CHECK(lhs.vertices.size() == rhs.vertices.size());
  for (std::size_t index = 0; index < std::min(lhs.vertices.size(), rhs.vertices.size()); ++index) {
    CAPTURE(index);
    CHECK(equivalent(lhs.vertices[index].position, rhs.vertices[index].position));
    CHECK(equivalent(lhs.vertices[index].normal, rhs.vertices[index].normal));
  }
  CHECK(lhs.faces.size() == rhs.faces.size());
  for (std::size_t index = 0; index < std::min(lhs.faces.size(), rhs.faces.size()); ++index) {
    CAPTURE(index);
    const pistoris::ftl::Face& left = lhs.faces[index];
    const pistoris::ftl::Face& right = rhs.faces[index];
    CHECK(left.type == right.type);
    CHECK(left.vertex_idx.x == right.vertex_idx.x);
    CHECK(left.vertex_idx.y == right.vertex_idx.y);
    CHECK(left.vertex_idx.z == right.vertex_idx.z);
    CHECK(left.texture_id == right.texture_id);
    CHECK(equivalent(left.u, right.u));
    CHECK(equivalent(left.v, right.v));
    CHECK(equivalent(left.transval, right.transval));
    CHECK(equivalent(left.norm, right.norm));
  }
  CHECK(lhs.texture_containers.size() == rhs.texture_containers.size());
  for (std::size_t index = 0; index < std::min(lhs.texture_containers.size(), rhs.texture_containers.size()); ++index)
    checkEquivalentArray(lhs.texture_containers[index].filename, rhs.texture_containers[index].filename);
  CHECK(lhs.groups.size() == rhs.groups.size());
  for (std::size_t index = 0; index < std::min(lhs.groups.size(), rhs.groups.size()); ++index) {
    const pistoris::ftl::Group& left = lhs.groups[index];
    const pistoris::ftl::Group& right = rhs.groups[index];
    checkEquivalentArray(left.name, right.name);
    CHECK(left.origin == right.origin);
    CHECK(left.indices == right.indices);
    CHECK(equivalent(left.blob_shadow_size, right.blob_shadow_size));
  }
  CHECK(lhs.actions.size() == rhs.actions.size());
  for (std::size_t index = 0; index < std::min(lhs.actions.size(), rhs.actions.size()); ++index) {
    const pistoris::ftl::Action& left = lhs.actions[index];
    const pistoris::ftl::Action& right = rhs.actions[index];
    checkEquivalentArray(left.name, right.name);
    CHECK(left.vertex_idx == right.vertex_idx);
    CHECK(left.action == right.action);
    CHECK(left.sfx == right.sfx);
  }
  CHECK(lhs.selections.size() == rhs.selections.size());
  for (std::size_t index = 0; index < std::min(lhs.selections.size(), rhs.selections.size()); ++index) {
    checkEquivalentArray(lhs.selections[index].name, rhs.selections[index].name);
    CHECK(lhs.selections[index].selected == rhs.selections[index].selected);
  }
}

struct TeaEquivalenceOptions {
  float comparison_epsilon = 0.0f;
};

inline void checkEquivalent(const pistoris::Tea& lhs, const pistoris::Tea& rhs, TeaEquivalenceOptions options = {}) {
  const auto float_equivalent = [epsilon = options.comparison_epsilon](float left, float right) {
    return epsilon == 0.0f ? equivalent(left, right) : equivalence::floatEquivalent(left, right, epsilon);
  };
  const auto vector_equivalent = [&](const pistoris::ArxVector3& left, const pistoris::ArxVector3& right) {
    return float_equivalent(left.x, right.x) && float_equivalent(left.y, right.y) && float_equivalent(left.z, right.z);
  };

  CHECK(lhs.num_frames == rhs.num_frames);
  CHECK(lhs.num_groups == rhs.num_groups);
  checkEquivalentArray(lhs.name, rhs.name);
  CHECK(lhs.keyframes.size() == rhs.keyframes.size());
  for (std::size_t keyframe = 0; keyframe < std::min(lhs.keyframes.size(), rhs.keyframes.size()); ++keyframe) {
    CAPTURE(keyframe);
    const pistoris::tea::Keyframe& left = lhs.keyframes[keyframe];
    const pistoris::tea::Keyframe& right = rhs.keyframes[keyframe];
    CHECK(left.num_frame == right.num_frame);
    CHECK(left.flag_frame == right.flag_frame);
    CHECK(left.translate.has_value() == right.translate.has_value());
    if (left.translate && right.translate) CHECK(vector_equivalent(*left.translate, *right.translate));
    CHECK(left.quat.has_value() == right.quat.has_value());
    if (left.quat && right.quat) {
      CHECK(float_equivalent(left.quat->w, right.quat->w));
      CHECK(float_equivalent(left.quat->x, right.quat->x));
      CHECK(float_equivalent(left.quat->y, right.quat->y));
      CHECK(float_equivalent(left.quat->z, right.quat->z));
    }
    CHECK(left.groups.size() == right.groups.size());
    for (std::size_t group = 0; group < std::min(left.groups.size(), right.groups.size()); ++group) {
      CAPTURE(group);
      const pistoris::tea::GroupAnim& left_group = left.groups[group];
      const pistoris::tea::GroupAnim& right_group = right.groups[group];
      CHECK(left_group.key_group == right_group.key_group);
      CHECK(float_equivalent(left_group.quat.w, right_group.quat.w));
      CHECK(float_equivalent(left_group.quat.x, right_group.quat.x));
      CHECK(float_equivalent(left_group.quat.y, right_group.quat.y));
      CHECK(float_equivalent(left_group.quat.z, right_group.quat.z));
      CHECK(vector_equivalent(left_group.translate, right_group.translate));
      CHECK(vector_equivalent(left_group.zoom, right_group.zoom));
    }
    CHECK(left.sample.has_value() == right.sample.has_value());
    if (left.sample && right.sample) checkEquivalentArray(left.sample->name, right.sample->name);
  }
}

inline void checkEquivalent(const pistoris::Fts& lhs, const pistoris::Fts& rhs) {
  std::vector<std::uint8_t> lhs_bytes;
  std::vector<std::uint8_t> rhs_bytes;
  const ArxReturnCode lhs_status = pistoris::writeFts(lhs, lhs_bytes);
  const ArxReturnCode rhs_status = pistoris::writeFts(rhs, rhs_bytes);
  CHECK(lhs_status == ARX_OK);
  CHECK(rhs_status == ARX_OK);
  if (lhs_status != ARX_OK || rhs_status != ARX_OK) return;
  CHECK(lhs_bytes == rhs_bytes);
}

}  // namespace test_support
