"""Runtime metadata for public aliases of extension types."""

from collections.abc import Collection, Iterable, Mapping, Sequence, Set
from importlib.machinery import ModuleSpec
import os
from types import ModuleType

_GENERATING_STUBS = os.environ.get("NB_STUBGEN") == "1"


def publish_types(
    namespace: Mapping[str, object], module_name: str, names: Iterable[str]
) -> None:
    # Stubgen needs the extension's original ownership to emit class bodies.
    if _GENERATING_STUBS:
        return
    for name in names:
        value = namespace.get(name)
        if not isinstance(value, type):
            continue
        value.__module__ = module_name
        value.__name__ = name
        value.__qualname__ = name


def publish_module(module: ModuleType, public_name: str) -> ModuleType:
    public = ModuleType(public_name, module.__doc__)
    public.__package__ = public_name.rpartition(".")[0]
    public.__spec__ = ModuleSpec(public_name, loader=None)
    for name in dir(module):
        if name.startswith("_"):
            continue
        value = getattr(module, name)
        setattr(public, name, value)
        if (
            not _GENERATING_STUBS
            and isinstance(value, type)
            and value.__module__ == module.__name__
        ):
            value.__module__ = public_name
            value.__name__ = name
            value.__qualname__ = name
    public.__all__ = [name for name in vars(public) if not name.startswith("_")]
    return public


def _structural_value(value: object) -> object:
    if isinstance(value, Sequence) and not isinstance(value, (str, bytes, bytearray)):
        return tuple(_structural_value(item.copy() if hasattr(item, "copy") else item) for item in value)
    if isinstance(value, Set):
        return frozenset(_structural_value(item.copy() if hasattr(item, "copy") else item) for item in value)
    return value


def _field_repr(value: object) -> str:
    if isinstance(value, (bytes, bytearray, memoryview)):
        return f"<{len(value)} bytes>"
    if isinstance(value, Collection) and not isinstance(value, str):
        return f"<{len(value)} items>"
    return repr(value)


def _represent_record(
    self: object,
    name: str,
    fields: tuple[str, ...],
    media: frozenset[str] = frozenset(),
) -> str:
    rendered = []
    for field in fields:
        if field in media:
            size = getattr(self, f"_pistoris_{field}_size")()
            value = "None" if size is None else f"<{size} bytes>"
        else:
            value = _field_repr(getattr(self, field))
        rendered.append(f"{field}={value}")
    body = ", ".join(rendered)
    if len(body) > 220:
        body = body[:217] + "..."
    return f"{name}({body})"


def install_record_repr(
    namespace: Mapping[str, object], records: Mapping[str, tuple[str, ...]]
) -> None:
    """Give public extension records bounded representations."""
    if _GENERATING_STUBS:
        return
    for name, fields in records.items():
        record_type = namespace.get(name)
        if not isinstance(record_type, type):
            continue

        def represent(
            self: object,
            *,
            _name: str = name,
            _fields: tuple[str, ...] = fields,
        ) -> str:
            return _represent_record(self, _name, _fields)

        record_type.__repr__ = represent


def install_immutable_values(
    namespace: Mapping[str, object], names: Iterable[str]
) -> None:
    """Report named properties when immutable extension values are assigned."""
    if _GENERATING_STUBS:
        return
    for name in names:
        value_type = namespace.get(name)
        if isinstance(value_type, type):
            value_type.__setattr__ = _reject_immutable_assignment


def install_read_only_properties(
    value_type: type, names: Iterable[str]
) -> None:
    """Report named read-only extension properties without freezing the type."""
    if _GENERATING_STUBS:
        return
    read_only = frozenset(names)
    inherited_setattr = value_type.__setattr__

    def set_attribute(self: object, name: str, value: object) -> None:
        if name in read_only:
            raise AttributeError(
                f"property {name!r} of '{type(self).__name__}' object has no setter"
            )
        inherited_setattr(self, name, value)

    value_type.__setattr__ = set_attribute


def _reject_immutable_assignment(
    self: object, name: str, value: object
) -> None:
    del value
    raise AttributeError(
        f"property {name!r} of '{type(self).__name__}' object has no setter"
    )


def install_record_semantics(
    namespace: Mapping[str, object],
    records: Mapping[str, tuple[str, ...]],
    media_fields: Mapping[str, tuple[str, ...]] | None = None,
    hashable: Iterable[str] = (),
) -> None:
    """Give public extension records value semantics without wrapping them."""
    media_fields = media_fields or {}
    hashable_records = frozenset(hashable)
    for name, fields in records.items():
        record_type = namespace.get(name)
        if not isinstance(record_type, type):
            continue
        if name in hashable_records:
            def hash_record(
                self: object,
                *,
                _fields: tuple[str, ...] = fields,
            ) -> int:
                return hash(tuple(_structural_value(getattr(self, field)) for field in _fields))

            record_type.__hash__ = hash_record
        else:
            record_type.__hash__ = None
        if _GENERATING_STUBS:
            continue

        media = frozenset(media_fields.get(name, ()))

        def equal(
            self: object,
            other: object,
            *,
            _type: type = record_type,
            _fields: tuple[str, ...] = fields,
            _media: frozenset[str] = media,
        ) -> bool:
            if type(other) is not _type:
                return NotImplemented
            for field in _fields:
                if field in _media:
                    if not getattr(self, f"_pistoris_{field}_equals")(other):
                        return False
                elif _structural_value(getattr(self, field)) != _structural_value(
                    getattr(other, field)
                ):
                    return False
            return True

        def represent(
            self: object,
            *,
            _name: str = name,
            _fields: tuple[str, ...] = fields,
            _media: frozenset[str] = media,
        ) -> str:
            return _represent_record(self, _name, _fields, _media)

        record_type.__eq__ = equal
        record_type.__repr__ = represent
