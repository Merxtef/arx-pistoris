// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "palette.h"

#include "external/glb/writer.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace pistoris::glb_level {
namespace {

struct PaletteEntry {
  std::string_view name;
  std::array<float, 4> color;
  bool double_sided = false;
};

constexpr std::array<PaletteEntry, static_cast<std::size_t>(PaletteItem::kCount)> kPalette = {{
    {"arx_portal", {0.10f, 0.45f, 1.00f, 0.35f}, true},
    {"arx_zone", {0.65f, 0.20f, 0.90f, 0.25f}, true},
    {"arx_nav_surface", {0.00f, 0.65f, 0.85f, 0.40f}, true},
    {"arx_navigation_support", {0.00f, 0.28f, 0.08f, 1.00f}, true},
    {"arx_anchor", {0.00f, 0.85f, 0.20f, 1.00f}},
    {"arx_anchor_connection", {1.00f, 0.80f, 0.00f, 1.00f}, true},
    {"arx_level_debug_geometry_context", {0.28f, 0.28f, 0.28f, 1.00f}, true},
    {"arx_level_debug_geometry", {0.65f, 0.65f, 0.65f, 1.00f}, true},
    {"arx_room_distance_debug_portal_centroid", {0.85f, 0.25f, 1.00f, 1.00f}},
    {"arx_room_distance_debug_stored_distance", {0.95f, 0.65f, 0.05f, 1.00f}, true},
    {"arx_navigation_debug_surface_generated", {0.15f, 0.45f, 0.95f, 1.00f}, true},
    {"arx_navigation_debug_surface_repaired", {0.00f, 0.75f, 0.95f, 1.00f}, true},
    {"arx_navigation_debug_surface_pruned", {0.95f, 0.20f, 0.15f, 0.45f}, true},
    {"arx_navigation_debug_anchor_repaired", {0.05f, 0.45f, 1.00f, 1.00f}},
    {"arx_navigation_debug_anchor_rejected", {1.00f, 0.15f, 0.05f, 1.00f}},
    {"arx_navigation_debug_anchor_repair_segment", {0.10f, 0.70f, 1.00f, 1.00f}, true},
    {"arx_navigation_debug_anchor_pruned", {0.95f, 0.20f, 0.15f, 0.80f}},
    {"arx_navigation_debug_invalid", {1.00f, 0.00f, 0.20f, 1.00f}, true},
    {"arx_navigation_debug_no_support", {1.00f, 0.45f, 0.00f, 1.00f}, true},
    {"arx_navigation_debug_too_far", {1.00f, 0.90f, 0.00f, 1.00f}, true},
    {"arx_navigation_debug_unresolved", {0.95f, 0.00f, 0.95f, 1.00f}, true},
    {"arx_navigation_debug_resolved", {0.00f, 0.70f, 1.00f, 1.00f}, true},
    {"arx_navigation_debug_resolution", {0.00f, 0.95f, 1.00f, 1.00f}, true},
    {"arx_navigation_debug_max_steps", {0.95f, 0.35f, 0.00f, 1.00f}, true},
    {"arx_navigation_debug_end_mismatch", {0.85f, 0.00f, 0.00f, 1.00f}, true},
    {"arx_navigation_debug_requested", {1.00f, 1.00f, 1.00f, 1.00f}},
    {"arx_room_distance_debug_portal_access_point", {0.00f, 0.85f, 1.00f, 1.00f}},
    {"arx_room_distance_debug_sampled_point", {0.10f, 0.90f, 0.25f, 1.00f}},
    {"arx_room_distance_debug_portal_access_segment", {0.00f, 0.65f, 1.00f, 1.00f}, true},
    {"arx_room_distance_debug_visibility_edge", {0.15f, 0.90f, 0.90f, 1.00f}, true},
    {"arx_room_distance_debug_in_room_portal_path", {0.95f, 0.25f, 1.00f, 1.00f}, true},
    {"arx_room_distance_debug_room_pair_path", {1.00f, 0.35f, 0.05f, 1.00f}, true},
}};

constexpr std::array<std::array<float, 4>, 8> kRoomColors = {{
    {0.70f, 0.70f, 0.70f, 1.0f},
    {0.82f, 0.42f, 0.42f, 1.0f},
    {0.42f, 0.64f, 0.86f, 1.0f},
    {0.52f, 0.72f, 0.45f, 1.0f},
    {0.82f, 0.68f, 0.36f, 1.0f},
    {0.62f, 0.50f, 0.80f, 1.0f},
    {0.38f, 0.76f, 0.72f, 1.0f},
    {0.86f, 0.55f, 0.35f, 1.0f},
}};

}  // namespace

bool isReservedPaletteStem(std::string_view stem) {
  return stem == "arx_portal" || stem == "arx_zone" || stem == "arx_nav_surface";
}

Palette::Palette(glb::Builder& builder) : builder_(builder) { materials_.fill(-1); }

int Palette::material(PaletteItem item) {
  const std::size_t index = static_cast<std::size_t>(item);
  assert(index < kPalette.size());
  if (materials_[index] >= 0) return materials_[index];
  const PaletteEntry& entry = kPalette[index];
  materials_[index] = builder_.addColorMaterial(std::string(entry.name), entry.color, entry.double_sided);
  return materials_[index];
}

int Palette::roomMaterial(std::size_t room) {
  if (room_materials_.size() <= room) room_materials_.resize(room + 1U, -1);
  if (room_materials_[room] < 0) {
    room_materials_[room] = builder_.addColorMaterial(
        std::format("arx_fts_debug_room_{}", room), kRoomColors[room % kRoomColors.size()], true);
  }
  return room_materials_[room];
}

}  // namespace pistoris::glb_level
