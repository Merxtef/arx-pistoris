// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"

#include "model/data.h"
#include "model/internal.h"
#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "utils/math/finite.h"
#include "utils/math/quat.h"
#include "utils/math/rotation.h"

#include <cmath>

namespace pistoris {

ArxReturnCode Model::scale(float factor) noexcept {
  if (!std::isfinite(factor) || factor <= 0.0f) return ARX_INVALID_OPTIONS;
  ArxReturnCode rc = model_detail::geometryError(geometry::validateScale(data_->geometry, factor));
  if (rc != ARX_OK) return rc;
  rc = model_detail::skeletonError(skeleton::validateScale(data_->skeleton, factor));
  if (rc != ARX_OK) return rc;
  rc = model_detail::actionPointError(action_points::validateScale(data_->action_points, factor));
  if (rc != ARX_OK) return rc;
  rc = model_detail::selectionError(selections::validateScale(data_->selections, factor));
  if (rc != ARX_OK) return rc;

  geometry::applyScale(data_->geometry, factor);
  skeleton::applyScale(data_->skeleton, factor);
  action_points::applyScale(data_->action_points, factor);
  selections::applyScale(data_->selections, factor);
  return ARX_OK;
}

ArxReturnCode Model::rotate(ArxQuat rotation) noexcept {
  if (!math::normalizeRotation(rotation)) return ARX_INVALID_OPTIONS;
  const ArxMat3 matrix = math::quatToRotation(rotation);
  ArxReturnCode rc = model_detail::geometryError(geometry::validateRotation(data_->geometry, matrix));
  if (rc != ARX_OK) return rc;
  rc = model_detail::skeletonError(skeleton::validateRotation(data_->skeleton, matrix));
  if (rc != ARX_OK) return rc;
  rc = model_detail::actionPointError(action_points::validateRotation(data_->action_points, matrix));
  if (rc != ARX_OK) return rc;
  rc = model_detail::selectionError(selections::validateRotation(data_->selections, matrix));
  if (rc != ARX_OK) return rc;

  geometry::applyRotation(data_->geometry, matrix);
  skeleton::applyRotation(data_->skeleton, matrix);
  action_points::applyRotation(data_->action_points, matrix);
  selections::applyRotation(data_->selections, matrix);
  return ARX_OK;
}

ArxReturnCode Model::translate(ArxVector3 offset) noexcept {
  if (!math::finite(offset)) return ARX_INVALID_OPTIONS;
  ArxReturnCode rc = model_detail::geometryError(geometry::validateTranslation(data_->geometry, offset));
  if (rc != ARX_OK) return rc;
  rc = model_detail::skeletonError(skeleton::validateTranslation(data_->skeleton, offset));
  if (rc != ARX_OK) return rc;
  rc = model_detail::actionPointError(action_points::validateTranslation(data_->action_points, offset));
  if (rc != ARX_OK) return rc;
  rc = model_detail::selectionError(selections::validateTranslation(data_->selections, offset));
  if (rc != ARX_OK) return rc;

  geometry::applyTranslation(data_->geometry, offset);
  skeleton::applyTranslation(data_->skeleton, offset);
  action_points::applyTranslation(data_->action_points, offset);
  selections::applyTranslation(data_->selections, offset);
  return ARX_OK;
}

}  // namespace pistoris
