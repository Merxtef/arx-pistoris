// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "fixture_catalog.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace test_support {

struct LevelNativeTriplet {
  std::filesystem::path fts;
  std::filesystem::path dlf;
  std::filesystem::path llf;
};

enum class NativeCorpusFormat : std::uint8_t { kAmb, kDlf, kFtl, kFts, kLlf, kTea };

inline bool hasExtension(const std::filesystem::path& path, std::string_view extension) {
  const std::string actual = path.extension().string();
  return actual.size() == extension.size() &&
         std::equal(actual.begin(), actual.end(), extension.begin(), [](char left, char right) {
           return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
         });
}

inline std::vector<std::filesystem::path> discoverCorpusFiles(const std::filesystem::path& directory,
                                                              const std::filesystem::path& extension) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::is_directory(directory)) return files;

  for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(directory)) {
    if (!entry.is_regular_file() || !hasExtension(entry.path(), extension.string())) continue;
    files.push_back(entry.path());
  }

  std::sort(files.begin(), files.end());
  return files;
}

inline std::vector<std::filesystem::path> nativeCorpusFiles(NativeCorpusFormat format) {
  std::vector<std::filesystem::path> files;
  std::string_view extension;
  const FixtureCatalog& catalog = fixtureCatalog();
  switch (format) {
    case NativeCorpusFormat::kAmb:
      extension = ".amb";
      for (const AmbianceFixture& fixture : catalog.ambiances) files.push_back(fixture.amb);
      break;
    case NativeCorpusFormat::kDlf:
      extension = ".dlf";
      for (const LevelFixture& fixture : catalog.levels) files.push_back(fixture.dlf);
      break;
    case NativeCorpusFormat::kFtl:
      extension = ".ftl";
      for (const ModelFixture& fixture : catalog.models) files.push_back(fixture.ftl);
      break;
    case NativeCorpusFormat::kFts:
      extension = ".fts";
      for (const LevelFixture& fixture : catalog.levels) files.push_back(fixture.fts);
      break;
    case NativeCorpusFormat::kLlf:
      extension = ".llf";
      for (const LevelFixture& fixture : catalog.levels) files.push_back(fixture.llf);
      break;
    case NativeCorpusFormat::kTea:
      extension = ".tea";
      for (const AnimationFixture& fixture : catalog.animations) files.push_back(fixture.tea);
      break;
  }

  std::vector<std::filesystem::path> optional = discoverCorpusFiles("data/arx", extension);
  files.insert(files.end(), optional.begin(), optional.end());
  std::sort(files.begin(), files.end());
  return files;
}

inline std::vector<LevelNativeTriplet> discoverLevelTriplets(const std::filesystem::path& mount) {
  std::vector<LevelNativeTriplet> triplets;
  for (const std::filesystem::path& dlf : discoverCorpusFiles(mount, ".dlf")) {
    std::error_code error;
    const std::filesystem::path relative = std::filesystem::relative(dlf, mount, error);
    if (error || relative.empty()) continue;

    std::filesystem::path llf = dlf;
    llf.replace_extension(".llf");
    const std::filesystem::path fts = mount / "game" / relative.parent_path() / "fast.fts";
    if (!std::filesystem::is_regular_file(fts) || !std::filesystem::is_regular_file(llf)) continue;
    triplets.push_back({fts, dlf, llf});
  }
  std::sort(triplets.begin(), triplets.end(), [](const LevelNativeTriplet& left, const LevelNativeTriplet& right) {
    return left.dlf < right.dlf;
  });
  return triplets;
}

inline std::vector<LevelNativeTriplet> levelNativeTriplets() {
  std::vector<LevelNativeTriplet> triplets;
  for (const LevelFixture& fixture : fixtureCatalog().levels)
    triplets.push_back({fixture.fts, fixture.dlf, fixture.llf});
  std::vector<LevelNativeTriplet> optional = discoverLevelTriplets("data/arx");
  triplets.insert(triplets.end(), optional.begin(), optional.end());
  return triplets;
}

}  // namespace test_support
