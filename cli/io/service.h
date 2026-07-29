// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "io/path_location.h"
#include "io/policy.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

enum class ResourceReadResult : std::uint8_t {
  kSuccess,
  kNotFound,
  kInvalidPath,
  kReadFailed,
};

enum class ResourceEnumerationResult : std::uint8_t {
  kSuccess,
  kInvalidPath,
  kReadFailed,
};

class IoService {
 public:
  IoService(OverwriteMode overwrite, bool dry_run, const std::vector<std::string>& mounts);
  ~IoService();

  bool writeFile(const char* path, const void* data, std::size_t size);
  bool writeResource(std::string_view resource_path, const void* data, std::size_t size);
  bool writePath(const PathLocation& location, const void* data, std::size_t size);
  bool resolvePathLocation(std::string_view requested, PathLocation& out, std::string& error) const;
  bool parentPathLocation(const PathLocation& location, PathLocation& out, std::string& error) const;
  bool appendPathLocation(const PathLocation& base, std::string_view resource_path, PathLocation& out,
                          std::string& error) const;
  ResourceReadResult readPath(const PathLocation& location, std::vector<std::uint8_t>& out,
                              std::string* resolved_path = nullptr);
  ResourceReadResult readPath(std::string_view requested, std::vector<std::uint8_t>& out,
                              std::string* resolved_path = nullptr);
  bool isAbsolutePath(std::string_view requested, bool& out, std::string& error) const;

  bool resolveOutputLocation(std::string_view requested, OutputLocation& out, std::string& error) const;
  bool normalizeResourcePath(std::string_view resource_path, std::string& out, std::string& error) const;
  ResourceReadResult readResource(std::string_view resource_path, std::vector<std::uint8_t>& out,
                                  std::string* resolved_path = nullptr);
  ResourceEnumerationResult enumerateResources(std::string_view base_path, std::uint32_t max_depth,
                                               std::vector<std::string>& out);
  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] bool hasReadMounts() const noexcept;

 private:
  struct MountState;

  bool writeNativeFile(const std::filesystem::path& path, const void* data, std::size_t size);

  State state_;
  std::unique_ptr<MountState> mounts_;
};

}  // namespace cli
