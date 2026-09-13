# Ambiance GLB Authoring Reference

Back to the [Authoring Reference](../AUTHORING_REFERENCE.md). For a worked
example, use the [Ambiance Authoring Guide](AMBIANCE_GUIDE.md). Shared notation
and naming terms are defined in [Shared Authoring Conventions](CONVENTIONS.md).

This reference is for authors and tools that need the exact standalone
Ambiance GLB contract.

Ambiance GLB represents exactly one Ambiance. It defaults to 10 Arx units per
GLB unit and uses the `(x,-y,-z)` native-to-GLB basis. Track paths reference
external audio sidecars; audio bytes are not embedded.

## Ambiance Layout

```text
reference_model (optional export-only mesh)
arx_ambiance[__MASTER_<track-ordinal>]__<label>
`-- TRACK_<track-ordinal>__<sample-path>__<label>
   `-- KEY_<key-ordinal>[__PLAY_COUNT_<count>][__START_<ms>][__DELAY_MIN_<ms>][__DELAY_MAX_<ms>]__<label>
      +-- VOLUME__VAL_<value>[__RANGE_<value>][__INTERVAL_<ms>][__<mode>]__<label>
      +-- PITCH__VAL_<value>[__RANGE_<value>][__INTERVAL_<ms>][__<mode>]__<label>
      +-- X[__RANGE_<value>][__INTERVAL_<ms>][__<mode>]__<label>
      +-- Y[__RANGE_<value>][__INTERVAL_<ms>][__<mode>]__<label>
      +-- Z[__RANGE_<value>][__INTERVAL_<ms>][__<mode>]__<label>
      `-- PAN[__VAL_<value>][__RANGE_<value>][__INTERVAL_<ms>][__<mode>]__<label>
```

Exactly one reachable Ambiance root is required. Root, track, key, and helper
labels are mandatory and ignored. Track and key ordinals are unique sortable
unsigned values; gaps are valid. Omitted `MASTER` selects the lowest track
ordinal. A supplied `MASTER` must identify one track ordinal.

`<sample-path>` may contain `__`. Parsing uses the first `__` after the ordinal
and the final `__` before the label. Canonical export uses dense track and key
ordinals padded to at least three digits and omits `MASTER_0`.

[Worked root and track example](AMBIANCE_GUIDE.md#root-and-tracks)

## Keys and Automation

Key options may appear in any order and at most once each. Defaults:

| Field | Default |
| --- | ---: |
| play count | `1` |
| start delay | `0` ms |
| delay minimum | `0` ms |
| delay maximum | delay minimum |
| volume | `1` |
| pitch | `1` |

Play count must be positive. A track with no key children creates one positioned
key at the track's evaluated root-relative position with all defaults.
Canonical export always writes explicit keys.

The master's shortest possible duration should cover every other track's
longest possible duration. Duration includes each key's start delay and every
play's delay plus sample duration adjusted by pitch. Pitch is clamped to
`[0.1,2]`; interpolated pitch may reach either limit. Native export warns when
attached audio shows that this condition is not met.

Modes:

```text
STEP
RANDOM_STEP
INTERPOLATED
RANDOM_INTERPOLATED
```

Omitted mode means `STEP`. `RANGE` is a nonzero signed half-range:

```text
first  = value - RANGE
second = value + RANGE
```

Positive ranges produce low-to-high deterministic automation. Negative ranges
produce high-to-low deterministic automation. Random modes require ordered
minimum and maximum endpoints, so a negative range warns and is treated as
positive.

A nonzero `RANGE` makes automation dynamic. Dynamic interval defaults to 1000
ms. `INTERVAL` or a mode without `RANGE` is invalid. `VOLUME` and `PITCH`
require `VAL`.

X, Y, and Z forbid `VAL` and take their centers from the corresponding key
coordinate. Their ranges use GLB units. PAN may use `VAL`; when present, it is
authoritative. Automation helper transforms are ignored and nonidentity
transforms warn.

[Worked automation example](AMBIANCE_GUIDE.md#keys-and-automation)

## Positioned and Panned Keys

The key's root-relative position supplies every X/Y/Z center. X, Y, or Z
helpers add automation without changing that center. Any axis helper makes the
key positioned. No spatial helpers also means positioned.

Only PAN makes a key panned. The key point maps to pan with:

```text
-x / hypot(x,z)
```

`-1` is left, `1` is right, and GLB asset forward is +Z. Blender imports this
forward direction as -Y. Canonical panned points lie on the front arc at 50 Arx
units. A back-arc point warns and maps to the equivalent front-arc pan. A point
at the horizontal origin warns and maps to zero.

PAN `VAL` preserves centers outside `[-1,1]`; the displayed point uses the
clamped center. A conflicting point warns and `VAL` wins. PAN combined with any
X/Y/Z helper warns, ignores PAN, and produces a positioned key. All keys in one
track must resolve to the same positioned or panned kind.

The `arx_ambiance` transform and all ancestor transforms are ignored. Semantic
translations are evaluated in the coordinate frame below that root. Transforms
below it affect descendants normally. A semantic node's own rotation and scale
have no independent meaning. Key translation is the sole key position;
automation helpers are canonically placed directly on the key.

Nested `arx_*` objects inside the Ambiance root invalidate the Ambiance. Other
unexpected descendants are ignored with warnings.

## Reference Model

Export may include one reference Model. Its mesh is written at Ambiance scale
under the ordinary export-only `reference_model` node outside the Ambiance
root.

The first action point named `view_attach` places the root. Additional matches
warn and are ignored. Without `view_attach`, the root remains at the Model
origin. Ambiance import ignores the reference mesh.

[Worked reference-Model workflow](AMBIANCE_GUIDE.md#reference-model)

[Back to the Authoring Reference](../AUTHORING_REFERENCE.md)
