// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "io/files_internal.h"
#include "io/native_path.h"
#include "io/path_location.h"
#include "io/paths.h"
#include "io/policy.h"
#include "io/service.h"

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

std::string lowerAscii(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    out.push_back(c);
  }
  return out;
}

bool resourcePathComponents(std::string_view path, std::vector<std::string>& out, bool allow_current = false) {
  if (path.empty() || path.front() == '/' || path.front() == '\\') return false;
  std::filesystem::path ignored;
  std::string path_error;
  if (!cli::io_detail::pathFromUtf8(path, ignored, path_error)) return false;

  const auto valid_component = [](std::string_view component) {
    if (component.empty() || component == ".." || component == "." || component.back() == '.' ||
        component.back() == ' ' || isPortableReservedFilename(component)) {
      return false;
    }
    for (unsigned char value : component) {
      if (value < 32) return false;
      switch (value) {
        case '<':
        case '>':
        case ':':
        case '"':
        case '|':
        case '?':
        case '*':
          return false;
        default:
          break;
      }
    }
    return true;
  };

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
      if (!valid_component(component)) return false;
      out.push_back(component);
      component.clear();
      continue;
    }
    component.push_back(c);
  }

  if (!component.empty()) {
    if (allow_current && component == ".") return !out.empty();
    if (!valid_component(component)) return false;
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

  explicit MountState(const std::vector<std::string>& requested_mounts) {
    const std::vector<std::string> default_mounts = {"."};
    const std::vector<std::string>& effective_mounts = requested_mounts.empty() ? default_mounts : requested_mounts;
    for (std::size_t index = 0; index < effective_mounts.size(); ++index) {
      const std::string& requested = effective_mounts[index];
      std::filesystem::path normalized;
      bool exists = false;
      std::string error;
      if (!normalizeMount(requested, normalized, exists, error)) {
        diagnostic(DiagnosticCode::kIoStatFailed, "Invalid mount '%s': %s", requested.c_str(), error.c_str());
        valid = false;
        continue;
      }
      if (index == 0) {
        write_root = normalized;
        write_root_valid = true;
      }
      if (!exists) {
        if (index == 0) {
          log(ARX_LOG_INFO,
              "write mount not found; using as write-only and will create on actual write: %s",
              requested.c_str());
        } else {
          log(ARX_LOG_WARN, "mount not found; excluding from readable mounts: %s", requested.c_str());
        }
        continue;
      }

      bool duplicate = false;
      std::error_code ec;
      for (const std::filesystem::path& existing : roots) {
        ec.clear();
        if (std::filesystem::equivalent(existing, normalized, ec) && !ec) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        log(ARX_LOG_INFO, "duplicate mount ignored: %s", requested.c_str());
        continue;
      }
      roots.push_back(std::move(normalized));
    }
  }

  ResourceReadResult find(std::string_view resource_path, std::filesystem::path& out) {
    std::vector<std::string> components;
    if (!resourcePathComponents(resource_path, components)) return ResourceReadResult::kInvalidPath;

    for (const std::filesystem::path& root : roots) {
      std::filesystem::path current = root;
      bool found = true;
      for (std::size_t index = 0; index < components.size(); ++index) {
        const DirectoryListing& directory = listing(current);
        auto entry = directory.entries.find(lowerAscii(components[index]));
        if (!directory.readable || entry == directory.entries.end()) {
          found = false;
          break;
        }
        current = entry->second;

        if (index + 1 != components.size()) {
          std::error_code ec;
          if (!std::filesystem::is_directory(current, ec)) {
            found = false;
            break;
          }
        }
      }
      if (!found) continue;

      std::error_code ec;
      if (std::filesystem::is_regular_file(current, ec)) {
        out = std::move(current);
        return ResourceReadResult::kSuccess;
      }
    }
    return ResourceReadResult::kNotFound;
  }

  ResourceEnumerationResult enumerate(std::string_view base_path, std::uint32_t max_depth,
                                      std::vector<std::string>& out) {
    out.clear();
    std::vector<std::string> components;
    if (!resourcePathComponents(base_path, components)) return ResourceEnumerationResult::kInvalidPath;
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
        auto entry = directory.entries.find(lowerAscii(component));
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
          logical_path.push_back('/');
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
          selected.try_emplace(lowerAscii(logical_path), std::move(logical_path));
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
      const std::string resource_name = lowerAscii(actual_name);
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
      auto existing = directory.entries.find(lowerAscii(components[index]));
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

IoService::IoService(OverwriteMode overwrite, bool dry_run, const std::vector<std::string>& mounts)
    : state_{overwrite, dry_run}, mounts_(std::make_unique<MountState>(mounts)) {}

IoService::~IoService() = default;

bool IoService::valid() const noexcept { return mounts_->valid; }

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
    diagnostic(DiagnosticCode::kIoCreateFailed,
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

bool IoService::hasReadMounts() const noexcept { return !mounts_->roots.empty(); }

ResourceEnumerationResult IoService::enumerateResources(std::string_view base_path, std::uint32_t max_depth,
                                                        std::vector<std::string>& out) {
  return mounts_->enumerate(base_path, max_depth, out);
}

}  // namespace cli
