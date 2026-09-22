# Level GLB Authoring Guide

Back to the [Authoring Guide](../AUTHORING_GUIDE.md). For exact syntax and
constraints, use the [Level Authoring Reference](LEVEL_REFERENCE.md).

This guide is for authors creating or editing Level GLB in a DCC such as
Blender. Converting a small original level first is the easiest way to obtain a
working hierarchy and material setup.

Level GLB is editable scene data, not an archive of native FTS cell layout.
Rooms, portals, entities, lights, zones, fogs, paths, and navigation support are
authored directly. Native cell slicing and other build products are recreated
or generated when requested.

## Coordinates

GLB uses +Y up. Level uses native Arx coordinates with -Y up. The default scale
is 100 Arx units per GLB unit. Converted levels normally contain an ordinary
`level_space` wrapper carrying the exported transform. Keep its children below
it while editing, but do not rely on its name when creating a new scene.

For a new Level:

1. Create room meshes and give each room a unique semantic name.
2. Place portal meshes between rooms.
3. Add one player spawn.
4. Add lights, entities, zones, fogs, paths, and navigation data as needed.
5. Export GLB with names, hierarchy, transforms, UVs, and material assignments
   preserved.
6. Request the native generation steps required by the level.

Use ordinary names for organizational nodes. Reserve `arx_` names for the
documented semantic objects.

Keep the final descriptive labels shown on semantic roots and helpers. Import
can recover labels from fixed helper grammars when omitted, but warns. Keep the
labels on entity `CLASS` and zone `AMBIANCE` helpers because their payloads may
contain `__` and have no other reliable end delimiter.

