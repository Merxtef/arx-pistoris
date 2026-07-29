// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

namespace pistoris {
class Level;
}

namespace cli {
class IoService;
}

namespace cli::level {

struct TextureInput;

bool hasMissingReferencedTextureImages(const pistoris::Level& level);
void loadMountedTextureImages(pistoris::Level& level, cli::IoService& io);
void loadMountedTextureImages(pistoris::Level& level, cli::IoService& io, const TextureInput& input);

}  // namespace cli::level
