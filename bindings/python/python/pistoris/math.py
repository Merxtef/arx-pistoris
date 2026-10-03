"""Immutable vectors, colors, rotations, and bounds."""

from ._core.math import Aabb, Angle, Color3, Quat, Rect, Vector2, Vector3
from ._facade import install_immutable_values as _install_immutable_values
from ._facade import publish_types as _publish_types

__all__ = ["Aabb", "Angle", "Color3", "Quat", "Rect", "Vector2", "Vector3"]

_publish_types(globals(), __name__, __all__)
_install_immutable_values(globals(), __all__)
