// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/pistoris.hpp"

#include "animation/dispatch.h"
#include "args.h"
#include "formats.h"
#include "io.h"
#include "model/dispatch.h"

#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <vector>

// --- Arg parsing ---

static bool parseFloat(const char* s, float& out) {
  char* end = nullptr;
  out       = std::strtof(s, &end);
  return end != s && *end == '\0';
}

static bool parseSizeT(const char* s, std::size_t& out) {
  if (!s || *s == '\0') return false;
  for (const char* p = s; *p; ++p)
    if (*p < '0' || *p > '9') return false;

  errno = 0;
  char* end = nullptr;
  unsigned long long value = std::strtoull(s, &end, 10);
  if (errno == ERANGE || end == s || *end != '\0') return false;
  if (value > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) return false;
  out = static_cast<std::size_t>(value);
  return true;
}

static bool consumeFloats(int n, int argc, char* argv[], int& i, float* out, const char* name) {
  if (i + n >= argc) {
    std::fprintf(stderr, "%s: expected %d values\n", name, n);
    return false;
  }
  for (int k = 0; k < n; ++k)
    if (!parseFloat(argv[i + 1 + k], out[k])) {
      std::fprintf(stderr, "%s: value %d ('%s') is not a number\n", name, k + 1, argv[i + 1 + k]);
      return false;
    }
  i += n;
  return true;
}

static bool parseArgs(int argc, char* argv[], CliArgs& args) {
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
        args.help = true;
      } else if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--version") == 0) {
        args.version = true;
      } else if (std::strcmp(argv[i], "--pretty") == 0) {
        args.pretty = true;
      } else if (std::strcmp(argv[i], "--overwrite") == 0) {
        args.overwrite = cli::OverwriteMode::kAlwaysYes;
      } else if (std::strcmp(argv[i], "--no-overwrite") == 0) {
        args.overwrite = cli::OverwriteMode::kAlwaysNo;
      } else if (std::strcmp(argv[i], "--rotate") == 0) {
        if (!consumeFloats(3, argc, argv, i, args.rotate, "--rotate")) return false;
        args.has_xform = true;
      } else if (std::strcmp(argv[i], "--scale") == 0) {
        // 1 uniform or 3 per-axis
        float vals[3];
        int consumed = 0;
        for (int k = 0; k < 3 && i + 1 + k < argc; ++k) {
          if (!parseFloat(argv[i + 1 + k], vals[k])) break;
          ++consumed;
        }
        if (consumed == 1) {
          args.scale[0] = args.scale[1] = args.scale[2] = vals[0];
        } else if (consumed == 3) {
          args.scale[0] = vals[0];
          args.scale[1] = vals[1];
          args.scale[2] = vals[2];
        } else {
          std::fprintf(stderr, "--scale: expected 1 value (uniform) or 3 values (x y z)\n");
          return false;
        }
        i += consumed;
        args.has_xform = true;
      } else if (std::strcmp(argv[i], "--offset") == 0) {
        if (!consumeFloats(3, argc, argv, i, args.offset, "--offset")) return false;
        args.has_xform = true;
      } else if (std::strcmp(argv[i], "--overwrite-texture") == 0) {
        if (i + 1 >= argc) {
          std::fprintf(stderr, "--overwrite-texture: expected path argument\n");
          return false;
        }
        args.overwrite_texture = argv[++i];
      } else if (std::strcmp(argv[i], "--rename-selections") == 0) {
        if (i + 1 >= argc) {
          std::fprintf(stderr, "--rename-selections: expected comma-separated names\n");
          return false;
        }
        args.rename_selections = argv[++i];
      } else if (std::strcmp(argv[i], "--reference-ftl") == 0) {
        if (i + 1 >= argc) {
          std::fprintf(stderr, "--reference-ftl: expected FTL path\n");
          return false;
        }
        args.reference_ftl = argv[++i];
      } else if (std::strcmp(argv[i], "--autosize-to-reference") == 0) {
        args.autosize_to_reference = true;
      } else if (std::strcmp(argv[i], "--snap-bone-origins-to-reference") == 0) {
        if (i + 1 >= argc) {
          std::fprintf(stderr,
                       "--snap-bone-origins-to-reference: expected mode snap-origins, delta-deform, or "
                       "hierarchy-deform [N]\n");
          return false;
        }
        const char* mode = argv[++i];
        if (std::strcmp(mode, "snap-origins") == 0) {
          args.bone_origin_reference_mode = BoneOriginReferenceMode::kSnapOrigins;
        } else if (std::strcmp(mode, "delta-deform") == 0) {
          args.bone_origin_reference_mode = BoneOriginReferenceMode::kDeltaDeform;
        } else if (std::strcmp(mode, "hierarchy-deform") == 0) {
          args.bone_origin_reference_mode = BoneOriginReferenceMode::kHierarchyDeform;
          if (i + 1 < argc) {
            std::size_t limit = 0;
            if (parseSizeT(argv[i + 1], limit)) {
              args.hierarchy_deform_step_limit = limit;
              ++i;
            }
          }
        } else {
          std::fprintf(stderr, "--snap-bone-origins-to-reference: unknown mode '%s'\n", mode);
          return false;
        }
      } else if (std::strcmp(argv[i], "--snap-action-points-to-reference") == 0) {
        args.snap_action_points = true;
      } else if (std::strcmp(argv[i], "--copy-reference-affiliations") == 0) {
        args.copy_reference_affiliations = true;
      } else {
        std::fprintf(stderr, "Unknown option: %s\n", argv[i]);
        return false;
      }
    } else {
      args.positionals.push_back(argv[i]);
    }
  }

  return args.help || args.version || !args.positionals.empty();
}

