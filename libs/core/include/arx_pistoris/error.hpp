// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance/location.hpp"
#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/cinematic/location.hpp"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/model/obj_location.hpp"
#include "arx_pistoris/native/location.hpp"

#include <string>
#include <string_view>

namespace pistoris {

[[nodiscard]] std::string_view errorElementName(LevelElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(ModelElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(AnimationElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(AmbianceElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(CinematicElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(GlbElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(ObjSource source) noexcept;

[[nodiscard]] std::string_view errorElementName(AmbElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(CinElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(DlfElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(FtlElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(FtsElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(LlfElement element) noexcept;
[[nodiscard]] std::string_view errorElementName(TeaElement element) noexcept;

[[nodiscard]] std::string describeError(const Error<LevelLocation>& error);
[[nodiscard]] std::string describeError(const Error<ModelLocation>& error);
[[nodiscard]] std::string describeError(const Error<AnimationLocation>& error);
[[nodiscard]] std::string describeError(const Error<AmbianceLocation>& error);
[[nodiscard]] std::string describeError(const Error<CinematicLocation>& error);
[[nodiscard]] std::string describeError(const Error<GlbLocation>& error);
[[nodiscard]] std::string describeError(const Error<ObjLocation>& error);

[[nodiscard]] std::string describeError(const Error<AmbLocation>& error);
[[nodiscard]] std::string describeError(const Error<CinLocation>& error);
[[nodiscard]] std::string describeError(const Error<DlfLocation>& error);
[[nodiscard]] std::string describeError(const Error<FtlLocation>& error);
[[nodiscard]] std::string describeError(const Error<FtsLocation>& error);
[[nodiscard]] std::string describeError(const Error<LlfLocation>& error);
[[nodiscard]] std::string describeError(const Error<TeaLocation>& error);
[[nodiscard]] std::string describeError(const Error<LevelNativeLocation>& error);
[[nodiscard]] std::string describeError(const Error<DlfWriteLocation>& error);

[[nodiscard]] std::string describeError(const Error<JsonLocation>& error);
[[nodiscard]] std::string describeError(const Error<AmbBinaryLocation>& error);
[[nodiscard]] std::string describeError(const Error<CinBinaryLocation>& error);
[[nodiscard]] std::string describeError(const Error<DlfBinaryLocation>& error);
[[nodiscard]] std::string describeError(const Error<FtlBinaryLocation>& error);
[[nodiscard]] std::string describeError(const Error<FtsBinaryLocation>& error);
[[nodiscard]] std::string describeError(const Error<LlfBinaryLocation>& error);
[[nodiscard]] std::string describeError(const Error<TeaBinaryLocation>& error);

[[nodiscard]] std::string describeError(const Error<LevelGlbExportLocation>& error);
[[nodiscard]] std::string describeError(const Error<ModelGlbExportLocation>& error);
[[nodiscard]] std::string describeError(const Error<AmbianceGlbExportLocation>& error);

}  // namespace pistoris
