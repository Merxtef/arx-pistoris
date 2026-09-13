// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime/types.h"

#include "base/ascii.h"
#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "io/default_mounts.h"
#include "io/files_internal.h"
#include "io/native_path.h"
#include "io/path_location.h"
#include "io/policy.h"
#include "io/service.h"
#include "media/encoded.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

bool resourcePathComponents(std::string_view path, std::vector<std::string>& out, bool allow_current = false) {
  if (path.empty() || path.front() == '/' || path.front() == '\\') return false;
  std::filesystem::path ignored;
  std::string path_error;
  if (!cli::io_detail::pathFromUtf8(path, ignored, path_error)) return false;

  std::string component;
  component.reserve(path.size());
  for (char c : path) {
    if (c == '\0') return false;
    if (c == '/' || c == '\\') {
      if (component.empty()) continue;
      if (allow_current && component == ".") {
        component.clear();
        continue;
      }
      if (!pistoris::paths::isPortableResourcePathComponent(component)) return false;
      out.push_back(component);
      component.clear();
      continue;
    }
    component.push_back(c);
  }

  if (!component.empty()) {
    if (allow_current && component == ".") return !out.empty();
    if (!pistoris::paths::isPortableResourcePathComponent(component)) return false;
    out.push_back(component);
  }
  return !out.empty();
}

std::string joinResourcePath(std::span<const std::string> components) {
  std::string out;
  for (const std::string& component : components) {
    if (!out.empty()) out.push_back('/');
    out += component;
  }
  return out;
}

bool normalizeMount(std::string_view requested, std::filesystem::path& normalized, bool& exists, std::string& error) {
  if (requested.empty()) {
    error = "mount path is empty";
    return false;
  }
  std::filesystem::path root;
  if (!cli::io_detail::pathFromUtf8(requested, root, error) || !cli::io_detail::validateNativePathSyntax(root, error)) {
    return false;
  }

  if (!cli::io_detail::resolveProspectiveNativePath(root, normalized, exists, error)) return false;
  if (exists) {
    std::error_code ec;
    if (!std::filesystem::is_directory(normalized, ec)) {
      error = ec ? ec.message() : "mount path is not a directory";
      return false;
    }
  }
  return true;
}

}  // namespace

namespace cli {

struct IoService::MountState {
  struct DirectoryListing {
    bool readable = false;
    bool failed = false;
    std::unordered_map<std::string, std::filesystem::path> entries;
  };

  MountState(const std::vector<std::string>& requested_mounts, std::string_view requested_write_mount,
             bool auto_mount) {
    configureWriteMount(requested_write_mount.empty() ? std::string_view{"."} : requested_write_mount);
    if (requested_mounts.empty()) {
      addReadMount(".", false);
    } else {
      for (const std::string& requested : requested_mounts) addReadMount(requested, false);
    }
    if (auto_mount) addDefaultReadMounts();
  }

  bool configureWriteMount(std::string_view requested) {
    bool exists = false;
    std::string error;
    std::filesystem::path normalized;
    if (!normalizeMount(requested, normalized, exists, error)) {
      diagnostic(DiagnosticCode::kIoStatFailed,
                 "Invalid write mount '%.*s': %s",
                 static_cast<int>(requested.size()),
                 requested.data(),
                 error.c_str());
      valid = false;
      write_root_valid = false;
      return false;
    }
    write_root = std::move(normalized);
    write_root_valid = true;
    if (!exists) {
      log(ARX_LOG_INFO,
          "write mount not found; will create on actual write: %.*s",
          static_cast<int>(requested.size()),
          requested.data());
    }
    return true;
  }

