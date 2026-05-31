# Authoring Guide

This guide provides the naming conventions and protocols required to author Arx Fatalis assets in modern 3D tools (Blender, Maya, etc.).

## GLB (GLTF)

GLB is the primary interchange format for `arx-pistoris`, supporting geometry, skinning, and animations.

### 1. Materials & Flags
Set Arx face flags by naming your materials with the following pattern:
`[STEM]__[FLAG1]__[FLAG2]__...`

*   **Example**: `goblin_body__NO_SHADOW__DOUBLESIDED`
*   **Important**: Never use a double underscore (`__`) in your base texture filename.

#### Full List of Supported Flags:
`NO_SHADOW`, `DOUBLESIDED`, `TRANS`, `WATER`, `GLOW`, `IGNORE`, `QUAD`, `TILED`, `METAL`, `HIDE`, `STONE`, `WOOD`, `GRAVEL`, `EARTH`, `NOCOL`, `LAVA`, `CLIMB`, `FALL`, `NOPATH`, `NODRAW`, `PRECISE_PATH`, `NO_CLIMB`, `ANGULAR`, `ANGULAR_IDX0`, `ANGULAR_IDX1`, `ANGULAR_IDX2`, `ANGULAR_IDX3`, `LATE_MIP`.

### 2. Action Points
Define engine attachment points by creating empty nodes (Nulls, Locators, or Plain Axes).
*   **Naming**: `arx_action__[NAME]`
*   **Example**: `arx_action__WEAPON_ATTACH`
*   **Parenting**: Parent the empty node to the **responsible bone**. During conversion, the action point is anchored to this bone and becomes part of its vertex group in the FTL.

### 3. Bone Order
GLB export prefixes bone names with their FTL group order so DCC tools can reorder their internal joint lists without losing TEA compatibility.

*   **Exported naming**: numeric order marker + `__` + name.
*   **Examples**: `000__root`, `003__chest`, `042__hand`.
*   **Import behavior**: The numeric prefix restores FTL/TEA bone order and is stripped from the imported FTL group name.

The exporter pads to at least three digits for readable sorting in DCC tools. This is not a 999-bone limit; larger ordinals expand as needed, e.g. `1000__extra_bone`. Import accepts any number of digits before `__`.

Keep these prefixes intact while editing. If every bone has a unique in-range prefix and parents still appear before children in that order, import uses the prefixes. If prefixes are missing, duplicated, out of range, or topologically invalid, import warns and falls back to glTF joint order/topology.

### 4. Vertex Selections
Define named vertex sets (e.g., for equipment hiding) using custom `VEC4` attributes.
*   **Naming**: `_[NAME]` (Uppercase, prefixed with single underscore).
*   **Example**: `_CHEST`
*   **Data Type**: Must be a `VEC4` attribute.
*   **Values**: `1.0` (White/Full) indicates membership; `0.0` (Black/Empty) indicates exclusion.

GLB export also writes the ordered `arx_selection_names` mesh extra so exact names can survive when the DCC tool preserves extras. If a tool such as Blender rewrites attributes to positional names like `COLOR_N` or loses name casing, the selection masks can still be recovered, but the names may need to be restored from the model's selection order during import. The required names and positions depend on the model.

### 5. Game-Compatible Replacement Models
When authoring a model that replaces or merges with original game assets, such as a body variant or an armor piece, treat the matching base-game FTL as part of the specification. Some game paths compare bone origin positions bit-exact, and merge/copy logic can depend on vertices belonging to the expected selections.

The most reliable approach is to keep the skeleton, bone origins, action points, and required selections compatible with the reference model from the beginning of authoring. If those details drift during DCC editing, use the reference model as the source of truth when repairing the imported FTL. See [Reference FTL Repair](CLI.md#reference-ftl-repair) for the CLI workflow.

### 6. Animations (TEA)
*   **Root Motion**: Animate the `SkelWrapper` node at the scene root to apply translation or rotation to the entity itself.
*   **Holder Frames (`__h`)**: If an animation is intended to loop, add the `__h` suffix to the animation track name.
    *   **Behavior**: The library treats the last keyframe as a "duration marker." On import, this frame and the `__h` suffix are removed, but the animation duration remains stable.
    *   **Example**: `run__h`

---

## OBJ

OBJ is supported for static geometry only. Skeletal data and animations are ignored.

### 1. Materials & Flags
Naming conventions for OBJ materials are identical to GLB:
`[STEM]__[FLAG1]__[FLAG2]__...`

### 2. Texture Path Discovery
When importing an OBJ, the library resolves the texture filename for each material using the following priority:
1.  **Arx Path**: The value of a `# arx_path` comment in the MTL file.
2.  **Texture Path**: The standard `map_Kd` path defined in the MTL.
3.  **Stem**: The decoded stem from the material name (fallback if no MTL is found).

---

## JSON

The JSON format is a structural data dump intended for high-fidelity tool interoperability.

*   **Compatibility**: Designed to be compatible with [arx-convert](https://github.com/arx-tools/arx-convert), facilitating data exchange with the TypeScript ecosystem.
