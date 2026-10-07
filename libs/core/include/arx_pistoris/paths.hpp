// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pistoris::paths {

enum class ModelPathType : std::uint8_t {
  kNone = ARX_MODEL_PATH_TYPE_NONE,
  kNpc = ARX_MODEL_PATH_TYPE_NPC,
  kFixInter = ARX_MODEL_PATH_TYPE_FIX_INTER,
  kSystem = ARX_MODEL_PATH_TYPE_SYSTEM,
  kArmor = ARX_MODEL_PATH_TYPE_ARMOR,
  kJewelry = ARX_MODEL_PATH_TYPE_JEWELRY,
  kMagic = ARX_MODEL_PATH_TYPE_MAGIC,
  kMovable = ARX_MODEL_PATH_TYPE_MOVABLE,
  kProvisions = ARX_MODEL_PATH_TYPE_PROVISIONS,
  kQuestItem = ARX_MODEL_PATH_TYPE_QUEST_ITEM,
  kSpecial = ARX_MODEL_PATH_TYPE_SPECIAL,
  kWeapons = ARX_MODEL_PATH_TYPE_WEAPONS,
  kUiRunes = ARX_MODEL_PATH_TYPE_UI_RUNES,
  kUiMenus = ARX_MODEL_PATH_TYPE_UI_MENUS,
  kEditor = ARX_MODEL_PATH_TYPE_EDITOR,
};

enum class AnimationPathType : std::uint8_t {
  kNone = ARX_ANIMATION_PATH_TYPE_NONE,
  kNpc = ARX_ANIMATION_PATH_TYPE_NPC,
  kFixInter = ARX_ANIMATION_PATH_TYPE_FIX_INTER,
};

// String fields returned by inverse helpers borrow from the input path; copy any component that must outlive it
struct ModelPathView {
  ModelPathType type = ModelPathType::kNone;
  std::string_view name = {};
  std::string_view tweak = {};
};

struct AnimationPathView {
  AnimationPathType type = AnimationPathType::kNone;
  std::string_view name = {};
};

struct CinematicPathView {
  std::string_view name = {};
};

struct AmbiancePathView {
  std::string_view name = {};
};

struct ResourceSearchLocation {
  std::string_view base_path = {};
  std::uint32_t max_discovery_depth = 0;
};

// Owning semantic identity for a resource selector. Fields not used by kind are empty/default.
struct ResourceSelector {
  ArxResourceKind kind = ARX_RESOURCE_KIND_NONE;
  ModelPathType model_type = ModelPathType::kNone;
  AnimationPathType animation_type = AnimationPathType::kNone;
  std::string name;
  std::string tweak;
  std::uint32_t level = 0;
};

// --- Common ---

[[nodiscard]] ArxResourceKind resourceSelectorKind(std::string_view selector) noexcept;
[[nodiscard]] bool parseResourceSelector(std::string_view selector, ResourceSelector& out);
[[nodiscard]] bool resourceSelector(const ResourceSelector& resource, std::string& out);

// Cross-platform policy for converter-emitted filename components
// Resource identities and host paths unchanged
[[nodiscard]] bool isPortableFilename(std::string_view filename) noexcept;
[[nodiscard]] std::string sanitizePortableFilename(std::string_view filename);

// Cross-platform policy for one component of a logical resource path
[[nodiscard]] bool isPortableResourcePathComponent(std::string_view component) noexcept;

[[nodiscard]] std::string_view textureDirectory() noexcept;
[[nodiscard]] std::string_view soundDirectory() noexcept;
[[nodiscard]] std::string_view ambianceSoundDirectory() noexcept;

[[nodiscard]] bool normalizeZoneAmbiance(std::string_view path, std::string& out);
[[nodiscard]] bool ambFromZoneAmbiance(std::string_view ambiance, std::string& out);

// --- Level ---

