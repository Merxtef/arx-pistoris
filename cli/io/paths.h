// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <string>
#include <string_view>

std::string sanitizeFilename(std::string_view name);
bool isPortableReservedFilename(std::string_view name);
