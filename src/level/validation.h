// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "level/data.h"

namespace pistoris {

namespace geometry {
enum class Error : std::uint8_t;
enum class ImageError : std::uint8_t;
}  // namespace geometry
namespace rooms {
enum class Error : std::uint8_t;
}
namespace navigation {
enum class Error : std::uint8_t;
}
namespace lights {
enum class Error : std::uint8_t;
}
namespace scene {
enum class Error : std::uint8_t;
}

constexpr LevelValidation operator|(LevelValidation lhs, LevelValidation rhs) noexcept {
  return static_cast<LevelValidation>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr LevelValidation operator&(LevelValidation lhs, LevelValidation rhs) noexcept {
  return static_cast<LevelValidation>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr LevelValidation operator~(LevelValidation value) noexcept {
  return static_cast<LevelValidation>(~static_cast<std::uint32_t>(value));
}

constexpr LevelValidation& operator|=(LevelValidation& lhs, LevelValidation rhs) noexcept { return lhs = lhs | rhs; }

constexpr LevelValidation& operator&=(LevelValidation& lhs, LevelValidation rhs) noexcept { return lhs = lhs & rhs; }

namespace level_validation {

inline constexpr std::size_t kMaxRooms = 254;

ArxReturnCode geometryError(geometry::Error error) noexcept;
ArxReturnCode imageError(geometry::ImageError error) noexcept;
ArxReturnCode roomsError(rooms::Error error) noexcept;
ArxReturnCode navigationError(navigation::Error error) noexcept;
ArxReturnCode lightingError(lights::Error error) noexcept;
ArxReturnCode sceneError(scene::Error error) noexcept;
ArxReturnCode faceTypes(std::span<const Face> faces) noexcept;
bool validPortalBounds(const Portal& portal) noexcept;

inline bool has(const LevelValidationState& state, LevelValidation validation) noexcept {
  return (state.valid & validation) == validation;
}

inline void markValid(LevelValidationState& state, LevelValidation validation) noexcept { state.valid |= validation; }

void invalidate(LevelValidationState& state, LevelValidation validation) noexcept;

ArxReturnCode vertices(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode textures(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode faces(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode faceRooms(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode cornerColors(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode rooms(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode portals(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode roomDistances(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode navSurface(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode anchors(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode anchorConnections(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode lightSources(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode playerSpawn(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode entities(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode fogs(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode zones(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode paths(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode mesh(const LevelModules& modules, LevelValidationState& state);
ArxReturnCode all(const LevelModules& modules, LevelValidationState& state);

}  // namespace level_validation
}  // namespace pistoris
