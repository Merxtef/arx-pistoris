# Level GLB Authoring Reference

Back to the [Authoring Reference](../AUTHORING_REFERENCE.md). For worked
examples, use the [Level Authoring Guide](LEVEL_GUIDE.md). Shared notation,
materials, and face flags are defined in
[Shared Authoring Conventions](CONVENTIONS.md).

This reference is for authors and tools that need the exact Level GLB naming
and structure contract.

## Complete Layout

Canonical export uses this organization. Names marked export-only are optional
ordinary wrappers on import.

```text
level_space                                      export-only scene root
+-- rooms_parent                                export-only
|   `-- arx_room__<room-name>                   required mesh root
+-- portals_parent                              export-only
|   `-- arx_portal__<room-1>__<room-2>__<portal-name>
+-- anchors_parent                              export-only
|   `-- arx_anchor[__RADIUS_<float>][__HEIGHT_<float>][__BLOCKED]__<anchor-name>
+-- lights_parent                               export-only
|   `-- arx_light[__FALLSTART_<float>][__FALLEND_<float>]__<light-name>
|       +-- SETTINGS__<light-setting>...__<label>
|       +-- FLAGS__<light-flag>...__<label>
|       `-- EFFECT__<light-effect>...__<label>
+-- entities_parent                             export-only
|   `-- arx_entity__<entity-components>
|       `-- CLASS_<class>__<label>
+-- zones_parent                                export-only
|   `-- arx_zone[__<ordinal>]__<zone-name>      required mesh root
|       +-- SETTINGS__<zone-setting>...__<label>
|       `-- AMBIANCE_<ambiance-reference>__<label>
+-- paths_parent                                export-only
|   `-- arx_path__<path-name>
|       `-- <ordinal>__<path-node-type>__TIME_<uint32>__<label>
+-- fogs_parent                                 export-only
|   `-- arx_fog__<fog-name>
|       +-- SETTINGS__<fog-setting>...__<label>
|       `-- DIRECTION__<label>
+-- arx_minimap__<label>                        optional mesh root
+-- arx_nav_surface__<label>                    optional mesh root
`-- arx_player_spawn__<label>                   optional empty root
```

Reachable ordinary GLB point lights also import as Level lights. A room may
contain ordinary geometry and other recognized non-room Level objects.
Recognized non-room objects are terminal: ordinary descendant payload below
them is ignored. Unrecognized reachable `arx_*` roots warn.

Player-spawn, anchor, light, entity, and fog roots must keep identity local
scale. Import discards nonidentity local scale with a warning; named size
fields remain authoritative.

Canonical labels are mandatory where shown. Import can recover missing labels
from the three singleton roots, light helpers, zone settings, path nodes, and
fog helpers because those grammars are self-delimiting; each recovery warns.
Entity `CLASS` and zone `AMBIANCE` labels remain mandatory because their payloads
may contain `__`. Semantic options take precedence over label recovery, and
labels that resemble convention tokens follow the shared informational-notice
rule.

## Level Space

Canonical export wrapper:

```text
level_space
```

Defaults:

```text
Arx units per GLB unit = 100
Arx offset             = (0, 0, 0)
root rotation          = identity
root uniform scale     = 1 / Arx units per GLB unit
```

GLB uses +Y up. Level uses native Arx coordinates with -Y up. The unit ratio
must be finite and inside inclusive `[1,1000]`. `level_space` is an ordinary
transform parent on import; its name is not semantic.

Every finite Level distance embedded in a name uses GLB units. With the default
ratio, `FALLEND_5` means 500 Arx units. Dimensionless values are not scaled.

