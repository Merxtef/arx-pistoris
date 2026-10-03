// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

namespace pistoris {
class Ambiance;
}

namespace cli::ambiance {

struct AmbianceOptions;

namespace operations {

bool apply(pistoris::Ambiance& ambiance, const AmbianceOptions& options);

}  // namespace operations
}  // namespace cli::ambiance
