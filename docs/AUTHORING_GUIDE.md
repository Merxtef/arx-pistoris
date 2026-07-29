# Authoring Guide

This guide shows how to author Pistoris OBJ and GLB inputs in a conventional
DCC such as Blender. Every example is a complete, literal example. For exact
name grammars, defaults, and all supported tokens, use the
[Authoring Reference](AUTHORING_REFERENCE.md).

Pistoris currently provides a coherent Level editing class and Level GLB
authoring surface. Static FTL OBJ and direct FTL/TEA GLB conversions are
supported legacy surfaces; future Model and Animations classes will replace
the direct editing workflow for those formats.

## Terminology

- **Name** - semantic text imported into Level or an FTL carrier.
- **Label** - nonempty free text ignored on import. Labels are safe places for
  DCC uniqueness suffixes such as `.001`.
- **Ordinal** - unique unsigned ordering number. Gaps are allowed.
- **Index** - exact zero-based position. All values from zero through the last
  index must be present exactly once.
- **Resource path** - portable game resource identity using `/`, independent
  of any host filesystem or mount.

Names are case-sensitive unless the reference says otherwise. `__` separates
semantic components, so do not put `__` inside a name or label. Use labels
where Blender may append `.001`.

## Level GLB Authoring

Level GLB is an authoring projection, not an archive of native FTS cell
layout. You can build it from scratch in Blender with ordinary meshes, empties,
materials, and point lights. Native cell slicing, room distances, anchor
connections, static lighting, and other derived data can be generated when
needed.

GLB uses standard +Y-up coordinates. Level uses native Arx coordinates with
-Y up. By default, one GLB unit represents 100 Arx units. Pistoris rotates the
exported content into the GLB basis and applies authoring scale and offset on
an identity-rotation `level_space` root. Keep semantic object transforms below
that root.

For a new Level:

1. Create room meshes and give each room a unique semantic name.
2. Place portal meshes between rooms.
3. Add one player spawn.
4. Add lights, entities, zones, fogs, paths, and navigation data as needed.
5. Export GLB with transforms and custom material names preserved.
6. Use the CLI generation options needed by the native bake.

Ordinary wrapper objects may organize the scene. The `arx_` prefix is reserved
for semantic Level roots, and unrecognized reachable `arx_*` objects warn on
import. Documented direct helpers are semantic within their parent. Ordinary
GLB point lights also import without an `arx_` name.

### Level Geometry and Materials

Room geometry uses ordinary triangle meshes. The material name supplies the
texture stem and native face flags. A referenced base-color image supplies the
actual texture identity when one exists.

For a two-sided iron gate using `iron_gate.png`, create this material:

```text
iron_gate__DOUBLESIDED__METAL
```

Assign `iron_gate.png` as its base-color image. Pistoris then identifies the
Level texture as `iron_gate.png`; the material stem remains the fallback.

Use `no_tex` for collision geometry that has no texture and is not rendered:

```text
no_tex
```

