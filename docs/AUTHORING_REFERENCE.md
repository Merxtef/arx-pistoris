# Authoring Reference

Exact naming and structure reference for authoring Arx resources through OBJ
and GLB. For concrete DCC workflows, see the
[Authoring Guide](AUTHORING_GUIDE.md).

## Terminology

- **Name** - semantic text imported into Level or an FTL carrier.
- **Label** - nonempty free text ignored on import. Labels are safe places for
  DCC uniqueness suffixes such as `.001`.
- **Ordinal** - unique unsigned ordering number. Gaps are allowed.
- **Index** - exact zero-based position. All values from zero through the last
  index must be present exactly once.
- **Resource path** - portable game resource identity using `/`, independent
  of any host filesystem or mount.

Notation used below:

- Literal text is case-sensitive.
- `<value>` is mandatory.
- `[text]` is optional.
- `A | B` separates alternatives.
- `...` repeats the preceding fragment.
- **Required root** means every represented item needs that node.
- **Optional child** means zero or one direct helper child.
- **Export-only** names are organizational output and are never required on
  import.

`__` is the structural delimiter. Names and labels embedded as one component
must not contain it. Every finite distance embedded in a Level node name uses
GLB units. With the default `100` Arx units per GLB unit, `FALLEND_5` means
`500` Arx units. `TRANSVAL` and other dimensionless values are not scaled.

The `arx_` node prefix is reserved for semantic Level roots. Unrecognized
reachable `arx_*` nodes warn on Level import. Documented direct helpers are
semantic within their parent, and ordinary GLB point lights import without an
`arx_` name. Ordinary wrapper nodes may be used freely. A recognized room may
contain ordinary geometry and other recognized non-room objects. Other
recognized Level objects are terminal: their ordinary descendant payload is
ignored.

## Shared Face Flags

Level GLB, legacy FTL GLB, and FTL OBJ materials use `__` suffixes:

```text
<texture-stem>[__<face-flag>]...
no_tex[__<face-flag>]...
```

Known flags, in canonical export order:

| Token | Public bit |
| --- | ---: |
| `NO_SHADOW` | `1 << 0` |
| `DOUBLESIDED` | `1 << 1` |
| `TRANS` | `1 << 2` |
| `WATER` | `1 << 3` |
| `GLOW` | `1 << 4` |
| `IGNORE` | `1 << 5` |
| `QUAD` | `1 << 6` |
| `TILED` | `1 << 7` |
| `METAL` | `1 << 8` |
| `HIDE` | `1 << 9` |
| `STONE` | `1 << 10` |
| `WOOD` | `1 << 11` |
| `GRAVEL` | `1 << 12` |
| `EARTH` | `1 << 13` |
| `NOCOL` | `1 << 14` |
| `LAVA` | `1 << 15` |
| `CLIMB` | `1 << 16` |
| `FALL` | `1 << 17` |
| `NOPATH` | `1 << 18` |
| `NODRAW` | `1 << 19` |
| `PRECISE_PATH` | `1 << 20` |
| `NO_CLIMB` | `1 << 21` |
| `ANGULAR` | `1 << 22` |
| `ANGULAR_IDX0` | `1 << 23` |
| `ANGULAR_IDX1` | `1 << 24` |
| `ANGULAR_IDX2` | `1 << 25` |
| `ANGULAR_IDX3` | `1 << 26` |
| `LATE_MIP` | `1 << 27` |

Each flag should appear at most once. Level accepts `QUAD` but discards it
because editable Level faces are triangles. Native baking reconstructs
compatible FTS quads independently.

## Level GLB

Level GLB is an authoring projection, not an archive of native FTS cell
layout. GLB uses standard +Y-up coordinates. Level retains native Arx
coordinates with -Y up.

### Complete Level Layout

Pistoris export uses this organization. The listed roots and direct helpers
carry name-encoded semantics. Reachable ordinary GLB point lights also import
as Level lights.

