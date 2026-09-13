// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/logging.h"

#include "arx_pistoris/runtime/types.h"

#include "console/style.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace {

enum class LogDomain : std::uint8_t {
  kCli,
  kPistoris,
};

ArxLogLevel g_log_level = ARX_LOG_INFO;

const char* levelName(ArxLogLevel level) {
  switch (level) {
    case ARX_LOG_DEBUG:
      return "DEBUG";
    case ARX_LOG_INFO:
      return "INFO";
    case ARX_LOG_WARN:
      return "WARN";
    case ARX_LOG_ERROR:
      return "ERROR";
    default:
      return "?";
  }
}

const char* domainName(LogDomain domain) {
  switch (domain) {
    case LogDomain::kCli:
      return "CLI";
    case LogDomain::kPistoris:
      return "PISTORIS";
  }
  return "?";
}

cli::ConsoleStyle levelStyle(ArxLogLevel level) {
  switch (level) {
    case ARX_LOG_DEBUG:
      return {.color = cli::ConsoleColor::kGray};
    case ARX_LOG_INFO:
      return {.color = cli::ConsoleColor::kCyan};
    case ARX_LOG_WARN:
      return {.color = cli::ConsoleColor::kYellow};
    case ARX_LOG_ERROR:
      return {.color = cli::ConsoleColor::kRed};
    default:
      return {};
  }
}

bool shouldEmit(ArxLogLevel level) { return static_cast<int>(level) >= static_cast<int>(g_log_level); }

void vlogWithDomain(ArxLogLevel level, LogDomain domain, const char* fmt, va_list ap) {
  if (!shouldEmit(level)) return;
  std::fputc('[', stderr);
  cli::writeStyled(stderr, levelStyle(level), levelName(level));
  std::fprintf(stderr, "/%s] ", domainName(domain));
  std::vfprintf(stderr, fmt, ap);
  std::fputc('\n', stderr);
}

void logWithDomain(ArxLogLevel level, LogDomain domain, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vlogWithDomain(level, domain, fmt, ap);
  va_end(ap);
}

}  // namespace

namespace cli {

void setLogLevel(ArxLogLevel level) { g_log_level = level; }

ArxLogLevel logLevel() { return g_log_level; }

const char* logLevelName(ArxLogLevel level) { return levelName(level); }

bool shouldLog(ArxLogLevel level) { return static_cast<int>(level) >= static_cast<int>(g_log_level); }

void log(ArxLogLevel level, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vlogWithDomain(level, LogDomain::kCli, fmt, ap);
  va_end(ap);
}

void pistorisLog(ArxLogLevel level, const char* msg, void* /*userdata*/) {
  logWithDomain(level, LogDomain::kPistoris, "%s", msg ? msg : "");
}

}  // namespace cli