Do not put `__` in authored texture names. The original game has exactly three
known texture-path exceptions; Pistoris maps those through safe aliases. See
[Level Materials](AUTHORING_REFERENCE.md#level-materials) for those mappings,
all flags, transparency, and GLB material-property behavior.

### Rooms

A room is a mesh root named `arx_room__` followed by its unique Level name.
The root may carry geometry directly or contain ordinary child meshes.

For a room named `cellar`, use:

```text
arx_room__cellar
+-- cellar_floor
+-- cellar_walls
+-- cellar_ceiling
```

The three ordinary child meshes all become geometry of the `cellar` room.
Lights, entities, fogs, zones, and other recognized non-room objects may also
be organized below the room without becoming bound to it.

If a GLB has no explicit rooms or portals, Pistoris collects ordinary geometry
into one generated room named `room`. Empty rooms are discarded on export.

See the exact [Room reference](AUTHORING_REFERENCE.md#room).

### Portals

A portal is one triangle or quad between two explicit rooms. The room order in
the name matters: the portal normal faces the first named room.

For a doorway between `cellar` and `hall`, create a four-corner mesh whose
front face and normal point into `cellar`:

```text
arx_portal__cellar__hall__north_door
```

The complete scene fragment is:

```text
arx_room__cellar
arx_room__hall
arx_portal__cellar__hall__north_door
```

Use exactly one face worth of perimeter geometry. A DCC may split its vertices
for rendering; Pistoris welds near-identical position splits before recovering
the three or four portal corners. Portal materials carry no Level data and are
ignored on import. Pistoris assigns the blue `arx_portal` material on export.

See the exact [Portal reference](AUTHORING_REFERENCE.md#portal).

### Player Spawn

Create an empty whose world translation and rotation define the spawn:

```text
arx_player_spawn__spawn
```

`spawn` is a label, so `arx_player_spawn__spawn.001` remains valid if Blender
deduplicates the object name. Keep one authored player spawn per exported
Level. A missing spawn remains distinguishable from an authored origin spawn
inside Level.

See the exact [Player Spawn reference](AUTHORING_REFERENCE.md#player-spawn).

### Navigation Surface

Create one ordinary triangle mesh covering the walkable support surface:

```text
arx_nav_surface__walkable
```

`walkable` is a label. Materials, UVs, normals, colors, room assignments, and
face flags do not carry navigation-surface meaning. This surface is distinct
from Level geometry; it supports later anchor generation.

You may author it directly, derive it from floor-like Level faces, or generate
it through the CLI. See the exact
[Navigation Surface reference](AUTHORING_REFERENCE.md#navigation-surface).

### Anchors

Anchors are empty objects. Default game-sized anchors omit radius and height:

```text
arx_anchor__doorway
```

At the default scale of 100 Arx units per GLB unit, a blocked anchor named
`low_arch` with radius 35 and height 120 Arx units is:

```text
arx_anchor__RADIUS_0.35__HEIGHT_1.2__BLOCKED__low_arch
```

Anchor connections are not represented in GLB. Import preserves authored
anchors, and connection generation remains an explicit operation.

See the exact [Anchor reference](AUTHORING_REFERENCE.md#anchor).

### Lights

Real lights can use Blender point-light data for color, intensity, and range.
Pistoris-specific helpers preserve the native falloff, flags, and effects.

For a torch named `west_torch`, create a point light and these direct empty
children:

```text
arx_light__FALLSTART_2.5__FALLEND_5__west_torch
+-- SETTINGS__RGB_1_0.55_0.2__INTENSITY_1.4__warm_torch
+-- FLAGS__SEMIDYNAMIC__EXTINGUISHABLE__SPAWNFIRE__SPAWNSMOKE__torch_flags
+-- EFFECT__FLICKER_0.15_0.08_0.03__FREQUENCY_0.2__torch_flicker
```

The helper suffixes `warm_torch`, `torch_flags`, and `torch_flicker` are
labels. `FALLSTART` and `FALLEND` are GLB lengths; at the default scale, the
example maps to 250 and 500 Arx units.

An effect-only source may be an empty light root with no positive falloff.
See the exact [Light reference](AUTHORING_REFERENCE.md#light) for every flag,
effect, and default.

### Entities

An entity is an empty root with one direct class helper. The root transform is
the entity transform. A static preview mesh may be attached directly to the
root for placement; it is ignored by Level import.

For the 43rd authored entity, a guard with native `ident` 12:

```text
arx_entity__042__IDENT_12__cellar_guard (mesh: human_preview)
+-- CLASS_model:npc:human_base__guard_class
```

`042` is an ordinal: it orders entities, must be unique, and may have gaps.
`cellar_guard` is the unique Level name. `guard_class` is a label. The
`model:npc:human_base` selector and its canonical full resource path identify
the same model. Attach `human_preview` as the root object's mesh, not as a
child object.

See the exact [Entity reference](AUTHORING_REFERENCE.md#entity) for class-path
forms, allowed model types, and defaults.

### Fogs

A fog source is an empty root. Put native settings in one direct helper and
use a second empty to define direction.

```text
arx_fog__cellar_mist
+-- SETTINGS__RGB_0.32_0.4_0.48__SIZE_80__SCALE_1.2__SPEED_0.25__LIFETIME_6000__mist_settings
+-- DIRECTION__forward
```

Place `DIRECTION__forward` away from the fog root along the intended emission
direction. Its distance is irrelevant; Pistoris normalizes the vector.
`mist_settings` and `forward` are labels.

See the exact [Fog reference](AUTHORING_REFERENCE.md#fog).

### Zones

A zone is a closed or open mesh root. Its name becomes game data; its optional
ordinal controls ordering. Direct helpers carry depth-fog and ambiance data.

For a purple `cellar_attack` zone with ordinal 8 and ambiance
`my_ambiance`:

```text
arx_zone__008__cellar_attack
+-- SETTINGS__RGB_0.63_0.1_0.9__FARCLIP_21.7__depth_fog
+-- AMBIANCE_my_ambiance__zone_sound
```

`008` is an ordinal, so other zones may use values such as 0, 9, and 15
without filling the gaps. `cellar_attack` is the semantic zone name.
`depth_fog` and `zone_sound` are labels. `my_ambiance` is the extensionless
ambiance resource path. At the default scale, `FARCLIP_21.7` sets the depth-fog
distance to 2170 Arx units.

Zone materials carry no Level data and are ignored on import. Pistoris assigns
the purple `arx_zone` material on export.

See the exact [Zone reference](AUTHORING_REFERENCE.md#zone).

### Spline Paths

A spline path is an empty root with one direct empty child per control point.
Child world positions define the path; ordinals define their order.

```text
arx_path__guard_patrol
+-- 000__STANDARD__TIME_0__start
+-- 008__BEZIER__TIME_1500__turn
+-- 015__STANDARD__TIME_3000__end
```

The gaps in ordinals are valid. `guard_patrol` is the path name; `start`,
`turn`, and `end` are labels. `TIME` values are milliseconds. The first
ordered node must have time zero in Level.

See the exact [Spline Path reference](AUTHORING_REFERENCE.md#spline-path).

### Generated and Rebuilt Data

GLB intentionally omits native cell slicing, room-distance records, and anchor
connections. Room mesh `COLOR_0` stores baked corner lighting; missing colors
use neutral `(0.5, 0.5, 0.5)`. Native baking reconstructs FTS cells and can
reconstruct compatible quads. Other work, including replacing corner colors
through static-lighting generation, stays explicit so authoring choices are
not silently overwritten.

The CLI applies requested Level operations in this order:

1. Vertex welding.
2. Navigation-surface generation.
3. Navigation-island pruning.
4. Anchor generation.
5. Anchor connection generation.
6. Anchor-island pruning.
7. Room-distance generation.
8. Static-lighting generation.

Use `arx-pistor --help` for the current option names and defaults.

### Blender Export

Export the intended collection as GLB. Preserve object and material names,
mesh UVs, punctual lights, hierarchy, and transforms. Apply mesh editing
operations as appropriate, but do not flatten the semantic parent-child
structure.

Blender object names are global, even across collections. Put arbitrary labels
at the documented trailing label position so suffixes such as `.001` remain
harmless. Semantic names themselves must remain unique.

For native conversion, prefer a resource selector such as `level:1` when
working against mounted game resources. A valid selector and its canonical
full resource path are interchangeable resource identities; mounts determine
where the CLI reads or writes the corresponding file.

## Static FTL OBJ Authoring

OBJ supports static FTL geometry and materials. It cannot represent skeletons,
action points, selections, or animations.

For a metal blade, use an OBJ material:

```text
newmtl sword_blade__METAL__NO_SHADOW
```

The MTL may identify its texture with both ordinary DCC data and an explicit
game resource path:

```text
newmtl sword_blade__METAL__NO_SHADOW
map_Kd sword_blade.png
# arx_path graph/obj3d/textures/sword_blade.png
```

Pistoris chooses texture identity in this order: `# arx_path`, `map_Kd`, then
the decoded material stem.

See the exact [Static FTL OBJ reference](AUTHORING_REFERENCE.md#static-ftl-obj).

## Legacy FTL and TEA GLB

Direct FTL/TEA GLB conversion remains available for compatibility. It is not
the future coherent Model and Animations editing API.

### Legacy FTL Materials

Legacy FTL material names use the same texture-stem and face-flag convention
as Level:

```text
goblin_body__NO_SHADOW__DOUBLESIDED
```

Unlike Level GLB, legacy FTL material names do not carry a `TRANSVAL` token.
See the exact [FTL Material reference](AUTHORING_REFERENCE.md#ftl-materials).

### Legacy FTL Skeleton and Actions

Bone prefixes are indices, not ordinals. For three bones, use every index from
zero through two exactly once:

```text
000__root
+-- 001__chest
    +-- 002__right_hand
        +-- arx_action__WEAPON_ATTACH
```

Keep parents before children in index order. The action empty becomes an FTL
attachment point owned by `right_hand`.

See the exact [FTL Bones](AUTHORING_REFERENCE.md#ftl-bones) and
[FTL Actions](AUTHORING_REFERENCE.md#ftl-actions) references.

### Legacy FTL Vertex Selections

Named vertex selections use custom `VEC4` primitive attributes. For a chest
equipment selection:

```text
_CHEST
```

Write `1` for selected vertices and `0` for excluded vertices.

See the exact [FTL Selection reference](AUTHORING_REFERENCE.md#ftl-selections).

### Legacy TEA Animations

Animate the structural skeleton wrapper to create entity root motion. Add
`__h` to a looping animation whose final keyframe is a duration hold:

```text
human_walk__h
```

Pistoris removes the hold frame and suffix on import while preserving the
duration.

See the exact [TEA Animation reference](AUTHORING_REFERENCE.md#tea-animations).