  void addReadMount(std::string_view requested, bool automatic) {
    std::filesystem::path normalized;
    bool exists = false;
    std::string error;
    if (!normalizeMount(requested, normalized, exists, error)) {
      if (automatic) {
        log(ARX_LOG_WARN,
            "automatic mount is invalid; excluding from reads: %.*s (%s)",
            static_cast<int>(requested.size()),
            requested.data(),
            error.c_str());
      } else {
        diagnostic(DiagnosticCode::kIoStatFailed,
                   "Invalid mount '%.*s': %s",
                   static_cast<int>(requested.size()),
                   requested.data(),
                   error.c_str());
        valid = false;
      }
      return;
    }
    if (!exists) {
      log(automatic ? ARX_LOG_INFO : ARX_LOG_WARN,
          automatic ? "automatic mount not found; excluding from reads: %.*s"
                    : "mount not found; excluding from reads: %.*s",
          static_cast<int>(requested.size()),
          requested.data());
      return;
    }

    std::error_code ec;
    for (const std::filesystem::path& existing : roots) {
      ec.clear();
      if (std::filesystem::equivalent(existing, normalized, ec) && !ec) {
        log(ARX_LOG_INFO, "duplicate mount ignored: %.*s", static_cast<int>(requested.size()), requested.data());
        return;
      }
    }
    roots.push_back(std::move(normalized));
  }

  void addDefaultReadMounts() {
    std::filesystem::path game_root;
    std::string error;
    if (!defaultGameResourceRoot(game_root, error)) {
      log(ARX_LOG_WARN, "automatic game mounts are unavailable: %s", error.c_str());
      return;
    }
    addReadMount(io_detail::pathToUtf8(game_root), true);
    addReadMount(io_detail::pathToUtf8(game_root / "unpacked"), true);
  }

  bool useDefaultGameWriteMount() {
    std::filesystem::path game_root;
    std::string error;
    if (!defaultGameResourceRoot(game_root, error)) {
      diagnostic(
          DiagnosticCode::kDefaultGameRootUnavailable, "Automatic game write mount is unavailable: %s", error.c_str());
      return false;
    }
    return configureWriteMount(io_detail::pathToUtf8(game_root));
  }

  ResourceReadResult find(std::string_view resource_path, std::filesystem::path& out) {
    std::vector<std::string> components;
    if (!resourcePathComponents(resource_path, components)) return ResourceReadResult::kInvalidPath;

    bool degraded = false;
    for (const std::filesystem::path& root : roots) {
      std::filesystem::path current = root;
      bool found = true;
      for (std::size_t index = 0; index < components.size(); ++index) {
        const DirectoryListing& directory = listing(current);
        if (directory.failed) {
          degraded = true;
          found = false;
          break;
        }
        auto entry = directory.entries.find(asciiLower(components[index]));
        if (!directory.readable || entry == directory.entries.end()) {
          found = false;
          break;
        }
        current = entry->second;

        if (index + 1 != components.size()) {
          std::error_code ec;
          if (!std::filesystem::is_directory(current, ec)) {
            if (ec) {
              degraded = true;
              const std::string display = io_detail::pathToUtf8(current);
              log(ARX_LOG_WARN, "cannot inspect mounted path %s: %s", display.c_str(), ec.message().c_str());
            }
            found = false;
            break;
          }
        }
      }
      if (!found) continue;

      std::error_code ec;
      if (std::filesystem::is_regular_file(current, ec)) {
        if (degraded) {
          const std::string display = io_detail::pathToUtf8(current);
          log(ARX_LOG_WARN,
              "using lower-priority mounted resource after an inspection failure: %.*s -> %s",
              static_cast<int>(resource_path.size()),
              resource_path.data(),
              display.c_str());
        }
        out = std::move(current);
        return ResourceReadResult::kSuccess;
      }
      if (ec) {
        degraded = true;
        const std::string display = io_detail::pathToUtf8(current);
        log(ARX_LOG_WARN, "cannot inspect mounted path %s: %s", display.c_str(), ec.message().c_str());
      }
    }
    return degraded ? ResourceReadResult::kReadFailed : ResourceReadResult::kNotFound;
  }

