// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "model/io.h"

#include "../io.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace cli::model {

static std::string_view textView(const std::vector<std::uint8_t>& buf) {
  return {reinterpret_cast<const char*>(buf.data()), buf.size()};
}

static bool outputFailure(const char* what, ArxReturnCode rc) {
  std::fprintf(stderr, "%s failed: %s (code %d)\n", what, pistoris::errorString(rc), static_cast<int>(rc));
  return false;
}

bool loadTeaFile(const char* path, pistoris::Tea& out) {
  std::vector<std::uint8_t> tea_buf;
  if (!readFile(path, tea_buf)) return false;

  ArxReturnCode rc = ARX_OK;
  switch (cli::formatFromPath(path)) {
    case cli::Format::kTea:
      rc = pistoris::readTea(tea_buf, out);
      break;
    case cli::Format::kJson:
      rc = pistoris::importJson(textView(tea_buf), out);
      break;
    default:
      std::fprintf(stderr, "Unsupported extra animation format: %s\n", path);
      return false;
  }
  if (rc != ARX_OK) {
    std::fprintf(stderr, "TEA parse failed (%s): %s (code %d)\n", path, pistoris::errorString(rc),
                 static_cast<int>(rc));
    return false;
  }
  return true;
}

bool loadReferenceFtl(const char* path, Context& ctx) {
  std::vector<std::uint8_t> ref_buf;
  if (!readFile(path, ref_buf)) return false;

  pistoris::Ftl ref;
  ArxReturnCode rc = pistoris::readFtl(ref_buf, ref);
  if (rc != ARX_OK) {
    std::fprintf(stderr, "Reference FTL failed: %s (code %d)\n", pistoris::errorString(rc), static_cast<int>(rc));
    return false;
  }

  std::printf("Using reference FTL: %s\n", path);
  ctx.reference_ftl = std::move(ref);
  ctx.has_reference = true;
  return true;
}

bool loadInput(const std::vector<std::uint8_t>& buf, const Invocation& inv, cli::Route route, Context& ctx) {
  ArxReturnCode rc = ARX_OK;
  switch (route.input) {
    case cli::Format::kFtl: {
      pistoris::Ftl ftl;
      rc = pistoris::readFtl(buf, ftl);
      if (rc != ARX_OK) break;
      ctx.ftl = std::move(ftl);
      return true;
    }

    case cli::Format::kObj: {
      std::string mtl_path(inv.input, fileExtension(inv.input));
      mtl_path += ".mtl";

      std::vector<std::uint8_t> mtl_buf;
      bool has_mtl = readFileOptional(mtl_path.c_str(), mtl_buf);
      if (has_mtl) std::printf("Using MTL: %s\n", mtl_path.c_str());

      pistoris::Ftl ftl;
      rc = pistoris::importObj(textView(buf), has_mtl ? textView(mtl_buf) : std::string_view{}, pathFilename(inv.input),
                               ftl);
      if (rc != ARX_OK) break;
      ctx.ftl = std::move(ftl);
      return true;
    }

    case cli::Format::kJson: {
      pistoris::Ftl ftl;
      rc = pistoris::importJson(textView(buf), ftl);
      if (rc != ARX_OK) break;
      ctx.ftl = std::move(ftl);
      return true;
    }

    case cli::Format::kGlb: {
      pistoris::Ftl ftl;
      std::vector<pistoris::Tea> teas;
      rc = pistoris::importGlb(buf, inv.input, ftl, teas);
      if (rc != ARX_OK) break;
      ctx.ftl  = std::move(ftl);
      ctx.teas = std::move(teas);
      return true;
    }

    default:
      std::fprintf(stderr, "Unsupported input format: %s\n", inv.input);
      return false;
  }
  std::fprintf(stderr, "%s input failed: %s (code %d)\n", cli::formatName(route.input), pistoris::errorString(rc),
               static_cast<int>(rc));
  return false;
}

bool loadExtras(const Invocation& inv, Context& ctx) {
  for (const char* tea_path : inv.extras) {
    pistoris::Tea tea;
    if (!loadTeaFile(tea_path, tea)) return false;
    ctx.teas.push_back(std::move(tea));
  }
  return true;
}

bool validateTeaCompatibility(const Context& ctx) {
  std::size_t group_count = ctx.ftl.groups.size();
  for (std::size_t i = 0; i < ctx.teas.size(); ++i) {
    const pistoris::Tea& tea = ctx.teas[i];
    if (tea.num_groups == static_cast<int32_t>(group_count)) continue;

    const char* name = tea.name[0] != '\0' ? tea.name : "<unnamed>";
    std::fprintf(stderr, "TEA group count mismatch (%s, index %zu): animation=%d FTL=%zu\n", name, i,
                 tea.num_groups, group_count);
    return false;
  }
  return true;
}

enum class TeaOutputFormat {
  kNative,
  kJson,
};

