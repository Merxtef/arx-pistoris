"""Immutable snapshots of selectors discovered across mounted roots."""

from collections.abc import Iterator, Sequence
from typing import Generic, TypeVar

from .. import paths
from .._core.resource_io.catalog import Catalog, Entry
from .._facade import install_immutable_values as _install_immutable_values
from .._facade import install_record_semantics as _install_record_semantics
from .._facade import publish_types as _publish_types

_Selector = TypeVar("_Selector", bound=paths.ResourceSelector)


class View(Sequence[Entry], Generic[_Selector]):
    """A re-iterable, resource-kind-filtered view of one catalog snapshot."""

    def __init__(self, catalog: Catalog, selector_type: type[_Selector]) -> None:
        self._catalog = catalog
        self._selector_type = selector_type

    def __iter__(self) -> Iterator[Entry]:
        return (
            entry
            for entry in self._catalog
            if isinstance(entry.selector, self._selector_type)
        )

    def __len__(self) -> int:
        return sum(1 for _ in self)

    def __getitem__(self, index: int | slice) -> Entry | list[Entry]:
        entries = list(self)
        return entries[index]

    def __repr__(self) -> str:
        return f"View(len={len(self)})"


def _get(catalog: Catalog, selector: paths.ResourceSelector) -> Entry:
    """Return the entry for selector without changing the catalog."""
    for entry in catalog:
        if entry.selector == selector:
            return entry
    raise KeyError(selector)


def _models(catalog: Catalog) -> View[paths.ModelSelector]:
    return View(catalog, paths.ModelSelector)


def _animations(catalog: Catalog) -> View[paths.AnimationSelector]:
    return View(catalog, paths.AnimationSelector)


def _levels(catalog: Catalog) -> View[paths.LevelSelector]:
    return View(catalog, paths.LevelSelector)


def _cinematics(catalog: Catalog) -> View[paths.CinematicSelector]:
    return View(catalog, paths.CinematicSelector)


def _ambiances(catalog: Catalog) -> View[paths.AmbianceSelector]:
    return View(catalog, paths.AmbianceSelector)


Catalog.get = _get
Catalog.models = _models
Catalog.animations = _animations
Catalog.levels = _levels
Catalog.cinematics = _cinematics
Catalog.ambiances = _ambiances

__all__ = ["Catalog", "Entry", "View"]

_publish_types(globals(), __name__, __all__)
_install_immutable_values(globals(), ("Entry",))
_install_record_semantics(globals(), {"Entry": ("selector", "provider_mask")})
