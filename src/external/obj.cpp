// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "external/obj.h"

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/ftl.h"
#include "external/mat_name.h"
#include "utils/log.h"
#include "utils/parse_utils.h"
#include "utils/text_cursor.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris {

// --- Exporters ---

ArxReturnCode exportFtlToObj(const ftl::Data& d, std::string_view obj_stem, std::string& out) {
  ARX_RETURN_IF_ERR(validateFtl(&d));

  log(ARX_LOG_INFO, std::format("OBJ export: {} vertices, {} faces, {} textures", d.vertices.size(), d.faces.size(),
                                d.texture_containers.size()));
  if (!d.groups.empty() || !d.actions.empty() || !d.selections.empty())
    log(ARX_LOG_WARN, std::format("OBJ export: {} groups, {} actions, {} selections not exported (no OBJ equivalent)",
                                  d.groups.size(), d.actions.size(), d.selections.size()));

  for (std::size_t i = 0; i < d.texture_containers.size(); ++i) {
    auto stem = pathStem(d.texture_containers[i].filename);
    if (stem.find("__") != std::string_view::npos)
      log(ARX_LOG_WARN, std::format("OBJ export: texture[{}] stem '{}' contains \"__\" which is the flag "
                                    "separator; re-import will mangle the material name",
                                    i, stem));
  }

  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss << std::setprecision(std::numeric_limits<float>::max_digits10);

  ss << "# arx-pistoris OBJ export\n";

  auto source = pathStem(d.header.name);
  if (!source.empty()) ss << "# source: " << source << '\n';
  ss << "# origin " << (d.header.origin + 1) << '\n';  // 1-based

  if (!d.texture_containers.empty()) {
    ss << "mtllib " << obj_stem << ".mtl\n";
  }
  ss << '\n';

  for (const auto& v : d.vertices) {
    ss << "v " << v.position.x << ' ' << v.position.y << ' ' << v.position.z << '\n';
  }
  if (!d.vertices.empty()) ss << '\n';

  for (const auto& v : d.vertices) {
    ss << "vn " << v.normal.x << ' ' << v.normal.y << ' ' << v.normal.z << '\n';
  }
  if (!d.vertices.empty()) ss << '\n';

  // one vt per face corner: face fi corner ci -> index fi*3 + ci + 1
  for (const auto& face : d.faces) {
    ss << "vt " << face.u.x << ' ' << face.v.x << '\n';
    ss << "vt " << face.u.y << ' ' << face.v.y << '\n';
    ss << "vt " << face.u.z << ' ' << face.v.z << '\n';
  }
  if (!d.faces.empty()) ss << '\n';

  auto used = collectMaterials(d);

  bool first_face             = true;
  std::int16_t current_tex_id = kFtlTextureNone;
  FaceType current_type       = 0;
  for (std::size_t fi = 0; fi < d.faces.size(); ++fi) {
    const auto& face = d.faces[fi];

    if (first_face || face.texture_id != current_tex_id || face.type != current_type) {
      first_face     = false;
      current_tex_id = face.texture_id;
      current_type   = face.type;

      std::string_view tex_stem;
      if (face.texture_id != kFtlTextureNone) tex_stem = pathStem(d.texture_containers[face.texture_id].filename);
      ss << "usemtl " << matName(tex_stem, face.type) << '\n';
    }

    auto vi0  = static_cast<int>(face.vertex_idx.x) + 1;
    auto vi1  = static_cast<int>(face.vertex_idx.y) + 1;
    auto vi2  = static_cast<int>(face.vertex_idx.z) + 1;
    auto vti0 = static_cast<int>(fi) * 3 + 1;
    auto vti1 = static_cast<int>(fi) * 3 + 2;
    auto vti2 = static_cast<int>(fi) * 3 + 3;

    ss << "f " << vi0 << '/' << vti0 << '/' << vi0 << ' ' << vi1 << '/' << vti1 << '/' << vi1 << ' ' << vi2 << '/'
       << vti2 << '/' << vi2 << '\n';
  }

  out = ss.str();
  return ARX_OK;
}