[Exact coordinate rules](LEVEL_REFERENCE.md#level-space)

## Geometry and Materials

Room geometry uses ordinary triangle meshes. A referenced base-color image
supplies texture identity. The material name supplies face flags and remains
the texture fallback when no image is referenced.

For a two-sided metal gate using `iron_gate.png`, create this material:

```text
iron_gate__DOUBLESIDED__METAL
```

Assign `iron_gate.png` as its base-color image. Use `no_tex` for untextured
collision geometry that should not be drawn:

```text
no_tex
```

Texture paths may contain `__`, spaces, square brackets, parentheses, and
ampersands. Avoid `__` when practical: material fallback names collapse
underscore runs and cannot recover the original path if the image reference is
lost.

[Exact Level material rules](LEVEL_REFERENCE.md#level-materials)

## Rooms

A room is a mesh root whose name becomes the room name. It may carry geometry
directly or contain ordinary child meshes.

For a room named `cellar`:

```text
arx_room__cellar
+-- cellar_floor
+-- cellar_walls
`-- cellar_ceiling
```

All three child meshes become geometry of `cellar`. Lights, entities, fogs,
zones, and other semantic objects may also be organized below the room without
becoming room geometry.

A GLB with no explicit rooms or portals collects ordinary geometry into one
room named `room`. Empty rooms do not appear in exported GLB.

[Exact room syntax](LEVEL_REFERENCE.md#room)

## Portals

A portal is one triangle or quad between two explicit rooms. Room order is
semantic: the portal face and normal point into the first named room.

For a doorway between `cellar` and `hall`, create a three- or four-corner mesh
facing into `cellar`:

```text
arx_room__cellar
arx_room__hall
arx_portal__cellar__hall__north_door
```

The mesh must have exactly three or four unique perimeter positions. Its
material carries no portal data, so no special authored portal material is
needed.

[Exact portal syntax](LEVEL_REFERENCE.md#portal)

## Player Spawn

Create one empty whose world translation and rotation define the spawn:

```text
arx_player_spawn__spawn
```

`spawn` is a label, so a DCC suffix such as `.001` does not change the meaning.
Keep one player spawn in the exported Level. Its local scale must be identity;
import discards any other local scale with a warning.

[Exact player-spawn syntax](LEVEL_REFERENCE.md#player-spawn)

## Navigation Surface

Create one ordinary triangle mesh covering the walkable support surface:

```text
arx_nav_surface__walkable
```

`walkable` is a label. Materials, UVs, normals, colors, room assignments, and
face flags do not carry navigation meaning. The navigation surface is separate
from visible Level geometry and supports anchor generation.

It may be authored directly or generated from Level geometry through the CLI.

[Exact navigation-surface syntax](LEVEL_REFERENCE.md#navigation-surface)

## Anchors

Anchors are empty objects. Default game-sized anchors need no size tokens:

```text
arx_anchor__doorway
```

At the default scale, a blocked anchor named `low_arch` with radius 35 and
height 120 Arx units is:

```text
arx_anchor__RADIUS_0.35__HEIGHT_1.2__BLOCKED__low_arch
```

The anchor's local scale must be identity. `RADIUS` and `HEIGHT` define its
size; import discards any other local scale with a warning.

Anchor connections use opaque GLB metadata for roundtrip preservation. DCC
tools may discard that metadata. Generate connections explicitly after
structural anchor edits; generation replaces the preserved graph.

[Exact anchor syntax](LEVEL_REFERENCE.md#anchor)

## Lights

Use Blender point lights for ordinary color, intensity, and range. Direct empty
helpers preserve native falloff, flags, and effects that standard GLB lights do
not describe.

For a torch named `west_torch`:

```text
arx_light__FALLSTART_2.5__FALLEND_5__west_torch
+-- SETTINGS__RGB_1_0.55_0.2__INTENSITY_1.4__warm_torch
+-- FLAGS__SEMIDYNAMIC__EXTINGUISHABLE__SPAWNFIRE__SPAWNSMOKE__torch_flags
`-- EFFECT__FLICKER_0.15_0.08_0.03__FREQUENCY_0.2__torch_flicker
```

The final components are labels. `FALLSTART` and `FALLEND` use GLB lengths, so
the example maps to 250 and 500 Arx units at the default scale. The light node's
local scale must be identity; import discards any other local scale with a
warning.

[Exact light syntax](LEVEL_REFERENCE.md#light)

## Entities

An entity is an empty root with one direct class helper. Its world translation
and rotation define the entity transform. Its local scale must be identity;
import discards any other local scale with a warning. A static preview mesh may
be attached directly to the root for placement and is not imported as Level
geometry.

For the 43rd authored entity, a guard with native `ident` 12:

```text
arx_entity__042__IDENT_12__cellar_guard (mesh: human_preview)
`-- CLASS_model:npc:human_base__guard_class
```

`042` is an ordinal and may have gaps. `cellar_guard` is the Level entity name.
`guard_class` is a label. The selector expands to the registered full Model
resource path.

Generate the same placement fragment from a Model with `--as-level-preview`.
Use `--load-previews` during Level GLB export to attach available Model previews
for referenced entity classes.

[Exact entity syntax](LEVEL_REFERENCE.md#entity)

## Fogs

A fog source is an empty root. Put settings in one direct helper and use a
second empty to define its direction:

```text
arx_fog__cellar_mist
+-- SETTINGS__RGB_0.32_0.4_0.48__SIZE_80__SCALE_1.2__SPEED_0.25__LIFETIME_6000__mist_settings
`-- DIRECTION__forward
```

Place `DIRECTION__forward` away from the root along the intended emission
direction. Its distance is irrelevant. The final helper components are labels.
The fog root's local scale must be identity; import discards any other local
scale with a warning. Use the named `SIZE` and `SCALE` settings for fog size.

[Exact fog syntax](LEVEL_REFERENCE.md#fog)

## Zones

A zone is a closed or open mesh root. Its name becomes game data and its
optional ordinal controls order. Direct helpers carry depth-fog and Ambiance
settings.

For a purple `cellar_attack` zone with ordinal 8 and Ambiance `my_ambiance`:

```text
arx_zone__008__cellar_attack
+-- SETTINGS__RGB_0.63_0.1_0.9__FARCLIP_21.7__depth_fog
`-- AMBIANCE_ambiance:my_ambiance__zone_sound
```

The ordinal may have gaps. `cellar_attack` is the zone name. `depth_fog` and
`zone_sound` are labels. At the default scale, `FARCLIP_21.7` means 2170 Arx
units. Author the zone's top and bottom as planes. Import flattens deviations
to the reconstructed planes with a warning. Zone materials carry no data, so
no special authored material is needed. The `ambiance:` selector refers to an
Ambiance resource below `sfx/ambiance`.

[Exact zone syntax](LEVEL_REFERENCE.md#zone)

## Spline Paths

A spline path is an empty root with one direct empty child per path point.
Child world positions define the path and ordinals define their order:

```text
arx_path__guard_patrol
+-- 000__BEZIER__TIME_0__start
+-- 008__STANDARD__TIME_0__curve_control
`-- 015__STANDARD__TIME_1500__end
```

Ordinal gaps are valid. `TIME` values are milliseconds. The first ordered node
must be authored with `TIME_0`; import resets any other value to zero. `BEZIER`
starts a curve through the next point to the following endpoint. The middle
point's `TIME` is ignored by that curve; the endpoint supplies its duration.

[Exact spline-path syntax](LEVEL_REFERENCE.md#spline-path)

## Minimap

An optional minimap is a flat rectangular mesh whose base-color image is
embedded in the GLB:

```text
arx_minimap__map
```

Place the sheet over the world X/Z area represented by the image. Its height
has no Level meaning. Keep it horizontal and preserve the corner UV layout so
the image top matches the sheet's maximum Z edge. Partial maps are valid; the
sheet does not need to cover the complete Level.

Loading screens are not authored in Level GLB. Place one beside the GLB as
`<name>[loading].png` when using the CLI.

[Exact minimap syntax](LEVEL_REFERENCE.md#minimap)

## Generated Data

Level GLB does not store native cell slicing or room-distance records. Anchor
connections use the best-effort opaque metadata described above. Room mesh
`COLOR_0` stores baked corner lighting; missing colors use neutral gray. Native
baking rebuilds cell layout and compatible quads.

Generate navigation, anchors, anchor connections, room distances, or static
lighting only when the Level needs them. See [Level Operations](../CLI.md#level-operations)
for commands and operation order.

## Export Checklist

- Export the intended collection as GLB.
- Preserve semantic object and material names.
- Preserve hierarchy and object transforms.
- Export UVs, vertex colors, and punctual lights where used.
- Embed the minimap base-color image where used.
- Do not flatten semantic helper nodes into meshes.
- Keep semantic names unique; place DCC suffixes in labels.
- Use the same explicit scale and offset for an exact inverse conversion.

[Back to the Authoring Guide](../AUTHORING_GUIDE.md)
