# CLI Usage

The `arx-pistor` executable is a multi-format tool for inspecting, transforming, and converting Arx Fatalis 3D assets.

## Basic Usage

```bash
arx-pistor <input> [extras...] [output]
```

The tool automatically detects the input format and determines the action based on the file extension of the output.

### Inspecting Files
If no output is provided, the tool prints a summary of the file contents.
```bash
arx-pistor model.ftl
arx-pistor anim.tea
```

### Execution Modes
The tool is context-aware and switches modes based on the primary input:

*   **Model Mode**: Triggered when the first input is a model (`.ftl`, `*.obj`, `*.glb`). Used for geometry and skeletal tasks.
*   **Animation Mode**: Triggered when the first input is an animation (`.tea`, `*.json`). Used for standalone animation tasks.

## Format Interoperability

`arx-pistor` acts as a bridge between Arx Fatalis native formats and modern interchange formats. Most mappings are bidirectional.

### FTL (Models)
| Format | Bidirectional | Notes |
| :--- | :---: | :--- |
| **JSON** | Yes | Full structural data dump and re-import. |
| **GLB** | Yes | Preserves geometry, skinning, and materials. |
| **OBJ** | Partial | Static geometry and materials only. Skeletal data is dropped. |

### TEA (Animations)
| Format | Bidirectional | Notes |
| :--- | :---: | :--- |
| **JSON** | Yes | Full animation data dump and re-import. |
| **GLB** | Yes* | **Requires an FTL** input to provide the skeletal structure. |

### Bundle Logic
The tool effectively handles file bundles. For example:
*   **Export**: `arx-pistor model.ftl anim1.tea anim2.tea out.glb` — Creates one GLB with a mesh, skeleton, and two animation tracks.
*   **JSON export**: `arx-pistor model.ftl anim1.tea out.json` — Writes the model JSON plus sibling animation JSON files.
*   **Import**: `arx-pistor in.glb out.ftl` — Generates `out.ftl` for the mesh and sibling `*.tea` files for each track.
    *   Sibling files are named using the animation name (sanitized with `_` for filesystem safety), e.g., `out_Run_Cycle.tea`.

Animations bundled with a model must have `num_groups` equal to the FTL group count.

## Advanced Usage

### Flexible Extras
In **Model Mode**, you can provide additional animation files after the primary model. These "extras" are appended to the bundle. The tool supports mixing native and modern formats:
```bash
# Mix TEA and JSON animations into a single GLB
arx-pistor model.ftl anim1.tea anim2.json out.glb
```

### GLB as a Model Source
You can use a GLB file as the primary model input while providing external animations. This is useful for appending new animations to an existing GLB mesh or converting a GLB to FTL while adding extra tracks:
```bash
# Use GLB geometry/skeleton but append a new TEA animation
arx-pistor mesh.glb extra_anim.tea out.glb
```

### Renaming Selections
Some DCC tools rewrite GLB selection VEC4 attributes as positional names such as `COLOR_0`, or change the case of custom attribute names. Use `--rename-selections` to rename FTL selections by their current order after import:

```bash
# Rename the first selection to chest, skip the second, rename the third and fourth
arx-pistor mesh.glb out.ftl --rename-selections "chest,,leggings,head"
```

The comma-separated list is positional. Empty fields skip a selection, so the names in the example are arbitrary and only demonstrate the first, third, and fourth positions. Real models may need different names or a different order. The list may be shorter than the selection count; trailing selections not mentioned in the list are left unchanged. If the list is longer than the selection count, the command fails.

### Reference FTL Repair
Arx Fatalis can require exact metadata compatibility between merged models, especially for humans and equipped armor. Use a real base-game FTL as a reference when converting an edited GLB back to FTL:

```bash
arx-pistor edited.glb fixed.ftl \
  --rename-selections ",,CUT_Lleg,CUT_Rleg" \
  --reference-ftl human_base.ftl \
  --autosize-to-reference \
  --snap-bone-origins-to-reference snap-origins \
  --snap-action-points-to-reference \
  --copy-reference-affiliations
```

The `--rename-selections` value above is only an example: it skips the first two imported selections and renames the next two. Real models may need different names or a different order. Reference operations run after `--rename-selections`, so repaired selection names are used for reference matching.

* `--autosize-to-reference` fits a uniform scale and translation from reference landmarks.
* `--snap-bone-origins-to-reference snap-origins` copies exact reference bone origin positions and leaves the mesh unchanged.
* `--snap-bone-origins-to-reference delta-deform` moves each mesh region by its owning bone-origin correction, then copies exact reference bone origin positions.
* `--snap-bone-origins-to-reference hierarchy-deform [N]` is experimental hierarchy-aware deformation. It can severely distort models whose proportions differ from the reference, even when group topology matches. The optional `N` stops after `N` hierarchy edges for debugging partial results.
* `--snap-action-points-to-reference` copies exact reference action point positions by action name.
* `--copy-reference-affiliations` copies reference selection membership for synthetic vertices such as header origin, bone origins, and action points.

## Transformation Flags

Apply coordinate transformations during conversion or rewriting.

```bash
# Scale a model by 2.0 and rotate 90 degrees on Y
arx-pistor model.ftl out.ftl --scale 2.0 --rotate 0 90 0

# Offset an animation's root motion
arx-pistor anim.tea out.tea --offset 100 0 0
```

## Options

| Option | Description |
| :--- | :--- |
| `--pretty` | Pretty-print JSON outputs. |
| `--overwrite` | Overwrite existing files without prompting. |
| `--no-overwrite` | Skip existing files without prompting. |
| `--rotate RX RY RZ` | Apply Euler rotation (degrees, XYZ order). |
| `--scale S` | Apply uniform scale. |
| `--scale SX SY SZ` | Apply per-axis scale. |
| `--offset OX OY OZ` | Apply translation offset. |
| `--overwrite-texture P` | Replace all texture paths in an FTL with the single path `P`. |
| `--rename-selections CSV` | Rename FTL selections by position. Empty CSV fields skip; extra fields are an error. |
| `--reference-ftl P` | Load base FTL `P` for reference repair operations. |
| `--autosize-to-reference` | Fit target model to the reference with uniform scale and translation. Requires `--reference-ftl`. |
| `--snap-bone-origins-to-reference snap-origins` | Copy exact reference bone origin positions. Requires `--reference-ftl`. |
| `--snap-bone-origins-to-reference delta-deform` | Move mesh regions by bone-origin deltas, then copy exact reference origins. Requires `--reference-ftl`. |
| `--snap-bone-origins-to-reference hierarchy-deform [N]` | Experimental hierarchy-aware deformation. May severely distort models with different proportions; use mainly for debugging/research. Optional `N` stops after `N` hierarchy edges. Requires `--reference-ftl`. |
| `--snap-action-points-to-reference` | Copy exact reference action point positions by name. Requires `--reference-ftl`. |
| `--copy-reference-affiliations` | Copy reference selection membership for synthetic vertices. Requires `--reference-ftl`. |