ArxReturnCode exportFtlToMtl(const ftl::Data& d, std::string& out) {
  ARX_RETURN_IF_ERR(validateFtl(&d));

  if (d.texture_containers.empty()) {
    out = {};
    return ARX_OK;
  }

  auto used = collectMaterials(d);

  struct TransvalAcc {
    float sum, min, max;
    int count;
  };
  std::map<std::pair<std::int16_t, FaceType>, TransvalAcc> transval_acc;
  for (const auto& face : d.faces) {
    if (!(face.type & kFaceBitTrans)) continue;
    auto& acc = transval_acc[{face.texture_id, face.type}];
    if (acc.count == 0) {
      acc.sum = acc.min = acc.max = face.transval;
    } else {
      acc.sum += face.transval;
      if (face.transval < acc.min) acc.min = face.transval;
      if (face.transval > acc.max) acc.max = face.transval;
    }
    acc.count++;
  }

  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss << std::setprecision(std::numeric_limits<float>::max_digits10);
  ss << "# arx-pistoris MTL export\n";

  for (const auto& [tex_id, type] : used) {
    if (tex_id == kFtlTextureNone) {
      ss << "\nnewmtl " << matName("", type) << '\n';
      if (type & kFaceBitTrans) {
        log(ARX_LOG_WARN, "MTL export: face with no texture has FACE_BIT_TRANS, exporting d value anyway");
        auto it = transval_acc.find({tex_id, type});
        if (it != transval_acc.end() && it->second.count > 0) {
          const auto& acc = it->second;
          if (acc.min != acc.max)
            log(ARX_LOG_WARN, std::format("MTL export: material '{}' has varying transval [{}, {}], averaging",
                                          matName("", type), acc.min, acc.max));
          ss << "d " << (1.0f - acc.sum / acc.count) << '\n';
        }
      }
      continue;
    }
    auto stem = pathStem(d.texture_containers[tex_id].filename);
    auto ext  = pathExt(d.texture_containers[tex_id].filename);
    if (stem.empty()) {
      log(ARX_LOG_WARN, std::format("MTL export: texture [{}] '{}' has no usable stem, skipping map_Kd", tex_id,
                                    d.texture_containers[tex_id].filename));
      ss << "\nnewmtl " << matName("", type) << '\n';
      ss << "# arx_path " << d.texture_containers[tex_id].filename << '\n';
      continue;
    }
    ss << "\nnewmtl " << matName(stem, type) << '\n';
    ss << "map_Kd " << stem << ext << '\n';
    ss << "# arx_path " << d.texture_containers[tex_id].filename << '\n';
    if (type & kFaceBitTrans) {
      auto it = transval_acc.find({tex_id, type});
      if (it != transval_acc.end() && it->second.count > 0) {
        const auto& acc = it->second;
        if (acc.min != acc.max)
          log(ARX_LOG_WARN, std::format("MTL export: material '{}' has varying transval [{}, {}], averaging",
                                        matName(stem, type), acc.min, acc.max));
        ss << "d " << (1.0f - acc.sum / acc.count) << '\n';
      }
    }
  }

  out = ss.str();
  return ARX_OK;
}

// --- Importer helpers ---

static bool resolveObjIdx(std::string_view s, std::size_t count, std::size_t& out) {
  int val        = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
  if (ec != std::errc{} || val == 0) return false;
  if (val > 0) {
    if (static_cast<std::size_t>(val) > count) return false;
    out = static_cast<std::size_t>(val) - 1;
  } else {
    auto neg = static_cast<std::size_t>(-static_cast<long long>(val));
    if (neg > count) return false;
    out = count - neg;
  }
  return true;
}

static std::optional<float> parseFloat(std::string_view s) {
  float f{};
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), f);
  if (ec != std::errc{}) return std::nullopt;
  return f;
}