  ResourceEnumerationResult enumerate(std::string_view base_path, std::uint32_t max_depth,
                                      std::vector<std::string>& out) {
    out.clear();
    std::vector<std::string> components;
    if (!base_path.empty() && !resourcePathComponents(base_path, components))
      return ResourceEnumerationResult::kInvalidPath;
    if (max_depth == 0) return ResourceEnumerationResult::kSuccess;

    struct PendingDirectory {
      std::filesystem::path native_path;
      std::string logical_path;
      std::uint32_t depth = 0;
    };

    std::unordered_map<std::string, std::string> selected;
    const std::string logical_base = joinResourcePath(components);
    for (const std::filesystem::path& root : roots) {
      std::filesystem::path native_base = root;
      bool found = true;
      for (const std::string& component : components) {
        const DirectoryListing& directory = listing(native_base);
        if (directory.failed) {
          out.clear();
          return ResourceEnumerationResult::kReadFailed;
        }
        auto entry = directory.entries.find(asciiLower(component));
        if (!directory.readable || entry == directory.entries.end()) {
          found = false;
          break;
        }
        native_base = entry->second;
        std::error_code ec;
        if (!std::filesystem::is_directory(native_base, ec)) {
          if (ec) {
            out.clear();
            return ResourceEnumerationResult::kReadFailed;
          }
          found = false;
          break;
        }
      }
      if (!found) continue;

      std::vector<PendingDirectory> pending = {{native_base, logical_base, 0}};
      while (!pending.empty()) {
        PendingDirectory current = std::move(pending.back());
        pending.pop_back();
        const DirectoryListing& directory = listing(current.native_path);
        if (directory.failed) {
          out.clear();
          return ResourceEnumerationResult::kReadFailed;
        }
        if (!directory.readable) continue;

        for (const auto& [unused, native_path] : directory.entries) {
          const std::string name = io_detail::pathToUtf8(native_path.filename());
          std::string logical_path = current.logical_path;
          if (!logical_path.empty()) logical_path.push_back('/');
          logical_path += name;

          std::error_code ec;
          if (std::filesystem::is_directory(native_path, ec)) {
            if (current.depth + 1 < max_depth) {
              pending.push_back({native_path, std::move(logical_path), current.depth + 1});
            }
            continue;
          }
          if (ec) {
            out.clear();
            return ResourceEnumerationResult::kReadFailed;
          }
          if (!std::filesystem::is_regular_file(native_path, ec)) {
            if (ec) {
              out.clear();
              return ResourceEnumerationResult::kReadFailed;
            }
            continue;
          }
          selected.try_emplace(asciiLower(logical_path), std::move(logical_path));
        }
      }
    }

    out.reserve(selected.size());
    for (auto& [unused, logical_path] : selected) out.push_back(std::move(logical_path));
    return ResourceEnumerationResult::kSuccess;
  }

  DirectoryListing& listing(const std::filesystem::path& directory) {
    const std::filesystem::path key = directory.lexically_normal();
    auto [cached, inserted] = listings.try_emplace(key);
    if (!inserted) return cached->second;

    DirectoryListing& result = cached->second;
    std::error_code ec;
    std::filesystem::directory_iterator it(directory, ec);
    if (ec) {
      if (ec != std::errc::no_such_file_or_directory && ec != std::errc::not_a_directory) {
        result.failed = true;
        const std::string display = io_detail::pathToUtf8(directory);
        log(ARX_LOG_WARN, "cannot inspect mounted directory: %s", display.c_str());
      }
      return result;
    }
    result.readable = true;

    const std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
      if (ec) {
        result.failed = true;
        const std::string display = io_detail::pathToUtf8(directory);
        log(ARX_LOG_WARN, "cannot finish inspecting mounted directory: %s", display.c_str());
        break;
      }

      const std::filesystem::path candidate = it->path();
      const std::string actual_name = io_detail::pathToUtf8(candidate.filename());
      const std::string resource_name = asciiLower(actual_name);
      auto [entry, unique] = result.entries.emplace(resource_name, candidate);
      if (unique || entry->second == candidate) continue;

      const std::string previous_name = io_detail::pathToUtf8(entry->second.filename());
      if (actual_name < previous_name) entry->second = candidate;
      const std::string display = io_detail::pathToUtf8(directory);
      log(ARX_LOG_WARN,
          "case-insensitive mount collision in %s: %s and %s",
          display.c_str(),
          previous_name.c_str(),
          actual_name.c_str());
    }
    return result;
  }

