// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/resource_io/catalog.hpp"
#include "arx_pistoris/resource_io/location.hpp"
#include "arx_pistoris/resource_io/output.hpp"
#include "arx_pistoris/resource_io/resource_mounts.hpp"
#include "arx_pistoris/resource_io/resources.hpp"
#include "arx_pistoris/resource_io/status.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "base/ascii.h"
#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "io/files_internal.h"
#include "io/native_path.h"
#include "io/path_location.h"
#include "io/policy.h"
#include "io/service.h"
#include "media/encoded.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
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

}  // namespace

namespace cli {

struct IoService::MountState {
  MountState(const std::vector<std::string>& requested_mounts, std::string_view requested_write_mount,
             bool auto_mount) {
    std::vector<std::filesystem::path> reads;
    reads.reserve(requested_mounts.size());
    if (requested_mounts.empty()) reads.emplace_back(".");
    for (const std::string& requested : requested_mounts) {
      std::filesystem::path path;
      std::string error;
      if (!io_detail::pathFromUtf8(requested, path, error)) {
        diagnostic(DiagnosticCode::kIoStatFailed, "Invalid mount '%s': %s", requested.c_str(), error.c_str());
        return;
      }
      reads.push_back(std::move(path));
    }
    const std::string_view requested_write =
        requested_write_mount.empty() ? std::string_view{"."} : requested_write_mount;
    std::filesystem::path write;
    std::string error;
    if (!io_detail::pathFromUtf8(requested_write, write, error)) {
      diagnostic(DiagnosticCode::kIoStatFailed,
                 "Invalid write mount '%.*s': %s",
                 static_cast<int>(requested_write.size()),
                 requested_write.data(),
                 error.c_str());
      return;
    }
    configure(std::move(reads), write);
    if (!valid || !auto_mount) return;

    pistoris::resource_io::MountValidationReport report;
    auto added = resources.mounts().addLibertatisMounts(&report);
    logValidation(report);
    if (added) return;
    if (added.code() == ARX_RESOURCE_IO_STAT_FAILED) {
      log(ARX_LOG_WARN, "automatic game mounts are unavailable");
      return;
    }
    failConfiguration(added);
  }

  static void logValidation(const pistoris::resource_io::MountValidationReport& report) {
    for (const pistoris::resource_io::MountValidationMessage& message : report.messages) {
      const std::string display = io_detail::pathToUtf8(message.path);
      switch (message.kind) {
        case pistoris::resource_io::MountValidationKind::kMissingReadMount:
          log(ARX_LOG_WARN, "mount not found; excluding from reads: %s", display.c_str());
          break;
        case pistoris::resource_io::MountValidationKind::kDuplicateReadMount:
          log(ARX_LOG_INFO, "duplicate mount ignored: %s", display.c_str());
          break;
        case pistoris::resource_io::MountValidationKind::kProspectiveWriteMount:
          log(ARX_LOG_INFO, "write mount not found; will create on actual write: %s", display.c_str());
          break;
      }
    }
  }

  template <class T>
  void failConfiguration(const pistoris::resource_io::ResourceIoResult<T>& result) {
    const auto* error = result.error();
    const std::string description =
        error ? pistoris::resource_io::describeError(*error) : std::string(pistoris::errorString(result.code()));
    diagnostic(DiagnosticCode::kIoStatFailed, "Invalid resource mounts: %s", description.c_str());
    valid = false;
  }

  void configure(std::vector<std::filesystem::path> reads, std::filesystem::path write) {
    pistoris::resource_io::MountValidationReport report;
    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = std::move(reads), .write_mount = std::move(write)}, &report);
    logValidation(report);
    if (!opened) {
      failConfiguration(opened);
      return;
    }
    resources = pistoris::resource_io::Resources(std::move(*opened));
    valid = true;
  }

  bool useDefaultGameWriteMount() {
    pistoris::resource_io::MountValidationReport report;
    auto changed = resources.mounts().setLibertatisWriteMount(&report);
    logValidation(report);
    if (!changed) {
      const auto* error = changed.error();
      const std::string description =
          error ? pistoris::resource_io::describeError(*error) : std::string(pistoris::errorString(changed.code()));
      diagnostic(DiagnosticCode::kDefaultGameRootUnavailable,
                 "Automatic game write mount is unavailable: %s",
                 description.c_str());
      return false;
    }
    return true;
  }

  ResourceReadResult find(std::string_view resource_path, std::filesystem::path& out) {
    auto resolved = resources.mounts().resolve(resource_path);
    if (resolved) {
      out = std::move(resolved->native_path);
      return ResourceReadResult::kSuccess;
    }
    const ArxReturnCode code = resolved.code();
    if (code == ARX_RESOURCE_IO_INVALID_PATH) return ResourceReadResult::kInvalidPath;
    if (code == ARX_RESOURCE_IO_NOT_FOUND) return ResourceReadResult::kNotFound;
    return ResourceReadResult::kReadFailed;
  }

  ResourceEnumerationResult enumerate(std::string_view base_path, std::uint32_t max_depth,
                                      std::vector<std::string>& out) {
    out.clear();
    auto files = resources.mounts().enumerate(base_path, max_depth);
    if (!files)
      return files.code() == ARX_RESOURCE_IO_INVALID_PATH ? ResourceEnumerationResult::kInvalidPath
                                                          : ResourceEnumerationResult::kReadFailed;
    out.reserve(files->size());
    for (auto& file : *files) out.push_back(std::move(file.logical_path));
    return ResourceEnumerationResult::kSuccess;
  }

  bool outputPath(std::string_view resource_path, std::filesystem::path& out) {
    auto resolved = resources.mounts().resolveWritePath(resource_path);
    if (!resolved) return false;
    out = std::move(*resolved);
    return true;
  }

  pistoris::resource_io::Resources resources;
  bool valid = false;
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

