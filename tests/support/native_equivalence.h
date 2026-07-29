// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace test_support {

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

inline void checkEquivalent(const pistoris::Llf& lhs, const pistoris::Llf& rhs) {
  CHECK(equivalent(lhs.version, rhs.version));
  REQUIRE(lhs.lights.size() == rhs.lights.size());
  REQUIRE(lhs.colors.size() == rhs.colors.size());

  for (std::size_t light_index = 0; light_index < lhs.lights.size(); ++light_index) {
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

  for (std::size_t color_index = 0; color_index < lhs.colors.size(); ++color_index) {
    CAPTURE(color_index);
    CHECK(equivalent(lhs.colors[color_index], rhs.colors[color_index]));
  }
}

inline void checkEquivalent(const pistoris::Dlf& lhs, const pistoris::Dlf& rhs) {
  CHECK(equivalent(lhs.version, rhs.version));
  CHECK(equivalent(lhs.player_spawn.position, rhs.player_spawn.position));
  CHECK(equivalent(lhs.player_spawn.angle, rhs.player_spawn.angle));
  CHECK(lhs.scene_path == rhs.scene_path);
  REQUIRE(lhs.entities.size() == rhs.entities.size());
  REQUIRE(lhs.fogs.size() == rhs.fogs.size());
  REQUIRE(lhs.zones.size() == rhs.zones.size());
  REQUIRE(lhs.paths.size() == rhs.paths.size());

  for (std::size_t entity_index = 0; entity_index < lhs.entities.size(); ++entity_index) {
    CAPTURE(entity_index);
    const pistoris::dlf::Entity& lhs_entity = lhs.entities[entity_index];
    const pistoris::dlf::Entity& rhs_entity = rhs.entities[entity_index];
    CHECK(lhs_entity.class_path == rhs_entity.class_path);
    CHECK(lhs_entity.ident == rhs_entity.ident);
    CHECK(equivalent(lhs_entity.position, rhs_entity.position));
    CHECK(equivalent(lhs_entity.angle, rhs_entity.angle));
  }

  for (std::size_t fog_index = 0; fog_index < lhs.fogs.size(); ++fog_index) {
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

  for (std::size_t zone_index = 0; zone_index < lhs.zones.size(); ++zone_index) {
    CAPTURE(zone_index);
    const pistoris::dlf::Zone& lhs_zone = lhs.zones[zone_index];
    const pistoris::dlf::Zone& rhs_zone = rhs.zones[zone_index];
    CHECK(lhs_zone.name == rhs_zone.name);
    CHECK(equivalent(lhs_zone.position, rhs_zone.position));
    CHECK(lhs_zone.height == rhs_zone.height);
    const auto& lhs_color = lhs_zone.color;
    const auto& rhs_color = rhs_zone.color;
    REQUIRE(lhs_color.has_value() == rhs_color.has_value());
    if (lhs_color) CHECK(equivalent(*lhs_color, *rhs_color));
    const auto& lhs_farclip = lhs_zone.farclip;
    const auto& rhs_farclip = rhs_zone.farclip;
    REQUIRE(lhs_farclip.has_value() == rhs_farclip.has_value());
    if (lhs_farclip) CHECK(equivalent(*lhs_farclip, *rhs_farclip));
    const auto& lhs_ambiance = lhs_zone.ambiance;
    const auto& rhs_ambiance = rhs_zone.ambiance;
    REQUIRE(lhs_ambiance.has_value() == rhs_ambiance.has_value());
    if (lhs_ambiance) {
      CHECK(lhs_ambiance->name == rhs_ambiance->name);
      CHECK(equivalent(lhs_ambiance->volume, rhs_ambiance->volume));
    }

    REQUIRE(lhs_zone.points.size() == rhs_zone.points.size());
    for (std::size_t point_index = 0; point_index < lhs_zone.points.size(); ++point_index) {
      CAPTURE(point_index);
      CHECK(equivalent(lhs_zone.points[point_index], rhs_zone.points[point_index]));
    }
  }

  for (std::size_t path_index = 0; path_index < lhs.paths.size(); ++path_index) {
    CAPTURE(path_index);
    const pistoris::dlf::Path& lhs_path = lhs.paths[path_index];
    const pistoris::dlf::Path& rhs_path = rhs.paths[path_index];
    CHECK(lhs_path.name == rhs_path.name);
    CHECK(equivalent(lhs_path.position, rhs_path.position));
    REQUIRE(lhs_path.nodes.size() == rhs_path.nodes.size());

    for (std::size_t node_index = 0; node_index < lhs_path.nodes.size(); ++node_index) {
      CAPTURE(node_index);
      const pistoris::dlf::PathNode& lhs_node = lhs_path.nodes[node_index];
      const pistoris::dlf::PathNode& rhs_node = rhs_path.nodes[node_index];
      CHECK(equivalent(lhs_node.relative_position, rhs_node.relative_position));
      CHECK(lhs_node.type == rhs_node.type);
      CHECK(lhs_node.time_ms == rhs_node.time_ms);
    }
  }
}

}  // namespace test_support