  bool outputPath(std::string_view resource_path, std::filesystem::path& out) {
    if (!write_root_valid) return false;

    std::vector<std::string> components;
    if (!resourcePathComponents(resource_path, components)) return false;

    std::filesystem::path current = write_root;
    for (std::size_t index = 0; index < components.size(); ++index) {
      const DirectoryListing& directory = listing(current);
      auto existing = directory.entries.find(asciiLower(components[index]));
      if (directory.readable && existing != directory.entries.end()) {
        current = existing->second;
        continue;
      }
      for (; index < components.size(); ++index) {
        std::filesystem::path native_component;
        std::string error;
        if (!io_detail::pathFromUtf8(components[index], native_component, error)) return false;
        current /= native_component;
      }
      break;
    }
    out = std::move(current);
    return true;
  }

  std::vector<std::filesystem::path> roots;
  std::unordered_map<std::filesystem::path, DirectoryListing> listings;
  std::filesystem::path write_root;
  bool valid = true;
  bool write_root_valid = false;
};

IoService::IoService(OverwriteMode overwrite, bool dry_run, const std::vector<std::string>& read_mounts,
                     std::string_view write_mount, bool auto_mount)
    : state_{overwrite, dry_run}, mounts_(std::make_unique<MountState>(read_mounts, write_mount, auto_mount)) {}

IoService::~IoService() = default;

bool IoService::valid() const noexcept { return mounts_->valid; }

bool IoService::useDefaultGameWriteMount() { return mounts_->useDefaultGameWriteMount(); }

bool IoService::normalizeResourcePath(std::string_view resource_path, std::string& out, std::string& error) const {
  out.clear();
  error.clear();
  std::vector<std::string> components;
  if (!resourcePathComponents(resource_path, components, true)) {
    error = "path is not a portable relative resource path";
    return false;
  }
  for (const std::string& component : components) {
    if (!out.empty()) out.push_back('/');
    out += component;
  }
  return true;
}

bool IoService::resolvePathLocation(std::string_view requested, PathLocation& out, std::string& error) const {
  out = {};
  std::filesystem::path native;
  if (requested.empty() || !io_detail::pathFromUtf8(requested, native, error) ||
      !io_detail::validateNativePathSyntax(native, error)) {
    if (error.empty()) error = "path is empty";
    return false;
  }

  if (native.is_absolute()) {
    out.path = io_detail::pathToUtf8(native.lexically_normal());
    out.address = OutputAddress::kAbsolute;
    return true;
  }
  if (native.has_root_name() || native.has_root_directory()) {
    error = "drive-relative and root-relative paths are ambiguous";
    return false;
  }
  return normalizeResourcePath(requested, out.path, error);
}

bool IoService::parentPathLocation(const PathLocation& location, PathLocation& out, std::string& error) const {
  out = {};
  out.address = location.address;
  error.clear();
  if (location.address == OutputAddress::kMountRelative) {
    const std::size_t separator = location.path.find_last_of('/');
    if (separator != std::string::npos) out.path = location.path.substr(0, separator);
    return true;
  }

  std::filesystem::path native;
  if (!io_detail::pathFromUtf8(location.path, native, error)) return false;
  out.path = io_detail::pathToUtf8(native.parent_path());
  return true;
}

