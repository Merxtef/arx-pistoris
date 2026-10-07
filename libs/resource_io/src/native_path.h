// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_RESOURCE_IO_NATIVE_PATH_H
#define ARX_PISTORIS_RESOURCE_IO_NATIVE_PATH_H

#include <filesystem>
#include <string>

namespace pistoris::resource_io::detail {

bool canonicalizeProspectivePath(const std::filesystem::path& requested, std::filesystem::path& normalized,
                                 bool& exists, std::string& error);

}  // namespace pistoris::resource_io::detail

#endif
