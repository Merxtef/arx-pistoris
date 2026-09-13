// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "io/service.h"

#include "arx_pistoris/runtime/types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "io/native_path.h"
#include "io/path_location.h"
#include "io/policy.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace {

bool missingPathError(const std::error_code& error) { return error == std::errc::no_such_file_or_directory; }

bool writeFileRaw(const std::filesystem::path& path, const void* data, std::size_t size, const char* display_path) {
  if (size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
    cli::diagnostic(cli::DiagnosticCode::kIoFileTooLarge, "Output is too large: %s", display_path);
    return false;
  }
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    cli::diagnostic(cli::DiagnosticCode::kIoCreateFailed, "Cannot create temporary output for: %s", display_path);
    return false;
  }
  if (size != 0) file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
  file.close();
  if (!file) {
    cli::diagnostic(cli::DiagnosticCode::kIoWriteFailed, "Write failed: %s", display_path);
    return false;
  }
  return true;
}

bool inspectWriteTarget(const std::filesystem::path& requested, std::filesystem::path& resolved, bool& exists,
                        std::string& error) {
  if (!cli::io_detail::resolveProspectiveNativePath(requested, resolved, exists, error)) return false;
  if (!exists) return true;

  std::error_code ec;
  const std::filesystem::file_status link_status = std::filesystem::symlink_status(requested, ec);
  if (ec) {
    error = ec.message();
    return false;
  }
  if (std::filesystem::is_regular_file(resolved, ec)) return true;
  error = ec ? ec.message()
             : (std::filesystem::is_symlink(link_status) ? "output symlink does not resolve to a regular file"
                                                         : "output target is not a regular file");
  return false;
}

bool temporaryPathFor(const std::filesystem::path& target, std::filesystem::path& out, std::string& error) {
  static std::atomic<std::uint64_t> sequence = 0;
  for (unsigned attempt = 0; attempt < 1024; ++attempt) {
    std::filesystem::path candidate = target;
    candidate += ".arx-pistor.tmp.";
    candidate += std::to_string(cli::io_detail::processId());
    candidate += ".";
    candidate += std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));

    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::symlink_status(candidate, ec);
    if ((ec && missingPathError(ec)) || (!ec && status.type() == std::filesystem::file_type::not_found)) {
      out = std::move(candidate);
      return true;
    }
    if (ec) {
      error = ec.message();
      return false;
    }
  }
  error = "cannot allocate a unique temporary output name";
  return false;
}

class TemporaryFileCleanup {
 public:
  explicit TemporaryFileCleanup(std::filesystem::path path) : path_(std::move(path)) {}
  ~TemporaryFileCleanup() {
    if (!active_) return;
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }

  void release() noexcept { active_ = false; }

 private:
  std::filesystem::path path_;
  bool active_ = true;
};

bool askOverwrite(cli::State& state, const char* path) {
  while (true) {
    std::fprintf(stderr, "Overwrite existing file '%s'? [y]es/[n]o/[a]ll/[s]kip all: ", path);

    char buffer[16] = {};
    if (!std::fgets(buffer, sizeof(buffer), stdin)) {
      std::fputc('\n', stderr);
      cli::log(ARX_LOG_INFO, "no overwrite response; skipping %s (use --overwrite or --no-overwrite)", path);
      return false;
    }

    switch (buffer[0]) {
      case 'y':
      case 'Y':
        return true;
      case 'n':
      case 'N':
        return false;
      case 'a':
      case 'A':
        state.overwrite = cli::OverwriteMode::kAlwaysYes;
        return true;
      case 's':
      case 'S':
        state.overwrite = cli::OverwriteMode::kAlwaysNo;
        return false;
      default:
        std::fprintf(stderr, "Please answer y, n, a, or s\n");
        break;
    }
  }
}

}  // namespace

bool cli::IoService::writeFile(const char* path, const void* data, std::size_t size) {
  std::filesystem::path native_path;
  std::string error = path ? std::string() : std::string("path is null");
  if (!path || !io_detail::pathFromUtf8(path, native_path, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Invalid output path: %s", error.c_str());
    return false;
  }
  return writeNativeFile(native_path, data, size);
}

bool cli::IoService::writePath(const PathLocation& location, const void* data, std::size_t size) {
  return location.address == PathAddress::kMountRelative ? writeResource(location.path, data, size)
                                                         : writeFile(location.path.c_str(), data, size);
}

bool cli::IoService::writeNativeFile(const std::filesystem::path& path, const void* data, std::size_t size) {
  const std::string display_path = io_detail::pathToUtf8(path);
  std::string error;
  if (!io_detail::validateNativePathSyntax(path, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Invalid output path '%s': %s", display_path.c_str(), error.c_str());
    return false;
  }

  if (state_.dry_run) {
    log(ARX_LOG_INFO, "dry-run: would write %zu byte(s) to: %s", size, display_path.c_str());
    return true;
  }

  if (!path.parent_path().empty()) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      const std::string display = io_detail::pathToUtf8(path.parent_path());
      diagnostic(DiagnosticCode::kIoCreateFailed, "Cannot create output directory: %s", display.c_str());
      return false;
    }
  }

  std::filesystem::path target;
  bool target_exists = false;
  if (!inspectWriteTarget(path, target, target_exists, error)) {
    diagnostic(
        DiagnosticCode::kIoStatFailed, "Cannot resolve output path '%s': %s", display_path.c_str(), error.c_str());
    return false;
  }

  if (target_exists) {
    bool overwrite = false;
    switch (state_.overwrite) {
      case cli::OverwriteMode::kAsk:
        overwrite = askOverwrite(state_, display_path.c_str());
        break;
      case cli::OverwriteMode::kAlwaysYes:
        overwrite = true;
        break;
      case cli::OverwriteMode::kAlwaysNo:
        overwrite = false;
        break;
    }

    if (!overwrite) {
      log(ARX_LOG_INFO, "skipped existing output: %s", display_path.c_str());
      return true;
    }
  }

  std::filesystem::path temporary;
  if (!temporaryPathFor(target, temporary, error)) {
    diagnostic(DiagnosticCode::kIoCreateFailed,
               "Cannot prepare temporary output for '%s': %s",
               display_path.c_str(),
               error.c_str());
    return false;
  }

  TemporaryFileCleanup cleanup(temporary);
  if (!writeFileRaw(temporary, data, size, display_path.c_str())) return false;

  std::error_code ec;
  if (!io_detail::replaceNativeFile(temporary, target, ec)) {
    diagnostic(
        DiagnosticCode::kIoWriteFailed, "Cannot replace output '%s': %s", display_path.c_str(), ec.message().c_str());
    return false;
  }
  cleanup.release();
  log(ARX_LOG_INFO, "written: %s", display_path.c_str());
  return true;
}
