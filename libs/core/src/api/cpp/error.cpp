// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/error.hpp"

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

#include "api/result_failure.h"

#include <string>
#include <string_view>

namespace pistoris {

#define ARX_DEFINE_ELEMENT_NAME(Element)                            \
  std::string_view errorElementName(Element element) noexcept {     \
    return api_detail::result_failure_detail::elementName(element); \
  }

ARX_DEFINE_ELEMENT_NAME(LevelElement)
ARX_DEFINE_ELEMENT_NAME(ModelElement)
ARX_DEFINE_ELEMENT_NAME(AnimationElement)
ARX_DEFINE_ELEMENT_NAME(AmbianceElement)
ARX_DEFINE_ELEMENT_NAME(CinematicElement)
ARX_DEFINE_ELEMENT_NAME(GlbElement)
ARX_DEFINE_ELEMENT_NAME(AmbElement)
ARX_DEFINE_ELEMENT_NAME(CinElement)
ARX_DEFINE_ELEMENT_NAME(DlfElement)
ARX_DEFINE_ELEMENT_NAME(FtlElement)
ARX_DEFINE_ELEMENT_NAME(FtsElement)
ARX_DEFINE_ELEMENT_NAME(LlfElement)
ARX_DEFINE_ELEMENT_NAME(TeaElement)

#undef ARX_DEFINE_ELEMENT_NAME

std::string_view errorElementName(ObjSource source) noexcept {
  return source == ObjSource::kObj ? "OBJ document" : "material library";
}

#define ARX_DEFINE_ERROR_DESCRIPTION(Location)                      \
  std::string describeError(const Error<Location>& error) {         \
    return api_detail::result_failure_detail::describeError(error); \
  }

ARX_DEFINE_ERROR_DESCRIPTION(LevelLocation)
ARX_DEFINE_ERROR_DESCRIPTION(ModelLocation)
ARX_DEFINE_ERROR_DESCRIPTION(AnimationLocation)
ARX_DEFINE_ERROR_DESCRIPTION(AmbianceLocation)
ARX_DEFINE_ERROR_DESCRIPTION(CinematicLocation)
ARX_DEFINE_ERROR_DESCRIPTION(GlbLocation)
ARX_DEFINE_ERROR_DESCRIPTION(ObjLocation)
ARX_DEFINE_ERROR_DESCRIPTION(AmbLocation)
ARX_DEFINE_ERROR_DESCRIPTION(CinLocation)
ARX_DEFINE_ERROR_DESCRIPTION(DlfLocation)
ARX_DEFINE_ERROR_DESCRIPTION(FtlLocation)
ARX_DEFINE_ERROR_DESCRIPTION(FtsLocation)
ARX_DEFINE_ERROR_DESCRIPTION(LlfLocation)
ARX_DEFINE_ERROR_DESCRIPTION(TeaLocation)
ARX_DEFINE_ERROR_DESCRIPTION(LevelNativeLocation)
ARX_DEFINE_ERROR_DESCRIPTION(DlfWriteLocation)
ARX_DEFINE_ERROR_DESCRIPTION(JsonLocation)
ARX_DEFINE_ERROR_DESCRIPTION(AmbBinaryLocation)
ARX_DEFINE_ERROR_DESCRIPTION(CinBinaryLocation)
ARX_DEFINE_ERROR_DESCRIPTION(DlfBinaryLocation)
ARX_DEFINE_ERROR_DESCRIPTION(FtlBinaryLocation)
ARX_DEFINE_ERROR_DESCRIPTION(FtsBinaryLocation)
ARX_DEFINE_ERROR_DESCRIPTION(LlfBinaryLocation)
ARX_DEFINE_ERROR_DESCRIPTION(TeaBinaryLocation)
ARX_DEFINE_ERROR_DESCRIPTION(LevelGlbExportLocation)
ARX_DEFINE_ERROR_DESCRIPTION(ModelGlbExportLocation)
ARX_DEFINE_ERROR_DESCRIPTION(AmbianceGlbExportLocation)

#undef ARX_DEFINE_ERROR_DESCRIPTION

}  // namespace pistoris
