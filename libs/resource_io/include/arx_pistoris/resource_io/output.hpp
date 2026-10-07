// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/resource_mounts.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pistoris::resource_io {

using ResourceOutputParts = std::uint32_t;
enum ResourceOutputPart : std::uint8_t {
  kResourceOutputNone = 0,
  kResourceOutputPrimary = 1U << 0,
  kResourceOutputCompanions = 1U << 1,
  kResourceOutputTextures = 1U << 2,
  kResourceOutputAudio = 1U << 3,
  kResourceOutputImages = 1U << 4,
  kResourceOutputAll = kResourceOutputPrimary | kResourceOutputCompanions | kResourceOutputTextures |
                       kResourceOutputAudio | kResourceOutputImages,
};

enum class ExistingFilePolicy : std::uint8_t {
  kError,
  kOverwrite,
  kPreserve,
};

struct ResourceOutputOptions {
  ResourceOutputParts outputs = kResourceOutputAll;
  ResourceIoFlags io_flags = kResourceIoFlagNone;
};

struct ResourceWriteOptions {
  ExistingFilePolicy existing_file_policy = ExistingFilePolicy::kError;
};

enum class ResourceOutputAddress : std::uint8_t {
  kLogical,
  kNative,
};

enum class ResourceOutputKind : std::uint8_t {
  kData,
  kImage,
  kAudio,
};

struct ResourceOutput {
  ResourceOutputAddress address = ResourceOutputAddress::kLogical;
  ResourceOutputKind kind = ResourceOutputKind::kData;
  bool primary = false;
  ArxResourceKind owner_kind = ARX_RESOURCE_KIND_NONE;
  std::string owner_identity;
  std::string resource_path;
  std::filesystem::path native_path;
  std::vector<std::uint8_t> data;
  ResourceIoFlags io_flags = kResourceIoFlagNone;
  bool written = false;
};

using ResourceOutputs = std::vector<ResourceOutput>;

enum class ResourceWriteStatus : std::uint8_t {
  kPending,
  kNeedsCandidate,
  kNeedsExistingFilePolicy,
  kReady,
  kWritten,
  kAlreadyCurrent,
  kPreserved,
  kFailed,
};

class ResourceWriteEntry {
 public:
  [[nodiscard]] std::span<const ResourceOutput> candidates() const noexcept { return candidates_; }
  [[nodiscard]] const std::filesystem::path& nativePath() const noexcept { return native_path_; }
  [[nodiscard]] std::optional<std::size_t> selectedCandidate() const noexcept { return selected_candidate_; }
  [[nodiscard]] std::optional<ExistingFilePolicy> existingFilePolicy() const noexcept { return existing_file_policy_; }
  [[nodiscard]] ResourceWriteStatus status() const noexcept { return status_; }

  [[nodiscard]] bool selectCandidate(std::size_t candidate) noexcept;
  void setExistingFilePolicy(ExistingFilePolicy policy) noexcept;
  void clearExistingFilePolicy() noexcept;

 private:
  std::vector<ResourceOutput> candidates_;
  std::filesystem::path native_path_;
  std::optional<std::size_t> selected_candidate_;
  std::optional<ExistingFilePolicy> existing_file_policy_;
  ResourceWriteStatus status_ = ResourceWriteStatus::kPending;

  friend class Resources;
  friend class ResourceWritePlan;
};

class ResourceWriteReportEntry {
 public:
  [[nodiscard]] const std::filesystem::path& nativePath() const noexcept { return native_path_; }
  [[nodiscard]] ResourceWriteStatus status() const noexcept { return status_; }

 private:
  std::filesystem::path native_path_;
  ResourceWriteStatus status_ = ResourceWriteStatus::kPending;

  friend class ResourceWritePlan;
};

class ResourceWriteReport {
 public:
  [[nodiscard]] std::span<const ResourceWriteReportEntry> entries() const noexcept { return entries_; }

 private:
  std::vector<ResourceWriteReportEntry> entries_;

  friend class ResourceWritePlan;
};

class ResourceWritePlan {
 public:
  ResourceWritePlan() = default;

  [[nodiscard]] std::span<ResourceWriteEntry> entries() noexcept { return entries_; }
  [[nodiscard]] std::span<const ResourceWriteEntry> entries() const noexcept { return entries_; }
  [[nodiscard]] ExistingFilePolicy defaultExistingFilePolicy() const noexcept { return default_existing_file_policy_; }
  void setDefaultExistingFilePolicy(ExistingFilePolicy policy) noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> preflight() noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> execute() noexcept;

 private:
  [[nodiscard]] ResourceIoResult<void> preflightInternal() noexcept;
  [[nodiscard]] ResourceWriteReport makeReport() const;

  std::vector<ResourceWriteEntry> entries_;
  ExistingFilePolicy default_existing_file_policy_ = ExistingFilePolicy::kError;

  friend class Resources;
};

}  // namespace pistoris::resource_io
