# Shared Authoring Conventions

Back to the [Authoring Guide](../AUTHORING_GUIDE.md) or
[Authoring Reference](../AUTHORING_REFERENCE.md).

These conventions apply across Pistoris GLB and OBJ authoring surfaces.
Resource-specific guides and references define where each convention is valid.

## Terminology

- **Name** - semantic text retained by the imported resource.
- **Label** - nonempty free text ignored on import. Labels safely absorb DCC
  suffixes such as `.001`.
- **Ordinal** - unique unsigned ordering number. Gaps are allowed.
- **Index** - exact zero-based position. Every value from zero through the last
  index must be present exactly once.
- **Resource path** - portable game resource identity using `/`, independent of
  a host filesystem or CLI mount.

Literal syntax is case-sensitive unless a resource reference says otherwise.
`__` separates semantic components and must not occur inside a name or label.
Resource paths are not semantic names and may contain `__` when the owning
reference defines an unambiguous path boundary. `#` is normalized to `-` and
cannot remain in a logical resource path.
A valid resource selector and the registered full resource path produced by
its helper identify the same resource. Selectors cover only registered game
layouts; other portable logical resource paths may also be valid.

Unless a resource section defines a specialized grammar, semantic names must
contain only ASCII letters, digits, `-`, and single interior `_` characters.
Import does not preserve noncanonical spelling: unsupported characters become
`-`, leading, repeated, and trailing underscores are removed, and a required
name that becomes empty is replaced with `unnamed`. Resource-specific
references state when names are lowercased and how normalized collisions are
handled. Labels are exempt from the semantic-name grammar but still cannot
contain `__`.

## Reference Notation

- `<value>` is mandatory.
- `<float>` is a finite floating-point value.
- `[text]` is optional.
- `A | B` separates alternatives.
- `...` repeats the preceding fragment.
- **Required root** means every represented item needs that node.
- **Optional child** means zero or one direct helper child.
- **Export-only** means an organizational name produced by Pistoris that is not
  required on import.

## Scene Names

The `arx_` prefix is reserved for documented semantic roots. Use ordinary names
for organizational nodes. Put arbitrary DCC text in the final label component
instead of changing semantic names.

## Materials and Face Flags

Level GLB, Model GLB, and Model OBJ use material names for face flags and as a
texture-identity fallback:

```text
<fallback-stem>[__<face-flag>]...[__TRANSVAL_<float>]
no_tex[__<face-flag>]...[__TRANSVAL_<float>]
```

For ordinary geometry, a referenced GLB image or OBJ `map_Kd` path defines
texture identity. The fallback stem is used only when that source is absent.
`no_tex` therefore means untextured only without a real texture source; a real
source wins with a warning. Face-flag tokens remain part of the material name.
Export derives the fallback from the final component of the logical texture
path, collapses underscore runs, and removes a trailing underscore; the
referenced image path remains authoritative. Repeated underscores in texture
paths are valid but not recommended because the fallback cannot preserve them.
Level semantic material names are reserved for their documented Level meshes.
Each flag should appear at most once. Repeated flags warn and are treated as
one.
Canonical export orders flags as listed below and places `TRANSVAL` last.

| Token | Author-visible meaning |
| --- | --- |
| `NO_SHADOW` | Native no-shadow behavior |
| `DOUBLESIDED` | Renders from both sides |
| `TRANS` | Uses native transparency and `TRANSVAL` |
| `WATER` | Water surface |
| `GLOW` | Full-bright surface lighting |
| `IGNORE` | Ignored by ordinary scene rendering |
| `QUAD` | Native FTS quad marker; discarded by Level and Model editing |
| `TILED` | Uses tiled texture coordinates |
| `METAL` | Metal surface response |
| `HIDE` | Hidden surface |
| `STONE` | Stone surface response |
| `WOOD` | Wood surface response |
| `GRAVEL` | Gravel surface response |
| `EARTH` | Earth surface response |
| `NOCOL` | No collision |
| `LAVA` | Lava surface |
| `CLIMB` | Climbable surface |
| `FALL` | Native fall-surface flag |
| `NOPATH` | Excluded from ordinary path generation |
| `NODRAW` | Not drawn by ordinary scene rendering |
| `PRECISE_PATH` | Native precise-path compatibility flag |
| `NO_CLIMB` | Explicitly not climbable |
| `ANGULAR` | Native angular compatibility flag |
| `ANGULAR_IDX0` | Native angular index bit 0 |
| `ANGULAR_IDX1` | Native angular index bit 1 |
| `ANGULAR_IDX2` | Native angular index bit 2 |
| `ANGULAR_IDX3` | Native angular index bit 3 |
| `LATE_MIP` | Native late-mipmap flag |

Level accepts `QUAD` in imported material names but discards it because editable
faces are triangles. Native Level baking reconstructs compatible quads from
geometry. Model also discards `QUAD` with a warning.
