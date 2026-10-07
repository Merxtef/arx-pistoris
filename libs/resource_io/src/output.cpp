// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/resource_io/output.hpp"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/resource_io/location.hpp"
#include "arx_pistoris/resource_io/resource_mounts.hpp"
#include "arx_pistoris/resource_io/resources.hpp"
#include "arx_pistoris/resource_io/status.h"

#include "native_path.h"
#include "result_failure.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <limits>
#include <new>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

using pistoris::resource_io::ResourceIoOperation;
using pistoris::resource_io::ResourceIoResult;
using pistoris::resource_io::ResourceOutput;
using pistoris::resource_io::ResourceOutputAddress;
using pistoris::resource_io::ResourceWriteEntry;
using pistoris::resource_io::ResourceWritePlan;
using pistoris::resource_io::ResourceWriteStatus;

template <class T>
ResourceIoResult<T> failure(ArxReturnCode code, std::string_view resource_path,
                            const std::filesystem::path& native_path, std::string_view detail = {}) {
  return pistoris::resource_io::detail::resourceIoFailure<T>(
      code, ResourceIoOperation::kWrite, resource_path, native_path, 0, detail);
}

std::string pathToUtf8(const std::filesystem::path& path) {
  const std::u8string encoded = path.u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

std::string asciiLower(std::string value) {
  for (char& character : value)
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
  return value;
}

std::string logicalKey(std::string_view path) {
  std::string result;
  result.reserve(path.size());
  bool separator = false;
  for (char character : path) {
    if (character == '/' || character == '\\') {
      separator = !result.empty();
      continue;
    }
    if (separator) {
      result.push_back('/');
      separator = false;
    }
    result.push_back(character);
  }
  return asciiLower(std::move(result));
}

std::string nativeKey(const std::filesystem::path& path) {
  std::string result = pathToUtf8(path.lexically_normal());
#ifdef _WIN32
  result = asciiLower(std::move(result));
#endif
  return result;
}

constexpr bool validExistingFilePolicy(pistoris::resource_io::ExistingFilePolicy policy) noexcept {
  switch (policy) {
    case pistoris::resource_io::ExistingFilePolicy::kError:
    case pistoris::resource_io::ExistingFilePolicy::kOverwrite:
    case pistoris::resource_io::ExistingFilePolicy::kPreserve:
      return true;
    default:
      return false;
  }
}

std::size_t findRoot(std::vector<std::size_t>& parents, std::size_t index) {
  while (parents[index] != index) {
    parents[index] = parents[parents[index]];
    index = parents[index];
  }
  return index;
}

void unite(std::vector<std::size_t>& parents, std::size_t left, std::size_t right) {
  left = findRoot(parents, left);
  right = findRoot(parents, right);
  if (left != right) parents[right] = left;
}

ResourceIoResult<std::vector<std::uint8_t>> readExisting(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) return failure<std::vector<std::uint8_t>>(ARX_RESOURCE_IO_OPEN_FAILED, {}, path);
  const std::streampos end = input.tellg();
  if (end < 0 ||
      static_cast<std::uintmax_t>(static_cast<std::streamoff>(end)) > std::numeric_limits<std::size_t>::max())
    return failure<std::vector<std::uint8_t>>(ARX_RESOURCE_IO_READ_FAILED, {}, path);
  std::vector<std::uint8_t> result(static_cast<std::size_t>(end));
  input.seekg(0);
  if (!result.empty()) input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
  if (!input) return failure<std::vector<std::uint8_t>>(ARX_RESOURCE_IO_READ_FAILED, {}, path);
  return ResourceIoResult<std::vector<std::uint8_t>>::success(std::move(result));
}

std::filesystem::path temporaryPath(const std::filesystem::path& target) {
  static std::atomic<std::uint64_t> sequence = 0;
  std::filesystem::path result = target;
  result += ".pistoris.tmp.";
  result += std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
  return result;
}

