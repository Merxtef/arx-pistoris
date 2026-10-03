// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/model/obj.hpp"  // IWYU pragma: export
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/texture.hpp"

#include "console/diagnostics.h"
#include "io/path_location.h"
#include "io/service.h"
#include "resources/layout.h"
#include "resources/resource_output.h"  // IWYU pragma: export

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {
class Cinematic;
class Level;
class Model;
}  // namespace pistoris

namespace cli {

struct TextureInput {
  bool use_format_sources = false;
  ImageLookupMode source_lookup = ImageLookupMode::kExact;
  PathLocation source_base;
};

struct TextureOutput {
  ResourceLayout layout = ResourceLayout::kGame;
  PathLocation base;
};

bool loadTextureImages(pistoris::Level& level, IoService& io, const TextureInput& input,
                       std::span<const std::string> source_paths = {});
bool loadTextureImages(pistoris::Model& model, IoService& io, const TextureInput& input,
                       std::span<const std::string> source_paths = {});
bool loadTextureImages(pistoris::Cinematic& cinematic, IoService& io, const TextureInput& input,
                       std::span<const std::string> source_paths = {});
void loadNativeTextureFiles(const pistoris::ftl::Data& ftl, pistoris::NativeTextMode text_mode, IoService& io,
                            const TextureInput& input, std::vector<pistoris::NativeTextureFile>& out);
void loadNativeTextureFiles(const pistoris::fts::Data& fts, pistoris::NativeTextMode text_mode, IoService& io,
                            const TextureInput& input, std::vector<pistoris::NativeTextureFile>& out);
void loadNativeTextureFiles(const pistoris::cin::Data& cin, pistoris::NativeTextMode text_mode, IoService& io,
                            const TextureInput& input, std::vector<pistoris::NativeTextureFile>& out);
bool addNativeTextureFileOutputs(ResourceOutputPlan& plan, IoService& io, const TextureOutput& output,
                                 std::span<const pistoris::NativeTextureFile> files, ResourceAssetId asset,
                                 DiagnosticCode failure_code, std::string_view owner);
bool addObjTextureFileOutputs(ResourceOutputPlan& plan, IoService& io, const TextureOutput& output,
                              std::span<const pistoris::ObjTextureFile> files, ResourceAssetId asset,
                              DiagnosticCode failure_code, std::string_view owner);

}  // namespace cli
