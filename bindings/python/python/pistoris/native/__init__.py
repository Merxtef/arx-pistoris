"""Low-level native Arx format carriers and binary conversion."""

import sys as _sys

from .._core.native import amb, cin, dlf, ftl, fts, llf, tea
from .._facade import publish_module as _publish_module

__all__ = ["amb", "cin", "dlf", "ftl", "fts", "llf", "tea"]

for _name in __all__:
    _public_name = f"{__name__}.{_name}"
    globals()[_name] = _publish_module(globals()[_name], _public_name)
    _sys.modules[_public_name] = globals()[_name]