static std::optional<std::size_t> parseSize(std::string_view s) {
  std::size_t n{};
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
  if (ec != std::errc{}) return std::nullopt;
  return n;
}

static std::string_view nextLineSv(std::string_view text, std::size_t& pos) {
  auto start = pos;
  auto nl    = text.find('\n', pos);
  if (nl == std::string_view::npos) {
    pos       = text.size();
    auto line = text.substr(start);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    return line;
  }
  pos       = nl + 1;
  auto line = text.substr(start, nl - start);
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  return line;
}

struct MtlEntry {
  std::string map_kd;
  std::string arx_path;  // original game path, from "# arx_path"
  float transval = 0.0f;
};

static ArxReturnCode parseMtl(std::string_view mtl, std::unordered_map<std::string, MtlEntry>& result) {
  std::string cur_mat;
  std::size_t pos = 0;

  while (pos < mtl.size()) {
    auto line_sv = nextLineSv(mtl, pos);
    TextCursor line(line_sv, "#");
    Token kw = line.next();

    if (kw.kind == TokenKind::kSpecialChar) {
      if (!cur_mat.empty()) {
        Token sub = line.next();
        if (sub.kind == TokenKind::kString && sub.text == "arx_path") {
          Token val = line.restOfLine();
          if (val.kind == TokenKind::kString)
            result[cur_mat].arx_path = std::string(val.text);
          else
            log(ARX_LOG_WARN, std::format("MTL: '# arx_path' missing value in '{}'", cur_mat));
        }
      }
      continue;
    }

    if (kw.kind != TokenKind::kString) continue;

    if (kw.text == "newmtl") {
      Token name = line.next();
      if (name.kind == TokenKind::kString) {
        cur_mat = std::string(name.text);
        if (result.size() >= kFtlMaxFaces) return ARX_OBJ_TOO_MANY_MATERIALS;
        result.emplace(cur_mat, MtlEntry{});
      } else {
        log(ARX_LOG_WARN, "MTL: 'newmtl' missing name");
      }
    } else if (!cur_mat.empty()) {
      if (kw.text == "map_Kd") {
        Token val = line.restOfLine();
        if (val.kind == TokenKind::kString)
          result[cur_mat].map_kd = std::string(val.text);
        else
          log(ARX_LOG_WARN, std::format("MTL: 'map_Kd' missing value in '{}'", cur_mat));
      } else if (kw.text == "d") {
        Token val = line.next();
        if (val.kind == TokenKind::kString) {
          auto f = parseFloat(val.text);
          if (f)
            result[cur_mat].transval = 1.0f - *f;  // d is opacity
          else
            log(ARX_LOG_WARN, std::format("MTL: 'd' bad float '{}' in '{}'", val.text, cur_mat));
        } else {
          log(ARX_LOG_WARN, std::format("MTL: 'd' missing value in '{}'", cur_mat));
        }
      }
    }
  }
  return ARX_OK;
}

// --- Importer types ---

// (v_idx, vn_idx) -> ftl vertex; vn_idx == SIZE_MAX means no vn
using VertexKey  = std::pair<std::size_t, std::size_t>;
using VertexMap  = std::map<VertexKey, std::uint16_t>;
using TexStemMap = std::unordered_map<std::string, std::int16_t>;

// --- Line handlers ---

static void handleComment(TextCursor& line, std::size_t& origin_pos_idx, bool& has_origin) {
  Token subkw = line.next();
  if (subkw.kind != TokenKind::kString || subkw.text != "origin") return;
  Token val = line.next();
  if (val.kind == TokenKind::kString) {
    auto n = parseSize(val.text);
    if (n && *n > 0) {
      origin_pos_idx = *n - 1;
      has_origin     = true;
      return;
    }
  }
  log(ARX_LOG_WARN, std::format("OBJ import: '# origin' has missing or invalid index"));
}