class TemporaryFile {
 public:
  explicit TemporaryFile(std::filesystem::path path) : path_(std::move(path)) {}
  ~TemporaryFile() {
    if (!active_) return;
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }
  void release() noexcept { active_ = false; }

 private:
  std::filesystem::path path_;
  bool active_ = true;
};

bool replaceFile(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& error) {
#ifdef _WIN32
  if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
  error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
  return false;
#else
  std::filesystem::rename(from, to, error);
  return !error;
#endif
}

ResourceIoResult<std::filesystem::path> writeTemporaryExclusive(const std::filesystem::path& target,
                                                                std::span<const std::uint8_t> data) {
  for (unsigned attempt = 0; attempt < 1024; ++attempt) {
    const std::filesystem::path temporary = temporaryPath(target);
#ifdef _WIN32
    HANDLE file = CreateFileW(temporary.c_str(),
                              GENERIC_WRITE,
                              0,
                              nullptr,
                              CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      const DWORD code = GetLastError();
      if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS) continue;
      return failure<std::filesystem::path>(
          ARX_RESOURCE_IO_OPEN_FAILED, {}, temporary, std::system_category().message(static_cast<int>(code)));
    }
    DWORD failure_code = ERROR_SUCCESS;
    std::size_t offset = 0;
    while (offset < data.size()) {
      const std::size_t remaining = data.size() - offset;
      const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
      DWORD written = 0;
      if (!WriteFile(file, data.data() + offset, chunk, &written, nullptr) || written != chunk) {
        failure_code = GetLastError();
        if (failure_code == ERROR_SUCCESS) failure_code = ERROR_WRITE_FAULT;
        break;
      }
      offset += written;
    }
    if (!CloseHandle(file) && failure_code == ERROR_SUCCESS) failure_code = GetLastError();
    if (failure_code != ERROR_SUCCESS) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return failure<std::filesystem::path>(
          ARX_RESOURCE_IO_WRITE_FAILED, {}, temporary, std::system_category().message(static_cast<int>(failure_code)));
    }
#else
    const int file = ::open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0666);
    if (file < 0) {
      if (errno == EEXIST) continue;
      return failure<std::filesystem::path>(
          ARX_RESOURCE_IO_OPEN_FAILED, {}, temporary, std::generic_category().message(errno));
    }
    int failure_code = 0;
    std::size_t offset = 0;
    while (offset < data.size()) {
      const std::size_t remaining = data.size() - offset;
      const std::size_t chunk =
          std::min<std::size_t>(remaining, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
      const ssize_t written = ::write(file, data.data() + offset, chunk);
      if (written <= 0) {
        failure_code = errno;
        break;
      }
      offset += static_cast<std::size_t>(written);
    }
    if (::close(file) != 0 && failure_code == 0) failure_code = errno;
    if (failure_code != 0) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return failure<std::filesystem::path>(
          ARX_RESOURCE_IO_WRITE_FAILED, {}, temporary, std::generic_category().message(failure_code));
    }
#endif
    return ResourceIoResult<std::filesystem::path>::success(temporary);
  }
  return failure<std::filesystem::path>(
      ARX_RESOURCE_IO_OPEN_FAILED, {}, target, "cannot allocate a temporary output path");
}

ResourceIoResult<void> writeAtomic(const std::filesystem::path& path, std::span<const std::uint8_t> data) {
  std::error_code error;
  if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), error);
  if (error) return failure<void>(ARX_RESOURCE_IO_WRITE_FAILED, {}, path, error.message());

  auto temporary = writeTemporaryExclusive(path, data);
  if (!temporary) return std::move(temporary).propagate<void>();
  TemporaryFile cleanup(*temporary);
  if (!replaceFile(*temporary, path, error))
    return failure<void>(ARX_RESOURCE_IO_WRITE_FAILED, {}, path, error.message());
  cleanup.release();
  return ResourceIoResult<void>::success();
}

}  // namespace

