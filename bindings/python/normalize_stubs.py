# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from pathlib import Path
import re
import sys


CLASS = re.compile(r"^class (?P<name>[A-Za-z0-9_]+):\n(?P<body>(?:^(?:    |$).*\n)*)", re.MULTILINE)
POSITIONAL_ELEMENT = re.compile(r"def __getitem__\(self, (?:index|arg): int[^)]*\) -> (?P<type>[^:]+):")
MAPPING_ELEMENT = re.compile(
    r"def __getitem__\(self, [A-Za-z_][A-Za-z0-9_]*: (?P<key>[^,]+), /\) -> (?P<value>[^:]+):"
)
ITERATOR_ELEMENT = re.compile(r"def __iter__\(self\) -> Iterator\[(?P<type>[^]]+)\]:")
PROPERTY_SETTER = re.compile(
    r"(?P<prefix>^    @(?P<name>[A-Za-z0-9_]+)\.setter\n"
    r"    def (?P=name)\(self, arg: )"
    r"(?P<type>[A-Za-z_][A-Za-z0-9_.]*)"
    r"(?P<suffix>, /\) -> None: \.\.\.)",
    re.MULTILINE,
)
BUILTIN_TYPES = {"bool", "bytes", "float", "int", "object", "str"}


def sequence_base(match: re.Match[str]) -> str:
    body = match.group("body")
    element = POSITIONAL_ELEMENT.search(body)
    if not element:
        return match.group(0)
    mutable = "def insert(" in body and "def __setitem__(" in body and "def __delitem__(" in body
    if not mutable and ("def __contains__" not in body or "def __reversed__" not in body):
        return match.group(0)
    base = "MutableSequence" if mutable else "Sequence"
    return f"class {match.group('name')}({base}[{element.group('type')}]):\n{body}"


def mapping_base(match: re.Match[str]) -> str:
    body = match.group("body")
    item = MAPPING_ELEMENT.search(body)
    required = ("def __len__(", "def __iter__(", "def __setitem__(", "def __delitem__(")
    if not item or not all(method in body for method in required):
        return match.group(0)
    return f"class {match.group('name')}(MutableMapping[{item.group('key')}, {item.group('value')}]):\n{body}"


def collection_base(match: re.Match[str]) -> str:
    body = match.group("body")
    element = ITERATOR_ELEMENT.search(body)
    if not element:
        return match.group(0)
    required = ("def __contains__(", "def __len__(")
    if not all(method in body for method in required):
        return match.group(0)
    mutable = all(method in body for method in ("def add(", "def discard(", "def remove(", "def clear("))
    base = "MutableSet" if mutable else "Collection"
    return f"class {match.group('name')}({base}[{element.group('type')}]):\n{body}"


def iterable_inputs(match: re.Match[str]) -> str:
    name = match.group("name")
    body = match.group("body")
    element = POSITIONAL_ELEMENT.search(body)
    if not element or "def insert(" not in body:
        return match.group(0)
    value = element.group("type")
    body = body.replace(f"def extend(self, arg: {name}, /)", f"def extend(self, arg: Iterable[{value}], /)")
    body = body.replace(
        f"def __setitem__(self, arg0: slice, arg1: {name}, /)",
        f"def __setitem__(self, arg0: slice, arg1: Iterable[{value}], /)",
    )
    return f"class {name}:\n{body}"


def module_name(path: Path) -> str:
    parts = []
    directory = path.parent
    while (directory / "__init__.pyi").exists():
        parts.append(directory.name)
        directory = directory.parent
    parts.reverse()
    if path.name != "__init__.pyi":
        parts.append(path.stem)
    return ".".join(parts)


def qualify_type(type_name: str, module: str) -> str:
    type_name = type_name.strip()
    if type_name in BUILTIN_TYPES or type_name.startswith("pistoris."):
        return type_name
    return f"{module}.{type_name}"