static ArxReturnCode handleV(TextCursor& line, std::vector<ArxVector3>& positions) {
  Token tx = line.next(), ty = line.next(), tz = line.next();
  auto x = parseFloat(tx.text), y = parseFloat(ty.text), z = parseFloat(tz.text);
  if (tx.kind != TokenKind::kString || !x || ty.kind != TokenKind::kString || !y || tz.kind != TokenKind::kString || !z)
    return ARX_OBJ_BAD_FORMAT;
  if (positions.size() >= kFtlMaxVertices) return ARX_OBJ_TOO_MANY_VERTICES;
  positions.push_back({*x, *y, *z});
  return ARX_OK;
}

static ArxReturnCode handleVn(TextCursor& line, std::vector<ArxVector3>& normals) {
  Token tx = line.next(), ty = line.next(), tz = line.next();
  auto x = parseFloat(tx.text), y = parseFloat(ty.text), z = parseFloat(tz.text);
  if (tx.kind != TokenKind::kString || !x || ty.kind != TokenKind::kString || !y || tz.kind != TokenKind::kString || !z)
    return ARX_OBJ_BAD_FORMAT;
  if (normals.size() >= kFtlMaxVertices) return ARX_OBJ_TOO_MANY_NORMALS;
  normals.push_back({*x, *y, *z});
  return ARX_OK;
}

static ArxReturnCode handleVt(TextCursor& line, std::vector<std::pair<float, float>>& texcoords) {
  Token tu = line.next();
  if (tu.kind != TokenKind::kString) return ARX_OBJ_BAD_FORMAT;
  auto u = parseFloat(tu.text);
  if (!u) return ARX_OBJ_BAD_FORMAT;
  Token tv               = line.next();  // v is optional, default 0
  std::optional<float> v = 0.0f;
  if (tv.kind == TokenKind::kString) {
    v = parseFloat(tv.text);
    if (!v) return ARX_OBJ_BAD_FORMAT;
  }
  if (texcoords.size() >= kFtlMaxVertices * 4) return ARX_OBJ_TOO_MANY_TEXCOORDS;
  texcoords.emplace_back(*u, *v);
  return ARX_OK;
}

struct MaterialState {
  std::string_view stem;
  FaceType type  = 0;
  float transval = 0.0f;
};

static void decodeMaterialState(std::string_view name_sv, const MtlEntry* mtl_entry, MaterialState& state) {
  auto [stem, type] = decodeMatName(name_sv);
  state.stem        = stem;
  state.type        = type;
  state.transval    = mtl_entry ? mtl_entry->transval : 0.0f;
}

// dedup by filename; empty stem -> kFtlTextureNone
static ArxReturnCode resolveOrCreateTexture(std::string_view stem, const MtlEntry* mtl_entry,
                                            TexStemMap& tex_stem_to_idx, ftl::Data* out, std::int16_t& tex_id) {
  if (stem.empty()) {
    tex_id = kFtlTextureNone;
    return ARX_OK;
  }

  std::string stem_str(stem);
  auto it = tex_stem_to_idx.find(stem_str);
  if (it != tex_stem_to_idx.end()) {
    tex_id = it->second;
    return ARX_OK;
  }

  std::string filename;
  if (mtl_entry && !mtl_entry->arx_path.empty()) {
    filename = mtl_entry->arx_path;
  } else if (mtl_entry && !mtl_entry->map_kd.empty()) {
    filename = mtl_entry->map_kd;
  } else {
    if (!mtl_entry)
      log(ARX_LOG_WARN, std::format("OBJ import: material '{}' not found in MTL, using stem as filename", stem_str));
    filename = stem_str;
  }

  // multiple materials pointing to the same file share one container
  for (std::size_t i = 0; i < out->texture_containers.size(); ++i) {
    if (std::string_view(out->texture_containers[i].filename) == filename) {
      auto idx                  = static_cast<std::int16_t>(i);
      tex_stem_to_idx[stem_str] = idx;
      tex_id                    = idx;
      return ARX_OK;
    }
  }

  ftl::TextureContainer tex_cont{};
  auto n = std::min(filename.size(), std::size_t(255));
  std::memcpy(tex_cont.filename, filename.data(), n);
  tex_cont.filename[n] = '\0';

  if (out->texture_containers.size() >= kFtlMaxTextures) return ARX_OBJ_TOO_MANY_TEXTURES;
  auto idx = static_cast<std::int16_t>(out->texture_containers.size());
  out->texture_containers.push_back(tex_cont);
  tex_stem_to_idx[stem_str] = idx;
  tex_id                    = idx;
  return ARX_OK;
}

