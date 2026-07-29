// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "io/native_path.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#ifdef _WIN32
#include "io/paths.h"

#include <iterator>
#include <limits>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>  // IWYU pragma: keep

#else
#include <unistd.h>
#endif

namespace cli::io_detail {
namespace {

bool validUtf8(std::string_view text) {
  const auto continuation = [](unsigned char value) { return value >= 0x80 && value <= 0xbf; };
  for (std::size_t index = 0; index < text.size();) {
    const unsigned char first = static_cast<unsigned char>(text[index++]);
    if (first <= 0x7f) continue;

    if (first >= 0xc2 && first <= 0xdf) {
      if (index >= text.size() || !continuation(static_cast<unsigned char>(text[index]))) return false;
      ++index;
      continue;
    }

    if (first >= 0xe0 && first <= 0xef) {
      if (index + 1 >= text.size()) return false;
      const unsigned char second = static_cast<unsigned char>(text[index++]);
      const unsigned char third = static_cast<unsigned char>(text[index++]);
      if (!continuation(third) || (first == 0xe0 && (second < 0xa0 || second > 0xbf)) ||
          (first == 0xed && (second < 0x80 || second > 0x9f)) ||
          (first != 0xe0 && first != 0xed && !continuation(second))) {
        return false;
      }
      continue;
    }

    if (first >= 0xf0 && first <= 0xf4) {
      if (index + 2 >= text.size()) return false;
      const unsigned char second = static_cast<unsigned char>(text[index++]);
      const unsigned char third = static_cast<unsigned char>(text[index++]);
      const unsigned char fourth = static_cast<unsigned char>(text[index++]);
      if (!continuation(third) || !continuation(fourth) || (first == 0xf0 && (second < 0x90 || second > 0xbf)) ||
          (first == 0xf4 && (second < 0x80 || second > 0x8f)) ||
          (first != 0xf0 && first != 0xf4 && !continuation(second))) {
        return false;
      }
      continue;
    }

    return false;
  }
  return true;
}

#ifdef _WIN32
bool utf8ToWide(std::string_view utf8, std::wstring& out) {
  out.clear();
  if (utf8.empty()) return true;
  if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;

  const int input_size = static_cast<int>(utf8.size());
  // Windows APIs consume explicit lengths; null termination is not required
  // NOLINTBEGIN(bugprone-suspicious-stringview-data-usage)
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), input_size, nullptr, 0);
  if (size <= 0) return false;
  out.resize(static_cast<std::size_t>(size));
  const bool converted =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), input_size, out.data(), size) == size;
  // NOLINTEND(bugprone-suspicious-stringview-data-usage)
  return converted;
}

bool invalidWindowsComponent(std::wstring_view component) {
  if (component == L"." || component == L"..") return false;
  if (component.empty() || component.back() == L'.' || component.back() == L' ') return true;
  for (wchar_t value : component) {
    if (value > 0 && value < 32) return true;
    switch (value) {
      case L'<':
      case L'>':
      case L':':
      case L'"':
      case L'|':
      case L'?':
      case L'*':
        return true;
      default:
        break;
    }
  }

  std::string utf8;
  return !wideToUtf8(component, utf8) || isPortableReservedFilename(utf8);
}
#endif

}  // namespace

bool pathFromUtf8(std::string_view utf8, std::filesystem::path& out, std::string& error) {
  out.clear();
  error.clear();
  if (utf8.find('\0') != std::string_view::npos) {
    error = "path contains an embedded NUL";
    return false;
  }
  if (!validUtf8(utf8)) {
    error = "path is not valid UTF-8";
    return false;
  }
#ifdef _WIN32
  std::wstring wide;
  if (!utf8ToWide(utf8, wide)) {
    error = "path cannot be converted to a native Windows path";
    return false;
  }
  out = std::filesystem::path(std::move(wide));
#else
  out = std::filesystem::path(std::string(utf8));
#endif
  return true;
}

std::string pathToUtf8(const std::filesystem::path& path) {
#ifdef _WIN32
  std::string out;
  if (wideToUtf8(path.native(), out)) return out;
  return "<unprintable path>";
#else
  return path.native();
#endif
}

bool validateNativePathSyntax(const std::filesystem::path& path, std::string& error) {
  error.clear();
#ifdef _WIN32
  const std::filesystem::path root_name = path.root_name();
  const std::filesystem::path root_dir = path.root_directory();
  const auto end = path.end();
  for (auto it = path.begin(); it != end; ++it) {
    const std::filesystem::path& part = *it;
    if (part.empty() && std::next(it) == end) continue;
    if ((!root_name.empty() && part == root_name) || (!root_dir.empty() && part == root_dir)) continue;
    if (invalidWindowsComponent(part.native())) {
      error = "path contains a Windows-reserved component";
      return false;
    }
  }
#else
  (void)path;
#endif
  return true;
}

bool resolveProspectiveNativePath(const std::filesystem::path& path, std::filesystem::path& out, bool& exists,
                                  std::string& error) {
  out.clear();
  exists = false;
  error.clear();

  std::error_code ec;
  std::filesystem::path absolute = std::filesystem::absolute(path, ec).lexically_normal();
  if (ec) {
    error = ec.message();
    return false;
  }

  std::filesystem::path probe = absolute;
  std::vector<std::filesystem::path> suffix;
  while (!probe.empty()) {
    ec.clear();
    const std::filesystem::file_status link_status = std::filesystem::symlink_status(probe, ec);
    const bool missing = (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) ||
                         (!ec && link_status.type() == std::filesystem::file_type::not_found);
    if (!missing) {
      if (ec) {
        error = ec.message();
        return false;
      }

      std::filesystem::path canonical = std::filesystem::canonical(probe, ec);
      if (ec) {
        error = ec.message();
        return false;
      }
      if (!suffix.empty() && !std::filesystem::is_directory(canonical, ec)) {
        error = ec ? ec.message() : "path has a non-directory component";
        return false;
      }
      if (ec) {
        error = ec.message();
        return false;
      }

      out = std::move(canonical);
      for (auto it = suffix.rbegin(); it != suffix.rend(); ++it) out /= *it;
      exists = suffix.empty();
      return true;
    }

    suffix.push_back(probe.filename());
    const std::filesystem::path parent = probe.parent_path();
    if (parent == probe) break;
    probe = parent;
  }

  error = "path has no resolvable directory ancestor";
  return false;
}

bool replaceNativeFile(const std::filesystem::path& source, const std::filesystem::path& destination,
                       std::error_code& error) noexcept {
  error.clear();
#ifdef _WIN32
  if (MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    return true;
  }
  error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
  return false;
#else
  std::filesystem::rename(source, destination, error);
  return !error;
#endif
}

std::uint64_t processId() noexcept {
#ifdef _WIN32
  return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
  return static_cast<std::uint64_t>(getpid());
#endif
}

#ifdef _WIN32
bool wideToUtf8(std::wstring_view wide, std::string& out) {
  out.clear();
  if (wide.empty()) return true;
  if (wide.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;

  const int input_size = static_cast<int>(wide.size());
  // Windows APIs consume explicit lengths; null termination is not required
  // NOLINTBEGIN(bugprone-suspicious-stringview-data-usage)
  const int size =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), input_size, nullptr, 0, nullptr, nullptr);
  if (size <= 0) return false;
  out.resize(static_cast<std::size_t>(size));
  const bool converted =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), input_size, out.data(), size, nullptr, nullptr) ==
      size;
  // NOLINTEND(bugprone-suspicious-stringview-data-usage)
  return converted;
}
#endif

}  // namespace cli::io_detail
