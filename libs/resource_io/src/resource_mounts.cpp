// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/resource_io/resource_mounts.hpp"

#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/resource_io/location.hpp"
#include "arx_pistoris/resource_io/status.h"
#include "arx_pistoris/runtime/types.h"

#include "native_path.h"
#include "result_failure.h"
#include "utils/log.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <combaseapi.h>
#include <iterator>
#include <knownfolders.h>
#include <shlobj.h>
#endif

namespace {

using pistoris::resource_io::kResourceIoFlagsAll;
using pistoris::resource_io::kResourceIoRecoverCaseCollisions;
using pistoris::resource_io::ResourceDirectoryEntry;
using pistoris::resource_io::ResourceIoFlags;
using pistoris::resource_io::ResourceIoOperation;
using pistoris::resource_io::ResourceMount;
using pistoris::resource_io::ResourceMountMask;

std::string asciiLower(std::string_view value) {
  std::string result(value);
  for (char& character : result) {
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
  }
  return result;
}

std::string pathToUtf8(const std::filesystem::path& path) {
  const std::u8string encoded = path.u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

constexpr bool validFlags(ResourceIoFlags flags) noexcept {
  return (flags & ~static_cast<ResourceIoFlags>(kResourceIoFlagsAll)) == 0;
}

#ifdef _WIN32
class ComInitialization {
 public:
  ComInitialization() : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
  ComInitialization(const ComInitialization&) = delete;
  ComInitialization& operator=(const ComInitialization&) = delete;
  ~ComInitialization() {
    if (SUCCEEDED(result_)) CoUninitialize();
  }

  [[nodiscard]] HRESULT result() const noexcept { return result_; }

 private:
  HRESULT result_;
};

struct CoTaskMemDeleter {
  void operator()(wchar_t* memory) const noexcept { CoTaskMemFree(memory); }
};

bool isWindowsReservedFilename(std::string_view filename) {
  const std::string lower = asciiLower(filename.substr(0, filename.find('.')));
  if (lower == "con" || lower == "prn" || lower == "aux" || lower == "nul" || lower == "clock$" || lower == "conin$" ||
      lower == "conout$")
    return true;
  return lower.size() == 4 && (lower.starts_with("com") || lower.starts_with("lpt")) && lower[3] >= '1' &&
         lower[3] <= '9';
}
#endif

bool validateNativeMountSyntax(const std::filesystem::path& path, std::string& error) {
#ifdef _WIN32
  const std::filesystem::path root_name = path.root_name();
  const std::filesystem::path root_directory = path.root_directory();
  const auto end = path.end();
  for (auto component = path.begin(); component != end; ++component) {
    if (component->empty() && std::next(component) == end) continue;
    if ((!root_name.empty() && *component == root_name) || (!root_directory.empty() && *component == root_directory))
      continue;
    const std::wstring_view value = component->native();
    if (value == L"." || value == L"..") continue;
    if (value.empty() || value.back() == L'.' || value.back() == L' ') {
      error = "mount path contains a Windows-reserved component";
      return false;
    }
    for (wchar_t character : value) {
      if ((character > 0 && character < 32) || character == L'<' || character == L'>' || character == L':' ||
          character == L'"' || character == L'|' || character == L'?' || character == L'*') {
        error = "mount path contains a Windows-reserved component";
        return false;
      }
    }
    if (isWindowsReservedFilename(pathToUtf8(*component))) {
      error = "mount path contains a Windows-reserved component";
      return false;
    }
  }
#else
  (void)path;
  (void)error;
#endif
  return true;
}

std::filesystem::path pathFromUtf8(std::string_view path) {
  const auto* begin = reinterpret_cast<const char8_t*>(path.data());
  return std::filesystem::path(std::u8string(begin, begin + path.size()));
}

bool resourcePathComponents(std::string_view path, std::vector<std::string>& out, bool allow_empty) {
  out.clear();
  if (path.empty()) return allow_empty;
  if (path.front() == '/' || path.front() == '\\') return false;
  std::string component;
  for (char character : path) {
    if (character == '\\' || character == '/') {
      if (component.empty()) continue;
      if (!pistoris::paths::isPortableResourcePathComponent(component)) return false;
      out.push_back(std::move(component));
      component.clear();
      continue;
    }
    if (character == '\0') return false;
    component.push_back(character);
  }
  if (!component.empty()) {
    if (!pistoris::paths::isPortableResourcePathComponent(component)) return false;
    out.push_back(std::move(component));
  }
  return allow_empty || !out.empty();
}

std::string joinPath(std::span<const std::string> components) {
  std::string path;
  for (const std::string& component : components) {
    if (!path.empty()) path.push_back('/');
    path += component;
  }
  return path;
}

template <class T>
pistoris::resource_io::ResourceIoResult<T> failure(
    ArxReturnCode code, ResourceIoOperation operation, std::string_view resource_path,
    const std::filesystem::path& native_path, ResourceMountMask mounts, std::string_view detail = {},
    std::source_location where = std::source_location::current()) noexcept {
  return pistoris::resource_io::detail::resourceIoFailure<T>(
      code, operation, resource_path, native_path, mounts, detail, where);
}

struct DirectoryListing {
  bool found = false;
  std::map<std::string, std::filesystem::path> entries;
};

ArxReturnCode listNativeDirectory(const std::filesystem::path& directory, DirectoryListing& out, std::string& error,
                                  ResourceIoFlags flags, std::string_view selected_key = {},
                                  bool include_symlinks = false) {
  out = {};
  std::error_code ec;
  std::filesystem::directory_iterator iterator(directory, ec);
  if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) return ARX_OK;
  if (ec) {
    error = ec.message();
    return ARX_RESOURCE_IO_STAT_FAILED;
  }
  out.found = true;
  const std::filesystem::directory_iterator end;
  for (; iterator != end; iterator.increment(ec)) {
    if (ec) {
      error = ec.message();
      return ARX_RESOURCE_IO_STAT_FAILED;
    }
    const std::filesystem::file_status status = iterator->symlink_status(ec);
    if (ec) {
      error = ec.message();
      return ARX_RESOURCE_IO_STAT_FAILED;
    }
    if (!include_symlinks && std::filesystem::is_symlink(status)) continue;
    const std::filesystem::path candidate = iterator->path();
    const std::string name = pathToUtf8(candidate.filename());
    const std::string key = asciiLower(name);
    auto [entry, inserted] = out.entries.try_emplace(key, candidate);
    if (!inserted) {
      const std::string previous = pathToUtf8(entry->second.filename());
      if (selected_key.empty() || selected_key == key) {
        error = std::format("case-colliding entries '{}' and '{}'", previous, name);
        if ((flags & kResourceIoRecoverCaseCollisions) == 0) return ARX_RESOURCE_IO_AMBIGUOUS_PATH;
        pistoris::log(ARX_LOG_WARN,
                      "Resource I/O recovered {} in '{}'; using deterministic lexical order",
                      error,
                      pathToUtf8(directory));
      }
      if (name < previous) entry->second = candidate;
    }
  }
  return ARX_OK;
}

struct MountedDirectory {
  const ResourceMount* mount = nullptr;
  std::filesystem::path path;
};

bool resolveDirectories(std::span<const ResourceMount> mounts, ResourceMountMask selected,
                        std::span<const std::string> components, std::vector<MountedDirectory>& out,
                        ResourceIoFlags flags, ArxReturnCode& error_code, ResourceMountMask& error_mount,
                        std::filesystem::path& error_path, std::string& error) {
  out.clear();
  for (const ResourceMount& mount : mounts)
    if ((mount.id & selected) != 0) out.push_back({&mount, mount.path});
  for (const std::string& component : components) {
    std::vector<MountedDirectory> next;
    bool kind_selected = false;
    bool directory_selected = false;
    for (const MountedDirectory& current : out) {
      DirectoryListing listing;
      const std::string key = asciiLower(component);
      error_code = listNativeDirectory(current.path, listing, error, flags, key);
      if (error_code != ARX_OK) {
        error_mount = current.mount->id;
        error_path = current.path;
        return false;
      }
      const auto entry = listing.entries.find(key);
      if (!listing.found || entry == listing.entries.end()) continue;
      std::error_code ec;
      const bool directory = std::filesystem::is_directory(entry->second, ec);
      if (ec) {
        error_code = ARX_RESOURCE_IO_STAT_FAILED;
        error_mount = current.mount->id;
        error_path = entry->second;
        error = ec.message();
        return false;
      }
      if (!kind_selected) {
        kind_selected = true;
        directory_selected = directory;
        if (!directory) {
          out.clear();
          return true;
        }
      }
      if (directory_selected && directory) next.push_back({current.mount, entry->second});
    }
    out = std::move(next);
    if (out.empty()) return true;
  }
  return true;
}

bool normalizeMount(const std::filesystem::path& requested, std::filesystem::path& normalized, bool& exists,
                    std::string& error) {
  if (requested.empty()) {
    error = "mount path is empty";
    return false;
  }
  if (!validateNativeMountSyntax(requested, error)) return false;
  if (!pistoris::resource_io::detail::canonicalizeProspectivePath(requested, normalized, exists, error)) return false;
  std::error_code ec;
  if (exists && !std::filesystem::is_directory(normalized, ec)) {
    error = ec ? ec.message() : "mount path has a file ancestor";
    return false;
  }
  return true;
}

bool containsSymlink(const std::filesystem::path& path, bool& found, std::string& error) {
  found = false;
  std::filesystem::path current = path.root_path();
  for (const std::filesystem::path& component : path.relative_path()) {
    current /= component;
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::symlink_status(current, ec);
    if (ec == std::errc::no_such_file_or_directory) return true;
    if (ec) {
      error = ec.message();
      return false;
    }
    if (std::filesystem::is_symlink(status)) {
      found = true;
      return true;
    }
  }
  return true;
}

ResourceDirectoryEntry::Kind entryKind(const std::filesystem::path& path, bool directory) {
  if (directory) return ResourceDirectoryEntry::Kind::kDirectory;
  const std::string extension = asciiLower(pathToUtf8(path.extension()));
  if (extension == ".ftl") return ResourceDirectoryEntry::Kind::kFtl;
  if (extension == ".tea") return ResourceDirectoryEntry::Kind::kTea;
  if (extension == ".fts") return ResourceDirectoryEntry::Kind::kFts;
  if (extension == ".dlf") return ResourceDirectoryEntry::Kind::kDlf;
  if (extension == ".llf") return ResourceDirectoryEntry::Kind::kLlf;
  if (extension == ".amb") return ResourceDirectoryEntry::Kind::kAmb;
  if (extension == ".cin") return ResourceDirectoryEntry::Kind::kCin;
  if (extension == ".glb") return ResourceDirectoryEntry::Kind::kGlb;
  if (extension == ".obj") return ResourceDirectoryEntry::Kind::kObj;
  if (extension == ".mtl") return ResourceDirectoryEntry::Kind::kMtl;
  if (extension == ".json") return ResourceDirectoryEntry::Kind::kJson;
  if (extension == ".png") return ResourceDirectoryEntry::Kind::kPng;
  if (extension == ".jpg" || extension == ".jpeg") return ResourceDirectoryEntry::Kind::kJpeg;
  if (extension == ".bmp") return ResourceDirectoryEntry::Kind::kBmp;
  if (extension == ".tga") return ResourceDirectoryEntry::Kind::kTga;
  if (extension == ".wav") return ResourceDirectoryEntry::Kind::kWav;
  if (extension == ".mp3") return ResourceDirectoryEntry::Kind::kMp3;
  if (extension == ".ogg") return ResourceDirectoryEntry::Kind::kOgg;
  return ResourceDirectoryEntry::Kind::kUnknownFile;
}

}  // namespace