static ArxReturnCode handleUsemtl(TextCursor& line, const std::unordered_map<std::string, MtlEntry>& mtl_map,
                                  TexStemMap& tex_stem_to_idx, ftl::Data* out, std::int16_t& cur_texid,
                                  FaceType& cur_type, float& cur_transval) {
  Token name_tok = line.next();
  if (name_tok.kind != TokenKind::kString) {
    log(ARX_LOG_WARN, "OBJ import: bare 'usemtl' - resetting to no texture");
    cur_texid    = kFtlTextureNone;
    cur_type     = 0;
    cur_transval = 0.0f;
    return ARX_OK;
  }

  auto mit                  = mtl_map.find(std::string(name_tok.text));
  const MtlEntry* mtl_entry = (mit != mtl_map.end()) ? &mit->second : nullptr;

  MaterialState st;
  decodeMaterialState(name_tok.text, mtl_entry, st);
  cur_type     = st.type;
  cur_transval = st.transval;

  return resolveOrCreateTexture(st.stem, mtl_entry, tex_stem_to_idx, out, cur_texid);
}

struct FaceCorner {
  std::size_t vi = 0, vti = 0, vni = 0;
  bool has_vt = false, has_vn = false;
};

// vi | vi/vti | vi//vni | vi/vti/vni
static ArxReturnCode parseFaceCorner(std::string_view token, const std::vector<ArxVector3>& positions,
                                     const std::vector<std::pair<float, float>>& texcoords,
                                     const std::vector<ArxVector3>& normals, FaceCorner& c) {
  auto s1 = token.find('/');
  if (s1 == std::string_view::npos) {
    if (!resolveObjIdx(token, positions.size(), c.vi)) return ARX_OBJ_BAD_VERTEX_IDX;
    return ARX_OK;
  }
  if (!resolveObjIdx(token.substr(0, s1), positions.size(), c.vi)) return ARX_OBJ_BAD_VERTEX_IDX;
  auto s2 = token.find('/', s1 + 1);
  if (s2 == std::string_view::npos) {
    auto vt_sv = token.substr(s1 + 1);
    c.has_vt   = !vt_sv.empty();
    if (c.has_vt && !resolveObjIdx(vt_sv, texcoords.size(), c.vti)) return ARX_OBJ_BAD_VERTEX_IDX;
    return ARX_OK;
  }
  auto vt_sv = token.substr(s1 + 1, s2 - s1 - 1);
  c.has_vt   = !vt_sv.empty();
  if (c.has_vt && !resolveObjIdx(vt_sv, texcoords.size(), c.vti)) return ARX_OBJ_BAD_VERTEX_IDX;
  auto vn_sv = token.substr(s2 + 1);
  c.has_vn   = !vn_sv.empty();
  if (c.has_vn && !resolveObjIdx(vn_sv, normals.size(), c.vni)) return ARX_OBJ_BAD_VERTEX_IDX;
  return ARX_OK;
}

static ArxReturnCode collectCorners(TextCursor& line, const std::vector<ArxVector3>& positions,
                                    const std::vector<std::pair<float, float>>& texcoords,
                                    const std::vector<ArxVector3>& normals, std::size_t current_face_count,
                                    std::vector<FaceCorner>& corners) {
  corners.clear();
  for (Token t = line.next(); t.kind == TokenKind::kString; t = line.next()) {
    FaceCorner c{};
    auto rc = parseFaceCorner(t.text, positions, texcoords, normals, c);
    if (rc != ARX_OK) return rc;
    corners.push_back(c);
    if (corners.size() > 2 && current_face_count + corners.size() - 2 > kFtlMaxFaces) return ARX_OBJ_TOO_MANY_FACES;
  }
  return ARX_OK;
}

