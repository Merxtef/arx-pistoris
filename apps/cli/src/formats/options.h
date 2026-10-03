// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

namespace cli {

struct FormatOptions {
  bool pretty = false;
  bool compress = true;
  bool allow_empty_animation = false;
};

}  // namespace cli