namespace pistoris::resource_io {

struct ResourceMounts::Data {
  std::vector<ResourceMount> reads;
  std::optional<std::filesystem::path> write;
};

ResourceMounts::ResourceMounts() : data_(std::make_unique<Data>()) {}
ResourceMounts::ResourceMounts(std::unique_ptr<Data> data) noexcept : data_(std::move(data)) {}
ResourceMounts::~ResourceMounts() = default;
ResourceMounts::ResourceMounts(const ResourceMounts& other)
    : data_(other.data_ ? std::make_unique<Data>(*other.data_) : nullptr) {}
ResourceMounts::ResourceMounts(ResourceMounts&&) noexcept = default;
ResourceMounts& ResourceMounts::operator=(const ResourceMounts& other) {
  if (this != &other) {
    auto replacement = other.data_ ? std::make_unique<Data>(*other.data_) : nullptr;
    data_ = std::move(replacement);
  }
  return *this;
}
ResourceMounts& ResourceMounts::operator=(ResourceMounts&&) noexcept = default;

ResourceIoResult<ResourceMounts> ResourceMounts::open(const ResourceMountOptions& options,
                                                      MountValidationReport* report) noexcept {
  if (report) report->messages.clear();
  try {
    ResourceMounts mounts;
    auto reads = mounts.addReadMounts(options.read_mounts, report, true);
    if (!reads) return std::move(reads).propagate<ResourceMounts>();
    auto write = mounts.setWriteMountInternal(options.write_mount, report);
    if (!write) return std::move(write).propagate<ResourceMounts>();
    return ResourceIoResult<ResourceMounts>::success(std::move(mounts));
  } catch (const std::bad_alloc&) {
    return failure<ResourceMounts>(ARX_BAD_ALLOC, ResourceIoOperation::kOpenMount, {}, {}, 0);
  } catch (...) {
    return failure<ResourceMounts>(ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kOpenMount, {}, {}, 0);
  }
}

ResourceIoResult<void> ResourceMounts::addReadMounts(std::span<const std::filesystem::path> paths,
                                                     MountValidationReport* report, bool report_duplicates) noexcept {
  if (!data_) return failure<void>(ARX_INVALID_STATE, ResourceIoOperation::kOpenMount, {}, {}, 0);
  try {
    auto replacement = std::make_unique<Data>(*data_);
    for (const std::filesystem::path& requested : paths) {
      std::filesystem::path normalized;
      bool exists = false;
      std::string error;
      if (!normalizeMount(requested, normalized, exists, error))
        return failure<void>(ARX_RESOURCE_IO_INVALID_MOUNT, ResourceIoOperation::kOpenMount, {}, requested, 0, error);
      if (!exists) {
        if (report) report->messages.push_back({MountValidationKind::kMissingReadMount, normalized});
        continue;
      }

      bool duplicate = false;
      std::error_code ec;
      for (const ResourceMount& mount : replacement->reads) {
        if (std::filesystem::equivalent(mount.path, normalized, ec) && !ec) {
          duplicate = true;
          break;
        }
        ec.clear();
      }
      if (duplicate) {
        if (report && report_duplicates)
          report->messages.push_back({MountValidationKind::kDuplicateReadMount, normalized});
        continue;
      }
      if (replacement->reads.size() == kMaximumReadMounts)
        return failure<void>(ARX_RESOURCE_IO_TOO_MANY_MOUNTS, ResourceIoOperation::kOpenMount, {}, normalized, 0);
      replacement->reads.push_back({ResourceMountMask{1} << replacement->reads.size(), std::move(normalized)});
    }
    data_ = std::move(replacement);
    return ResourceIoResult<void>::success();
  } catch (const std::bad_alloc&) {
    return failure<void>(ARX_BAD_ALLOC, ResourceIoOperation::kOpenMount, {}, {}, 0);
  } catch (...) {
    return failure<void>(ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kOpenMount, {}, {}, 0);
  }
}

ResourceIoResult<void> ResourceMounts::addReadMount(const std::filesystem::path& path,
                                                    MountValidationReport* report) noexcept {
  if (report) report->messages.clear();
  return addReadMounts(std::span(&path, 1), report, false);
}

ResourceIoResult<void> ResourceMounts::setWriteMountInternal(const std::optional<std::filesystem::path>& path,
                                                             MountValidationReport* report) noexcept {
  if (!data_) return failure<void>(ARX_INVALID_STATE, ResourceIoOperation::kOpenMount, {}, {}, 0);
  try {
    std::optional<std::filesystem::path> normalized;
    bool exists = false;
    if (path) {
      std::filesystem::path value;
      std::string error;
      if (!normalizeMount(*path, value, exists, error))
        return failure<void>(ARX_RESOURCE_IO_INVALID_MOUNT, ResourceIoOperation::kOpenMount, {}, *path, 0, error);
      normalized = std::move(value);
    }

    auto replacement = std::make_unique<Data>(*data_);
    replacement->write = std::move(normalized);
    if (replacement->write && !exists && report)
      report->messages.push_back({MountValidationKind::kProspectiveWriteMount, *replacement->write});
    data_ = std::move(replacement);
    return ResourceIoResult<void>::success();
  } catch (const std::bad_alloc&) {
    return failure<void>(ARX_BAD_ALLOC, ResourceIoOperation::kOpenMount, {}, {}, 0);
  } catch (...) {
    return failure<void>(ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kOpenMount, {}, {}, 0);
  }
}

ResourceIoResult<void> ResourceMounts::setWriteMount(const std::optional<std::filesystem::path>& path,
                                                     MountValidationReport* report) noexcept {
  if (report) report->messages.clear();
  return setWriteMountInternal(path, report);
}

ResourceIoResult<std::filesystem::path> libertatisResourceRoot() noexcept {
  try {
#ifdef _WIN32
    const ComInitialization initialization;
    if (FAILED(initialization.result()) && initialization.result() != RPC_E_CHANGED_MODE)
      return failure<std::filesystem::path>(
          ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kOpenMount, {}, {}, 0, "COM initialization failed");
    PWSTR saved_games = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_SavedGames, KF_FLAG_DONT_VERIFY, nullptr, &saved_games);
    const std::unique_ptr<wchar_t, CoTaskMemDeleter> saved_games_owner(saved_games);
    std::filesystem::path root;
    if (SUCCEEDED(result) && saved_games_owner)
      root = std::filesystem::path(saved_games_owner.get()) / L"Arx Libertatis";
    if (root.empty())
      return failure<std::filesystem::path>(
          ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kOpenMount, {}, {}, 0, "Saved Games folder is unavailable");
    return ResourceIoResult<std::filesystem::path>::success(std::move(root));
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home || !*home)
      return failure<std::filesystem::path>(
          ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kOpenMount, {}, {}, 0, "HOME is unavailable");
    return ResourceIoResult<std::filesystem::path>::success(std::filesystem::path(home) / "Library" /
                                                            "Application Support" / "ArxLibertatis");
#else
    if (const char* data_home = std::getenv("XDG_DATA_HOME"); data_home && *data_home) {
      std::filesystem::path root(data_home);
      if (root.is_absolute()) return ResourceIoResult<std::filesystem::path>::success(root / "arx");
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
      std::filesystem::path root(home);
      if (root.is_absolute())
        return ResourceIoResult<std::filesystem::path>::success(root / ".local" / "share" / "arx");
    }
    return failure<std::filesystem::path>(ARX_RESOURCE_IO_STAT_FAILED,
                                          ResourceIoOperation::kOpenMount,
                                          {},
                                          {},
                                          0,
                                          "no absolute XDG_DATA_HOME or HOME is available");
#endif
  } catch (const std::bad_alloc&) {
    return failure<std::filesystem::path>(ARX_BAD_ALLOC, ResourceIoOperation::kOpenMount, {}, {}, 0);
  } catch (...) {
    return failure<std::filesystem::path>(ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kOpenMount, {}, {}, 0);
  }
}