// no-vn corners share slot (vi, SIZE_MAX) so averaging accumulates into one vertex
static ArxReturnCode dedupVertex(const FaceCorner& corner, const std::vector<ArxVector3>& positions,
                                 const std::vector<ArxVector3>& normals, VertexMap& vertex_map, ftl::Data* out,
                                 std::vector<ArxVector3>& accum_normals, std::vector<bool>& vertex_needs_normal,
                                 std::uint16_t& ftl_vi) {
  auto key = std::make_pair(corner.vi, corner.has_vn ? corner.vni : std::numeric_limits<std::size_t>::max());
  auto vit = vertex_map.find(key);
  if (vit != vertex_map.end()) {
    ftl_vi = vit->second;
    return ARX_OK;
  }
  if (out->vertices.size() >= kFtlMaxVertices) return ARX_OBJ_TOO_MANY_VERTICES;
  auto idx      = static_cast<std::uint16_t>(out->vertices.size());
  ArxVector3 vn = corner.has_vn ? normals[corner.vni] : ArxVector3{0.0f, 0.0f, 0.0f};
  out->vertices.push_back({positions[corner.vi], vn});
  accum_normals.push_back({0.0f, 0.0f, 0.0f});
  vertex_needs_normal.push_back(!corner.has_vn);
  vertex_map[key] = idx;
  ftl_vi          = idx;
  return ARX_OK;
}

static ArxReturnCode emitTriangle(const FaceCorner* const tri[3], const std::uint16_t tri_ftl_vi[3],
                                  const std::vector<ArxVector3>& positions,
                                  const std::vector<std::pair<float, float>>& texcoords, std::int16_t cur_texid,
                                  FaceType cur_type, float cur_transval, ftl::Data* out,
                                  std::vector<ArxVector3>& accum_normals) {
  ftl::Face face{};
  face.type       = cur_type;
  face.transval   = cur_transval;
  face.texture_id = cur_texid;
  face.vertex_idx = Vec3<std::uint16_t>{tri_ftl_vi[0], tri_ftl_vi[1], tri_ftl_vi[2]};

  face.u.x = tri[0]->has_vt ? texcoords[tri[0]->vti].first : 0.0f;
  face.v.x = tri[0]->has_vt ? texcoords[tri[0]->vti].second : 0.0f;
  face.u.y = tri[1]->has_vt ? texcoords[tri[1]->vti].first : 0.0f;
  face.v.y = tri[1]->has_vt ? texcoords[tri[1]->vti].second : 0.0f;
  face.u.z = tri[2]->has_vt ? texcoords[tri[2]->vti].first : 0.0f;
  face.v.z = tri[2]->has_vt ? texcoords[tri[2]->vti].second : 0.0f;

  const auto& p0 = positions[tri[0]->vi];
  const auto& p1 = positions[tri[1]->vi];
  const auto& p2 = positions[tri[2]->vi];
  float e1x = p1.x - p0.x, e1y = p1.y - p0.y, e1z = p1.z - p0.z;
  float e2x = p2.x - p0.x, e2y = p2.y - p0.y, e2z = p2.z - p0.z;
  float nx  = e1y * e2z - e1z * e2y;
  float ny  = e1z * e2x - e1x * e2z;
  float nz  = e1x * e2y - e1y * e2x;
  float len = std::sqrt(nx * nx + ny * ny + nz * nz);
  face.norm = (len > 0.0f) ? ArxVector3{nx / len, ny / len, nz / len} : ArxVector3{0.0f, 0.0f, 1.0f};

  for (int i = 0; i < 3; ++i) {
    if (!tri[i]->has_vn) {
      auto& a = accum_normals[tri_ftl_vi[i]];
      a.x += face.norm.x;
      a.y += face.norm.y;
      a.z += face.norm.z;
    }
  }

  if (out->faces.size() >= kFtlMaxFaces) return ARX_OBJ_TOO_MANY_FACES;
  out->faces.push_back(face);
  return ARX_OK;
}

