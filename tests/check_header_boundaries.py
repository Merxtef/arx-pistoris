#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from __future__ import annotations

import pathlib
import re
import sys


UMBRELLA_HEADERS = {
    "arx_pistoris/arx_pistoris.h",
    "arx_pistoris/pistoris.hpp",
}
UMBRELLA_EXPORTS = {
    "arx_pistoris/arx_pistoris.h": {
        "arx_pistoris/ambiance.h",
        "arx_pistoris/animation.h",
        "arx_pistoris/base/abi.h",
        "arx_pistoris/base/audio.h",
        "arx_pistoris/base/buffer.h",
        "arx_pistoris/base/flags.h",
        "arx_pistoris/base/image.h",
        "arx_pistoris/base/indices.h",
        "arx_pistoris/base/math.h",
        "arx_pistoris/base/status.h",
        "arx_pistoris/base/string_view.h",
        "arx_pistoris/binary.h",
        "arx_pistoris/glb.h",
        "arx_pistoris/level.h",
        "arx_pistoris/model.h",
        "arx_pistoris/native.h",
        "arx_pistoris/paths.h",
        "arx_pistoris/runtime.h",
        "arx_pistoris/runtime/types.h",
        "arx_pistoris/sound.h",
        "arx_pistoris/texture.h",
    },
    "arx_pistoris/pistoris.hpp": {
        "arx_pistoris/ambiance.hpp",
        "arx_pistoris/animation.hpp",
        "arx_pistoris/animation/bake.hpp",
        "arx_pistoris/base/audio.h",
        "arx_pistoris/base/flags.h",
        "arx_pistoris/base/image.h",
        "arx_pistoris/base/indices.h",
        "arx_pistoris/base/math.hpp",
        "arx_pistoris/base/status.h",
        "arx_pistoris/base/string_view.h",
        "arx_pistoris/binary.hpp",
        "arx_pistoris/glb.hpp",
        "arx_pistoris/level.hpp",
        "arx_pistoris/level/bake.hpp",
        "arx_pistoris/model.hpp",
        "arx_pistoris/model/bake.hpp",
        "arx_pistoris/model/glb.hpp",
        "arx_pistoris/model/obj.hpp",
        "arx_pistoris/native.hpp",
        "arx_pistoris/paths.hpp",
        "arx_pistoris/runtime.hpp",
        "arx_pistoris/runtime/types.h",
        "arx_pistoris/sound.hpp",
        "arx_pistoris/texture.hpp",
    },
}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".inc", ".inl", ".ipp"}
INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


def includes(path: pathlib.Path) -> list[tuple[int, str]]:
    result = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        match = INCLUDE_PATTERN.match(line)
        if match:
            result.append((line_number, match.group(1).replace("\\", "/")))
    return result


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    violations: list[str] = []

    for source_root in (root / "src", root / "cli", root / "include" / "arx_pistoris"):
        for path in source_root.rglob("*"):
            if path.suffix.lower() not in SOURCE_SUFFIXES:
                continue
            relative = path.relative_to(root)
            public_name = None
            if relative.parts[0] == "include":
                public_name = relative.relative_to("include").as_posix()
            if public_name in UMBRELLA_HEADERS:
                continue
            for line_number, included in includes(path):
                included = included.lower()
                if included in UMBRELLA_HEADERS:
                    violations.append(f"{relative}:{line_number}: focused code must not include umbrella {included}")
                if relative.as_posix().startswith("include/arx_pistoris/base/") and included.startswith(
                    "arx_pistoris/"
                ) and not included.startswith("arx_pistoris/base/"):
                    violations.append(f"{relative}:{line_number}: base header depends on non-base header {included}")

    for umbrella, expected in UMBRELLA_EXPORTS.items():
        path = root / "include" / umbrella
        actual = {included for _, included in includes(path)}
        for missing in sorted(expected - actual):
            violations.append(f"{path.relative_to(root)}: missing umbrella export {missing}")

    for violation in violations:
        print(violation)
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