ResourceIoResult<void> ResourceMounts::addLibertatisMounts(MountValidationReport* report) noexcept {
  if (report) report->messages.clear();
  try {
    auto root = libertatisResourceRoot();
    if (!root) return std::move(root).propagate<void>();
    const std::array paths{*root, *root / "unpacked"};
    return addReadMounts(paths, report, false);
  } catch (const std::bad_alloc&) {
    return failure<void>(ARX_BAD_ALLOC, ResourceIoOperation::kOpenMount, {}, {}, 0);
  } catch (...) {
    return failure<void>(ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kOpenMount, {}, {}, 0);
  }
}

ResourceIoResult<void> ResourceMounts::setLibertatisWriteMount(MountValidationReport* report) noexcept {
  if (report) report->messages.clear();
  try {
    auto root = libertatisResourceRoot();
    if (!root) return std::move(root).propagate<void>();
    const std::optional<std::filesystem::path> write_mount(std::move(*root));
    return setWriteMountInternal(write_mount, report);
  } catch (const std::bad_alloc&) {
    return failure<void>(ARX_BAD_ALLOC, ResourceIoOperation::kOpenMount, {}, {}, 0);
  } catch (...) {
    return failure<void>(ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kOpenMount, {}, {}, 0);
  }
}

std::span<const ResourceMount> ResourceMounts::readMounts() const noexcept {
  return data_ ? std::span<const ResourceMount>(data_->reads) : std::span<const ResourceMount>();
}