bool IoService::appendPathLocation(const PathLocation& base, std::string_view resource_path, PathLocation& out,
                                   std::string& error) const {
  std::string normalized;
  if (!normalizeResourcePath(resource_path, normalized, error)) return false;
  out.address = base.address;
  if (base.address == OutputAddress::kMountRelative) {
    out.path = base.path;
    if (!out.path.empty()) out.path.push_back('/');
    out.path += normalized;
    return true;
  }

  std::filesystem::path native;
  if (!io_detail::pathFromUtf8(base.path, native, error)) return false;
  std::vector<std::string> components;
  if (!resourcePathComponents(normalized, components)) {
    error = "path is not a portable relative resource path";
    return false;
  }
  for (const std::string& component : components) {
    std::filesystem::path native_component;
    if (!io_detail::pathFromUtf8(component, native_component, error)) return false;
    native /= native_component;
  }
  out.path = io_detail::pathToUtf8(native);
  return true;
}

bool IoService::hasPathComponent(const PathLocation& location, std::string_view component, bool& out,
                                 std::string& error) const {
  out = false;
  std::string normalized;
  if (!normalizeResourcePath(component, normalized, error) || normalized.find('/') != std::string::npos) {
    if (error.empty()) error = "component must be one portable filename";
    return false;
  }
  if (location.address == PathAddress::kMountRelative) {
    std::size_t begin = 0;
    while (begin < location.path.size()) {
      const std::size_t end = location.path.find('/', begin);
      const std::string_view candidate = std::string_view(location.path).substr(begin, end - begin);
      if (equalAsciiInsensitive(candidate, normalized)) {
        out = true;
        return true;
      }
      if (end == std::string::npos) break;
      begin = end + 1;
    }
    return true;
  }

  std::filesystem::path native;
  if (!io_detail::pathFromUtf8(location.path, native, error)) return false;
  for (const std::filesystem::path& part : native) {
    if (equalAsciiInsensitive(io_detail::pathToUtf8(part), normalized)) {
      out = true;
      return true;
    }
  }
  return true;
}

bool IoService::resolveOutputLocation(std::string_view requested, OutputLocation& out, std::string& error) const {
  return resolvePathLocation(requested, out, error);
}

bool IoService::isAbsolutePath(std::string_view requested, bool& out, std::string& error) const {
  PathLocation location;
  if (!resolvePathLocation(requested, location, error)) return false;
  out = location.address == PathAddress::kAbsolute;
  return true;
}

bool IoService::writeResource(std::string_view resource_path, const void* data, std::size_t size) {
  std::filesystem::path resolved;
  if (!mounts_->outputPath(resource_path, resolved)) {
    diagnostic(DiagnosticCode::kResourcePathInvalid,
               "Cannot resolve resource output in the write mount: %.*s",
               static_cast<int>(resource_path.size()),
               resource_path.data());
    return false;
  }

  return writeNativeFile(resolved, data, size);
}

ResourceReadResult IoService::readResource(std::string_view resource_path, std::vector<std::uint8_t>& out,
                                           std::string* resolved_path) {
  out.clear();
  if (resolved_path) resolved_path->clear();

  std::filesystem::path resolved;
  ResourceReadResult result = mounts_->find(resource_path, resolved);
  if (result != ResourceReadResult::kSuccess) return result;
  if (resolved_path) *resolved_path = io_detail::pathToUtf8(resolved);
  return io_detail::readFileSized(resolved, out, true) ? ResourceReadResult::kSuccess : ResourceReadResult::kReadFailed;
}

ResourceReadResult IoService::readPath(const PathLocation& location, std::vector<std::uint8_t>& out,
                                       std::string* resolved_path) {
  if (location.address == PathAddress::kMountRelative) return readResource(location.path, out, resolved_path);

  out.clear();
  if (resolved_path) resolved_path->clear();
  std::filesystem::path native;
  std::string error;
  if (!io_detail::pathFromUtf8(location.path, native, error) || !io_detail::validateNativePathSyntax(native, error)) {
    return ResourceReadResult::kInvalidPath;
  }
  std::error_code ec;
  if (!std::filesystem::exists(native, ec)) {
    return ec ? ResourceReadResult::kReadFailed : ResourceReadResult::kNotFound;
  }
  if (resolved_path) *resolved_path = io_detail::pathToUtf8(native);
  return io_detail::readFileSized(native, out, true) ? ResourceReadResult::kSuccess : ResourceReadResult::kReadFailed;
}