static ArxReturnCode handleFace(TextCursor& line, std::uint32_t kw_line, const std::vector<ArxVector3>& positions,
                                const std::vector<ArxVector3>& normals,
                                const std::vector<std::pair<float, float>>& texcoords, std::int16_t cur_texid,
                                FaceType cur_type, float cur_transval, VertexMap& vertex_map, ftl::Data* out,
                                std::vector<ArxVector3>& accum_normals, std::vector<bool>& vertex_needs_normal) {
  std::vector<FaceCorner> corners;
  ARX_RETURN_IF_ERR(collectCorners(line, positions, texcoords, normals, out->faces.size(), corners));

  if (corners.size() < 3) return ARX_OBJ_BAD_FORMAT;
  if (corners.size() == 4)
    log(ARX_LOG_DEBUG, std::format("OBJ import: quad at line {} triangulated into 2 triangles", kw_line));
  else if (corners.size() > 4)
    log(ARX_LOG_INFO, std::format("OBJ import: {}-gon at line {} triangulated into {} triangles", corners.size(),
                                  kw_line, corners.size() - 2));

  // fan: (0, fi, fi+1)
  for (std::size_t fi = 1; fi + 1 < corners.size(); ++fi) {
    const FaceCorner* tri[3]    = {&corners[0], &corners[fi], &corners[fi + 1]};
    std::uint16_t tri_ftl_vi[3] = {};
    for (int i = 0; i < 3; ++i) {
      ARX_RETURN_IF_ERR(
          dedupVertex(*tri[i], positions, normals, vertex_map, out, accum_normals, vertex_needs_normal, tri_ftl_vi[i]));
    }
    ARX_RETURN_IF_ERR(
        emitTriangle(tri, tri_ftl_vi, positions, texcoords, cur_texid, cur_type, cur_transval, out, accum_normals));
  }
  return ARX_OK;
}

// --- Importer ---