ResourceReadResult IoService::readAudio(const PathLocation& base, std::string_view path, AudioLookupMode mode,
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
  if (mode == AudioLookupMode::kExact) return read_candidate(path);

  const ResourceReadResult authored = read_candidate(path);
  if (authored != ResourceReadResult::kNotFound) return authored;

  const std::span<const std::string_view> extensions = media::audioLookupExtensions();
  std::size_t preferred = extensions.size();
  for (std::size_t index = 0; index < extensions.size(); ++index) {
    if (endsWithAsciiInsensitive(path, extensions[index])) {
      preferred = index;
      break;
    }
  }

  std::string stem(path);
  if (preferred != extensions.size()) {
    stem.resize(stem.size() - extensions[preferred].size());
  }
  for (std::size_t index = 0; index < extensions.size(); ++index) {
    if (index == preferred) continue;
    std::string candidate = stem;
    candidate += extensions[index];
    const ResourceReadResult result = read_candidate(candidate);
    if (result != ResourceReadResult::kNotFound) return result;
  }
  out.clear();
  return ResourceReadResult::kNotFound;
}

bool IoService::hasReadMounts() const noexcept { return !mounts_->resources.mounts().readMounts().empty(); }

ResourceEnumerationResult IoService::enumerateResources(std::string_view base_path, std::uint32_t max_depth,
                                                        std::vector<std::string>& out) {
  return mounts_->enumerate(base_path, max_depth, out);
}

pistoris::resource_io::ResourceIoResult<pistoris::resource_io::ResourceCatalog> IoService::scanCatalog() const {
  return mounts_->resources.scanCatalog();
}

pistoris::resource_io::Resources& IoService::resources() noexcept { return mounts_->resources; }

const pistoris::resource_io::Resources& IoService::resources() const noexcept { return mounts_->resources; }

bool IoService::executeWritePlan(pistoris::resource_io::ResourceWritePlan& plan) {
  auto preflight = plan.preflight();
  if (!preflight) {
    const auto* error = preflight.error();
    diagnostic(DiagnosticCode::kResourceOutputInvalid,
               "Cannot inspect resource outputs: %s",
               error ? pistoris::resource_io::describeError(*error).c_str()
                     : pistoris::resource_io::errorString(preflight.code()));
    return false;
  }

  if (state_.dry_run) {
    for (const auto& entry : plan.entries()) {
      const std::string display = io_detail::pathToUtf8(entry.nativePath());
      if (entry.status() == pistoris::resource_io::ResourceWriteStatus::kNeedsCandidate) continue;
      if (entry.status() == pistoris::resource_io::ResourceWriteStatus::kAlreadyCurrent) {
        log(ARX_LOG_INFO, "dry-run: output already current: %s", display.c_str());
      } else {
        log(ARX_LOG_INFO, "dry-run: would write resource output to: %s", display.c_str());
      }
    }
    return true;
  }

  for (auto& entry : plan.entries()) {
    if (entry.status() == pistoris::resource_io::ResourceWriteStatus::kNeedsCandidate) {
      diagnostic(DiagnosticCode::kResourceOutputCollision,
                 "Resource output candidate was not selected: %s",
                 io_detail::pathToUtf8(entry.nativePath()).c_str());
      return false;
    }
    if (entry.status() != pistoris::resource_io::ResourceWriteStatus::kNeedsExistingFilePolicy) continue;
    const std::string display = io_detail::pathToUtf8(entry.nativePath());
    const bool overwrite = allowOverwrite(display.c_str());
    entry.setExistingFilePolicy(overwrite ? pistoris::resource_io::ExistingFilePolicy::kOverwrite
                                          : pistoris::resource_io::ExistingFilePolicy::kPreserve);
  }

  auto written = plan.execute();
  if (!written) {
    const auto* error = written.error();
    diagnostic(DiagnosticCode::kResourceOutputInvalid,
               "Cannot write resource outputs: %s",
               error ? pistoris::resource_io::describeError(*error).c_str()
                     : pistoris::resource_io::errorString(written.code()));
    return false;
  }
  for (const auto& entry : plan.entries()) {
    const std::string display = io_detail::pathToUtf8(entry.nativePath());
    switch (entry.status()) {
      case pistoris::resource_io::ResourceWriteStatus::kWritten:
        log(ARX_LOG_INFO, "written: %s", display.c_str());
        break;
      case pistoris::resource_io::ResourceWriteStatus::kPreserved:
        log(ARX_LOG_INFO, "skipped existing output: %s", display.c_str());
        break;
      default:
        break;
    }
  }
  return true;
}