```text
level_space                                      export-only scene root
+-- rooms_parent                                export-only
|   +-- arx_room__<room-name>                   required mesh root
+-- portals_parent                              export-only
|   +-- arx_portal__<room-1>__<room-2>__<portal-name>
+-- anchors_parent                              export-only
|   +-- arx_anchor[__RADIUS_<float>][__HEIGHT_<float>][__BLOCKED]__<anchor-name>
+-- lights_parent                               export-only
|   +-- arx_light[__FALLSTART_<float>][__FALLEND_<float>]__<light-name>
|       +-- SETTINGS__<light-setting>...__<label>
|       +-- FLAGS__<light-flag>...__<label>
|       +-- EFFECT__<light-effect>...__<label>
+-- entities_parent                             export-only
|   +-- arx_entity__<entity-components>
|       +-- CLASS_<class>__<label>
+-- zones_parent                                export-only
|   +-- arx_zone[__<ordinal>]__<zone-name>      required mesh root
|       +-- SETTINGS__<zone-setting>...__<label>
|       +-- AMBIANCE_<ambiance-path>__<label>
+-- paths_parent                                export-only
|   +-- arx_path__<path-name>
|       +-- <ordinal>__<path-node-type>__TIME_<uint32>[__<label>]
+-- fogs_parent                                 export-only
|   +-- arx_fog__<fog-name>
|       +-- SETTINGS__<fog-setting>...__<label>
|       +-- DIRECTION__<label>
+-- arx_nav_surface[__<label>]                  optional mesh root
+-- arx_player_spawn[__<label>]                 optional empty root
```

### Level Space

Export-only root:

```text
level_space
```

Defaults:

```text
arx_units_per_glb_unit = 100
arx_offset             = (0, 0, 0)
root rotation          = identity
root uniform scale     = 1 / arx_units_per_glb_unit
```

The unit ratio must be finite and inside inclusive `[1,1000]`. Import treats
`level_space` as an ordinary transform parent. World transforms remain
authoritative; the name has no independent import meaning.

### Level Materials

Optional ordinary material:

```text
<texture-stem>[__<face-flag>]...[__TRANSVAL_<finite-float>]
```

Reserved no-texture material:

```text
no_tex[__<face-flag>]...[__TRANSVAL_<finite-float>]
```

Defaults:

```text
missing material        = no texture
missing flags           = 0
missing TRANSVAL        = 1 - baseColorFactor.a when TRANS applies
missing referenced image = material stem supplies texture identity
```

A material that supplies texture identity requires its selected `TEXCOORD_N`
attribute on every primitive. The selection defaults to `TEXCOORD_0`.
Untextured primitives use `(0, 0)` UVs when no texture-coordinate attribute is
present.

A referenced base-color image is authoritative texture identity. Its external
URI remains the Level resource path. Embedded JPEG and PNG images use their
image name as the preferred filename stem; BMP and TGA data is converted to
PNG for GLB.

`TRANSVAL` is valid only when `TRANS` applies. It is mandatory on Pistoris
export for transparent ordinary materials and preserves the raw native value.
For standard `0 < TRANSVAL < 1`, preview alpha is `1 - TRANSVAL`. Values outside
that interval remain preserved in the name but core GLB cannot reproduce the
native blend mode.

`doubleSided=true` also supplies `DOUBLESIDED`. `alphaMode=BLEND` also supplies
`TRANS`. `alphaMode=MASK` represents image alpha cutout and does not supply
`TRANS`.

Real texture stems cannot be `no_tex`, `arx_portal`, `arx_zone`, or
`arx_nav_surface`.

Do not author texture names containing `__`. The original game has exactly
three known exception paths. Pistoris maps them to safe Level aliases and
restores the native paths during baking:

```text
graph/obj3d/textures/l4_dwarf_[stone]__wall01
<-> graph/obj3d/textures/l4_dwarf_[stone]_wall01

graph/obj3d/textures/l4_dwarf_[stone]__wall24
<-> graph/obj3d/textures/l4_dwarf_[stone]_wall24

graph/obj3d/textures/npc_human__base_hero_head
<-> graph/obj3d/textures/npc_human_base_hero_head_1
```

