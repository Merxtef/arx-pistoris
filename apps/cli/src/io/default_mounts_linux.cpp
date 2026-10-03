// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "io/default_mounts.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace cli {

bool defaultGameResourceRoot(std::filesystem::path& out, std::string& error) {
  out.clear();
  error.clear();
  if (const char* data_home = std::getenv("XDG_DATA_HOME"); data_home && *data_home) {
    std::filesystem::path base(data_home);
    if (base.is_absolute()) {
      out = base / "arx";
      return true;
    }
  }
  if (const char* home = std::getenv("HOME"); home && *home) {
    std::filesystem::path base(home);
    if (base.is_absolute()) {
      out = base / ".local" / "share" / "arx";
      return true;
    }
  }
  error = "no absolute XDG_DATA_HOME or HOME is available";
  return false;
}

}  // namespace cli