const std::optional<std::filesystem::path>& ResourceMounts::writeMount() const noexcept {
  static const std::optional<std::filesystem::path> kNoWriteMount;
  return data_ ? data_->write : kNoWriteMount;
}

ResourceMountMask ResourceMounts::availableMounts() const noexcept {
  if (!data_) return 0;
  ResourceMountMask result = 0;
  for (const ResourceMount& mount : data_->reads) result |= mount.id;
  return result;
}

std::optional<ResourceMountMask> ResourceMounts::highestPriorityMountId(ResourceMountMask mount_mask) const noexcept {
  const ResourceMountMask available = mount_mask & availableMounts();
  if (available == 0) return std::nullopt;
  return ResourceMountMask{1} << std::countr_zero(available);
}

ResourceIoResult<ResourceRead> ResourceMounts::read(std::string_view logical_path,
                                                    const ResourceLookupOptions& options) const noexcept {
  auto resolved = resolve(logical_path, options);
  if (!resolved) return std::move(resolved).propagate<ResourceRead>();
  try {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(resolved->native_path, ec);
    if (ec || !std::in_range<std::size_t>(size) || !std::in_range<std::streamsize>(size))
      return failure<ResourceRead>(ARX_RESOURCE_IO_STAT_FAILED,
                                   ResourceIoOperation::kRead,
                                   std::string(logical_path),
                                   resolved->native_path,
                                   resolved->mount_id,
                                   ec.message());
    ResourceRead result;
    result.native_path = resolved->native_path;
    result.mount_id = resolved->mount_id;
    result.data.resize(static_cast<std::size_t>(size));
    std::ifstream file(result.native_path, std::ios::binary);
    if (!file)
      return failure<ResourceRead>(ARX_RESOURCE_IO_OPEN_FAILED,
                                   ResourceIoOperation::kRead,
                                   std::string(logical_path),
                                   result.native_path,
                                   result.mount_id);
    if (!result.data.empty())
      file.read(reinterpret_cast<char*>(result.data.data()), static_cast<std::streamsize>(result.data.size()));
    if (!file)
      return failure<ResourceRead>(ARX_RESOURCE_IO_READ_FAILED,
                                   ResourceIoOperation::kRead,
                                   std::string(logical_path),
                                   result.native_path,
                                   result.mount_id);
    return ResourceIoResult<ResourceRead>::success(std::move(result));
  } catch (const std::bad_alloc&) {
    return failure<ResourceRead>(
        ARX_BAD_ALLOC, ResourceIoOperation::kRead, logical_path, resolved->native_path, resolved->mount_id);
  } catch (...) {
    return failure<ResourceRead>(ARX_RESOURCE_IO_READ_FAILED,
                                 ResourceIoOperation::kRead,
                                 logical_path,
                                 resolved->native_path,
                                 resolved->mount_id);
  }
}