[Worked introduction](LEVEL_GUIDE.md#coordinates)

## Level Materials

Optional ordinary material:

```text
<fallback-stem>[__<face-flag>]...[__TRANSVAL_<float>]
```

Reserved no-texture material:

```text
no_tex[__<face-flag>]...[__TRANSVAL_<float>]
```

Defaults:

```text
missing material         = no texture
missing flags            = 0
missing TRANSVAL         = 1 - baseColorFactor.a when TRANS applies
missing referenced image = fallback stem supplies texture identity
```

A textured primitive requires its selected `TEXCOORD_N` attribute. Selection
defaults to `TEXCOORD_0`. Untextured primitives use `(0,0)` UVs when no texture
coordinates are present.

For ordinary materials, a referenced base-color image is authoritative texture
identity, including on a `no_tex` material. Import normalizes separators and
removes exactly one final image suffix from its URI or preferred image name.
Dots before that suffix remain part of the identity. `arx_portal`, `arx_zone`,
and `arx_nav_surface` are reserved for their corresponding Level meshes and
cannot identify ordinary textures.

`TRANSVAL` requires effective `TRANS`. A `TRANS` token always supplies it. With
`alphaMode=BLEND`, a `TRANSVAL` token or base alpha below `1` also supplies it.
Canonical export writes `TRANSVAL` for every transparent ordinary material. For
`0 < TRANSVAL < 1`, preview alpha is `1 - TRANSVAL`. Other values remain
preserved in the name even though core GLB cannot preview every native blend
mode.

GLB properties also imply flags:

- `doubleSided=true` supplies `DOUBLESIDED`.
- `alphaMode=BLEND` supplies `TRANS` when base alpha is below `1` or a
  `TRANSVAL` token is present. With base alpha `1` and no native transparency
  token, texture alpha remains native cutout; a material without a texture
  becomes opaque. Import warns in either case.
- `alphaMode=MASK` must use base alpha `1` and cutoff `0.5`. Import treats it
  as image cutout, does not supply `TRANS`, and normalizes any other base alpha
  or cutoff to those values with a warning.

Texture paths may contain `__`. Export collapses underscore runs only in the
material fallback stem and warns once per affected texture. The image path
remains authoritative. Fallback stems that collide with `no_tex`, `arx_portal`,
`arx_zone`, or `arx_nav_surface` receive the lowest available `_N` suffix.

[Worked material example](LEVEL_GUIDE.md#geometry-and-materials)

## Room

```text
export-only parent: rooms_parent
required mesh root: arx_room__<room-name>
```

`<room-name>` becomes the unique Level room name. The root may carry a mesh and
may contain ordinary child meshes. Recognized non-room Level roots may also be
nested below it without becoming room geometry.

Room names use the shared semantic-name grammar. Import normalizes their
spelling before resolving portals. If two room names become equal after
normalization, import fails because the references would be ambiguous.
`__` is structural syntax and is invalid inside a room name.

With no room or portal roots, all ordinary geometry imports into one generated
room named `room`. Export omits rooms without faces.

Optional room mesh attribute:

```text
COLOR_0
```

`COLOR_0` supplies baked linear RGB corner lighting. Import accepts `VEC3` or
`VEC4` values with components inside `[0,1]` and ignores alpha. Missing colors
use `(0.5,0.5,0.5)`. Canonical export writes float `VEC3` colors. Static-lighting
generation replaces them only when explicitly requested.

[Worked room example](LEVEL_GUIDE.md#rooms)

## Portal

```text
export-only parent: portals_parent
required mesh root: arx_portal__<room-1>__<room-2>__<portal-name>
export-only material: arx_portal
```

All three components are mandatory. Room names must identify distinct explicit
rooms. Room order is semantic: the portal normal faces `<room-1>`.
Room references are normalized with the same grammar as room definitions.
`<portal-name>` becomes the unique Level portal name; import normalizes it and
resolves collisions with `_N` suffixes.

The mesh must resolve to three or four perimeter positions. Author quad
positions on one plane. `--flatten-portals` can project an accepted mildly
nonplanar quad onto its canonical plane after import, but cannot recover a
portal rejected as invalid. Materials carry no portal data and are ignored.
Canonical export assigns `arx_portal` for visualization.

[Worked portal example](LEVEL_GUIDE.md#portals)

## Room Distances

Room distances have no authored GLB notation. Canonical export can preserve a
complete, nonempty collection as opaque round-trip data when every Level room
has exported geometry. Import either restores the complete collection or
discards it without failing Level import when the stored room or portal
structure no longer matches.

Keep imported custom properties or metadata when re-exporting GLB if the
stored distances should survive the edit. Geometry edits can still make a
recovered collection stale; generate room distances explicitly when updated
correctness is required.

## Player Spawn

```text
required root when authored: arx_player_spawn__<label>
canonical export:            arx_player_spawn__spawn
```

The root is empty. World translation and rotation define the spawn. A missing
root leaves the player spawn absent. Native baking writes the origin/identity
fallback; GLB export omits the node. Multiple roots are accepted, but which
duplicate wins is unspecified. Export emits at most one.

[Worked player-spawn example](LEVEL_GUIDE.md#player-spawn)

## Navigation Surface

```text
required mesh root when present: arx_nav_surface__<label>
canonical export:                arx_nav_surface__surface
export-only material:            arx_nav_surface
```

At most one navigation surface is valid. The label is ignored. Materials,
rooms, UVs, normals, colors, and face flags carry no navigation meaning.

[Worked navigation example](LEVEL_GUIDE.md#navigation-surface)

## Anchor

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

`RADIUS` and `HEIGHT` are nonnegative GLB lengths. At the default ratio, the
defaults are `0.5` and `1.65`. Canonical export omits default-valued tokens.
Option order is arbitrary on import and canonical on export as `RADIUS`,
`HEIGHT`, `BLOCKED`, then name.

Nonempty anchor names are unique. Empty Level names export as
`anchor_<ordinal>`. Name collisions receive `_N` suffixes.

Anchor connections have no authored GLB notation. Canonical export can
preserve the current graph as opaque round-trip data. Import keeps usable
connections and ignores unusable declarations without failing Level import.
Keep imported custom properties or metadata when re-exporting GLB, and
generate connections explicitly after structural anchor edits when the graph
matters.

[Worked anchor example](LEVEL_GUIDE.md#anchors)

## Light

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
          otherwise positive point-light range
          otherwise 0

real light = FALLEND > 0

FALLSTART = 0.5 * FALLEND for a real light
            otherwise 0

RGB = explicit RGB
      otherwise point-light color
      otherwise (1,1,1) for a real light
      otherwise (0,0,0)

INTENSITY = explicit INTENSITY
            otherwise point-light intensity
            otherwise 1 for a real light
            otherwise 0

flags, effects = 0
```

When supplied, `FALLSTART` must satisfy `0 <= FALLSTART < FALLEND`. Import
replaces an invalid value with the default above and warns.

The light name is mandatory and unique. Import normalizes it and resolves
collisions with `_N` suffixes. Helper labels are ignored. Settings in one
helper may appear in any order. RGB components are inside `[0,1]` and intensity
is nonnegative.

Canonical real lights carry `KHR_lights_punctual`. Generic positive-range point
lights are accepted without an `arx_` name. Effect-only roots use zero falloff
and need no point-light payload.

[Worked light example](LEVEL_GUIDE.md#lights)

## Entity

Required root, one of:

```text
arx_entity__<entity-name>
arx_entity__IDENT_<int32>[__<entity-name>]
arx_entity__<ordinal>__<entity-name>
arx_entity__<ordinal>__IDENT_<int32>[__<entity-name>]
```

Required direct class child, one of:

```text
CLASS_model:<model-type>:<model-name>[:<tweak>]__<label>
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
case-sensitive and unique. Import normalizes them and resolves collisions with
`_N` suffixes.

World translation and rotation define the entity transform. Local scale has no
entity representation and is discarded with a warning.

Valid Model selector types:

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
ui-runes
ui-menus
editor
```

The item categories map below `graph/obj3d/interactive/items/`. A selector is
accepted only when its resulting FTL path is an engine-recognized entity class.
Full class paths are lowercase portable `/` paths without a final extension
and may omit `graph/`. The final `__` separates the required label, so a class
path may contain `__`. If a full path contains `graph` after a prefix, the
engine discards the prefix; import stores the effective path and warns. Import
normalizes legacy `.teo` with a warning, but `.teo` is not a Level class-path
form.

The entity root may carry one static preview mesh. It is ignored without a
warning and never becomes Level geometry or a Level texture.

[Worked entity example](LEVEL_GUIDE.md#entities)

## Fog

Required root:

```text
arx_fog__<fog-name>
```

Optional direct children:

```text
SETTINGS__<fog-setting>[__<fog-setting>]...__<label>
DIRECTION__<label>
```

Settings and defaults:

```text
RGB_<float>_<float>_<float> = (0,0,0)
SIZE_<float>                = 0
SCALE_<float>               = 0
SPEED_<float>               = 0
ROTATESPEED_<float>         = 0
LIFETIME_<int32>            = 0 milliseconds
FREQUENCY_<float>           = 0
DIRECTION                   = absent
```

The root name becomes the Level fog name. Nonempty names are unique. Empty
Level names export as `fog_<ordinal>`. Import normalizes nonempty names and
resolves collisions with `_N` suffixes. The direction is the normalized vector
from the fog root to its helper. `SIZE`, `SCALE`, and `SPEED` use GLB units.
`LIFETIME` uses milliseconds; the remaining settings are not scaled. Local
object scale is discarded with a warning.

[Worked fog example](LEVEL_GUIDE.md#fogs)

## Zone

```text
required mesh root:   arx_zone[__<ordinal>]__<zone-name>
export-only material: arx_zone
```

Optional direct children:

```text
SETTINGS__<zone-setting>[__<zone-setting>]...__<label>
AMBIANCE_<ambiance-reference>__<label>
```

Settings and defaults:

```text
ordinal  = absent
RGB      = absent
FARCLIP  = absent
AMBIANCE = absent
VOLUME   = 100 when AMBIANCE exists
```

Setting forms:

```text
RGB_<float>_<float>_<float>
FARCLIP_<float>
VOLUME_<float>
```

An Ambiance reference may be an `ambiance:<name>` selector, a canonical
`sfx/ambiance/<name>.amb` path, or a portable relative name. These forms all
refer to the same normalized name. A final `.amb` is treated as the file suffix;
other dots remain part of the name. Canonical export uses the selector form,
except that `none` is written literally. The final `__` separates the required
label, so an Ambiance name may contain `__` or dots.

Zone ordinals are optional, unique, and may have gaps. Names are mandatory,
lowercase, and unique among zones. Import lowercases and normalizes them, then
resolves collisions with `_N` suffixes. Zone and path names use separate
namespaces.

`none` is a valid explicit Ambiance; do not prefix it with `ambiance:`. `VOLUME`
without Ambiance is invalid.
Finite or infinite zone height is derived from the mesh. A zone is infinite
when its reconstructed top and bottom planes span the complete vertical bounds
of the imported Level geometry, within zone-plane tolerance. Zone top and
bottom surfaces must be planar; import flattens deviations to the reconstructed
planes and warns. Materials carry no zone data.

[Worked zone example](LEVEL_GUIDE.md#zones)

## Spline Path

Required root:

```text
arx_path__<path-name>
```

One or more required direct children:

```text
<ordinal>__STANDARD__TIME_<uint32>__<label>
<ordinal>__BEZIER__TIME_<uint32>__<label>
```

Path names are mandatory, lowercase, and unique among paths. Import lowercases
and normalizes them, then assigns the lowest available `_N` suffix after
collisions. Child ordinals are mandatory, unique, may have gaps, and determine
order. Labels are mandatory and ignored. `TIME` uses milliseconds. The first
ordered child must be authored with `TIME_0`; import resets any other value to
zero. World positions define geometry.

`STANDARD` starts a linear segment to the next point. `BEZIER` starts a curve
through the next point to the point after it. The middle point remains
`STANDARD`; its `TIME` is ignored by that curve, while the endpoint's `TIME`
sets the curve duration. Author `BEZIER` only when two following points exist.

[Worked spline-path example](LEVEL_GUIDE.md#spline-paths)

## Minimap

```text
required mesh root when present: arx_minimap__<label>
canonical export:                arx_minimap__map
```

At most one minimap is valid. It is a triangle mesh with one embedded
base-color image. Its world X/Z bounds define the rectangular area represented
by the image; height is discarded.

The canonical position and UV pairs are:

```text
(min X, max Z) -> (0, 0)
(max X, max Z) -> (1, 0)
(max X, min Z) -> (1, 1)
(min X, min Z) -> (0, 1)
```

Canonical authoring uses all four pairs and two triangles covering the
rectangle. Exact quarter-turn UV layouts are accepted and the image is rotated
to preserve its world orientation. Missing or noncanonical UVs use canonical
orientation with a warning. Nonplanar or unusual topology is reduced to its
X/Z bounds with a warning. Unreferenced positions carry no data. Normals and
tangents are ignored; other vertex attributes are invalid. The image must be
embedded. Canonical export preserves PNG and JPEG and transcodes BMP or TGA to
PNG. Malformed or duplicate minimaps are omitted with a warning.

Loading screens have no Level GLB representation.

[Worked minimap example](LEVEL_GUIDE.md#minimap)

## Export-Only Names

These ordinary wrappers carry no import semantics:

```text
level_space
rooms_parent
anchors_parent
portals_parent
lights_parent
entities_parent
zones_parent
paths_parent
fogs_parent
```

Diagnostic GLB names are volatile and are not part of this authoring contract.

[Back to the Authoring Reference](../AUTHORING_REFERENCE.md)
