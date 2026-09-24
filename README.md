# arx-pistoris

Pistoris is a C++20 library and command-line converter for Arx Fatalis
resources. The library reads and writes the native FTL, TEA, FTS, DLF, LLF,
AMB, and CIN formats. It exposes editable resources through GLB, supports OBJ
for static Models, and provides arx-convert-compatible JSON for native carrier
interchange.

The editing API is organized around five resource classes: Level, Model,
Animation, Ambiance, and Cinematic. Level owns geometry, rooms, portals,
navigation, lighting, and scene objects as one asset. Model owns FTL geometry,
skeleton, action points, and generic named selections. Animation owns one
TEA-compatible timeline and its dense per-bone transforms. Ambiance owns one
logical AMB resource and its semantic audio tracks. Cinematic owns one
CIN-compatible illustration timeline with effects and localized speech
references.

The library has a C++20 API and a C ABI for all five editing classes and
supported native carriers. The CLI owns filesystem discovery, mounts, sidecars,
overwrite policy, and game-resource placement. The library itself operates on
memory buffers and logical resource paths. Third-party source dependencies are
vendored, and produced binaries have no third-party runtime dependencies.

## Supported Formats

| Editing class | Native carriers | GLB | OBJ | arx-convert JSON |
| --- | --- | :---: | :---: | :---: |
| Level | FTS + DLF + LLF | bidirectional | - | FTS, DLF, LLF |
| Model | FTL | bidirectional | static, bidirectional | FTL |
| Animation | TEA | through Model GLB | - | TEA |
| Ambiance | AMB | bidirectional | - | AMB |
| Cinematic | CIN | bidirectional | - | - |

All listed native carriers are readable and writable. GLB covers the complete
editing surface: Level, Model, Ambiance, and Cinematic support round trips and
from-scratch authoring in ordinary DCC tools, while Animation is authored in
Model GLB alongside the skeleton it targets. OBJ deliberately supports static
Models only. JSON follows arx-convert native-carrier schemas and is not a
separate editing model; no compatible CIN schema exists.

FTS, DLF, and LLF combine into one Level. FTS is mandatory when constructing a
Level; LLF and DLF are optional inputs in the loose-file workflow. Game-layout
DLF input discovers its mandatory FTS and optional LLF through mounted
resources. Native and compatible JSON carriers can also be converted directly
without constructing an editing class.

FTL, FTS, DLF, and LLF readers accept raw and PKWARE DCL-compressed data.
Their writers use game-compatible compression by default and can emit raw data
when requested. TEA, AMB, and CIN remain uncompressed.

The public interfaces are still pre-1.0. Source and ABI compatibility are not
promised between minor releases.

## Quick Start

Build the CLI and export the installed Level 1 through the platform's standard
game folders:

```text
just configure dev
just build cli
build/bin/arx-pistor --auto-mount level:1 level1.glb
```

Loose native files can instead be supplied directly:

```text
arx-pistor level.fts level.llf level.dlf level.glb
```

See `arx-pistor --help` for the installed option set and defaults.

## Documentation

- **[CLI Guide](docs/CLI.md)** - conversion, mounts, selectors, and output
  layouts.
- **[Authoring Guide](docs/AUTHORING_GUIDE.md)** - practical Level, Model,
  Animation, Ambiance, and Cinematic authoring guides.
- **[Authoring Reference](docs/AUTHORING_REFERENCE.md)** - exact authoring
  names, structures, flags, and defaults for Level, Model, Animation,
  Ambiance, and Cinematic.
- **[API Guide](docs/API.md)** - C++ and C integration.
- **[Fidelity and Limitations](docs/LIMITATIONS.md)** - deliberate losses and
  format constraints.
- **[Building](docs/BUILD.md)** - prerequisites, targets, and installation.
- **[Testing and Fuzzing](docs/TESTING.md)** -
  local quality workflows and optional game corpora.

## License

This project is distributed under the GNU General Public License version 3 or
later; see [LICENSE](LICENSE). Arx Fatalis-derived portions, including
modifications inherited through Arx Libertatis, retain their upstream notices
and are also subject to [ADDITIONAL_TERMS](ADDITIONAL_TERMS).

This is not the original Arx Fatalis program. This is an independent,
derived work that parses Arx Fatalis file formats.

Arx Fatalis is a trademark of ZeniMax Media Inc. This project is not
affiliated with or endorsed by Arkane Studios or ZeniMax Media Inc.

## Acknowledgments

This project was inspired by **Pedro Ordaz** and the **Arx Insanity** team.

Format parsing knowledge and data structure definitions in this project were
derived from the following GPL-licensed sources:

- **Arx Fatalis GPL Source Code**
  Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.
  Released under GPLv3 with additional terms.

- **[Arx Libertatis](https://arx-libertatis.org/)**
  Copyright (C) 2011-2023 Arx Libertatis Team and Contributors.
  A cross-platform port of Arx Fatalis, licensed under GPLv3+.

Additionally, the following project was used as a reference:

- **[arx-convert](https://github.com/arx-tools/arx-convert)**
  Copyright (C) arx-tools contributors.
  A TypeScript converter for Arx Fatalis formats, licensed under MIT.

Fixture assets used in this project are credited in
[Attribution](https://github.com/Merxtef/arx-pistoris/blob/main/data/Attribution.md).