ResourceIoResult<ResolvedResource> ResourceMounts::resolve(std::string_view logical_path,
                                                           const ResourceLookupOptions& options) const noexcept {
  const ResourceMountMask mount_mask = options.mount_mask;
  const ResourceIoFlags flags = options.flags;
  try {
    if (!validFlags(flags))
      return failure<ResolvedResource>(ARX_INVALID_OPTIONS, ResourceIoOperation::kRead, logical_path, {}, mount_mask);
    if (!data_)
      return failure<ResolvedResource>(ARX_INVALID_STATE, ResourceIoOperation::kRead, logical_path, {}, mount_mask);
    std::vector<std::string> components;
    if (!resourcePathComponents(logical_path, components, false))
      return failure<ResolvedResource>(
          ARX_RESOURCE_IO_INVALID_PATH, ResourceIoOperation::kRead, std::string(logical_path), {}, mount_mask);
    std::vector<MountedDirectory> directories;
    ArxReturnCode error_code = ARX_OK;
    ResourceMountMask error_mount = 0;
    std::filesystem::path error_path;
    std::string error;
    if (!resolveDirectories(data_->reads,
                            mount_mask,
                            std::span(components).first(components.size() - 1),
                            directories,
                            flags,
                            error_code,
                            error_mount,
                            error_path,
                            error))
      return failure<ResolvedResource>(
          error_code, ResourceIoOperation::kRead, std::string(logical_path), error_path, error_mount, error);
    for (const MountedDirectory& directory : directories) {
      DirectoryListing listing;
      const std::string key = asciiLower(components.back());
      error_code = listNativeDirectory(directory.path, listing, error, flags, key);
      if (error_code != ARX_OK)
        return failure<ResolvedResource>(error_code,
                                         ResourceIoOperation::kRead,
                                         std::string(logical_path),
                                         directory.path,
                                         directory.mount->id,
                                         error);
      const auto entry = listing.entries.find(key);
      if (!listing.found || entry == listing.entries.end()) continue;
      std::error_code ec;
      if (std::filesystem::is_regular_file(entry->second, ec))
        return ResourceIoResult<ResolvedResource>::success({entry->second, directory.mount->id});
      if (ec)
        return failure<ResolvedResource>(ARX_RESOURCE_IO_STAT_FAILED,
                                         ResourceIoOperation::kRead,
                                         std::string(logical_path),
                                         entry->second,
                                         directory.mount->id,
                                         ec.message());
      break;
    }
    return failure<ResolvedResource>(
        ARX_RESOURCE_IO_NOT_FOUND, ResourceIoOperation::kRead, std::string(logical_path), {}, mount_mask);
  } catch (const std::bad_alloc&) {
    return failure<ResolvedResource>(ARX_BAD_ALLOC, ResourceIoOperation::kRead, logical_path, {}, mount_mask);
  } catch (...) {
    return failure<ResolvedResource>(
        ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kRead, logical_path, {}, mount_mask);
  }
}

