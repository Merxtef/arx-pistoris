// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace pistoris {

// Hamilton product; a*b applies b first then a (rotation composition order)
inline ArxQuat operator*(const ArxQuat& a, const ArxQuat& b) {
  return ArxQuat{a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
                 a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                 a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                 a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

inline bool operator==(const ArxQuat& a, const ArxQuat& b) {
  return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z;
}

namespace math {

inline constexpr ArxQuat kIdentityQuat = {1.0f, 0.0f, 0.0f, 0.0f};
inline constexpr float kRadiansPerDegree = std::numbers::pi_v<float> / 180.0f;
inline constexpr float kDegreesPerRadian = 180.0f / std::numbers::pi_v<float>;

inline ArxQuat conjugate(const ArxQuat& q) { return {q.w, -q.x, -q.y, -q.z}; }

inline ArxQuat canonicalizeQuaternionSign(ArxQuat q) {
  if (q.w < 0.0f) return {-q.w, -q.x, -q.y, -q.z};
  return q;
}

inline float norm(const ArxQuat& q) { return std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z); }

// zero-norm -> identity
inline ArxQuat normalize(const ArxQuat& q) {
  float n2 = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
  if (n2 == 0.0f) return kIdentityQuat;
  float inv = 1.0f / std::sqrt(n2);
  return {q.w * inv, q.x * inv, q.y * inv, q.z * inv};
}

inline ArxQuat slerp(ArxQuat first, ArxQuat second, float amount) {
  first = normalize(first);
  second = normalize(second);
  float dot = first.w * second.w + first.x * second.x + first.y * second.y + first.z * second.z;
  if (dot < 0.0f) {
    second = {-second.w, -second.x, -second.y, -second.z};
    dot = -dot;
  }
  dot = std::clamp(dot, -1.0f, 1.0f);
  if (dot > 0.9995f)
    return normalize({first.w + (second.w - first.w) * amount,
                      first.x + (second.x - first.x) * amount,
                      first.y + (second.y - first.y) * amount,
                      first.z + (second.z - first.z) * amount});

  const float angle = std::acos(dot);
  const float inverse_sine = 1.0f / std::sin(angle);
  const float first_weight = std::sin((1.0f - amount) * angle) * inverse_sine;
  const float second_weight = std::sin(amount * angle) * inverse_sine;
  return normalize({first.w * first_weight + second.w * second_weight,
                    first.x * first_weight + second.x * second_weight,
                    first.y * first_weight + second.y * second_weight,
                    first.z * first_weight + second.z * second_weight});
}

inline ArxQuat axisAngle(float x, float y, float z, float radians) {
  float half = radians * 0.5f;
  float sine = std::sin(half);
  return {std::cos(half), x * sine, y * sine, z * sine};
}

inline ArxQuat angleToQuat(const ArxAngle& angle) {
  ArxQuat yaw = axisAngle(0.0f, 1.0f, 0.0f, angle.yaw * kRadiansPerDegree);
  ArxQuat pitch = axisAngle(1.0f, 0.0f, 0.0f, angle.pitch * kRadiansPerDegree);
  ArxQuat roll = axisAngle(0.0f, 0.0f, 1.0f, -angle.roll * kRadiansPerDegree);
  return normalize(roll * pitch * yaw);
}

inline ArxQuat rotationToQuat(const ArxMat3& rotation) {
  float trace = rotation(0, 0) + rotation(1, 1) + rotation(2, 2);
  ArxQuat out;
  if (trace > 0.0f) {
    float s = std::sqrt(trace + 1.0f) * 2.0f;
    out.w = 0.25f * s;
    out.x = (rotation(2, 1) - rotation(1, 2)) / s;
    out.y = (rotation(0, 2) - rotation(2, 0)) / s;
    out.z = (rotation(1, 0) - rotation(0, 1)) / s;
  } else if (rotation(0, 0) > rotation(1, 1) && rotation(0, 0) > rotation(2, 2)) {
    float s = std::sqrt(1.0f + rotation(0, 0) - rotation(1, 1) - rotation(2, 2)) * 2.0f;
    out.w = (rotation(2, 1) - rotation(1, 2)) / s;
    out.x = 0.25f * s;
    out.y = (rotation(0, 1) + rotation(1, 0)) / s;
    out.z = (rotation(0, 2) + rotation(2, 0)) / s;
  } else if (rotation(1, 1) > rotation(2, 2)) {
    float s = std::sqrt(1.0f + rotation(1, 1) - rotation(0, 0) - rotation(2, 2)) * 2.0f;
    out.w = (rotation(0, 2) - rotation(2, 0)) / s;
    out.x = (rotation(0, 1) + rotation(1, 0)) / s;
    out.y = 0.25f * s;
    out.z = (rotation(1, 2) + rotation(2, 1)) / s;
  } else {
    float s = std::sqrt(1.0f + rotation(2, 2) - rotation(0, 0) - rotation(1, 1)) * 2.0f;
    out.w = (rotation(1, 0) - rotation(0, 1)) / s;
    out.x = (rotation(0, 2) + rotation(2, 0)) / s;
    out.y = (rotation(1, 2) + rotation(2, 1)) / s;
    out.z = 0.25f * s;
  }
  return normalize(out);
}

inline ArxMat3 quatToRotation(const ArxQuat& rotation) {
  ArxQuat q = normalize(rotation);
  ArxMat3 out{};
  float xx = q.x * q.x;
  float yy = q.y * q.y;
  float zz = q.z * q.z;
  float xy = q.x * q.y;
  float xz = q.x * q.z;
  float yz = q.y * q.z;
  float wx = q.w * q.x;
  float wy = q.w * q.y;
  float wz = q.w * q.z;

  out(0, 0) = 1.0f - 2.0f * (yy + zz);
  out(0, 1) = 2.0f * (xy - wz);
  out(0, 2) = 2.0f * (xz + wy);
  out(1, 0) = 2.0f * (xy + wz);
  out(1, 1) = 1.0f - 2.0f * (xx + zz);
  out(1, 2) = 2.0f * (yz - wx);
  out(2, 0) = 2.0f * (xz - wy);
  out(2, 1) = 2.0f * (yz + wx);
  out(2, 2) = 1.0f - 2.0f * (xx + yy);
  return out;
}

inline ArxAngle quatToAngle(const ArxQuat& rotation) {
  ArxMat3 matrix = quatToRotation(rotation);
  float sin_pitch = std::clamp(matrix(2, 1), -1.0f, 1.0f);
  float pitch = std::asin(sin_pitch);
  float cos_pitch = std::cos(pitch);
  float yaw = 0.0f;
  float roll = 0.0f;
  float tolerance = 1.0e-4f;
  if (std::abs(cos_pitch) > tolerance) {
    yaw = std::atan2(-matrix(2, 0), matrix(2, 2));
    roll = std::atan2(matrix(0, 1), matrix(1, 1));
  } else {
    roll = -std::atan2(matrix(1, 0), matrix(0, 0));
  }
  return {pitch * kDegreesPerRadian, yaw * kDegreesPerRadian, roll * kDegreesPerRadian};
}

inline ArxVector3 rotate(const ArxQuat& rotation, const ArxVector3& value) {
  ArxQuat q = normalize(rotation);
  ArxQuat v{0.0f, value.x, value.y, value.z};
  ArxQuat out = q * v * conjugate(q);
  return {out.x, out.y, out.z};
}

}  // namespace math
}  // namespace pistoris
