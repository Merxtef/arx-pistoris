// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "io.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>

namespace {

bool fileExists(const char* path) {
  std::error_code ec;
  return std::filesystem::exists(path, ec);
}

bool readFileSized(const char* path, std::vector<std::uint8_t>& out, bool quiet) {
  std::error_code ec;
  std::uintmax_t file_size = std::filesystem::file_size(path, ec);
  if (ec) {
    if (!quiet) std::fprintf(stderr, "Cannot stat: %s\n", path);
    return false;
  }
  if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
    if (!quiet) std::fprintf(stderr, "File too large: %s\n", path);
    return false;
  }

  try {
    out.resize(static_cast<std::size_t>(file_size));
  } catch (const std::bad_alloc&) {
    if (!quiet) std::fprintf(stderr, "Unable to allocate space for: %s\n", path);
    return false;
  }

  std::ifstream f(path, std::ios::binary);
  if (!f) {
    if (!quiet) std::fprintf(stderr, "Cannot open: %s\n", path);
    return false;
  }

  if (!out.empty()) f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
  if (!f) {
    if (!quiet) std::fprintf(stderr, "Failed to read: %s\n", path);
    return false;
  }
  return true;
}

bool writeFileRaw(const char* path, const void* data, std::size_t size) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    std::fprintf(stderr, "Cannot create: %s\n", path);
    return false;
  }
  if (size != 0) f.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
  if (!f) {
    std::fprintf(stderr, "Write failed: %s\n", path);
    return false;
  }
  return true;
}

bool askOverwrite(cli::State& state, const char* path) {
  while (true) {
    std::fprintf(stderr, "Overwrite existing file '%s'? [y]es/[n]o/[a]ll/[s]kip all: ", path);

    char buf[16] = {};
    if (!std::fgets(buf, sizeof(buf), stdin)) return false;

    switch (buf[0]) {
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

bool readFile(const char* path, std::vector<std::uint8_t>& out) { return readFileSized(path, out, false); }

bool readFileOptional(const char* path, std::vector<std::uint8_t>& out) { return readFileSized(path, out, true); }

bool writeFile(cli::State& state, const char* path, const void* data, std::size_t size) {
  if (fileExists(path)) {
    bool overwrite = false;
    switch (state.overwrite) {
      case cli::OverwriteMode::kAsk:
        overwrite = askOverwrite(state, path);
        break;
      case cli::OverwriteMode::kAlwaysYes:
        overwrite = true;
        break;
      case cli::OverwriteMode::kAlwaysNo:
        overwrite = false;
        break;
    }

    if (!overwrite) {
      std::printf("Skipped: %s\n", path);
      return true;
    }
  }

  if (!writeFileRaw(path, data, size)) return false;
  std::printf("Written: %s\n", path);
  return true;
}

bool isFtl(const std::vector<std::uint8_t>& buf) { return buf.size() >= 4 && std::memcmp(buf.data(), "FTL", 4) == 0; }

bool isTea(const std::vector<std::uint8_t>& buf) {
  constexpr std::size_t kN = sizeof(pistoris::kTeaMagic) - 1;
  return buf.size() >= kN && std::memcmp(buf.data(), pistoris::kTeaMagic, kN) == 0;
}

bool isGlb(const std::vector<std::uint8_t>& buf) { return buf.size() >= 4 && std::memcmp(buf.data(), "glTF", 4) == 0; }

std::string sanitizeFilename(std::string_view name) {
  std::string out;
  out.reserve(name.size());
  for (char c : name) {
    bool ok =
        (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
    out += ok ? c : '_';
  }
  while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
  return out;
}

const char* pathFilename(const char* path) {
  const char* last = path;
  for (const char* p = path; *p; ++p)
    if (*p == '/' || *p == '\\') last = p + 1;
  return last;
}

const char* fileExtension(const char* path) {
  const char* dot = nullptr;
  const char* p;
  for (p = path; *p; ++p) {
    if (*p == '/' || *p == '\\')
      dot = nullptr;
    else if (*p == '.')
      dot = p;
  }
  return dot ? dot : p;
}
