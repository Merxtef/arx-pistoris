from collections.abc import Sequence
from typing import Generic, TypeVar

from ..paths import (
    AmbianceSelector,
    AnimationSelector,
    CinematicSelector,
    LevelSelector,
    ModelSelector,
    ResourceSelector,
)

_Selector = TypeVar("_Selector", bound=ResourceSelector, covariant=True)

class Entry(Generic[_Selector]):
    """One discovered selector and the mounts that provide it."""
    @property
    def selector(self) -> _Selector: ...
    @property
    def provider_mask(self) -> int: ...

class View(Sequence[Entry[_Selector]], Generic[_Selector]):
    """A read-only, re-iterable resource-kind view of one catalog snapshot."""
    ...

class Catalog(Sequence[Entry[ResourceSelector]]):
    """An immutable snapshot of selectors visible through selected mounts."""
    def get(self, selector: _Selector) -> Entry[_Selector]: ...
    def models(self) -> View[ModelSelector]: ...
    def animations(self) -> View[AnimationSelector]: ...
    def levels(self) -> View[LevelSelector]: ...
    def cinematics(self) -> View[CinematicSelector]: ...
    def ambiances(self) -> View[AmbianceSelector]: ...