def collection_inputs(sources: dict[Path, str]) -> dict[str, str]:
    result = {}
    ambiguous = set()
    for path, source in sources.items():
        module = module_name(path)
        for match in CLASS.finditer(source):
            name = match.group("name")
            body = match.group("body")
            element = POSITIONAL_ELEMENT.search(body)
            item = MAPPING_ELEMENT.search(body)
            input_type = None
            if element and "def insert(" in body:
                input_type = f"Iterable[{qualify_type(element.group('type'), module)}]"
            elif item and all(
                method in body
                for method in ("def __len__(", "def __iter__(", "def __setitem__(", "def __delitem__(")
            ):
                key = qualify_type(item.group("key"), module)
                value = qualify_type(item.group("value"), module)
                input_type = f"Mapping[{key}, {value}]"
            elif element and all(method in body for method in ("def add(", "def discard(", "def remove(")):
                input_type = f"Iterable[{qualify_type(element.group('type'), module)}]"
            if input_type is None:
                continue

            result[f"{module}.{name}"] = input_type
            if name in result and result[name] != input_type:
                ambiguous.add(name)
            else:
                result[name] = input_type
    for name in ambiguous:
        del result[name]
    return result


def collection_outputs(sources: dict[Path, str]) -> dict[str, str]:
    result = {}
    for path, source in sources.items():
        module = module_name(path)
        for match in CLASS.finditer(source):
            name = match.group("name")
            body = match.group("body")
            element = POSITIONAL_ELEMENT.search(body)
            item = MAPPING_ELEMENT.search(body)
            if (
                element
                and "def insert(" in body
                and "def __setitem__(" in body
                and "def __delitem__(" in body
            ):
                interface = f"MutableSequence[{qualify_type(element.group('type'), module)}]"
            elif item and all(
                method in body
                for method in (
                    "def __len__(",
                    "def __iter__(",
                    "def __setitem__(",
                    "def __delitem__(",
                )
            ):
                key = qualify_type(item.group("key"), module)
                value = qualify_type(item.group("value"), module)
                interface = f"MutableMapping[{key}, {value}]"
            else:
                continue
            result[f"{module}.{name}"] = interface
    return result


def collection_output_types(source: str, module: str, outputs: dict[str, str]) -> str:
    for concrete, interface in outputs.items():
        interface = interface.replace(f"{module}.", "")
        interface = interface.replace("pistoris._core.native.", "pistoris.native.")
        source = source.replace(concrete, interface)
    return source


def collection_setter_inputs(source: str, inputs: dict[str, str]) -> str:
    def replace(match: re.Match[str]) -> str:
        input_type = inputs.get(match.group("type"))
        if input_type is None:
            return match.group(0)
        return f"{match.group('prefix')}{input_type}{match.group('suffix')}"

    return PROPERTY_SETTER.sub(replace, source)


def add_collection_imports(source: str) -> str:
    available = {
        "Collection",
        "Iterable",
        "Iterator",
        "Mapping",
        "MutableMapping",
        "MutableSequence",
        "MutableSet",
        "Sequence",
    }
    required = {name for name in available if re.search(rf"\b{name}\b", source)}
    match = re.search(r"^from collections\.abc import (?P<names>[^\n]+)$", source, re.MULTILINE)
    if match:
        names = sorted(set(match.group("names").split(", ")) | required)
        return source[: match.start()] + f"from collections.abc import {', '.join(names)}" + source[match.end() :]
    return f"from collections.abc import {', '.join(sorted(required))}\n" + source


def normalize(path: Path, original: str, inputs: dict[str, str], outputs: dict[str, str]) -> None:
    source = original
    source = CLASS.sub(iterable_inputs, source)
    source = collection_setter_inputs(source, inputs)
    source = collection_output_types(source, module_name(path), outputs)
    source = CLASS.sub(sequence_base, source)
    source = CLASS.sub(mapping_base, source)
    source = CLASS.sub(collection_base, source)
    if source != original:
        source = add_collection_imports(source)
    if source != original:
        with path.open("w", encoding="utf-8", newline="\n") as output:
            output.write(source)


def main() -> None:
    path = Path(sys.argv[1])
    stubs = sorted(path.rglob("*.pyi")) if path.is_dir() else [path]
    sources = {stub: stub.read_text(encoding="utf-8") for stub in stubs}
    inputs = collection_inputs(sources)
    outputs = collection_outputs(sources)
    for stub, source in sources.items():
        normalize(stub, source, inputs, outputs)


if __name__ == "__main__":
    main()