[[nodiscard]] std::string levelDlf(std::uint32_t level);
[[nodiscard]] std::string levelLlf(std::uint32_t level);
[[nodiscard]] std::string levelFts(std::uint32_t level);
[[nodiscard]] std::uint32_t minimapResourceLevel(std::uint32_t level) noexcept;
[[nodiscard]] std::string levelMinimap(std::uint32_t level);
[[nodiscard]] std::string levelLoadingScreen(std::uint32_t level);
[[nodiscard]] std::string_view minimapOffsetsFile() noexcept;
[[nodiscard]] bool levelFromDlf(std::string_view path, std::uint32_t& level) noexcept;
[[nodiscard]] bool levelFromLlf(std::string_view path, std::uint32_t& level) noexcept;
[[nodiscard]] bool levelFromFts(std::string_view path, std::uint32_t& level) noexcept;
[[nodiscard]] bool dlfSceneFromLevelName(std::string_view name, std::string& out);
[[nodiscard]] std::string levelSelector(std::uint32_t level);
[[nodiscard]] bool levelFromSelector(std::string_view selector, std::uint32_t& level) noexcept;
[[nodiscard]] bool ftsFromDlfScene(std::string_view scene_path, std::string& out);

// --- Model ---

[[nodiscard]] std::span<const ModelPathType> modelPathTypes() noexcept;
[[nodiscard]] std::string_view modelPathTypeName(ModelPathType type) noexcept;
[[nodiscard]] bool modelPathTypeFromName(std::string_view name, ModelPathType& out) noexcept;
[[nodiscard]] bool modelFtl(ModelPathView model, std::string& out);
[[nodiscard]] bool modelFromFtl(std::string_view path, ModelPathView& out) noexcept;
[[nodiscard]] bool entityClassFromFtl(std::string_view path, std::string& out);
[[nodiscard]] bool ftlFromEntityClass(std::string_view path, std::string& out);
[[nodiscard]] bool entityClassKind(std::string_view path, ArxEntityClassKind& out);
[[nodiscard]] bool itemIconFromEntityClass(std::string_view path, std::string& out);
[[nodiscard]] bool entityClassFromModel(ModelPathView model, std::string& path);
[[nodiscard]] bool baseEntityClassFromModel(ModelPathView model, std::string& path);
[[nodiscard]] bool modelFromEntityClass(std::string_view path, ModelPathView& out) noexcept;
[[nodiscard]] bool modelSelector(ModelPathView model, std::string& out);
[[nodiscard]] bool modelFromSelector(std::string_view selector, ModelPathView& out) noexcept;

// --- Animation ---

[[nodiscard]] std::span<const AnimationPathType> animationPathTypes() noexcept;
[[nodiscard]] std::string_view animationPathTypeName(AnimationPathType type) noexcept;
[[nodiscard]] bool animationPathTypeFromName(std::string_view name, AnimationPathType& out) noexcept;
// Non-NPC interactive model types map to the fix_inter animation directory
[[nodiscard]] bool animationDirectory(ModelPathType type, std::string& out);
[[nodiscard]] bool animationDirectory(AnimationPathType type, std::string& out);
[[nodiscard]] bool animationTea(AnimationPathView animation, std::string& out);
[[nodiscard]] bool animationFromTea(std::string_view path, AnimationPathView& out) noexcept;
[[nodiscard]] bool animationSelector(AnimationPathView animation, std::string& out);
[[nodiscard]] bool animationFromSelector(std::string_view selector, AnimationPathView& out) noexcept;

// --- Cinematic ---

[[nodiscard]] std::string_view cinematicIllustrationDirectory() noexcept;
[[nodiscard]] bool cinematicCin(CinematicPathView cinematic, std::string& out);
[[nodiscard]] bool cinematicFromCin(std::string_view path, CinematicPathView& out) noexcept;
[[nodiscard]] bool cinematicSelector(CinematicPathView cinematic, std::string& out);
[[nodiscard]] bool cinematicFromSelector(std::string_view selector, CinematicPathView& out) noexcept;

// --- Ambiance ---

[[nodiscard]] bool ambianceAmb(AmbiancePathView ambiance, std::string& out);
[[nodiscard]] bool ambianceFromAmb(std::string_view path, AmbiancePathView& out) noexcept;
[[nodiscard]] bool ambianceSelector(AmbiancePathView ambiance, std::string& out);
[[nodiscard]] bool ambianceFromSelector(std::string_view selector, AmbiancePathView& out) noexcept;

// --- Discovery ---

[[nodiscard]] bool modelSearchLocation(ModelPathType type, ResourceSearchLocation& out) noexcept;
[[nodiscard]] bool animationSearchLocation(AnimationPathType type, ResourceSearchLocation& out) noexcept;
[[nodiscard]] ResourceSearchLocation levelSearchLocation() noexcept;
[[nodiscard]] ResourceSearchLocation cinematicSearchLocation() noexcept;
[[nodiscard]] ResourceSearchLocation ambianceSearchLocation() noexcept;

}  // namespace pistoris::paths
