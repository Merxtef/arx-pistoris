// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native.hpp"

#include "arx_pistoris/base/status.h"

#include "api/status_boundary.h"
#include "native/amb.h"
#include "native/cin.h"
#include "native/dlf.h"
#include "native/ftl.h"
#include "native/fts.h"
#include "native/llf.h"
#include "native/storage.h"
#include "native/tea.h"
#include "utils/cursor.h"

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pistoris {

using api_detail::statusBoundary;

ArxReturnCode readAmb(std::span<const std::uint8_t> data, Amb& out) noexcept {
  return statusBoundary([&] {
    Amb tmp;
    ReadCursor cursor(data.data(), data.size());
    ArxReturnCode rc = loadAmb(&tmp, cursor);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode writeAmb(const Amb& amb, std::vector<std::uint8_t>& out) noexcept {
  return statusBoundary([&] {
    WriteCursor cursor;
    ArxReturnCode rc = saveAmb(&amb, cursor);
    if (rc == ARX_OK) out = cursor.take();
    return rc;
  });
}

ArxReturnCode readCin(std::span<const std::uint8_t> data, Cin& out) noexcept {
  return statusBoundary([&] {
    Cin tmp;
    ReadCursor cursor(data.data(), data.size());
    const ArxReturnCode rc = loadCin(&tmp, cursor);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode writeCin(const Cin& cin, std::vector<std::uint8_t>& out) noexcept {
  return statusBoundary([&] {
    WriteCursor cursor;
    const ArxReturnCode rc = saveCin(&cin, cursor);
    if (rc == ARX_OK) out = cursor.take();
    return rc;
  });
}

ArxReturnCode readDlf(std::span<const std::uint8_t> data, Dlf& out, std::optional<Llf>* embedded_lighting) noexcept {
  return statusBoundary([&] {
    Dlf tmp;
    std::optional<Llf> lighting;
    ArxReturnCode rc = loadDlfStorage(&tmp, embedded_lighting ? &lighting : nullptr, data);
    if (rc == ARX_OK) {
      out = std::move(tmp);
      if (embedded_lighting) *embedded_lighting = std::move(lighting);
    }
    return rc;
  });
}

ArxReturnCode writeDlf(const Dlf& dlf, const DlfWriteOptions& options, std::vector<std::uint8_t>& out,
                       bool compress) noexcept {
  return statusBoundary([&] {
    std::vector<std::uint8_t> tmp;
    ArxReturnCode rc = saveDlfStorage(&dlf, options.embedded_lighting, options.signer, tmp, compress);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode readFtl(std::span<const std::uint8_t> data, Ftl& out) noexcept {
  return statusBoundary([&] {
    Ftl tmp;
    ArxReturnCode rc = loadFtlStorage(&tmp, data);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode writeFtl(const Ftl& ftl, std::vector<std::uint8_t>& out, bool compress) noexcept {
  return statusBoundary([&] {
    std::vector<std::uint8_t> tmp;
    ArxReturnCode rc = saveFtlStorage(&ftl, tmp, compress);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode readFts(std::span<const std::uint8_t> data, Fts& out) noexcept {
  return statusBoundary([&] {
    Fts tmp;
    ArxReturnCode rc = loadFtsStorage(&tmp, data);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode writeFts(const Fts& fts, std::vector<std::uint8_t>& out, bool compress) noexcept {
  return statusBoundary([&] {
    std::vector<std::uint8_t> tmp;
    ArxReturnCode rc = saveFtsStorage(&fts, tmp, compress);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode readLlf(std::span<const std::uint8_t> data, Llf& out) noexcept {
  return statusBoundary([&] {
    Llf tmp;
    ArxReturnCode rc = loadLlfStorage(&tmp, data);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode writeLlf(const Llf& llf, std::vector<std::uint8_t>& out, bool compress) noexcept {
  return writeLlf(llf, LlfWriteOptions{}, out, compress);
}

ArxReturnCode writeLlf(const Llf& llf, const LlfWriteOptions& options, std::vector<std::uint8_t>& out,
                       bool compress) noexcept {
  return statusBoundary([&] {
    std::vector<std::uint8_t> tmp;
    ArxReturnCode rc = saveLlfStorage(&llf, options.signer, tmp, compress);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode readTea(std::span<const std::uint8_t> data, Tea& out) noexcept {
  return statusBoundary([&] {
    Tea tmp;
    ReadCursor cursor(data.data(), data.size());
    ArxReturnCode rc = loadTea(&tmp, cursor);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode writeTea(const Tea& tea, std::vector<std::uint8_t>& out) noexcept {
  return statusBoundary([&] {
    WriteCursor cursor;
    ArxReturnCode rc = saveTea(&tea, cursor);
    if (rc == ARX_OK) out = cursor.take();
    return rc;
  });
}

ArxReturnCode validate(const Amb& amb) noexcept {
  return statusBoundary([&] { return validateAmb(&amb); });
}

ArxReturnCode validate(const Cin& cin) noexcept {
  return statusBoundary([&] { return validateCin(&cin); });
}

ArxReturnCode validate(const Dlf& dlf) noexcept {
  return statusBoundary([&] { return validateDlf(&dlf); });
}

ArxReturnCode validate(const Ftl& ftl) noexcept {
  return statusBoundary([&] { return validateFtl(&ftl); });
}

ArxReturnCode validate(const Fts& fts) noexcept {
  return statusBoundary([&] { return validateFts(&fts); });
}

ArxReturnCode validate(const Llf& llf) noexcept {
  return statusBoundary([&] { return validateLlf(&llf); });
}

ArxReturnCode validate(const Tea& tea) noexcept {
  return statusBoundary([&] { return validateTea(&tea); });
}

}  // namespace pistoris
