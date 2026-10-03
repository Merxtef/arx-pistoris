// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "binding_utils.h"
#include "bindings.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <sstream>
#include <string>
#include <utility>

namespace pistoris::python {
namespace {

std::size_t valueIndex(std::int64_t requested, std::size_t size) {
  const auto signed_size = static_cast<std::int64_t>(size);
  const auto index = requested < 0 ? requested + signed_size : requested;
  if (index < 0 || index >= signed_size) throw nb::index_error();
  return static_cast<std::size_t>(index);
}

template <class... Values>
nb::iterator valueIterator(Values... values) {
  return nb::iter(nb::make_tuple(values...));
}

template <class... Fields>
std::string valueRepr(std::string_view name, const Fields&... fields) {
  std::ostringstream stream;
  stream << name << '(';
  std::size_t index = 0;
  ((stream << (index++ == 0 ? "" : ", ") << fields.first << '=' << fields.second), ...);
  stream << ')';
  return stream.str();
}

template <class T>
void setExceptionAttribute(PyObject* instance, const char* name, T&& value) {
  nb::object object = nb::cast(std::forward<T>(value));
  if (PyObject_SetAttrString(instance, name, object.ptr()) != 0) throw nb::python_error();
}

void translatePistorisException(const std::exception_ptr& pointer, void* payload) {
  try {
    std::rethrow_exception(pointer);
  } catch (const PistorisException& error) {
    PyObject* instance = PyObject_CallFunction(static_cast<PyObject*>(payload), "s", error.what());
    if (!instance) return;
    try {
      setExceptionAttribute(instance, "code", error.code);
      setExceptionAttribute(instance, "message", std::string(error.what()));
      setExceptionAttribute(instance, "detail", error.detail);
      if (error.location) {
        setExceptionAttribute(instance, "location", *error.location);
      } else {
        if (PyObject_SetAttrString(instance, "location", Py_None) != 0) throw nb::python_error();
      }
      PyErr_SetObject(static_cast<PyObject*>(payload), instance);
    } catch (...) {
      PyErr_Clear();
      PyErr_SetString(static_cast<PyObject*>(payload), error.what());
    }
    Py_DECREF(instance);
  }
}

}  // namespace

void bindCommon(nb::module_& module) {
  nb::class_<ErrorLocation>(module, "ErrorLocation")
      .def_prop_ro("domain", [](const ErrorLocation& value) { return value.domain; })
      .def_prop_ro("element", [](const ErrorLocation& value) { return value.element; })
      .def_prop_ro("element_name", [](const ErrorLocation& value) { return value.element_name; })
      .def_prop_ro("input_index", [](const ErrorLocation& value) { return value.input_index; })
      .def_prop_ro("index", [](const ErrorLocation& value) { return value.index; })
      .def_prop_ro("subindex", [](const ErrorLocation& value) { return value.subindex; })
      .def_prop_ro("byte_offset", [](const ErrorLocation& value) { return value.byte_offset; })
      .def_prop_ro("requested_bytes", [](const ErrorLocation& value) { return value.requested_bytes; })
      .def_prop_ro("source_index", [](const ErrorLocation& value) { return value.source_index; })
      .def_prop_ro("line", [](const ErrorLocation& value) { return value.line; })
      .def_prop_ro("sound_handle", [](const ErrorLocation& value) { return value.sound_handle; })
      .def_prop_ro("language_id", [](const ErrorLocation& value) { return value.language_id; })
      .def_prop_ro("label", [](const ErrorLocation& value) { return value.label; })
      .def_prop_ro("property", [](const ErrorLocation& value) { return value.property; })
      .def_prop_ro("field", [](const ErrorLocation& value) { return value.field; })
      .def_prop_ro("resource_path", [](const ErrorLocation& value) { return value.resource_path; })
      .def_prop_ro("source_path", [](const ErrorLocation& value) { return value.source_path; })
      .def_prop_ro("json_pointer", [](const ErrorLocation& value) { return value.json_pointer; })
      .def_prop_ro("binary_region", [](const ErrorLocation& value) { return value.binary_region; });

  nb::object error_type =
      nb::steal<nb::object>(PyErr_NewException("pistoris.PistorisError", PyExc_RuntimeError, nullptr));
  if (!error_type.is_valid()) throw nb::python_error();
  module.attr("PistorisError") = error_type;
  nb::register_exception_translator(translatePistorisException, error_type.ptr());

  nb::module_ math = module.def_submodule("math", "Immutable mathematical value types.");

  nb::class_<ArxVector2>(math, "Vector2")
      .def(nb::init<float, float>(), nb::arg("x") = 0.0f, nb::arg("y") = 0.0f)
      .def_ro("x", &ArxVector2::x)
      .def_ro("y", &ArxVector2::y)
      .def("__len__", [](const ArxVector2&) { return 2; })
      .def("__getitem__",
           [](const ArxVector2& value, std::int64_t index) { return valueIndex(index, 2) == 0 ? value.x : value.y; })
      .def(
          "__iter__",
          [](const ArxVector2& value) { return valueIterator(value.x, value.y); },
          nb::sig("def __iter__(self) -> Iterator[float]"))
      .def(
          "__eq__",
          [](const ArxVector2& left, nb::handle right) {
            return valueEquals(
                left, right, [](const auto& lhs, const auto& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__", [](const ArxVector2& value) { return valueHash(value.x, value.y); })
      .def("__add__",
           [](const ArxVector2& left, const ArxVector2& right) {
             return ArxVector2{left.x + right.x, left.y + right.y};
           })
      .def("__sub__",
           [](const ArxVector2& left, const ArxVector2& right) {
             return ArxVector2{left.x - right.x, left.y - right.y};
           })
      .def("__neg__", [](const ArxVector2& value) { return ArxVector2{-value.x, -value.y}; })
      .def("__mul__",
           [](const ArxVector2& value, float scalar) { return ArxVector2{value.x * scalar, value.y * scalar}; })
      .def("__rmul__",
           [](const ArxVector2& value, float scalar) { return ArxVector2{value.x * scalar, value.y * scalar}; })
      .def("__truediv__",
           [](const ArxVector2& value, float scalar) {
             if (scalar == 0.0f) {
               PyErr_SetString(PyExc_ZeroDivisionError, "cannot divide a vector by zero");
               throw nb::python_error();
             }
             return ArxVector2{value.x / scalar, value.y / scalar};
           })
      .def("dot", [](const ArxVector2& left, const ArxVector2& right) { return left.x * right.x + left.y * right.y; })
      .def("cross", [](const ArxVector2& left, const ArxVector2& right) { return left.x * right.y - left.y * right.x; })
      .def("length_squared", [](const ArxVector2& value) { return value.x * value.x + value.y * value.y; })
      .def("length", [](const ArxVector2& value) { return std::sqrt(value.x * value.x + value.y * value.y); })
      .def("normalized",
           [](const ArxVector2& value) {
             const float length = std::sqrt(value.x * value.x + value.y * value.y);
             if (length == 0.0f) throw nb::value_error("cannot normalize a zero-length vector");
             return ArxVector2{value.x / length, value.y / length};
           })
      .def("__repr__", [](const ArxVector2& value) {
        return valueRepr("Vector2", std::pair{"x", value.x}, std::pair{"y", value.y});
      });
  nb::class_<ArxVector3>(math, "Vector3")
      .def(nb::init<float, float, float>(), nb::arg("x") = 0.0f, nb::arg("y") = 0.0f, nb::arg("z") = 0.0f)
      .def_ro("x", &ArxVector3::x)
      .def_ro("y", &ArxVector3::y)
      .def_ro("z", &ArxVector3::z)
      .def("__len__", [](const ArxVector3&) { return 3; })
      .def("__getitem__",
           [](const ArxVector3& value, std::int64_t index) {
             const float values[] = {value.x, value.y, value.z};
             return values[valueIndex(index, 3)];
           })
      .def(
          "__iter__",
          [](const ArxVector3& value) { return valueIterator(value.x, value.y, value.z); },
          nb::sig("def __iter__(self) -> Iterator[float]"))
      .def(
          "__eq__",
          [](const ArxVector3& left, nb::handle right) {
            return valueEquals(left, right, [](const auto& lhs, const auto& rhs) {
              return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
            });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__", [](const ArxVector3& value) { return valueHash(value.x, value.y, value.z); })
      .def("__add__",
           [](const ArxVector3& left, const ArxVector3& right) {
             return ArxVector3{left.x + right.x, left.y + right.y, left.z + right.z};
           })
      .def("__sub__",
           [](const ArxVector3& left, const ArxVector3& right) {
             return ArxVector3{left.x - right.x, left.y - right.y, left.z - right.z};
           })
      .def("__neg__", [](const ArxVector3& value) { return ArxVector3{-value.x, -value.y, -value.z}; })
      .def("__mul__",
           [](const ArxVector3& value, float scalar) {
             return ArxVector3{value.x * scalar, value.y * scalar, value.z * scalar};
           })
      .def("__rmul__",
           [](const ArxVector3& value, float scalar) {
             return ArxVector3{value.x * scalar, value.y * scalar, value.z * scalar};
           })
      .def("__truediv__",
           [](const ArxVector3& value, float scalar) {
             if (scalar == 0.0f) {
               PyErr_SetString(PyExc_ZeroDivisionError, "cannot divide a vector by zero");
               throw nb::python_error();
             }
             return ArxVector3{value.x / scalar, value.y / scalar, value.z / scalar};
           })
      .def("dot",
           [](const ArxVector3& left, const ArxVector3& right) {
             return left.x * right.x + left.y * right.y + left.z * right.z;
           })
      .def("cross",
           [](const ArxVector3& left, const ArxVector3& right) {
             return ArxVector3{left.y * right.z - left.z * right.y,
                               left.z * right.x - left.x * right.z,
                               left.x * right.y - left.y * right.x};
           })
      .def("length_squared",
           [](const ArxVector3& value) { return value.x * value.x + value.y * value.y + value.z * value.z; })
      .def("length",
           [](const ArxVector3& value) { return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z); })
      .def("normalized",
           [](const ArxVector3& value) {
             const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
             if (length == 0.0f) throw nb::value_error("cannot normalize a zero-length vector");
             return ArxVector3{value.x / length, value.y / length, value.z / length};
           })
      .def("__repr__", [](const ArxVector3& value) {
        return valueRepr("Vector3", std::pair{"x", value.x}, std::pair{"y", value.y}, std::pair{"z", value.z});
      });
  nb::class_<ArxAngle>(math, "Angle")
      .def(nb::init<float, float, float>(), nb::arg("pitch") = 0.0f, nb::arg("yaw") = 0.0f, nb::arg("roll") = 0.0f)
      .def_ro("pitch", &ArxAngle::pitch)
      .def_ro("yaw", &ArxAngle::yaw)
      .def_ro("roll", &ArxAngle::roll)
      .def("__len__", [](const ArxAngle&) { return 3; })
      .def("__getitem__",
           [](const ArxAngle& value, std::int64_t index) {
             const float values[] = {value.pitch, value.yaw, value.roll};
             return values[valueIndex(index, 3)];
           })
      .def(
          "__iter__",
          [](const ArxAngle& value) { return valueIterator(value.pitch, value.yaw, value.roll); },
          nb::sig("def __iter__(self) -> Iterator[float]"))
      .def(
          "__eq__",
          [](const ArxAngle& left, nb::handle right) {
            return valueEquals(left, right, [](const auto& lhs, const auto& rhs) {
              return lhs.pitch == rhs.pitch && lhs.yaw == rhs.yaw && lhs.roll == rhs.roll;
            });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__", [](const ArxAngle& value) { return valueHash(value.pitch, value.yaw, value.roll); })
      .def("__repr__", [](const ArxAngle& value) {
        return valueRepr(
            "Angle", std::pair{"pitch", value.pitch}, std::pair{"yaw", value.yaw}, std::pair{"roll", value.roll});
      });
  nb::class_<ArxRect>(math, "Rect")
      .def(nb::init<ArxVector2, ArxVector2>(), nb::arg("min") = ArxVector2{}, nb::arg("max") = ArxVector2{})
      .def_ro("min", &ArxRect::min)
      .def_ro("max", &ArxRect::max)
      .def(
          "__eq__",
          [](const ArxRect& left, nb::handle right) {
            return valueEquals(left, right, [](const auto& lhs, const auto& rhs) {
              return lhs.min.x == rhs.min.x && lhs.min.y == rhs.min.y && lhs.max.x == rhs.max.x &&
                     lhs.max.y == rhs.max.y;
            });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__",
           [](const ArxRect& value) { return valueHash(value.min.x, value.min.y, value.max.x, value.max.y); })
      .def("__repr__", [](const ArxRect& value) {
        std::ostringstream stream;
        stream << "Rect(min=Vector2(x=" << value.min.x << ", y=" << value.min.y << "), max=Vector2(x=" << value.max.x
               << ", y=" << value.max.y << "))";
        return stream.str();
      });
  nb::class_<ArxAabb>(math, "Aabb")
      .def(nb::init<ArxVector3, ArxVector3>(), nb::arg("min") = ArxVector3{}, nb::arg("max") = ArxVector3{})
      .def_ro("min", &ArxAabb::min)
      .def_ro("max", &ArxAabb::max)
      .def(
          "__eq__",
          [](const ArxAabb& left, nb::handle right) {
            return valueEquals(left, right, [](const auto& lhs, const auto& rhs) {
              return lhs.min.x == rhs.min.x && lhs.min.y == rhs.min.y && lhs.min.z == rhs.min.z &&
                     lhs.max.x == rhs.max.x && lhs.max.y == rhs.max.y && lhs.max.z == rhs.max.z;
            });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__",
           [](const ArxAabb& value) {
             return valueHash(value.min.x, value.min.y, value.min.z, value.max.x, value.max.y, value.max.z);
           })
      .def("__repr__", [](const ArxAabb& value) {
        std::ostringstream stream;
        stream << "Aabb(min=Vector3(x=" << value.min.x << ", y=" << value.min.y << ", z=" << value.min.z
               << "), max=Vector3(x=" << value.max.x << ", y=" << value.max.y << ", z=" << value.max.z << "))";
        return stream.str();
      });
  nb::class_<ArxColor3>(math, "Color3")
      .def(nb::init<float, float, float>(), nb::arg("r") = 0.0f, nb::arg("g") = 0.0f, nb::arg("b") = 0.0f)
      .def_ro("r", &ArxColor3::r)
      .def_ro("g", &ArxColor3::g)
      .def_ro("b", &ArxColor3::b)
      .def("__len__", [](const ArxColor3&) { return 3; })
      .def("__getitem__",
           [](const ArxColor3& value, std::int64_t index) {
             const float values[] = {value.r, value.g, value.b};
             return values[valueIndex(index, 3)];
           })
      .def(
          "__iter__",
          [](const ArxColor3& value) { return valueIterator(value.r, value.g, value.b); },
          nb::sig("def __iter__(self) -> Iterator[float]"))
      .def(
          "__eq__",
          [](const ArxColor3& left, nb::handle right) {
            return valueEquals(left, right, [](const auto& lhs, const auto& rhs) {
              return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
            });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__", [](const ArxColor3& value) { return valueHash(value.r, value.g, value.b); })
      .def("__add__",
           [](const ArxColor3& left, const ArxColor3& right) {
             return ArxColor3{left.r + right.r, left.g + right.g, left.b + right.b};
           })
      .def("__sub__",
           [](const ArxColor3& left, const ArxColor3& right) {
             return ArxColor3{left.r - right.r, left.g - right.g, left.b - right.b};
           })
      .def("__mul__",
           [](const ArxColor3& value, float scalar) {
             return ArxColor3{value.r * scalar, value.g * scalar, value.b * scalar};
           })
      .def("__rmul__",
           [](const ArxColor3& value, float scalar) {
             return ArxColor3{value.r * scalar, value.g * scalar, value.b * scalar};
           })
      .def("__truediv__",
           [](const ArxColor3& value, float scalar) {
             if (scalar == 0.0f) {
               PyErr_SetString(PyExc_ZeroDivisionError, "cannot divide a color by zero");
               throw nb::python_error();
             }
             return ArxColor3{value.r / scalar, value.g / scalar, value.b / scalar};
           })
      .def("__repr__", [](const ArxColor3& value) {
        return valueRepr("Color3", std::pair{"r", value.r}, std::pair{"g", value.g}, std::pair{"b", value.b});
      });
  nb::class_<ArxQuat>(math, "Quat")
      .def(nb::init<float, float, float, float>(),
           nb::arg("w") = 1.0f,
           nb::arg("x") = 0.0f,
           nb::arg("y") = 0.0f,
           nb::arg("z") = 0.0f)
      .def_ro("w", &ArxQuat::w)
      .def_ro("x", &ArxQuat::x)
      .def_ro("y", &ArxQuat::y)
      .def_ro("z", &ArxQuat::z)
      .def("__len__", [](const ArxQuat&) { return 4; })
      .def("__getitem__",
           [](const ArxQuat& value, std::int64_t index) {
             const float values[] = {value.w, value.x, value.y, value.z};
             return values[valueIndex(index, 4)];
           })
      .def(
          "__iter__",
          [](const ArxQuat& value) { return valueIterator(value.w, value.x, value.y, value.z); },
          nb::sig("def __iter__(self) -> Iterator[float]"))
      .def(
          "__eq__",
          [](const ArxQuat& left, nb::handle right) {
            return valueEquals(left, right, [](const auto& lhs, const auto& rhs) {
              return lhs.w == rhs.w && lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
            });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__", [](const ArxQuat& value) { return valueHash(value.w, value.x, value.y, value.z); })
      .def("__repr__", [](const ArxQuat& value) {
        return valueRepr(
            "Quat", std::pair{"w", value.w}, std::pair{"x", value.x}, std::pair{"y", value.y}, std::pair{"z", value.z});
      });

  nb::enum_<NativeTextMode>(module, "NativeTextMode")
      .value("AUTO", NativeTextMode::kAuto)
      .value("UTF8", NativeTextMode::kUtf8)
      .value("LATIN1", NativeTextMode::kLatin1);
  nb::enum_<ImageFormat>(module, "ImageFormat")
      .value("JPEG", ImageFormat::kJpeg)
      .value("PNG", ImageFormat::kPng)
      .value("BMP", ImageFormat::kBmp)
      .value("TGA", ImageFormat::kTga);
  nb::enum_<SoundKind>(module, "SoundKind").value("EFFECT", SoundKind::kEffect).value("SPEECH", SoundKind::kSpeech);
  nb::enum_<binary::TextEncoding>(module, "TextEncoding")
      .value("ASCII", binary::TextEncoding::kAscii)
      .value("UTF8", binary::TextEncoding::kUtf8)
      .value("LATIN1", binary::TextEncoding::kLatin1);

  module.attr("__version__") = version();
  module.attr("build_time") = buildTime();
  module.def(
      "sound_handle",
      [](SoundKind kind, SoundIndex index) {
        SoundHandle result = kNoSoundHandle;
        checkStatus(pistoris::soundHandle(kind, index, result));
        return result;
      },
      nb::arg("kind"),
      nb::arg("index"));
  module.def(
      "sound_handle_kind",
      [](SoundHandle handle) {
        SoundKind result = SoundKind::kEffect;
        checkStatus(pistoris::soundHandleKind(handle, result));
        return result;
      },
      nb::arg("handle"));
  module.def(
      "sound_handle_index",
      [](SoundHandle handle) {
        SoundIndex result = kNoSound;
        checkStatus(pistoris::soundHandleIndex(handle, result));
        return result;
      },
      nb::arg("handle"));
  module.def(
      "classify_text_encoding",
      [](nb::handle value) {
        const auto bytes = byteSpan(value);
        return binary::classifyTextEncoding({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
      },
      nb::arg("data"));
  module.def(
      "latin1_to_utf8",
      [](nb::handle value) {
        const auto bytes = byteSpan(value);
        std::string result;
        checkStatus(binary::latin1ToUtf8({reinterpret_cast<const char*>(bytes.data()), bytes.size()}, result));
        return result;
      },
      nb::arg("data"));
  module.def(
      "utf8_to_latin1",
      [](std::string_view value) {
        std::string result;
        checkStatus(binary::utf8ToLatin1(value, result));
        return nb::bytes(result.data(), result.size());
      },
      nb::arg("text"));
}

}  // namespace pistoris::python
