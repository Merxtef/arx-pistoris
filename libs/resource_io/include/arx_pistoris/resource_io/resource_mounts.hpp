// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/resource_io/location.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris::resource_io {

using ResourceMountMask = std::uint64_t;
inline constexpr ResourceMountMask kAllResourceMounts = UINT64_MAX;
inline constexpr std::size_t kMaximumReadMounts = 64;

using ResourceIoFlags = std::uint32_t;
enum ResourceIoFlag : std::uint8_t {
  kResourceIoFlagNone = 0,
  kResourceIoRecoverCaseCollisions = 1U << 0,
  kResourceIoFlagsAll = kResourceIoRecoverCaseCollisions,
};

struct ResourceLookupOptions {
  ResourceMountMask mount_mask = kAllResourceMounts;
  ResourceIoFlags flags = kResourceIoFlagNone;
};

struct ResourceMount {
  ResourceMountMask id = 0;
  std::filesystem::path path;
};

enum class MountValidationKind : std::uint8_t {
  kMissingReadMount,
  kDuplicateReadMount,
  kProspectiveWriteMount,
};

struct MountValidationMessage {
  MountValidationKind kind = MountValidationKind::kMissingReadMount;
  std::filesystem::path path;
};

struct MountValidationReport {
  std::vector<MountValidationMessage> messages;
};

struct ResourceMountOptions {
  std::vector<std::filesystem::path> read_mounts;
  std::optional<std::filesystem::path> write_mount;
};

struct ResourceRead {
  std::vector<std::uint8_t> data;
  std::filesystem::path native_path;
  ResourceMountMask mount_id = 0;
};

struct ResolvedResource {
  std::filesystem::path native_path;
  ResourceMountMask mount_id = 0;
};

struct ResourceFile {
  std::string logical_path;
  ResourceMountMask mount_mask = 0;
};

struct ResourceDirectoryEntry {
  enum class Kind : std::uint8_t {
    kDirectory = 0,
    kUnknownFile = 1,

    kFtl = 8,
    kTea = 9,
    kFts = 10,
    kDlf = 11,
    kLlf = 12,
    kAmb = 13,
    kCin = 14,

    kGlb = 24,
    kObj = 25,
    kMtl = 26,
    kJson = 27,

    kPng = 40,
    kJpeg = 41,
    kBmp = 42,
    kTga = 43,

    kWav = 56,
    kMp3 = 57,
    kOgg = 58,
  };

  std::string name;
  Kind kind = Kind::kUnknownFile;
  ResourceMountMask mount_mask = 0;
};

[[nodiscard]] ResourceIoResult<std::filesystem::path> libertatisResourceRoot() noexcept;

class ResourceMounts {
 public:
  ResourceMounts();
  ~ResourceMounts();
  ResourceMounts(const ResourceMounts&);
  ResourceMounts(ResourceMounts&&) noexcept;
  ResourceMounts& operator=(const ResourceMounts&);
  ResourceMounts& operator=(ResourceMounts&&) noexcept;

  [[nodiscard]] static ResourceIoResult<ResourceMounts> open(const ResourceMountOptions& options,
                                                             MountValidationReport* report = nullptr) noexcept;

  [[nodiscard]] ResourceIoResult<void> addReadMount(const std::filesystem::path& path,
                                                    MountValidationReport* report = nullptr) noexcept;
  [[nodiscard]] ResourceIoResult<void> addLibertatisMounts(MountValidationReport* report = nullptr) noexcept;
  [[nodiscard]] ResourceIoResult<void> setWriteMount(const std::optional<std::filesystem::path>& path,
                                                     MountValidationReport* report = nullptr) noexcept;
  [[nodiscard]] ResourceIoResult<void> setLibertatisWriteMount(MountValidationReport* report = nullptr) noexcept;

  [[nodiscard]] std::span<const ResourceMount> readMounts() const noexcept;
  [[nodiscard]] const std::optional<std::filesystem::path>& writeMount() const noexcept;
  [[nodiscard]] ResourceMountMask availableMounts() const noexcept;
  [[nodiscard]] std::optional<ResourceMountMask> highestPriorityMountId(ResourceMountMask mount_mask) const noexcept;

  [[nodiscard]] ResourceIoResult<ResourceRead> read(std::string_view logical_path,
                                                    const ResourceLookupOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResolvedResource> resolve(std::string_view logical_path,
                                                           const ResourceLookupOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<std::filesystem::path> resolveWritePath(
      std::string_view logical_path, ResourceIoFlags flags = kResourceIoFlagNone) const noexcept;
  [[nodiscard]] ResourceIoResult<void> write(std::string_view logical_path, std::span<const std::uint8_t> data,
                                             ResourceIoFlags flags = kResourceIoFlagNone) const noexcept;
  [[nodiscard]] ResourceIoResult<std::vector<ResourceFile>> enumerate(
      std::string_view logical_directory, std::uint32_t max_depth,
      const ResourceLookupOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<std::vector<ResourceDirectoryEntry>> listDirectory(
      std::string_view logical_directory = {}, const ResourceLookupOptions& options = {}) const noexcept;

 private:
  struct Data;
  explicit ResourceMounts(std::unique_ptr<Data> data) noexcept;
  [[nodiscard]] ResourceIoResult<void> addReadMounts(std::span<const std::filesystem::path> paths,
                                                     MountValidationReport* report, bool report_duplicates) noexcept;
  [[nodiscard]] ResourceIoResult<void> setWriteMountInternal(const std::optional<std::filesystem::path>& path,
                                                             MountValidationReport* report) noexcept;
  std::unique_ptr<Data> data_;
};

}  // namespace pistoris::resource_io