ResourceEnumerationResult IoService::enumerateFiles(const PathLocation& directory, std::uint32_t max_depth,
                                                    std::vector<EnumeratedFile>& out) {
  out.clear();
  if (max_depth == 0) return ResourceEnumerationResult::kSuccess;
  if (directory.address == PathAddress::kMountRelative) {
    std::string normalized_directory;
    if (!directory.path.empty()) {
      std::string error;
      if (!normalizeResourcePath(directory.path, normalized_directory, error))
        return ResourceEnumerationResult::kInvalidPath;
    }
    std::vector<std::string> paths;
    const ResourceEnumerationResult result = mounts_->enumerate(normalized_directory, max_depth, paths);
    if (result != ResourceEnumerationResult::kSuccess) return result;
    out.reserve(paths.size());
    for (std::string& path : paths) {
      const std::size_t relative_begin = normalized_directory.empty() ? 0 : normalized_directory.size() + 1U;
      std::string relative = path.substr(relative_begin);
      out.push_back({std::move(relative), {.path = std::move(path), .address = PathAddress::kMountRelative}});
    }
  } else {
    std::filesystem::path native;
    std::string error;
    if (!io_detail::pathFromUtf8(directory.path, native, error) ||
        !io_detail::validateNativePathSyntax(native, error)) {
      return ResourceEnumerationResult::kInvalidPath;
    }
    std::error_code ec;
    std::filesystem::directory_iterator initial(native, ec);
    if (ec) {
      return ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory
                 ? ResourceEnumerationResult::kSuccess
                 : ResourceEnumerationResult::kReadFailed;
    }

    struct PendingDirectory {
      std::filesystem::path path;
      std::filesystem::path relative;
      std::uint32_t depth = 0;
    };
    std::vector<PendingDirectory> pending = {{native, {}, 0}};
    while (!pending.empty()) {
      PendingDirectory current = std::move(pending.back());
      pending.pop_back();
      std::filesystem::directory_iterator it(current.path, ec);
      if (ec) {
        out.clear();
        return ResourceEnumerationResult::kReadFailed;
      }
      const std::filesystem::directory_iterator end;
      for (; it != end; it.increment(ec)) {
        if (ec) {
          out.clear();
          return ResourceEnumerationResult::kReadFailed;
        }
        const std::filesystem::directory_entry& entry = *it;
        const std::filesystem::file_status status = entry.symlink_status(ec);
        if (ec) {
          out.clear();
          return ResourceEnumerationResult::kReadFailed;
        }
        if (std::filesystem::is_symlink(status)) continue;
        const std::filesystem::path relative = current.relative / entry.path().filename();
        if (std::filesystem::is_directory(status)) {
          if (current.depth + 1U < max_depth) pending.push_back({entry.path(), relative, current.depth + 1U});
          continue;
        }
        if (!std::filesystem::is_regular_file(status)) continue;
        out.push_back({.relative_path = normalizeResourceSeparators(io_detail::pathToUtf8(relative)),
                       .location = {.path = io_detail::pathToUtf8(entry.path().lexically_normal()),
                                    .address = PathAddress::kAbsolute}});
      }
    }
  }
  std::ranges::sort(out, [](const EnumeratedFile& lhs, const EnumeratedFile& rhs) {
    if (resourcePathLess(lhs.relative_path, rhs.relative_path)) return true;
    if (resourcePathLess(rhs.relative_path, lhs.relative_path)) return false;
    return lhs.relative_path < rhs.relative_path;
  });
  return ResourceEnumerationResult::kSuccess;
}

ResourceEnumerationResult IoService::enumerateFiles(const PathLocation& directory, std::vector<PathLocation>& out) {
  std::vector<EnumeratedFile> files;
  const ResourceEnumerationResult result = enumerateFiles(directory, 1, files);
  out.clear();
  if (result != ResourceEnumerationResult::kSuccess) return result;
  out.reserve(files.size());
  for (EnumeratedFile& file : files) out.push_back(std::move(file.location));
  return ResourceEnumerationResult::kSuccess;
}

}  // namespace cli
