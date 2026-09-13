// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime/types.h"

#include "level/data.h"
#include "level/native/api.h"
#include "level/native/internal.h"
#include "level/validation.h"
#include "native/dlf.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::level_native {
namespace {

bool resolveDlfScenePath(std::string_view level_name, std::string_view explicit_path, std::string& out) {
  if (!explicit_path.empty()) {
    out = explicit_path;
    return validDlfScenePath(out);
  }
  return paths::dlfSceneFromLevelName(level_name, out) && validDlfScenePath(out);
}

void logTextureShardWarnings(const NativeTextureResources& textures) {
  for (const NativeTextureFamily& family : textures.families) {
    if (family.shards.size() <= 1U) continue;
    std::string aliases;
    for (std::size_t shard = 1; shard < family.shards.size(); ++shard) {
      if (!aliases.empty()) aliases += ", ";
      aliases += '\'';
      aliases += family.shards[shard].resource_path;
      aliases += '\'';
    }
    log(ARX_LOG_WARN,
        "Level native bake: texture resource '{}' was sharded as {} to stay within {} corners per room "
        "texture batch; when managing texture files manually, provide a copy of the original image under "
        "every shard resource name",
        family.shards.front().resource_path,
        aliases,
        kFtsMaxRoomTextureVertices);
  }
}

}  // namespace

ArxReturnCode bakeValidatedNativeLevelBundle(const LevelModules& level, const Level::NativeBakeOptions& options,
                                             NativeLevelBundle& out) {
  std::string dlf_scene_path;
  if (!resolveDlfScenePath(options.level_name, options.dlf_scene_path, dlf_scene_path)) return ARX_DLF_BAD_SCENE_PATH;

  NativeLevelBundle tmp;
  NativeTextureResources textures;
  NativeBakeWarnings warnings;
  NativeBakeStatistics statistics;
  ArxReturnCode rc = projectNativeTextures(level.textures, options, textures, warnings);
  if (rc != ARX_OK) return rc;
  std::vector<ArxColor3> baked_colors;
  rc = bakeFts(level, textures, options.reconstruct_quads, tmp.fts, baked_colors, warnings, statistics);
  if (rc != ARX_OK) return rc;
  buildNativeTextureFiles(textures, tmp.texture_files);
  rc = bakeLlf(level.lighting, std::move(baked_colors), tmp.llf);
  if (rc != ARX_OK) return rc;
  rc = bakeDlf(level.scene, dlf_scene_path, {}, tmp.dlf);
  if (rc != ARX_OK) return rc;

  logRoomDistanceBakeWarnings(level);
  if (warnings.discarded_fragments != 0)
    log(ARX_LOG_WARN,
        "Level native bake: {} clipped polygon fragment(s) with non-finite or degenerate geometry discarded",
        warnings.discarded_fragments);
  if (warnings.fully_discarded_faces != 0)
    log(ARX_LOG_WARN,
        "Level native bake: {} source face(s) produced no output polygons after clipping",
        warnings.fully_discarded_faces);
  if (warnings.rescaled_texture_images != 0)
    log(ARX_LOG_WARN,
        "Level native bake: rescaled {} non-power-of-two texture image(s) to power-of-two dimensions for "
        "native repeat sampling",
        warnings.rescaled_texture_images);
  logTextureShardWarnings(textures);
  if (options.reconstruct_quads) {
    const std::uint64_t reconstructed_quads = statistics.clipped_fragment_quads + statistics.cross_face_quads;
    log(ARX_LOG_DEBUG,
        "Level native bake: reconstructed {} FTS quad(s): {} clipped-fragment, {} cross-face; {} "
        "triangle(s) remain",
        reconstructed_quads,
        statistics.clipped_fragment_quads,
        statistics.cross_face_quads,
        statistics.triangles);
  } else {
    log(ARX_LOG_DEBUG,
        "Level native bake: FTS quad reconstruction disabled; emitted {} triangle(s)",
        statistics.triangles);
  }
  log(ARX_LOG_INFO,
      "Level native bake: {} FTS polygon(s), {} LLF color(s), DLF scene '{}'",
      tmp.fts.scene.num_polys,
      tmp.llf.colors.size(),
      tmp.dlf.scene_path);
  out = std::move(tmp);
  return ARX_OK;
}

ArxReturnCode bakeNativeLevelBundle(const LevelModules& level, const Level::NativeBakeOptions& options,
                                    NativeLevelBundle& out) {
  if (options.level_name.empty() && options.dlf_scene_path.empty()) return ARX_DLF_BAD_SCENE_PATH;
  ArxReturnCode rc = validateLevelModules(level);
  if (rc != ARX_OK) return rc;
  return bakeValidatedNativeLevelBundle(level, options, out);
}

ArxReturnCode bakeValidatedNativeDlf(const SceneData& scene, const Level::DlfBakeOptions& options, dlf::Data& out) {
  std::string scene_path;
  if (!resolveDlfScenePath(options.level_name, options.dlf_scene_path, scene_path)) return ARX_DLF_BAD_SCENE_PATH;
  return bakeDlf(scene, scene_path, options.target_fts_offset, out);
}

}  // namespace pistoris::level_native