namespace pistoris::resource_io {

bool ResourceWriteEntry::selectCandidate(std::size_t candidate) noexcept {
  if (candidate >= candidates_.size()) return false;
  selected_candidate_ = candidate;
  status_ = candidates_[candidate].written ? ResourceWriteStatus::kWritten : ResourceWriteStatus::kPending;
  return true;
}

void ResourceWriteEntry::setExistingFilePolicy(ExistingFilePolicy policy) noexcept {
  existing_file_policy_ = policy;
  if (status_ != ResourceWriteStatus::kWritten) status_ = ResourceWriteStatus::kPending;
}

void ResourceWriteEntry::clearExistingFilePolicy() noexcept {
  existing_file_policy_.reset();
  if (status_ != ResourceWriteStatus::kWritten) status_ = ResourceWriteStatus::kPending;
}

void ResourceWritePlan::setDefaultExistingFilePolicy(ExistingFilePolicy policy) noexcept {
  default_existing_file_policy_ = policy;
  for (ResourceWriteEntry& entry : entries_)
    if (entry.status_ != ResourceWriteStatus::kWritten) entry.status_ = ResourceWriteStatus::kPending;
}

ResourceIoResult<ResourceWritePlan> Resources::prepareWrite(ResourceOutputs outputs,
                                                            const ResourceWriteOptions& options) const noexcept {
  try {
    if (!validExistingFilePolicy(options.existing_file_policy))
      return failure<ResourceWritePlan>(ARX_INVALID_OPTIONS, {}, {}, "invalid existing-file policy");
    ResourceWritePlan plan;
    plan.default_existing_file_policy_ = options.existing_file_policy;
    if (outputs.empty()) return ResourceIoResult<ResourceWritePlan>::success(std::move(plan));

    std::vector<std::filesystem::path> native_paths;
    native_paths.reserve(outputs.size());
    for (const ResourceOutput& output : outputs) {
      if ((output.io_flags & ~static_cast<ResourceIoFlags>(kResourceIoFlagsAll)) != 0)
        return failure<ResourceWritePlan>(
            ARX_INVALID_OPTIONS, output.resource_path, output.native_path, "unsupported Resource I/O flags");
      switch (output.address) {
        case ResourceOutputAddress::kLogical: {
          auto resolved = mounts_.resolveWritePath(output.resource_path, output.io_flags);
          if (!resolved) return std::move(resolved).propagate<ResourceWritePlan>();
          native_paths.push_back(std::move(*resolved));
          break;
        }
        case ResourceOutputAddress::kNative: {
          std::filesystem::path resolved;
          bool exists = false;
          std::string error;
          if (!detail::canonicalizeProspectivePath(output.native_path, resolved, exists, error))
            return failure<ResourceWritePlan>(
                ARX_RESOURCE_IO_STAT_FAILED, output.resource_path, output.native_path, error);
          native_paths.push_back(std::move(resolved));
          break;
        }
        default:
          return failure<ResourceWritePlan>(
              ARX_INVALID_OPTIONS, output.resource_path, output.native_path, "invalid Resource output address");
      }
    }

    std::vector<std::size_t> parents(outputs.size());
    std::iota(parents.begin(), parents.end(), 0U);
    std::unordered_map<std::string, std::size_t> by_native;
    std::unordered_map<std::string, std::size_t> by_logical;
    by_native.reserve(outputs.size());
    by_logical.reserve(outputs.size());
    for (std::size_t index = 0; index < outputs.size(); ++index) {
      const auto [native, native_inserted] = by_native.emplace(nativeKey(native_paths[index]), index);
      if (!native_inserted) unite(parents, native->second, index);
      if (outputs[index].address == ResourceOutputAddress::kLogical) {
        const auto [logical, logical_inserted] = by_logical.emplace(logicalKey(outputs[index].resource_path), index);
        if (!logical_inserted) unite(parents, logical->second, index);
      }
    }

    std::unordered_map<std::size_t, std::size_t> entries;
    entries.reserve(outputs.size());
    for (std::size_t index = 0; index < outputs.size(); ++index) {
      const std::size_t root = findRoot(parents, index);
      auto [entry, inserted] = entries.emplace(root, plan.entries_.size());
      if (inserted) {
        plan.entries_.emplace_back();
        plan.entries_.back().native_path_ = native_paths[index];
      }
      plan.entries_[entry->second].candidates_.push_back(std::move(outputs[index]));
    }

    for (ResourceWriteEntry& entry : plan.entries_) {
      const auto same_data = [&](const ResourceOutput& candidate) {
        return candidate.data == entry.candidates_.front().data;
      };
      if (std::ranges::all_of(entry.candidates_, same_data)) {
        entry.selected_candidate_ = 0;
        if (std::ranges::any_of(entry.candidates_, [](const ResourceOutput& candidate) { return candidate.written; })) {
          const auto written = std::ranges::find_if(entry.candidates_,
                                                    [](const ResourceOutput& candidate) { return candidate.written; });
          entry.selected_candidate_ = static_cast<std::size_t>(written - entry.candidates_.begin());
          entry.status_ = ResourceWriteStatus::kWritten;
        }
      } else {
        entry.status_ = ResourceWriteStatus::kNeedsCandidate;
      }
    }
    return ResourceIoResult<ResourceWritePlan>::success(std::move(plan));
  } catch (const std::bad_alloc&) {
    return failure<ResourceWritePlan>(ARX_BAD_ALLOC, {}, {});
  } catch (...) {
    return failure<ResourceWritePlan>(ARX_INTERNAL_ERROR, {}, {});
  }
}

ResourceIoResult<void> ResourceWritePlan::preflightInternal() noexcept {
  try {
    if (!validExistingFilePolicy(default_existing_file_policy_))
      return failure<void>(ARX_INVALID_OPTIONS, {}, {}, "invalid default existing-file policy");
    for (ResourceWriteEntry& entry : entries_) {
      if (entry.existing_file_policy_ && !validExistingFilePolicy(*entry.existing_file_policy_)) {
        entry.status_ = ResourceWriteStatus::kFailed;
        return failure<void>(ARX_INVALID_OPTIONS, {}, entry.native_path_, "invalid existing-file policy");
      }
      if (entry.status_ == ResourceWriteStatus::kWritten) continue;
      if (!entry.selected_candidate_ || *entry.selected_candidate_ >= entry.candidates_.size()) {
        entry.status_ = ResourceWriteStatus::kNeedsCandidate;
        continue;
      }

      std::error_code error;
      const auto status = std::filesystem::symlink_status(entry.native_path_, error);
      if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory ||
          (!error && status.type() == std::filesystem::file_type::not_found)) {
        entry.status_ = ResourceWriteStatus::kReady;
        continue;
      }
      if (error) {
        entry.status_ = ResourceWriteStatus::kFailed;
        return failure<void>(ARX_RESOURCE_IO_STAT_FAILED, {}, entry.native_path_, error.message());
      }
      if (!std::filesystem::is_regular_file(status)) {
        entry.status_ = ResourceWriteStatus::kFailed;
        return failure<void>(ARX_RESOURCE_IO_WRITE_FAILED, {}, entry.native_path_, "output is not a regular file");
      }

      auto existing = readExisting(entry.native_path_);
      if (!existing) {
        entry.status_ = ResourceWriteStatus::kFailed;
        return std::move(existing).propagate<void>();
      }
      if (*existing == entry.candidates_[*entry.selected_candidate_].data) {
        entry.status_ = ResourceWriteStatus::kAlreadyCurrent;
        continue;
      }

      const ExistingFilePolicy policy = entry.existing_file_policy_.value_or(default_existing_file_policy_);
      switch (policy) {
        case ExistingFilePolicy::kOverwrite:
          entry.status_ = ResourceWriteStatus::kReady;
          break;
        case ExistingFilePolicy::kPreserve:
          entry.status_ = ResourceWriteStatus::kPreserved;
          break;
        case ExistingFilePolicy::kError:
          entry.status_ = ResourceWriteStatus::kNeedsExistingFilePolicy;
          break;
        default:
          entry.status_ = ResourceWriteStatus::kFailed;
          return failure<void>(ARX_INVALID_OPTIONS, {}, entry.native_path_, "invalid existing-file policy");
      }
    }
    return ResourceIoResult<void>::success();
  } catch (const std::bad_alloc&) {
    return failure<void>(ARX_BAD_ALLOC, {}, {});
  } catch (...) {
    return failure<void>(ARX_INTERNAL_ERROR, {}, {});
  }
}

