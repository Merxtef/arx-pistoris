#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef
#
# Checks that every ARX_FTL_* and ARX_OBJ_* return code has at least one
# CHECK(...) assertion referencing it somewhere in the tests/ tree.
# Exits non-zero if any codes are missing coverage.

import re
import sys
import pathlib

header = pathlib.Path("include/arx_pistoris/pistoris_types.h").read_text()
test_src = "".join(p.read_text() for p in pathlib.Path("tests").rglob("*.cpp"))

missing = []
for prefix in ("ARX_FTL_", "ARX_OBJ_", "ARX_JSON_"):
    for code in re.findall(rf"({prefix}\w+)", header):
        if not re.search(r"CHECK\b.*\b" + re.escape(code) + r"\b", test_src, re.DOTALL):
            missing.append(code)

for code in missing:
    print(f"MISSING coverage: {code}")

sys.exit(len(missing))
