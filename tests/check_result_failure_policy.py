#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from __future__ import annotations

import pathlib
import re
import sys


ALLOWED = {
    "libs/core/include/arx_pistoris/base/result.hpp",
    "libs/core/src/api/result_failure.h",
    "libs/core/src/api/status_boundary.h",
}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".inc", ".inl", ".ipp"}
FAILURE_PATTERN = re.compile(r"::failure\s*\(")


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    violations: list[str] = []

    for source_root in (
        root / "libs" / "core" / "src",
        root / "apps" / "cli" / "src",
        root / "libs" / "core" / "include" / "arx_pistoris",
    ):
        for path in source_root.rglob("*"):
            if path.suffix.lower() not in SOURCE_SUFFIXES:
                continue
            relative = path.relative_to(root).as_posix()
            if relative in ALLOWED:
                continue
            for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                if FAILURE_PATTERN.search(line):
                    violations.append(
                        f"{relative}:{line_number}: direct Result::failure construction bypasses source logging"
                    )

    for violation in violations:
        print(violation)
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
