// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace test_support {

struct LevelNativeTriplet {
  std::filesystem::path fts;
  std::filesystem::path dlf;
  std::filesystem::path llf;
};

inline std::vector<std::filesystem::path> discoverCorpusFiles(const std::filesystem::path& directory,
                                                              const std::filesystem::path& extension) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::is_directory(directory)) return files;

  for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
    if (!entry.is_regular_file() || entry.path().extension() != extension) continue;
    files.push_back(entry.path());
  }

  std::sort(files.begin(), files.end());
  return files;
}

inline std::vector<std::filesystem::path> discoverCorpusFiles(std::initializer_list<std::filesystem::path> directories,
                                                              const std::filesystem::path& extension) {
  std::vector<std::filesystem::path> files;
  for (const std::filesystem::path& directory : directories) {
    std::vector<std::filesystem::path> discovered = discoverCorpusFiles(directory, extension);
    files.insert(files.end(), discovered.begin(), discovered.end());
  }
  std::sort(files.begin(), files.end());
  return files;
}

inline std::vector<LevelNativeTriplet> discoverLevelTriplets(const std::filesystem::path& fts_directory,
                                                             const std::filesystem::path& dlf_directory,
                                                             const std::filesystem::path& llf_directory) {
  struct PartialTriplet {
    std::filesystem::path fts;
    std::filesystem::path dlf;
    std::filesystem::path llf;
  };

  std::map<std::string, PartialTriplet> partial;
  for (const std::filesystem::path& path : discoverCorpusFiles(fts_directory, ".fts"))
    partial[path.stem().string()].fts = path;
  for (const std::filesystem::path& path : discoverCorpusFiles(dlf_directory, ".dlf"))
    partial[path.stem().string()].dlf = path;
  for (const std::filesystem::path& path : discoverCorpusFiles(llf_directory, ".llf"))
    partial[path.stem().string()].llf = path;

  std::vector<LevelNativeTriplet> triplets;
  for (const auto& [name, paths] : partial) {
    (void)name;
    if (paths.fts.empty() || paths.dlf.empty() || paths.llf.empty()) continue;
    triplets.push_back({paths.fts, paths.dlf, paths.llf});
  }
  return triplets;
}

}  // namespace test_support