ResourceIoResult<std::filesystem::path> ResourceMounts::resolveWritePath(std::string_view logical_path,
                                                                         ResourceIoFlags flags) const noexcept {
  try {
    if (!validFlags(flags))
      return failure<std::filesystem::path>(ARX_INVALID_OPTIONS, ResourceIoOperation::kWrite, logical_path, {}, 0);
    if (!data_)
      return failure<std::filesystem::path>(ARX_INVALID_STATE, ResourceIoOperation::kWrite, logical_path, {}, 0);
    std::vector<std::string> components;
    if (!data_->write || !resourcePathComponents(logical_path, components, false)) {
      const ArxReturnCode code = data_->write ? static_cast<ArxReturnCode>(ARX_RESOURCE_IO_INVALID_PATH)
                                              : static_cast<ArxReturnCode>(ARX_INVALID_STATE);
      return failure<std::filesystem::path>(code, ResourceIoOperation::kWrite, std::string(logical_path), {}, 0);
    }
    std::filesystem::path result = *data_->write;
    std::string error;
    for (std::size_t index = 0; index < components.size(); ++index) {
      DirectoryListing listing;
      const std::string key = asciiLower(components[index]);
      const ArxReturnCode listing_result = listNativeDirectory(result, listing, error, flags, key, true);
      if (listing_result != ARX_OK)
        return failure<std::filesystem::path>(
            listing_result, ResourceIoOperation::kWrite, std::string(logical_path), result, 0, error);
      const auto entry = listing.entries.find(key);
      if (!listing.found || entry == listing.entries.end()) {
        for (; index < components.size(); ++index) result /= pathFromUtf8(components[index]);
        break;
      }
      std::error_code ec;
      const std::filesystem::file_status status = std::filesystem::symlink_status(entry->second, ec);
      if (ec)
        return failure<std::filesystem::path>(ARX_RESOURCE_IO_STAT_FAILED,
                                              ResourceIoOperation::kWrite,
                                              std::string(logical_path),
                                              entry->second,
                                              0,
                                              ec.message());
      if (std::filesystem::is_symlink(status))
        return failure<std::filesystem::path>(ARX_RESOURCE_IO_INVALID_PATH,
                                              ResourceIoOperation::kWrite,
                                              std::string(logical_path),
                                              entry->second,
                                              0,
                                              "write path contains a symbolic link");
      result = entry->second;
    }
    bool symlink = false;
    if (!containsSymlink(result, symlink, error))
      return failure<std::filesystem::path>(
          ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kWrite, std::string(logical_path), result, 0, error);
    if (symlink)
      return failure<std::filesystem::path>(ARX_RESOURCE_IO_INVALID_PATH,
                                            ResourceIoOperation::kWrite,
                                            std::string(logical_path),
                                            result,
                                            0,
                                            "write path contains a symbolic link");
    return ResourceIoResult<std::filesystem::path>::success(std::move(result));
  } catch (const std::bad_alloc&) {
    return failure<std::filesystem::path>(ARX_BAD_ALLOC, ResourceIoOperation::kWrite, logical_path, {}, 0);
  } catch (...) {
    return failure<std::filesystem::path>(
        ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kWrite, logical_path, {}, 0);
  }
}