// --- Logger ---

static const char* logLevelPrefix(ArxLogLevel level) {
  switch (level) {
    case ARX_LOG_DEBUG:
      return "DEBUG";
    case ARX_LOG_INFO:
      return "INFO";
    case ARX_LOG_WARN:
      return "WARN";
    default:
      return "?";
  }
}

static void cliLog(ArxLogLevel level, const char* msg, void* /*userdata*/) {
  std::fprintf(stdout, "[%s] %s\n", logLevelPrefix(level), msg);
}

// --- Usage ---

static void printUsage(const char* argv0) {
  std::fprintf(stderr, "Usage:\n");
  std::fprintf(stderr, "  %s <input> [extras...] [output]     inspect or convert file bundle\n", argv0);
  std::fprintf(stderr, "\nExamples:\n");
  std::fprintf(stderr, "  %s model.ftl                        inspect FTL\n", argv0);
  std::fprintf(stderr, "  %s model.ftl anim1.tea anim2.tea    inspect FTL+TEA bundle\n", argv0);
  std::fprintf(stderr, "  %s model.ftl anim1.tea out.glb      export FTL+TEA bundle\n", argv0);
  std::fprintf(stderr, "  %s anim.tea                         inspect TEA\n", argv0);
  std::fprintf(stderr, "  %s anim.tea out.tea                 rewrite/transform TEA\n", argv0);
  std::fprintf(stderr, "  %s anim.tea out.json                export TEA JSON\n", argv0);
  std::fprintf(stderr, "  %s anim.json out.tea                import TEA JSON\n", argv0);
  std::fprintf(stderr, "\nOptions:\n");
  std::fprintf(stderr, "  Options are global and may appear before, between, or after file paths.\n");
  std::fprintf(stderr, "  -h, --help                          print this help\n");
  std::fprintf(stderr, "  -v, --version                       print version\n");
  std::fprintf(stderr, "  --pretty                            pretty-print JSON output\n");
  std::fprintf(stderr, "  --overwrite                         overwrite existing outputs without asking\n");
  std::fprintf(stderr, "  --no-overwrite                      skip existing outputs without asking\n");
  std::fprintf(stderr, "  --rotate RX RY RZ                   apply transform, Euler XYZ degrees\n");
  std::fprintf(stderr, "  --scale S | --scale SX SY SZ        apply scale\n");
  std::fprintf(stderr, "  --offset OX OY OZ                   apply translation\n");
  std::fprintf(stderr, "  --overwrite-texture PATH            overwrite FTL texture paths when FTL is present\n");
  std::fprintf(stderr, "  --rename-selections CSV             rename FTL selections by position; empty CSV fields skip\n");
  std::fprintf(stderr, "  --reference-ftl PATH                 load base FTL for reference repair operations:\n");
  std::fprintf(stderr, "      --autosize-to-reference          uniform-scale and shift model to reference landmarks\n");
  std::fprintf(stderr, "      --snap-bone-origins-to-reference MODE\n");
  std::fprintf(stderr, "                                      MODE is snap-origins, delta-deform, or hierarchy-deform [N]\n");
  std::fprintf(stderr, "      --snap-action-points-to-reference\n");
  std::fprintf(stderr, "                                      copy exact reference action point positions by name\n");
  std::fprintf(stderr, "      --copy-reference-affiliations    copy reference selection membership for synthetic vertices\n");
  std::fprintf(stderr, "  %s <input.glb> <out.ftl>            GLB -> FTL + sibling .tea files\n", argv0);
  std::fprintf(stderr, "\nSupported input formats: FTL, TEA, OBJ, JSON, GLB\n");
  std::fprintf(stderr, "Model outputs:           FTL, OBJ, JSON, GLB\n");
  std::fprintf(stderr, "Animation outputs:       TEA, JSON\n");
}

// --- Dispatch ---

static int dispatch(const CliArgs& args) {
  std::vector<std::uint8_t> buf;
  const char* primary = args.positionals[0];
  if (!readFile(primary, buf)) return 1;

  cli::Route route = cli::detectRoute(buf, primary);

  cli::State state{args.overwrite};

  switch (route.kind) {
    case cli::RouteKind::kUnknown:
      std::fprintf(stderr, "Unable to determine input route: %s\n", primary);
      return 1;
    case cli::RouteKind::kModel:
      return cli::model::dispatch(args, state, buf, route);
    case cli::RouteKind::kAnimation:
      return cli::animation::dispatch(args, state, buf, route);
  }

  return 1;
}

// --- Entry point ---

int main(int argc, char* argv[]) {
  try {
    pistoris::setLogCallback(cliLog, nullptr);

    CliArgs args;
    if (!parseArgs(argc, argv, args)) {
      printUsage(argv[0]);
      return 1;
    }

    if (args.help) {
      printUsage(argv[0]);
      return 0;
    }

    if (args.version) {
      std::printf("arx-pistor %s\n", pistoris::version());
      return 0;
    }

    return dispatch(args);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Unhandled exception: %s\n", e.what());
    return 1;
  } catch (...) {
    std::fprintf(stderr, "Unhandled non-standard exception\n");
    return 1;
  }
}
