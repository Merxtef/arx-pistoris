// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"

#include "arx_pistoris/paths/types.h"

#include "bindings.h"

#include <cstdint>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::python {
namespace {

enum class ResourceKind : std::uint8_t {
  kNone = ARX_RESOURCE_KIND_NONE,
  kLevel = ARX_RESOURCE_KIND_LEVEL,
  kModel = ARX_RESOURCE_KIND_MODEL,
  kAnimation = ARX_RESOURCE_KIND_ANIMATION,
  kCinematic = ARX_RESOURCE_KIND_CINEMATIC,
  kAmbiance = ARX_RESOURCE_KIND_AMBIANCE,
};

enum class EntityClassKind : std::uint8_t {
  kUnknown = ARX_ENTITY_CLASS_KIND_UNKNOWN,
  kItem = ARX_ENTITY_CLASS_KIND_ITEM,
  kNpc = ARX_ENTITY_CLASS_KIND_NPC,
  kFix = ARX_ENTITY_CLASS_KIND_FIX,
  kCamera = ARX_ENTITY_CLASS_KIND_CAMERA,
  kMarker = ARX_ENTITY_CLASS_KIND_MARKER,
};

struct ModelPath {
  paths::ModelPathType type = paths::ModelPathType::kNone;
  std::string name;
  std::string tweak;

  [[nodiscard]] paths::ModelPathView view() const noexcept { return {type, name, tweak}; }
};

struct AnimationPath {
  paths::AnimationPathType type = paths::AnimationPathType::kNone;
  std::string name;

  [[nodiscard]] paths::AnimationPathView view() const noexcept { return {type, name}; }
};

struct CinematicPath {
  std::string name;

  [[nodiscard]] paths::CinematicPathView view() const noexcept { return {name}; }
};

struct AmbiancePath {
  std::string name;

  [[nodiscard]] paths::AmbiancePathView view() const noexcept { return {name}; }
};

struct SearchLocation {
  std::string base_path;
  std::uint32_t max_discovery_depth = 0;
};

template <class Builder>
std::string buildPath(std::string_view description, Builder&& builder) {
  std::string result;
  if (!builder(result)) throw nb::value_error(("invalid " + std::string(description)).c_str());
  return result;
}

SearchLocation searchLocation(paths::ResourceSearchLocation value) {
  return {std::string(value.base_path), value.max_discovery_depth};
}

template <class Type, class Search>
SearchLocation searchedLocation(Type type, std::string_view description, Search&& search) {
  paths::ResourceSearchLocation result;
  if (!search(type, result)) throw nb::value_error(("invalid " + std::string(description)).c_str());
  return searchLocation(result);
}

ModelPath modelPath(paths::ModelPathView value) {
  return {value.type, std::string(value.name), std::string(value.tweak)};
}

AnimationPath animationPath(paths::AnimationPathView value) { return {value.type, std::string(value.name)}; }

}  // namespace

