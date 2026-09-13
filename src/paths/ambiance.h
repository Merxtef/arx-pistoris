// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <string>
#include <string_view>

namespace pistoris::paths {

bool parseZoneAmbianceReference(std::string_view reference, std::string& out);
bool formatZoneAmbianceReference(std::string_view ambiance, std::string& out);
bool decodeNativeZoneAmbiance(std::string_view stored, std::string& out);
bool encodeNativeZoneAmbiance(std::string_view ambiance, std::string& out);

}  // namespace pistoris::paths