ResourceIoResult<void> ResourceMounts::write(std::string_view logical_path, std::span<const std::uint8_t> data,
                                             ResourceIoFlags flags) const noexcept {
  auto path = resolveWritePath(logical_path, flags);
  if (!path) return std::move(path).propagate<void>();
  try {
    if (!std::in_range<std::streamsize>(data.size()))
      return failure<void>(ARX_RESOURCE_IO_WRITE_FAILED,
                           ResourceIoOperation::kWrite,
                           std::string(logical_path),
                           *path,
                           0,
                           "resource is too large to write");
    std::error_code ec;
    std::filesystem::create_directories(path->parent_path(), ec);
    if (ec)
      return failure<void>(
          ARX_RESOURCE_IO_WRITE_FAILED, ResourceIoOperation::kWrite, std::string(logical_path), *path, 0, ec.message());
    std::ofstream file(*path, std::ios::binary | std::ios::trunc);
    if (!file)
      return failure<void>(
          ARX_RESOURCE_IO_OPEN_FAILED, ResourceIoOperation::kWrite, std::string(logical_path), *path, 0);
    if (!data.empty())
      file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file)
      return failure<void>(
          ARX_RESOURCE_IO_WRITE_FAILED, ResourceIoOperation::kWrite, std::string(logical_path), *path, 0);
    return ResourceIoResult<void>::success();
  } catch (const std::bad_alloc&) {
    return failure<void>(ARX_BAD_ALLOC, ResourceIoOperation::kWrite, logical_path, *path, 0);
  } catch (...) {
    return failure<void>(ARX_RESOURCE_IO_WRITE_FAILED, ResourceIoOperation::kWrite, logical_path, *path, 0);
  }
}

ResourceIoResult<std::vector<ResourceFile>> ResourceMounts::enumerate(
    std::string_view logical_directory, std::uint32_t max_depth, const ResourceLookupOptions& options) const noexcept {
  const ResourceMountMask mount_mask = options.mount_mask;
  const ResourceIoFlags flags = options.flags;
  try {
    if (!validFlags(flags))
      return failure<std::vector<ResourceFile>>(
          ARX_INVALID_OPTIONS, ResourceIoOperation::kListDirectory, logical_directory, {}, mount_mask);
    if (!data_)
      return failure<std::vector<ResourceFile>>(
          ARX_INVALID_STATE, ResourceIoOperation::kListDirectory, logical_directory, {}, mount_mask);
    std::vector<std::string> components;
    if (!resourcePathComponents(logical_directory, components, true))
      return failure<std::vector<ResourceFile>>(ARX_RESOURCE_IO_INVALID_PATH,
                                                ResourceIoOperation::kListDirectory,
                                                std::string(logical_directory),
                                                {},
                                                mount_mask);
    if (max_depth == 0) return ResourceIoResult<std::vector<ResourceFile>>::success({});
    struct Pending {
      std::string logical;
      std::uint32_t depth = 0;
      ResourceMountMask mounts = 0;
    };
    std::map<std::string, ResourceFile> selected;
    std::vector<Pending> pending = {{joinPath(components), 0, mount_mask}};
    while (!pending.empty()) {
      Pending current = std::move(pending.back());
      pending.pop_back();
      auto entries = listDirectory(current.logical, {.mount_mask = current.mounts, .flags = flags});
      if (!entries) return std::move(entries).propagate<std::vector<ResourceFile>>();
      for (const ResourceDirectoryEntry& entry : *entries) {
        std::string logical = current.logical;
        if (!logical.empty()) logical.push_back('/');
        logical += entry.name;
        if (entry.kind == ResourceDirectoryEntry::Kind::kDirectory) {
          if (current.depth + 1 < max_depth)
            pending.push_back({std::move(logical), current.depth + 1, entry.mount_mask});
          continue;
        }
        const std::string key = asciiLower(logical);
        selected.try_emplace(key, ResourceFile{std::move(logical), entry.mount_mask});
      }
    }
    std::vector<ResourceFile> result;
    result.reserve(selected.size());
    for (auto& [unused, file] : selected) result.push_back(std::move(file));
    return ResourceIoResult<std::vector<ResourceFile>>::success(std::move(result));
  } catch (const std::bad_alloc&) {
    return failure<std::vector<ResourceFile>>(
        ARX_BAD_ALLOC, ResourceIoOperation::kListDirectory, logical_directory, {}, mount_mask);
  } catch (...) {
    return failure<std::vector<ResourceFile>>(
        ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kListDirectory, logical_directory, {}, mount_mask);
  }
}