ResourceReadResult IoService::readPath(std::string_view requested, std::vector<std::uint8_t>& out,
                                       std::string* resolved_path) {
  PathLocation location;
  std::string error;
  if (!resolvePathLocation(requested, location, error)) return ResourceReadResult::kInvalidPath;
  return readPath(location, out, resolved_path);
}

ResourceReadResult IoService::readImage(const PathLocation& base, std::string_view path, ImageLookupMode mode,
                                        std::vector<std::uint8_t>& out, std::string* selected_path,
                                        std::string* resolved_path) {
  if (selected_path) selected_path->clear();
  if (resolved_path) resolved_path->clear();

  const auto read_candidate = [&](std::string_view candidate) {
    PathLocation location;
    std::string error;
    if (!appendPathLocation(base, candidate, location, error)) return ResourceReadResult::kInvalidPath;
    const ResourceReadResult result = readPath(location, out, resolved_path);
    if (result != ResourceReadResult::kNotFound && selected_path) *selected_path = candidate;
    return result;
  };
  if (mode == ImageLookupMode::kExact) return read_candidate(path);

  for (std::string_view extension : media::imageLookupExtensions()) {
    std::string candidate(path);
    candidate += extension;
    const ResourceReadResult result = read_candidate(candidate);
    if (result != ResourceReadResult::kNotFound) return result;
  }
  out.clear();
  return ResourceReadResult::kNotFound;
}

bool IoService::hasReadMounts() const noexcept { return !mounts_->roots.empty(); }

ResourceEnumerationResult IoService::enumerateResources(std::string_view base_path, std::uint32_t max_depth,
                                                        std::vector<std::string>& out) {
  return mounts_->enumerate(base_path, max_depth, out);
}

ResourceEnumerationResult IoService::enumerateFiles(const PathLocation& directory, std::vector<PathLocation>& out) {
  out.clear();
  if (directory.address == PathAddress::kMountRelative) {
    std::vector<std::string> paths;
    const ResourceEnumerationResult result = mounts_->enumerate(directory.path, 1, paths);
    if (result != ResourceEnumerationResult::kSuccess) return result;
    out.reserve(paths.size());
    for (std::string& path : paths) out.push_back({.path = std::move(path), .address = PathAddress::kMountRelative});
  } else {
    std::filesystem::path native;
    std::string error;
    if (!io_detail::pathFromUtf8(directory.path, native, error) ||
        !io_detail::validateNativePathSyntax(native, error)) {
      return ResourceEnumerationResult::kInvalidPath;
    }
    std::error_code ec;
    std::filesystem::directory_iterator it(native, ec);
    if (ec) {
      return ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory
                 ? ResourceEnumerationResult::kSuccess
                 : ResourceEnumerationResult::kReadFailed;
    }
    const std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
      if (ec) {
        out.clear();
        return ResourceEnumerationResult::kReadFailed;
      }
      const std::filesystem::directory_entry& entry = *it;
      if (!entry.is_regular_file(ec)) {
        if (ec) {
          out.clear();
          return ResourceEnumerationResult::kReadFailed;
        }
        continue;
      }
      out.push_back(
          {.path = io_detail::pathToUtf8(entry.path().lexically_normal()), .address = PathAddress::kAbsolute});
    }
  }
  std::ranges::sort(out, [](const PathLocation& lhs, const PathLocation& rhs) {
    if (resourcePathLess(lhs.path, rhs.path)) return true;
    if (resourcePathLess(rhs.path, lhs.path)) return false;
    return lhs.path < rhs.path;
  });
  return ResourceEnumerationResult::kSuccess;
}

}  // namespace cli