void bindPaths(nb::module_& module) {
  nb::module_ path_module = module.def_submodule("paths", "Logical Arx resource paths and selectors.");

  auto model_type = nb::enum_<paths::ModelPathType>(path_module, "ModelType", nb::is_str());
  model_type.str_value("NPC", paths::ModelPathType::kNpc, "npc")
      .str_value("FIX_INTER", paths::ModelPathType::kFixInter, "fix_inter")
      .str_value("SYSTEM", paths::ModelPathType::kSystem, "system")
      .str_value("ARMOR", paths::ModelPathType::kArmor, "armor")
      .str_value("JEWELRY", paths::ModelPathType::kJewelry, "jewelry")
      .str_value("MAGIC", paths::ModelPathType::kMagic, "magic")
      .str_value("MOVABLE", paths::ModelPathType::kMovable, "movable")
      .str_value("PROVISIONS", paths::ModelPathType::kProvisions, "provisions")
      .str_value("QUEST_ITEM", paths::ModelPathType::kQuestItem, "quest_item")
      .str_value("SPECIAL", paths::ModelPathType::kSpecial, "special")
      .str_value("WEAPONS", paths::ModelPathType::kWeapons, "weapons")
      .str_value("UI_RUNES", paths::ModelPathType::kUiRunes, "ui-runes")
      .str_value("UI_MENUS", paths::ModelPathType::kUiMenus, "ui-menus")
      .str_value("EDITOR", paths::ModelPathType::kEditor, "editor")
      .def("__str__", [](paths::ModelPathType value) { return std::string(paths::modelPathTypeName(value)); });
  auto animation_type = nb::enum_<paths::AnimationPathType>(path_module, "AnimationType", nb::is_str());
  animation_type.str_value("NPC", paths::AnimationPathType::kNpc, "npc")
      .str_value("FIX_INTER", paths::AnimationPathType::kFixInter, "fix_inter")
      .def("__str__", [](paths::AnimationPathType value) { return std::string(paths::animationPathTypeName(value)); });
  nb::enum_<ResourceKind>(path_module, "ResourceKind")
      .value("NONE", ResourceKind::kNone)
      .value("LEVEL", ResourceKind::kLevel)
      .value("MODEL", ResourceKind::kModel)
      .value("ANIMATION", ResourceKind::kAnimation)
      .value("CINEMATIC", ResourceKind::kCinematic)
      .value("AMBIANCE", ResourceKind::kAmbiance);
  nb::enum_<EntityClassKind>(path_module, "EntityClassKind")
      .value("UNKNOWN", EntityClassKind::kUnknown)
      .value("ITEM", EntityClassKind::kItem)
      .value("NPC", EntityClassKind::kNpc)
      .value("FIX", EntityClassKind::kFix)
      .value("CAMERA", EntityClassKind::kCamera)
      .value("MARKER", EntityClassKind::kMarker);

  nb::class_<ModelPath>(path_module, "ModelPath")
      .def(nb::init<paths::ModelPathType, std::string, std::string>(),
           nb::arg("type"),
           nb::arg("name"),
           nb::arg("tweak") = "")
      .def_rw("type", &ModelPath::type)
      .def_rw("name", &ModelPath::name)
      .def_rw("tweak", &ModelPath::tweak);
  nb::class_<AnimationPath>(path_module, "AnimationPath")
      .def(nb::init<paths::AnimationPathType, std::string>(), nb::arg("type"), nb::arg("name"))
      .def_rw("type", &AnimationPath::type)
      .def_rw("name", &AnimationPath::name);
  nb::class_<CinematicPath>(path_module, "CinematicPath")
      .def(nb::init<std::string>(), nb::arg("name"))
      .def_rw("name", &CinematicPath::name);
  nb::class_<AmbiancePath>(path_module, "AmbiancePath")
      .def(nb::init<std::string>(), nb::arg("name"))
      .def_rw("name", &AmbiancePath::name);
  nb::class_<SearchLocation>(path_module, "SearchLocation")
      .def_ro("base_path", &SearchLocation::base_path)
      .def_ro("max_discovery_depth", &SearchLocation::max_discovery_depth);

  path_module.def(
      "resource_selector_kind",
      [](std::string_view selector) { return static_cast<ResourceKind>(paths::resourceSelectorKind(selector)); },
      nb::arg("selector"));
  path_module.def("is_portable_filename", &paths::isPortableFilename, nb::arg("filename"));
  path_module.def("sanitize_portable_filename", &paths::sanitizePortableFilename, nb::arg("filename"));
  path_module.def("is_portable_resource_path_component", &paths::isPortableResourcePathComponent, nb::arg("component"));
  path_module.def("texture_directory", [] { return std::string(paths::textureDirectory()); });
  path_module.def("sound_directory", [] { return std::string(paths::soundDirectory()); });
  path_module.def("ambiance_sound_directory", [] { return std::string(paths::ambianceSoundDirectory()); });
  path_module.def("normalize_zone_ambiance", [](std::string_view path) {
    return buildPath("zone ambiance path", [&](std::string& out) { return paths::normalizeZoneAmbiance(path, out); });
  });
  path_module.def("amb_from_zone_ambiance", [](std::string_view path) {
    return buildPath("zone ambiance path", [&](std::string& out) { return paths::ambFromZoneAmbiance(path, out); });
  });

  path_module.def("level_dlf", &paths::levelDlf, nb::arg("level"));
  path_module.def("level_llf", &paths::levelLlf, nb::arg("level"));
  path_module.def("level_fts", &paths::levelFts, nb::arg("level"));
  path_module.def("minimap_resource_level", &paths::minimapResourceLevel, nb::arg("level"));
  path_module.def("level_minimap", &paths::levelMinimap, nb::arg("level"));
  path_module.def("level_loading_screen", &paths::levelLoadingScreen, nb::arg("level"));
  path_module.def("minimap_offsets_file", [] { return std::string(paths::minimapOffsetsFile()); });
  path_module.def("level_from_dlf", [](std::string_view path) {
    std::uint32_t level = 0;
    if (!paths::levelFromDlf(path, level)) throw nb::value_error("invalid DLF resource path");
    return level;
  });
  path_module.def("level_from_llf", [](std::string_view path) {
    std::uint32_t level = 0;
    if (!paths::levelFromLlf(path, level)) throw nb::value_error("invalid LLF resource path");
    return level;
  });
  path_module.def("level_from_fts", [](std::string_view path) {
    std::uint32_t level = 0;
    if (!paths::levelFromFts(path, level)) throw nb::value_error("invalid FTS resource path");
    return level;
  });
  path_module.def("dlf_scene_from_level_name", [](std::string_view name) {
    return buildPath("level name", [&](std::string& out) { return paths::dlfSceneFromLevelName(name, out); });
  });
  path_module.def("level_selector", &paths::levelSelector, nb::arg("level"));
  path_module.def("level_from_selector", [](std::string_view selector) {
    std::uint32_t level = 0;
    if (!paths::levelFromSelector(selector, level)) throw nb::value_error("invalid Level selector");
    return level;
  });
  path_module.def("fts_from_dlf_scene", [](std::string_view path) {
    return buildPath("DLF scene path", [&](std::string& out) { return paths::ftsFromDlfScene(path, out); });
  });

  path_module.def("model_ftl", [](const ModelPath& model) {
    return buildPath("Model path", [&](std::string& out) { return paths::modelFtl(model.view(), out); });
  });
  path_module.def("model_from_ftl", [](std::string_view path) {
    paths::ModelPathView result;
    if (!paths::modelFromFtl(path, result)) throw nb::value_error("invalid FTL resource path");
    return modelPath(result);
  });
  path_module.def("entity_class_from_ftl", [](std::string_view path) {
    return buildPath("FTL resource path", [&](std::string& out) { return paths::entityClassFromFtl(path, out); });
  });
  path_module.def("ftl_from_entity_class", [](std::string_view path) {
    return buildPath("entity class path", [&](std::string& out) { return paths::ftlFromEntityClass(path, out); });
  });
  path_module.def("entity_class_kind", [](std::string_view path) {
    ArxEntityClassKind result = ARX_ENTITY_CLASS_KIND_UNKNOWN;
    if (!paths::entityClassKind(path, result)) throw nb::value_error("invalid entity class path");
    return static_cast<EntityClassKind>(result);
  });
  path_module.def("item_icon_from_entity_class", [](std::string_view path) -> nb::object {
    std::string result;
    if (!paths::itemIconFromEntityClass(path, result)) throw nb::value_error("invalid entity class path");
    return result.empty() ? nb::none() : nb::cast(std::move(result));
  });
  path_module.def("entity_class_from_model", [](const ModelPath& model) {
    return buildPath("Model path", [&](std::string& out) { return paths::entityClassFromModel(model.view(), out); });
  });
  path_module.def("base_entity_class_from_model", [](const ModelPath& model) {
    return buildPath("Model path",
                     [&](std::string& out) { return paths::baseEntityClassFromModel(model.view(), out); });
  });
  path_module.def("model_from_entity_class", [](std::string_view path) {
    paths::ModelPathView result;
    if (!paths::modelFromEntityClass(path, result)) throw nb::value_error("invalid entity class path");
    return modelPath(result);
  });
  path_module.def("model_selector", [](const ModelPath& model) {
    return buildPath("Model path", [&](std::string& out) { return paths::modelSelector(model.view(), out); });
  });
  path_module.def("model_from_selector", [](std::string_view selector) {
    paths::ModelPathView result;
    if (!paths::modelFromSelector(selector, result)) throw nb::value_error("invalid Model selector");
    return modelPath(result);
  });

  path_module.def("animation_directory", [](paths::ModelPathType type) {
    return buildPath("Model path type", [&](std::string& out) { return paths::animationDirectory(type, out); });
  });
  path_module.def("animation_directory", [](paths::AnimationPathType type) {
    return buildPath("Animation path type", [&](std::string& out) { return paths::animationDirectory(type, out); });
  });
  path_module.def("animation_tea", [](const AnimationPath& animation) {
    return buildPath("Animation path", [&](std::string& out) { return paths::animationTea(animation.view(), out); });
  });
  path_module.def("animation_from_tea", [](std::string_view path) {
    paths::AnimationPathView result;
    if (!paths::animationFromTea(path, result)) throw nb::value_error("invalid TEA resource path");
    return animationPath(result);
  });
  path_module.def("animation_selector", [](const AnimationPath& animation) {
    return buildPath("Animation path",
                     [&](std::string& out) { return paths::animationSelector(animation.view(), out); });
  });
  path_module.def("animation_from_selector", [](std::string_view selector) {
    paths::AnimationPathView result;
    if (!paths::animationFromSelector(selector, result)) throw nb::value_error("invalid Animation selector");
    return animationPath(result);
  });

  path_module.def("cinematic_illustration_directory",
                  [] { return std::string(paths::cinematicIllustrationDirectory()); });
  path_module.def("cinematic_cin", [](const CinematicPath& cinematic) {
    return buildPath("Cinematic path", [&](std::string& out) { return paths::cinematicCin(cinematic.view(), out); });
  });
  path_module.def("cinematic_from_cin", [](std::string_view path) {
    paths::CinematicPathView result;
    if (!paths::cinematicFromCin(path, result)) throw nb::value_error("invalid CIN resource path");
    return CinematicPath{std::string(result.name)};
  });
  path_module.def("cinematic_selector", [](const CinematicPath& cinematic) {
    return buildPath("Cinematic path",
                     [&](std::string& out) { return paths::cinematicSelector(cinematic.view(), out); });
  });
  path_module.def("cinematic_from_selector", [](std::string_view selector) {
    paths::CinematicPathView result;
    if (!paths::cinematicFromSelector(selector, result)) throw nb::value_error("invalid Cinematic selector");
    return CinematicPath{std::string(result.name)};
  });

  path_module.def("ambiance_amb", [](const AmbiancePath& ambiance) {
    return buildPath("Ambiance path", [&](std::string& out) { return paths::ambianceAmb(ambiance.view(), out); });
  });
  path_module.def("ambiance_from_amb", [](std::string_view path) {
    paths::AmbiancePathView result;
    if (!paths::ambianceFromAmb(path, result)) throw nb::value_error("invalid AMB resource path");
    return AmbiancePath{std::string(result.name)};
  });
  path_module.def("ambiance_selector", [](const AmbiancePath& ambiance) {
    return buildPath("Ambiance path", [&](std::string& out) { return paths::ambianceSelector(ambiance.view(), out); });
  });
  path_module.def("ambiance_from_selector", [](std::string_view selector) {
    paths::AmbiancePathView result;
    if (!paths::ambianceFromSelector(selector, result)) throw nb::value_error("invalid Ambiance selector");
    return AmbiancePath{std::string(result.name)};
  });

  path_module.def("model_search_location", [](paths::ModelPathType type) {
    return searchedLocation(type, "Model path type", paths::modelSearchLocation);
  });
  path_module.def("animation_search_location", [](paths::AnimationPathType type) {
    return searchedLocation(type, "Animation path type", paths::animationSearchLocation);
  });
  path_module.def("level_search_location", [] { return searchLocation(paths::levelSearchLocation()); });
  path_module.def("cinematic_search_location", [] { return searchLocation(paths::cinematicSearchLocation()); });
  path_module.def("ambiance_search_location", [] { return searchLocation(paths::ambianceSearchLocation()); });
}

}  // namespace pistoris::python
