// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/parsing.h"

#include "console/diagnostics.h"
#include "modules/module.h"

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <system_error>

namespace cli::modules {

bool parseFloat(const char* text, float& out) {
  if (!text) return false;
  char* end = nullptr;
  out = std::strtof(text, &end);
  return end != text && *end == '\0';
}

bool parseDouble(const char* text, double& out) {
  if (!text) return false;
  char* end = nullptr;
  out = std::strtod(text, &end);
  return end != text && *end == '\0';
}

bool parseSize(const char* text, std::size_t& out) {
  if (!text || *text == '\0') return false;
  for (const char* current = text; *current; ++current)
    if (*current < '0' || *current > '9') return false;

  errno = 0;
  char* end = nullptr;
  unsigned long long value = std::strtoull(text, &end, 10);
  if (errno == ERANGE || end == text || *end != '\0' ||
      value > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max()))
    return false;
  out = static_cast<std::size_t>(value);
  return true;
}

bool parseUint32(const char* text, std::uint32_t& out) {
  if (!text) return false;
  std::string_view input(text);
  std::uint32_t value = 0;
  auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value, 10);
  if (error != std::errc{} || end != input.data() + input.size()) return false;
  out = value;
  return true;
}

bool consumeFloat(ModuleParseContext& ctx, float& out, const char* name, const char* expected) {
  if (ctx.index + 1 >= ctx.argc || !parseFloat(ctx.argv[ctx.index + 1], out)) {
    diagnostic(DiagnosticCode::kInvalidNumber, "%s: expected %s", name, expected);
    return false;
  }
  ++ctx.index;
  return true;
}

bool consumeDouble(ModuleParseContext& ctx, double& out, const char* name, const char* expected) {
  if (ctx.index + 1 >= ctx.argc || !parseDouble(ctx.argv[ctx.index + 1], out)) {
    diagnostic(DiagnosticCode::kInvalidNumber, "%s: expected %s", name, expected);
    return false;
  }
  ++ctx.index;
  return true;
}

bool consumeFloats(ModuleParseContext& ctx, float* out, int count, const char* name, const char* expected) {
  if (ctx.index + count >= ctx.argc) {
    diagnostic(DiagnosticCode::kMissingValues, "%s: expected %d values", name, count);
    return false;
  }
  for (int offset = 0; offset < count; ++offset) {
    if (parseFloat(ctx.argv[ctx.index + 1 + offset], out[offset])) continue;
    diagnostic(DiagnosticCode::kInvalidNumber,
               "%s: value %d ('%s') is not %s",
               name,
               offset + 1,
               ctx.argv[ctx.index + 1 + offset],
               expected);
    return false;
  }
  ctx.index += count;
  return true;
}

}  // namespace cli::modules