[Authoring example](AUTHORING_GUIDE.md#level-geometry-and-materials)

### Room

```text
export-only parent: rooms_parent
required mesh root: arx_room__<room-name>
```

`<room-name>` becomes the Level name and must be unique. The root may carry a
mesh directly and may contain ordinary child mesh nodes. Recognized non-room
Level roots may also be nested below a room for DCC organization without
becoming room geometry.

With no room or portal roots, all ordinary geometry imports into one generated
room named `room`. Export omits rooms without faces.

Optional room mesh attribute:

```text
COLOR_0
```

`COLOR_0` supplies baked linear RGB corner lighting. Import accepts `VEC3` or
`VEC4` colors with components inside `[0,1]` and ignores alpha. Missing colors
use `(0.5, 0.5, 0.5)`. Canonical export always writes float `VEC3` colors.
Static-lighting generation replaces these values only when explicitly
requested.

[Authoring example](AUTHORING_GUIDE.md#rooms)

### Portal

```text
export-only parent: portals_parent
required mesh root: arx_portal__<room-1>__<room-2>__<portal-name>
export-only material: arx_portal
```

All three components are mandatory. Room names must identify distinct explicit
rooms. Room order is semantic: the portal normal faces `<room-1>`.
`<portal-name>` becomes the unique Level portal name.

The mesh must resolve to three or four perimeter positions. Materials carry no
portal data and are ignored on import. Canonical export assigns `arx_portal`
for visualization.

[Authoring example](AUTHORING_GUIDE.md#portals)

### Player Spawn

```text
required root when authored: arx_player_spawn[__<label>]
canonical export:            arx_player_spawn__spawn
```

The root is empty. World translation and rotation define the spawn. A missing
root retains the Level fallback spawn state. Native baking writes the fallback
at the origin with identity rotation when no usable spawn exists; GLB export
omits that fallback. Multiple reachable roots are accepted and one is selected,
with no guarantee about which duplicate wins. Export emits at most one
player-spawn root.

[Authoring example](AUTHORING_GUIDE.md#player-spawn)

### Navigation Surface

```text
required mesh root when present: arx_nav_surface[__<label>]
canonical export:                arx_nav_surface__surface
export-only material:            arx_nav_surface
```

At most one navigation surface is valid. The label is ignored. Materials,
rooms, UVs, normals, colors, and face flags are not navigation-surface data.

[Authoring example](AUTHORING_GUIDE.md#navigation-surface)

### Anchor

```text
export-only parent: anchors_parent
required root:
  arx_anchor[__RADIUS_<float>][__HEIGHT_<float>][__BLOCKED]__<anchor-name>
```

Defaults:

```text
RADIUS  = 50 Arx units
HEIGHT  = 165 Arx units
BLOCKED = false
```

`RADIUS` and `HEIGHT` are nonnegative GLB lengths. At the default unit ratio,
their native defaults of 50 and 165 Arx units correspond to `0.5` and `1.65`.
Omitting either token selects its default; canonical export omits tokens whose
values are default. Level stores native anchor height as the corresponding
negative -Y extent. Option order is arbitrary on import; canonical export
order is `RADIUS`, `HEIGHT`, `BLOCKED`, then the name.

Nonempty anchor names are unique. Empty internal Level names export as
`anchor_<ordinal>`. GLB name collisions are repaired with `_N` suffixes.
Anchor connections have no GLB representation.

[Authoring example](AUTHORING_GUIDE.md#anchors)

### Light

Required root:

```text
arx_light[__FALLSTART_<float>][__FALLEND_<float>]__<light-name>
```

Optional direct children:

```text
SETTINGS__<light-setting>[__<light-setting>]...__<label>
FLAGS__<light-flag>[__<light-flag>]...__<label>
EFFECT__<light-effect>[__<light-effect>]...__<label>
```

Settings:

```text
RGB_<float>_<float>_<float>
INTENSITY_<float>
```

Effects:

```text
FLICKER_<float>_<float>_<float>
RADIUS_<float>
FREQUENCY_<float>
SIZE_<float>
SPEED_<float>
FLARESIZE_<float>
```

Flags, in canonical export order:

```text
SEMIDYNAMIC
EXTINGUISHABLE
STARTEXTINGUISHED
SPAWNFIRE
SPAWNSMOKE
OFF
COLORLEGACY
NOCASTED
FIXFLARESIZE
FIREPLACE
NOIGNIT
FLARE
```

Defaults, in dependency order:

```text
FALLEND = explicit FALLEND
          otherwise positive KHR_lights_punctual range
          otherwise 0

real light = FALLEND > 0

FALLSTART = 0.5 * FALLEND for a real light
            otherwise 0

RGB = explicit RGB
      otherwise point-light color
      otherwise (1, 1, 1) for a real light
      otherwise (0, 0, 0)

INTENSITY = explicit INTENSITY
            otherwise point-light intensity
            otherwise 1 for a real light
            otherwise 0

flags, effects = 0
```

The light name is mandatory and unique. Helper labels are ignored. Settings
inside one helper may appear in any order. RGB is inside `[0,1]`; intensity is
nonnegative.

Pistoris attaches `KHR_lights_punctual` data to exported real lights. Generic
positive-range point lights are accepted as an import convenience. Effect-only
roots use zero falloff and need no point-light payload.

[Authoring example](AUTHORING_GUIDE.md#lights)

### Entity

Required root, one of:

```text
arx_entity__<entity-name>
arx_entity__IDENT_<int32>[__<entity-name>]
arx_entity__<ordinal>__<entity-name>
arx_entity__<ordinal>__IDENT_<int32>[__<entity-name>]
```

Required direct class child, one of:

```text
CLASS_model:<model-type>:<model-name>__<label>
CLASS_<normalized-class-path>__<label>
```

Defaults:

```text
ordinal = absent
IDENT   = -1
name    = explicit entity name
          otherwise final class-path component without trailing _base
```

At least one root component is required. Ordinals are optional, unique, and
order numbered entities before unnumbered entities. Entity names are
case-sensitive and unique; collisions receive `_N` suffixes.

The class helper is empty. Valid shorthand types:

```text
npc
system
fix_inter
armor
jewelry
magic
movable
provisions
quest_item
special
weapons
```

The last eight values map below `graph/obj3d/interactive/items/`. Canonical
entity class paths are extensionless and begin at `graph/`. A final `.ftl` is
accepted and normalized. Legacy `.teo` is accepted by external import with a
warning and normalized, but is not a Level class-path form.

The entity root may carry a static preview mesh. It is ignored without warning
and never becomes Level geometry or a Level texture.

[Authoring example](AUTHORING_GUIDE.md#entities)

### Fog

Required root:

```text
arx_fog__<fog-name>
```

Optional direct children:

```text
SETTINGS__<fog-setting>[__<fog-setting>]...__<label>
DIRECTION__<label>
```

Settings:

```text
RGB_<float>_<float>_<float>
SIZE_<float>
SCALE_<float>
SPEED_<float>
ROTATESPEED_<float>
LIFETIME_<int32>
FREQUENCY_<float>
```

Defaults:

```text
RGB         = (0, 0, 0)
SIZE        = 0
SCALE       = 0
SPEED       = 0
ROTATESPEED = 0
LIFETIME    = 0 milliseconds
FREQUENCY   = 0
DIRECTION   = absent
```

The root name becomes the Level fog name. Nonempty names are unique. Empty
internal Level names export as `fog_<ordinal>`. Helper labels are ignored.
The direction is the normalized vector from the fog root to the direction
helper. `SIZE`, `SCALE`, and `SPEED` use GLB units and are converted by the
authoring unit ratio. `LIFETIME` uses milliseconds; the remaining settings are
not scaled.

[Authoring example](AUTHORING_GUIDE.md#fogs)

### Zone

```text
required mesh root:   arx_zone[__<ordinal>]__<zone-name>
export-only material: arx_zone
```

Optional direct children:

```text
SETTINGS__<zone-setting>[__<zone-setting>]...__<label>
AMBIANCE_<extensionless-ambiance-path>__<label>
```

Settings:

```text
RGB_<float>_<float>_<float>
FARCLIP_<float>
VOLUME_<float>
```

Defaults, in dependency order:

```text
ordinal  = absent
RGB      = absent
FARCLIP  = absent
AMBIANCE = absent
VOLUME   = 100 when AMBIANCE exists
```

Zone ordinals are optional, unique, and may contain gaps. The name is
mandatory and ASCII case-insensitive unique among zones. Zone and path names
occupy separate namespaces.

The ambiance path is lowercase, extensionless, relative, and uses `/`.
`none` is a valid explicit ambiance. `VOLUME` without ambiance is invalid.
Finite versus infinite zone height is derived from the mesh.
Materials carry no zone data and are ignored on import. Canonical export
assigns `arx_zone` for visualization.

[Authoring example](AUTHORING_GUIDE.md#zones)

### Spline Path

Required root:

```text
arx_path__<path-name>
```

One or more required direct children:

```text
<ordinal>__STANDARD__TIME_<uint32>[__<label>]
<ordinal>__BEZIER__TIME_<uint32>[__<label>]
<ordinal>__CONTROL__TIME_<uint32>[__<label>]
```

Defaults:

```text
ordinal = none; every child requires one
type    = none; every child requires one
TIME    = none; every child requires one
first ordered child TIME = 0 in Level
```

The path name is mandatory and ASCII case-insensitive unique among paths.
Native and GLB duplicate names receive the lowest available `_N` suffix.
Child ordinals are mandatory, unique, may contain gaps, and determine node
order. `TIME` uses milliseconds. Labels are ignored. World positions carry
path geometry.

[Authoring example](AUTHORING_GUIDE.md#spline-paths)

### Export-Only Level Names

These parents carry no import semantics:

| Name | Native Y placement relative to geometry AABB bottom |
| --- | ---: |
| `rooms_parent` | `+0` |
| `anchors_parent` | `+10` |
| `portals_parent` | `+20` |
| `lights_parent` | `+30` |
| `entities_parent` | `+40` |
| `zones_parent` | `+50` |
| `paths_parent` | `+60` |
| `fogs_parent` | `+70` |

Diagnostic GLB names are volatile and are not part of this reference.

## Static FTL OBJ

OBJ represents static FTL geometry and materials only. Skeletons, actions,
selections, and animations have no OBJ representation.

Material names use the [shared face flags](#shared-face-flags):

```text
newmtl <texture-stem>[__<face-flag>]...
```

Optional MTL texture identity:

```text
map_Kd <texture-path>
# arx_path <resource-path>
```

Texture identity precedence:

```text
# arx_path
map_Kd
decoded material stem
```

For `TRANS` materials, MTL `d` stores one opacity per material. Faces sharing
one texture and flag set therefore also share the imported transparency value.

[Authoring example](AUTHORING_GUIDE.md#static-ftl-obj-authoring)

## Legacy FTL and TEA GLB

This is the current direct FTL/TEA conversion surface. It predates the future
coherent Model and Animations editing classes.

### FTL Materials

Material names use the [shared face flags](#shared-face-flags):

```text
<texture-stem>[__<face-flag>]...
no_tex[__<face-flag>]...
```

GLB material alpha carries one transparency value per material. Unlike Level
GLB, the legacy FTL grammar has no `TRANSVAL` name token.

[Authoring example](AUTHORING_GUIDE.md#legacy-ftl-materials)

### FTL Bones

```text
<index>__<bone-name>
```

Indices are exact zero-based positions, not sparse ordinals. With `N` bones,
every index in `[0,N)` must appear once. Parents must precede children. Export
pads to at least three digits for readable DCC sorting; values above `999`
expand normally.

If indices are missing, duplicated, out of range, or topologically invalid,
import warns and falls back to GLB joint order.

[Authoring example](AUTHORING_GUIDE.md#legacy-ftl-skeleton-and-actions)

### FTL Actions

```text
arx_action__<action-name>
```

The action is an empty node. Parent it below the responsible bone. Everything
after the prefix becomes the FTL action name.

[Authoring example](AUTHORING_GUIDE.md#legacy-ftl-skeleton-and-actions)

### FTL Selections

Custom primitive attribute:

```text
_<selection-name>
```

The attribute must be `VEC4`. A value of `1` means membership and `0` means
exclusion. Import can recover selection masks from rewritten attributes such
as `COLOR_N`, but names may then need CLI repair.

[Authoring example](AUTHORING_GUIDE.md#legacy-ftl-vertex-selections)

### TEA Animations

Animation name:

```text
<animation-name>[__h]
```

`__h` marks the final keyframe as an exported duration hold. Import removes
that keyframe and suffix while preserving duration.

Root entity translation and rotation animate the structural skeleton wrapper.
The wrapper is identified by hierarchy; its node name is not semantic.

[Authoring example](AUTHORING_GUIDE.md#legacy-tea-animations)
