// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"  // IWYU pragma: export

#include <cmath>

namespace pistoris {

template <class T>
struct Vec2 {
  T x, y;
};

static_assert(sizeof(ArxVector2) == 8);

template <class T>
struct Vec3 {
  T x, y, z;
};

static_assert(sizeof(ArxVector3) == 12);
static_assert(sizeof(ArxAngle) == 12);
static_assert(sizeof(ArxRect) == 16);
static_assert(sizeof(ArxAabb) == 24);
static_assert(sizeof(ArxColor3) == 12);
static_assert(sizeof(ArxQuat) == 16);
static_assert(sizeof(ArxMat3) == 36);

// --- Operators ---

inline ArxVector2 operator+(const ArxVector2& a, const ArxVector2& b) { return {a.x + b.x, a.y + b.y}; }

inline ArxVector2 operator-(const ArxVector2& a, const ArxVector2& b) { return {a.x - b.x, a.y - b.y}; }

inline ArxVector2 operator-(const ArxVector2& v) { return {-v.x, -v.y}; }

inline ArxVector2 operator*(const ArxVector2& v, float s) { return {v.x * s, v.y * s}; }

inline ArxVector2 operator*(float s, const ArxVector2& v) { return v * s; }

inline ArxVector2 operator/(const ArxVector2& v, float s) {
  const float inv = 1.0f / s;
  return {v.x * inv, v.y * inv};
}

inline bool operator==(const ArxVector2& a, const ArxVector2& b) { return a.x == b.x && a.y == b.y; }

template <class T>
inline Vec2<T> operator+(const Vec2<T>& a, const Vec2<T>& b) {
  return {a.x + b.x, a.y + b.y};
}

template <class T>
inline Vec2<T> operator-(const Vec2<T>& a, const Vec2<T>& b) {
  return {a.x - b.x, a.y - b.y};
}

template <class T>
inline Vec2<T> operator-(const Vec2<T>& v) {
  return {-v.x, -v.y};
}

template <class T>
inline Vec2<T> operator*(const Vec2<T>& v, T s) {
  return {v.x * s, v.y * s};
}

template <class T>
inline Vec2<T> operator*(T s, const Vec2<T>& v) {
  return v * s;
}

template <class T>
inline Vec2<T> operator/(const Vec2<T>& v, T s) {
  T inv = T{1} / s;
  return {v.x * inv, v.y * inv};
}

template <class T>
inline bool operator==(const Vec2<T>& a, const Vec2<T>& b) {
  return a.x == b.x && a.y == b.y;
}

template <class T>
inline Vec3<T> operator+(const Vec3<T>& a, const Vec3<T>& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

template <class T>
inline Vec3<T> operator-(const Vec3<T>& a, const Vec3<T>& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

template <class T>
inline Vec3<T> operator-(const Vec3<T>& v) {
  return {-v.x, -v.y, -v.z};
}

template <class T>
inline Vec3<T> operator*(const Vec3<T>& v, T s) {
  return {v.x * s, v.y * s, v.z * s};
}

template <class T>
inline Vec3<T> operator*(T s, const Vec3<T>& v) {
  return v * s;
}

template <class T>
inline Vec3<T> operator/(const Vec3<T>& v, T s) {
  const T inverse = T{1} / s;
  return {v.x * inverse, v.y * inverse, v.z * inverse};
}

template <class T>
inline bool operator==(const Vec3<T>& a, const Vec3<T>& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline ArxVector3 operator+(const ArxVector3& a, const ArxVector3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

inline ArxVector3 operator-(const ArxVector3& a, const ArxVector3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

inline ArxVector3 operator-(const ArxVector3& v) { return {-v.x, -v.y, -v.z}; }

inline ArxVector3 operator*(const ArxVector3& v, float s) { return {v.x * s, v.y * s, v.z * s}; }

inline ArxVector3 operator*(float s, const ArxVector3& v) { return v * s; }

inline ArxVector3 operator/(const ArxVector3& v, float s) {
  const float inv = 1.0f / s;
  return {v.x * inv, v.y * inv, v.z * inv};
}

inline bool operator==(const ArxVector3& a, const ArxVector3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

namespace math {

// --- Math operations ---

inline float dotf(const ArxVector2& a, const ArxVector2& b) { return a.x * b.x + a.y * b.y; }

inline double dot(const ArxVector2& a, const ArxVector2& b) {
  return static_cast<double>(a.x) * static_cast<double>(b.x) + static_cast<double>(a.y) * static_cast<double>(b.y);
}

template <class T>
inline double dot(const Vec2<T>& a, const Vec2<T>& b) {
  return static_cast<double>(a.x) * static_cast<double>(b.x) + static_cast<double>(a.y) * static_cast<double>(b.y);
}

template <class T>
inline double cross(const Vec2<T>& a, const Vec2<T>& b) {
  return static_cast<double>(a.x) * static_cast<double>(b.y) - static_cast<double>(a.y) * static_cast<double>(b.x);
}

inline double cross(const ArxVector2& a, const ArxVector2& b) {
  return static_cast<double>(a.x) * static_cast<double>(b.y) - static_cast<double>(a.y) * static_cast<double>(b.x);
}

inline float lengthSquaredf(const ArxVector2& v) { return dotf(v, v); }

inline double lengthSquared(const ArxVector2& v) { return dot(v, v); }

template <class T>
inline double lengthSquared(const Vec2<T>& v) {
  return dot(v, v);
}

inline float lengthf(const ArxVector2& v) { return std::sqrt(lengthSquaredf(v)); }

inline double length(const ArxVector2& v) { return std::sqrt(lengthSquared(v)); }

template <class T>
inline double length(const Vec2<T>& v) {
  return std::sqrt(lengthSquared(v));
}

inline float dotf(const ArxVector3& a, const ArxVector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline double dot(const ArxVector3& a, const ArxVector3& b) {
  return static_cast<double>(a.x) * static_cast<double>(b.x) + static_cast<double>(a.y) * static_cast<double>(b.y) +
         static_cast<double>(a.z) * static_cast<double>(b.z);
}

template <class T>
inline double dot(const Vec3<T>& a, const Vec3<T>& b) {
  return static_cast<double>(a.x) * static_cast<double>(b.x) + static_cast<double>(a.y) * static_cast<double>(b.y) +
         static_cast<double>(a.z) * static_cast<double>(b.z);
}

inline ArxVector3 cross(const ArxVector3& a, const ArxVector3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

template <class T>
inline Vec3<T> cross(const Vec3<T>& a, const Vec3<T>& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float lengthSquaredf(const ArxVector3& v) { return dotf(v, v); }

inline double lengthSquared(const ArxVector3& v) { return dot(v, v); }

template <class T>
inline double lengthSquared(const Vec3<T>& v) {
  return dot(v, v);
}

inline float lengthf(const ArxVector3& v) { return std::sqrt(lengthSquaredf(v)); }

inline double length(const ArxVector3& v) { return std::sqrt(lengthSquared(v)); }

template <class T>
inline double length(const Vec3<T>& v) {
  return std::sqrt(lengthSquared(v));
}

inline ArxVector3 normalize(const ArxVector3& v) {
  const float length_squared = lengthSquaredf(v);
  if (length_squared == 0.0f) return {0.0f, 0.0f, 0.0f};
  const float inverse_length = 1.0f / std::sqrt(length_squared);
  return {v.x * inverse_length, v.y * inverse_length, v.z * inverse_length};
}

inline ArxVector3 normalizeZeroOr(const ArxVector3& v, const ArxVector3& fallback) {
  const float length_squared = lengthSquaredf(v);
  if (length_squared == 0.0f) return fallback;
  const float inverse_length = 1.0f / std::sqrt(length_squared);
  return {v.x * inverse_length, v.y * inverse_length, v.z * inverse_length};
}

inline ArxVector3 normalizeFiniteOr(const ArxVector3& v, const ArxVector3& fallback, double min_length = 0.0) {
  const double vector_length = length(v);
  if (!std::isfinite(vector_length) || vector_length <= min_length) return fallback;
  const double inverse_length = 1.0 / vector_length;
  return {static_cast<float>(static_cast<double>(v.x) * inverse_length),
          static_cast<float>(static_cast<double>(v.y) * inverse_length),
          static_cast<float>(static_cast<double>(v.z) * inverse_length)};
}

inline ArxVector3 componentMin(const ArxVector3& a, const ArxVector3& b) {
  return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z};
}

inline ArxVector3 componentMax(const ArxVector3& a, const ArxVector3& b) {
  return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z};
}

}  // namespace math
}  // namespace pistoris
