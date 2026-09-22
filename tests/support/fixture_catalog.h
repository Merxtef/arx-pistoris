// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace test_support {

struct GlbFixture {
  std::filesystem::path path;
  float arx_units_per_glb_unit = 0.0f;

  [[nodiscard]] bool empty() const noexcept { return path.empty(); }
};

struct LevelFixture {
  std::string name;
  std::string selector;
  GlbFixture glb;
  std::filesystem::path fts;
  std::filesystem::path llf;
  std::filesystem::path dlf;
};

struct ModelFixture {
  std::string name;
  std::string selector;
  std::filesystem::path ftl;
  GlbFixture glb;
  std::filesystem::path obj;
};

struct AnimationFixture {
  std::string name;
  std::string selector;
  std::string model;
  std::filesystem::path tea;
};

struct AmbianceFixture {
  std::string name;
  std::string selector;
  std::string reference_model;
  std::filesystem::path amb;
  GlbFixture glb;
};

struct CinematicAudioCoverage {
  std::size_t effect_references = 0;
  std::size_t speech_references = 0;
  std::vector<std::string> languages;
  std::size_t encodings = 0;
};

struct CinematicFixture {
  std::string name;
  std::string selector;
  std::filesystem::path glb;
  std::filesystem::path cin;
  CinematicAudioCoverage audio;
};

struct JsonFixture {
  std::string format;
  std::filesystem::path path;
};

struct NativeAliasGroup {
  std::filesystem::path source;
  std::vector<std::filesystem::path> aliases;
};

struct FixtureCatalog {
  std::vector<LevelFixture> levels;
  std::vector<ModelFixture> models;
  std::vector<AnimationFixture> animations;
  std::vector<AmbianceFixture> ambiances;
  std::vector<CinematicFixture> cinematics;
  std::vector<std::filesystem::path> native_image_sidecars;
  std::vector<std::filesystem::path> native_audio_sidecars;
  std::vector<NativeAliasGroup> native_aliases;
  std::vector<JsonFixture> json;
};

inline constexpr const char* kFixtureRoot = "data/fixtures";
inline constexpr const char* kFixtureCatalog = "data/fixtures/catalog.json";
inline constexpr const char* kFixtureMount = "data/fixtures/mount";

inline std::filesystem::path fixturePath(const nlohmann::json& entry, const char* field) {
  if (!entry.contains(field)) return {};
  return std::filesystem::path(kFixtureRoot) / entry.at(field).get<std::string>();
}

inline GlbFixture fixtureGlb(const nlohmann::json& entry) {
  if (!entry.contains("glb")) return {};
  const nlohmann::json& glb = entry.at("glb");
  return {std::filesystem::path(kFixtureRoot) / glb.at("path").get<std::string>(),
          glb.at("arx_units_per_glb_unit").get<float>()};
}

inline const FixtureCatalog& fixtureCatalog() {
  static const FixtureCatalog kCatalog = [] {
    std::ifstream input(kFixtureCatalog);
    if (!input) throw std::runtime_error("Cannot open fixture catalog");
    const nlohmann::json source = nlohmann::json::parse(input);

    FixtureCatalog result;
    for (const nlohmann::json& entry : source.at("levels")) {
      result.levels.push_back({entry.at("name").get<std::string>(),
                               entry.at("selector").get<std::string>(),
                               fixtureGlb(entry),
                               fixturePath(entry, "fts"),
                               fixturePath(entry, "llf"),
                               fixturePath(entry, "dlf")});
    }
    for (const nlohmann::json& entry : source.at("models")) {
      result.models.push_back({entry.at("name").get<std::string>(),
                               entry.at("selector").get<std::string>(),
                               fixturePath(entry, "ftl"),
                               fixtureGlb(entry),
                               fixturePath(entry, "obj")});
    }
    for (const nlohmann::json& entry : source.at("animations")) {
      result.animations.push_back({entry.at("name").get<std::string>(),
                                   entry.at("selector").get<std::string>(),
                                   entry.at("model").get<std::string>(),
                                   fixturePath(entry, "tea")});
    }
    for (const nlohmann::json& entry : source.at("ambiances")) {
      result.ambiances.push_back({entry.at("name").get<std::string>(),
                                  entry.at("selector").get<std::string>(),
                                  entry.value("reference_model", std::string{}),
                                  fixturePath(entry, "amb"),
                                  fixtureGlb(entry)});
    }
    for (const nlohmann::json& entry : source.at("cinematics")) {
      const nlohmann::json& audio = entry.at("audio");
      result.cinematics.push_back({entry.at("name").get<std::string>(),
                                   entry.at("selector").get<std::string>(),
                                   fixturePath(entry, "glb"),
                                   fixturePath(entry, "cin"),
                                   {audio.at("effect_references").get<std::size_t>(),
                                    audio.at("speech_references").get<std::size_t>(),
                                    audio.at("languages").get<std::vector<std::string>>(),
                                    audio.at("encodings").get<std::size_t>()}});
    }
    const nlohmann::json& native_sidecars = source.at("native_sidecars");
    for (const nlohmann::json& path : native_sidecars.at("images"))
      result.native_image_sidecars.push_back(std::filesystem::path(kFixtureRoot) / path.get<std::string>());
    for (const nlohmann::json& path : native_sidecars.at("audio"))
      result.native_audio_sidecars.push_back(std::filesystem::path(kFixtureRoot) / path.get<std::string>());
    for (const nlohmann::json& group : source.at("native_aliases")) {
      NativeAliasGroup aliases{std::filesystem::path(kFixtureRoot) / group.at("source").get<std::string>(), {}};
      for (const nlohmann::json& path : group.at("aliases"))
        aliases.aliases.push_back(std::filesystem::path(kFixtureRoot) / path.get<std::string>());
      result.native_aliases.push_back(std::move(aliases));
    }
    for (const nlohmann::json& entry : source.at("json")) {
      result.json.push_back({entry.at("format").get<std::string>(), fixturePath(entry, "path")});
    }
    return result;
  }();
  return kCatalog;
}

}  // namespace test_support
