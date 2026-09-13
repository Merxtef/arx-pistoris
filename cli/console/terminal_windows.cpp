// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/terminal.h"

#include <cstdio>
#include <cstdlib>
#include <io.h>
#include <windows.h>

namespace {

bool detectColorSupport(std::FILE* output) {
  if (std::getenv("NO_COLOR")) return false;
  const int descriptor = _fileno(output);
  if (descriptor < 0 || _isatty(descriptor) == 0) return false;

  const auto raw_handle = _get_osfhandle(descriptor);
  if (raw_handle == -1) return false;
  HANDLE handle = reinterpret_cast<HANDLE>(raw_handle);  // NOLINT(performance-no-int-to-ptr)
  DWORD mode = 0;
  if (!GetConsoleMode(handle, &mode)) return false;
  if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) return true;
  return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

}  // namespace

namespace cli {

bool terminalSupportsColor(std::FILE* output) noexcept {
  if (output == stdout) {
    static const bool kSupported = detectColorSupport(stdout);
    return kSupported;
  }
  if (output == stderr) {
    static const bool kSupported = detectColorSupport(stderr);
    return kSupported;
  }
  return detectColorSupport(output);
}

}  // namespace cli