ArxReturnCode importObjToFtl(std::string_view obj, std::string_view mtl, std::string_view obj_filename,
                             ftl::Data* out) {
  std::unordered_map<std::string, MtlEntry> mtl_map;
  if (auto rc = parseMtl(mtl, mtl_map); rc != ARX_OK) return rc;

  std::vector<ArxVector3> positions;
  std::vector<ArxVector3> normals;
  std::vector<std::pair<float, float>> texcoords;

  std::int16_t cur_texid = kFtlTextureNone;
  FaceType cur_type      = 0;
  float cur_transval     = 0.0f;

  VertexMap vertex_map;
  TexStemMap tex_stem_to_idx;

  std::size_t origin_pos_idx = 0;
  bool has_origin            = false;

  std::vector<ArxVector3> accum_normals;
  std::vector<bool> vertex_needs_normal;

  std::size_t pos        = 0;
  std::uint32_t line_num = 0;

  while (pos < obj.size()) {
    auto line_sv = nextLineSv(obj, pos);
    line_num++;
    TextCursor line(line_sv, "#");
    Token kw = line.next();

    if (kw.kind == TokenKind::kSpecialChar) {
      handleComment(line, origin_pos_idx, has_origin);
      continue;
    }

    if (kw.kind != TokenKind::kString) continue;

    auto keyword     = kw.text;
    ArxReturnCode rc = ARX_OK;
    bool recognized  = true;
    if (keyword == "v")
      rc = handleV(line, positions);
    else if (keyword == "vn")
      rc = handleVn(line, normals);
    else if (keyword == "vt")
      rc = handleVt(line, texcoords);
    else if (keyword == "usemtl")
      rc = handleUsemtl(line, mtl_map, tex_stem_to_idx, out, cur_texid, cur_type, cur_transval);
    else if (keyword == "f")
      rc = handleFace(line, line_num, positions, normals, texcoords, cur_texid, cur_type, cur_transval, vertex_map, out,
                      accum_normals, vertex_needs_normal);
    else {
      if (keyword == "mtllib") {
        Token val = line.next();
        if (val.kind == TokenKind::kString)
          log(ARX_LOG_INFO, std::format("OBJ import: mtllib '{}' ignored (MTL passed separately)", val.text));
      }
      recognized = false;
    }

    if (rc != ARX_OK) return rc;

    if (recognized) {
      Token leftover = line.next();
      if (leftover.kind == TokenKind::kString)
        log(ARX_LOG_WARN,
            std::format("OBJ import: unexpected token '{}' after '{}' on line {}", leftover.text, keyword, line_num));
    }
  }

  std::size_t normals_computed = 0;
  for (std::size_t i = 0; i < out->vertices.size(); ++i) {
    if (!vertex_needs_normal[i]) continue;
    normals_computed++;
    auto& n   = accum_normals[i];
    float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len > 0.0f) {
      out->vertices[i].normal = ArxVector3{n.x / len, n.y / len, n.z / len};
    } else {
      log(ARX_LOG_WARN, std::format("OBJ import: vertex {} has degenerate normal, using fallback", i));
      out->vertices[i].normal = ArxVector3{0.0f, 0.0f, 1.0f};
    }
  }
  if (normals_computed > 0)
    log(ARX_LOG_INFO, std::format("OBJ import: {} vertex normals computed from face normals", normals_computed));

  if (out->vertices.empty()) return ARX_OBJ_NO_GEOMETRY;

  if (has_origin && origin_pos_idx >= positions.size()) {
    log(ARX_LOG_WARN, std::format("OBJ import: origin index {} out of range ({} positions), ignoring",
                                  origin_pos_idx + 1, positions.size()));
    has_origin = false;
  }

  if (has_origin) {
    bool found = false;
    for (const auto& [key, ftl_idx] : vertex_map) {
      if (key.first == origin_pos_idx) {
        out->header.origin = ftl_idx;
        found              = true;
        break;
      }
    }
    if (!found) {
      if (out->vertices.size() >= kFtlMaxVertices) return ARX_OBJ_TOO_MANY_VERTICES;
      log(ARX_LOG_INFO, std::format("OBJ import: origin position {} unreferenced by any face, appended as vertex {}",
                                    origin_pos_idx + 1, out->vertices.size()));
      out->header.origin = static_cast<std::uint32_t>(out->vertices.size());
      out->vertices.push_back({positions[origin_pos_idx], {0.0f, 0.0f, 0.0f}});
    }
  } else {
    if (out->vertices.size() >= kFtlMaxVertices) return ARX_OBJ_TOO_MANY_VERTICES;
    log(ARX_LOG_INFO, "OBJ import: no origin comment found, appending {0,0,0} as origin vertex");
    out->header.origin = static_cast<std::uint32_t>(out->vertices.size());
    out->vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});
  }

  if (!obj_filename.empty()) {
    auto fname        = pathFilename(obj_filename);
    std::string hname = "arx_pistoris\\";
    hname += fname;
    auto n = std::min(hname.size(), std::size_t(255));
    std::memcpy(out->header.name, hname.data(), n);
    out->header.name[n] = '\0';
  } else {
    out->header.name[0] = '\0';
  }

  log(ARX_LOG_INFO, std::format("OBJ import done: {} vertices, {} faces, {} textures", out->vertices.size(),
                                out->faces.size(), out->texture_containers.size()));

  if (ArxReturnCode rc = validateFtl(out); rc != ARX_OK) {
    log(ARX_LOG_ERROR, std::format("OBJ import: constructed data failed validation (rc={}); possible importer bug",
                                   static_cast<int>(rc)));
    return rc;
  }

  return ARX_OK;
}

}  // namespace pistoris
