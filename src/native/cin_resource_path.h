// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/cin.hpp"

#include <string>
#include <string_view>

namespace pistoris {

bool decodeCinIllustrationPath(std::string_view stored_path, std::string& out);
bool encodeCinIllustrationPath(std::string_view path, std::string& out);
bool decodeCinSoundPath(std::string_view stored_path, cin::Sound& out);
bool encodeCinSoundPath(const cin::Sound& sound, std::string& out);

}  // namespace pistoris