ResourceIoResult<std::vector<ResourceDirectoryEntry>> ResourceMounts::listDirectory(
    std::string_view logical_directory, const ResourceLookupOptions& options) const noexcept {
  const ResourceMountMask mount_mask = options.mount_mask;
  const ResourceIoFlags flags = options.flags;
  try {
    if (!validFlags(flags))
      return failure<std::vector<ResourceDirectoryEntry>>(
          ARX_INVALID_OPTIONS, ResourceIoOperation::kListDirectory, logical_directory, {}, mount_mask);
    if (!data_)
      return failure<std::vector<ResourceDirectoryEntry>>(
          ARX_INVALID_STATE, ResourceIoOperation::kListDirectory, logical_directory, {}, mount_mask);
    std::vector<std::string> components;
    if (!resourcePathComponents(logical_directory, components, true))
      return failure<std::vector<ResourceDirectoryEntry>>(ARX_RESOURCE_IO_INVALID_PATH,
                                                          ResourceIoOperation::kListDirectory,
                                                          std::string(logical_directory),
                                                          {},
                                                          mount_mask);
    std::vector<MountedDirectory> directories;
    ArxReturnCode error_code = ARX_OK;
    ResourceMountMask error_mount = 0;
    std::filesystem::path error_path;
    std::string error;
    if (!resolveDirectories(
            data_->reads, mount_mask, components, directories, flags, error_code, error_mount, error_path, error))
      return failure<std::vector<ResourceDirectoryEntry>>(error_code,
                                                          ResourceIoOperation::kListDirectory,
                                                          std::string(logical_directory),
                                                          error_path,
                                                          error_mount,
                                                          error);
    std::map<std::string, ResourceDirectoryEntry> selected;
    for (const MountedDirectory& directory : directories) {
      DirectoryListing listing;
      error_code = listNativeDirectory(directory.path, listing, error, flags);
      if (error_code != ARX_OK)
        return failure<std::vector<ResourceDirectoryEntry>>(error_code,
                                                            ResourceIoOperation::kListDirectory,
                                                            std::string(logical_directory),
                                                            directory.path,
                                                            directory.mount->id,
                                                            error);
      for (const auto& [key, native] : listing.entries) {
        std::error_code ec;
        const bool directory_entry = std::filesystem::is_directory(native, ec);
        if (ec)
          return failure<std::vector<ResourceDirectoryEntry>>(ARX_RESOURCE_IO_STAT_FAILED,
                                                              ResourceIoOperation::kListDirectory,
                                                              std::string(logical_directory),
                                                              native,
                                                              directory.mount->id,
                                                              ec.message());
        if (!directory_entry && !std::filesystem::is_regular_file(native, ec)) {
          if (ec)
            return failure<std::vector<ResourceDirectoryEntry>>(ARX_RESOURCE_IO_STAT_FAILED,
                                                                ResourceIoOperation::kListDirectory,
                                                                std::string(logical_directory),
                                                                native,
                                                                directory.mount->id,
                                                                ec.message());
          continue;
        }
        const auto kind = entryKind(native, directory_entry);
        auto [entry, inserted] =
            selected.try_emplace(key, ResourceDirectoryEntry{pathToUtf8(native.filename()), kind, directory.mount->id});
        if (!inserted && entry->second.kind == kind) entry->second.mount_mask |= directory.mount->id;
      }
    }
    std::vector<ResourceDirectoryEntry> result;
    result.reserve(selected.size());
    for (auto& [unused, entry] : selected) result.push_back(std::move(entry));
    return ResourceIoResult<std::vector<ResourceDirectoryEntry>>::success(std::move(result));
  } catch (const std::bad_alloc&) {
    return failure<std::vector<ResourceDirectoryEntry>>(
        ARX_BAD_ALLOC, ResourceIoOperation::kListDirectory, logical_directory, {}, mount_mask);
  } catch (...) {
    return failure<std::vector<ResourceDirectoryEntry>>(
        ARX_RESOURCE_IO_STAT_FAILED, ResourceIoOperation::kListDirectory, logical_directory, {}, mount_mask);
  }
}

}  // namespace pistoris::resource_io