ResourceWriteReport ResourceWritePlan::makeReport() const {
  ResourceWriteReport report;
  report.entries_.reserve(entries_.size());
  for (const ResourceWriteEntry& entry : entries_) {
    ResourceWriteReportEntry report_entry;
    report_entry.native_path_ = entry.native_path_;
    report_entry.status_ = entry.status_;
    report.entries_.push_back(std::move(report_entry));
  }
  return report;
}

ResourceIoResult<ResourceWriteReport> ResourceWritePlan::preflight() noexcept {
  auto result = preflightInternal();
  if (!result) return std::move(result).propagate<ResourceWriteReport>();
  try {
    return ResourceIoResult<ResourceWriteReport>::success(makeReport());
  } catch (const std::bad_alloc&) {
    return failure<ResourceWriteReport>(ARX_BAD_ALLOC, {}, {});
  } catch (...) {
    return failure<ResourceWriteReport>(ARX_INTERNAL_ERROR, {}, {});
  }
}

ResourceIoResult<ResourceWriteReport> ResourceWritePlan::execute() noexcept {
  try {
    auto preflight = preflightInternal();
    if (!preflight) return std::move(preflight).propagate<ResourceWriteReport>();
    for (const ResourceWriteEntry& entry : entries_) {
      if (entry.status_ == ResourceWriteStatus::kNeedsCandidate ||
          entry.status_ == ResourceWriteStatus::kNeedsExistingFilePolicy)
        return failure<ResourceWriteReport>(ARX_RESOURCE_IO_DECISION_REQUIRED,
                                            {},
                                            entry.native_path_,
                                            entry.status_ == ResourceWriteStatus::kNeedsCandidate
                                                ? "multiple outputs target this path; select a candidate"
                                                : "a differing file already exists; choose PRESERVE or OVERWRITE");
    }
    ResourceWriteReport report = makeReport();
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      ResourceWriteEntry& entry = entries_[index];
      if (entry.status_ != ResourceWriteStatus::kReady) continue;
      const std::size_t selected_candidate = entry.selected_candidate_.value_or(entry.candidates_.size());
      if (selected_candidate >= entry.candidates_.size()) {
        entry.status_ = ResourceWriteStatus::kFailed;
        return failure<ResourceWriteReport>(
            ARX_INTERNAL_ERROR, {}, entry.native_path_, "ready output has no valid selected candidate");
      }
      ResourceOutput& selected = entry.candidates_[selected_candidate];
      auto written = writeAtomic(entry.native_path_, selected.data);
      if (!written) {
        entry.status_ = ResourceWriteStatus::kFailed;
        return std::move(written).propagate<ResourceWriteReport>();
      }
      for (ResourceOutput& candidate : entry.candidates_)
        if (candidate.data == selected.data) candidate.written = true;
      entry.status_ = ResourceWriteStatus::kWritten;
      report.entries_[index].status_ = ResourceWriteStatus::kWritten;
    }
    return ResourceIoResult<ResourceWriteReport>::success(std::move(report));
  } catch (const std::bad_alloc&) {
    return failure<ResourceWriteReport>(ARX_BAD_ALLOC, {}, {});
  } catch (...) {
    return failure<ResourceWriteReport>(ARX_INTERNAL_ERROR, {}, {});
  }
}

}  // namespace pistoris::resource_io
