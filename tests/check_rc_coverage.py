#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef
#
# Checks strerror completeness and focused validation-code coverage

from __future__ import annotations

import pathlib
import re
import sys


CODE_PREFIXES = (
    "ARX_AMB_",
    "ARX_FTL_",
    "ARX_MODEL_",
    "ARX_TEA_",
    "ARX_FTS_",
    "ARX_LLF_",
    "ARX_DLF_",
    "ARX_LEVEL_",
    "ARX_OBJ_",
    "ARX_GLB_",
    "ARX_JSON_",
)

ASSERTION_MACROS = (
    "CHECK",
    "CHECK_FALSE",
    "REQUIRE",
    "REQUIRE_FALSE",
    "REQUIRE_MESSAGE",
)

def mask_comments_and_literals(source: str) -> str:
    masked = list(source)
    index = 0
    while index < len(source):
        if source.startswith("//", index):
            end = source.find("\n", index + 2)
            end = len(source) if end == -1 else end
            masked[index:end] = " " * (end - index)
            index = end
            continue
        if source.startswith("/*", index):
            end = source.find("*/", index + 2)
            end = len(source) if end == -1 else end + 2
            for pos in range(index, end):
                if source[pos] != "\n":
                    masked[pos] = " "
            index = end
            continue
        if source.startswith('R"', index):
            delimiter_end = source.find("(", index + 2, index + 19)
            if delimiter_end != -1:
                delimiter = source[index + 2 : delimiter_end]
                terminator = ")" + delimiter + '"'
                end = source.find(terminator, delimiter_end + 1)
                end = len(source) if end == -1 else end + len(terminator)
                for pos in range(index, end):
                    if source[pos] != "\n":
                        masked[pos] = " "
                index = end
                continue
        if source[index] in ('"', "'"):
            quote = source[index]
            end = index + 1
            while end < len(source):
                if source[end] == "\\":
                    end += 2
                    continue
                end += 1
                if source[end - 1] == quote:
                    break
            for pos in range(index, min(end, len(source))):
                if source[pos] != "\n":
                    masked[pos] = " "
            index = end
            continue
        index += 1
    return "".join(masked)


def asserted_codes(source: str) -> set[str]:
    source = mask_comments_and_literals(source)
    macro_pattern = re.compile(r"\b(" + "|".join(ASSERTION_MACROS) + r")\s*\(")
    result = set()
    for match in macro_pattern.finditer(source):
        open_paren = source.find("(", match.start())
        depth = 1
        cursor = open_paren + 1
        while cursor < len(source) and depth:
            if source[cursor] == "(":
                depth += 1
            elif source[cursor] == ")":
                depth -= 1
            cursor += 1
        if depth:
            raise ValueError(f"unterminated {match.group(1)} assertion")
        result.update(re.findall(r"\bARX_[A-Z0-9_]+\b", source[open_paren + 1 : cursor - 1]))
    return result

header = pathlib.Path("include/arx_pistoris/base/status.h").read_text()
strerror = pathlib.Path("src/api/strerror.cpp").read_text()
covered = set()
for test_path in pathlib.Path("tests").rglob("*.cpp"):
    try:
        covered.update(asserted_codes(test_path.read_text()))
    except ValueError as error:
        print(f"{test_path}: {error}")
        sys.exit(1)

missing = []
return_code_block = re.search(r"typedef int32_t ArxReturnCode;\s*enum\s*\{(.*?)\n\};", header, re.DOTALL)
if return_code_block is None:
    print("Unable to find the ArxReturnCode constant block")
    sys.exit(1)

assigned = set(re.findall(r"^\s*(ARX_[A-Z0-9_]+)\b", return_code_block.group(1), re.MULTILINE))
messages = set(re.findall(r"\bcase\s+(ARX_[A-Z0-9_]+)\s*:", strerror))
for code in sorted(assigned - messages):
    missing.append(f"strerror: {code}")
for code in sorted(messages - assigned):
    missing.append(f"unassigned strerror case: {code}")

for prefix in CODE_PREFIXES:
    for code in re.findall(rf"({prefix}\w+)", header):
        if code not in covered:
            missing.append(code)

for code in missing:
    print(f"MISSING coverage: {code}")

sys.exit(len(missing))