static bool writeTeas(cli::State& state, const Context& ctx, const CliArgs& args, const char* output,
                      TeaOutputFormat format) {
  std::string_view out_sv(output);
  auto sep        = out_sv.find_last_of("/\\");
  std::string dir = (sep == std::string_view::npos) ? "" : std::string(out_sv.substr(0, sep + 1));
  const char* ext = format == TeaOutputFormat::kNative ? ".tea" : ".json";

  bool ok = true;
  for (std::size_t i = 0; i < ctx.teas.size(); ++i) {
    const char* name = ctx.teas[i].name;
    std::string stem;
    if (name[0] != '\0') {
      stem = sanitizeFilename(name);
      if (std::strcmp(stem.c_str(), name) != 0)
        std::fprintf(stderr, "Note: anim name '%s' sanitized to '%s' for filesystem\n", name, stem.c_str());
    }
    if (stem.empty()) stem = "animation_" + std::to_string(i);
    std::string tea_path = dir + stem + ext;

    if (format == TeaOutputFormat::kNative) {
      std::vector<std::uint8_t> tea_data;
      ArxReturnCode rc = pistoris::writeTea(ctx.teas[i], tea_data);
      if (rc != ARX_OK) {
        std::fprintf(stderr, "TEA write failed (%s): %s (code %d)\n", tea_path.c_str(), pistoris::errorString(rc),
                     static_cast<int>(rc));
        ok = false;
        continue;
      }
      if (!writeFile(state, tea_path.c_str(), tea_data.data(), tea_data.size())) {
        ok = false;
        continue;
      }
    } else {
      std::string tea_json;
      ArxReturnCode rc = pistoris::exportJson(ctx.teas[i], tea_json, args.pretty);
      if (rc != ARX_OK) {
        std::fprintf(stderr, "TEA JSON output failed (%s): %s (code %d)\n", tea_path.c_str(), pistoris::errorString(rc),
                     static_cast<int>(rc));
        ok = false;
        continue;
      }
      if (!writeFile(state, tea_path.c_str(), tea_json.data(), tea_json.size())) {
        ok = false;
        continue;
      }
    }
  }
  return ok;
}

static void warnTeasIgnored(const Context& ctx, const char* output_format) {
  if (!ctx.teas.empty()) {
    std::fprintf(stderr, "Warning: %s output ignores %zu TEA animation(s)\n", output_format, ctx.teas.size());
  }
}

bool saveOutput(const Context& ctx, const CliArgs& args, cli::State& state, const Invocation& inv, cli::Route route) {
  switch (route.output) {
    case cli::Format::kFtl: {
      std::vector<std::uint8_t> out;
      ArxReturnCode rc = pistoris::writeFtl(ctx.ftl, out);
      if (rc != ARX_OK) return outputFailure("FTL output", rc);
      if (!writeFile(state, inv.output, out.data(), out.size())) return false;

      if (!ctx.teas.empty() && !writeTeas(state, ctx, args, inv.output, TeaOutputFormat::kNative)) return false;
      return true;
    }

    case cli::Format::kObj: {
      warnTeasIgnored(ctx, "OBJ");

      std::string_view obj_stem(inv.output, fileExtension(inv.output) - inv.output);
      auto sep = obj_stem.find_last_of("/\\");
      if (sep != std::string_view::npos) obj_stem = obj_stem.substr(sep + 1);

      pistoris::Obj obj;
      ArxReturnCode rc = pistoris::exportObj(ctx.ftl, obj_stem, obj);
      if (rc != ARX_OK) return outputFailure("OBJ output", rc);

      if (!writeFile(state, inv.output, obj.text.data(), obj.text.size())) return false;

      if (!obj.mtl.empty()) {
        std::string mtl_path(inv.output, fileExtension(inv.output));
        mtl_path += ".mtl";
        if (!writeFile(state, mtl_path.c_str(), obj.mtl.data(), obj.mtl.size())) return false;
      }
      return true;
    }

    case cli::Format::kJson: {
      std::string out;
      ArxReturnCode rc = pistoris::exportJson(ctx.ftl, out, args.pretty);
      if (rc != ARX_OK) return outputFailure("JSON output", rc);
      if (!writeFile(state, inv.output, out.data(), out.size())) return false;
      if (!ctx.teas.empty() && !writeTeas(state, ctx, args, inv.output, TeaOutputFormat::kJson)) return false;
      return true;
    }

    case cli::Format::kGlb: {
      std::vector<std::uint8_t> out;
      ArxReturnCode rc = pistoris::exportGlb(ctx.ftl, ctx.teas, out);
      if (rc != ARX_OK) return outputFailure("GLB output", rc);
      if (!writeFile(state, inv.output, out.data(), out.size())) return false;
      return true;
    }

    case cli::Format::kUnset:
    default:
      std::fprintf(stderr, "Unsupported output format\n");
      return false;
  }
}

}  // namespace cli::model
