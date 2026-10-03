// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/read_diagnostics.h"

#include "console/diagnostics.h"
#include "io/service.h"

#include <cassert>
#include <string_view>

namespace cli {

void reportRequiredReadFailure(ResourceReadResult result, std::string_view description, std::string_view path) {
  switch (result) {
    case ResourceReadResult::kNotFound:
      diagnostic(DiagnosticCode::kResourceNotFound,
                 "%.*s not found: %.*s",
                 static_cast<int>(description.size()),
                 description.data(),
                 static_cast<int>(path.size()),
                 path.data());
      return;
    case ResourceReadResult::kInvalidPath:
      diagnostic(DiagnosticCode::kResourcePathInvalid,
                 "%.*s path is invalid: %.*s",
                 static_cast<int>(description.size()),
                 description.data(),
                 static_cast<int>(path.size()),
                 path.data());
      return;
    case ResourceReadResult::kReadFailed:
      diagnostic(DiagnosticCode::kResourceReadFailed,
                 "%.*s cannot be read: %.*s",
                 static_cast<int>(description.size()),
                 description.data(),
                 static_cast<int>(path.size()),
                 path.data());
      return;
    case ResourceReadResult::kSuccess:
      break;
  }
  assert(false && "successful reads have no failure diagnostic");
}

}  // namespace cli
