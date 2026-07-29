# arx-pistoris

Pistoris is a C++20 library and command-line converter for Arx Fatalis 3D
resources. It reads and writes the native FTL, TEA, FTS, DLF, and LLF formats
and exchanges editable data through OBJ, GLB, and arx-convert-compatible JSON.

Level is the current coherent editing class. It owns geometry, rooms, portals,
navigation, lighting, and scene objects while maintaining cross-module state.
Level GLB is a from-scratch authoring surface for ordinary DCC tools. FTL and
TEA currently use legacy direct conversion APIs; coherent Model and Animations
editing classes are not part of this release.

The library has a C++20 API and a matching C ABI for Level operations. The CLI
owns filesystem discovery, mounts, sidecars, overwrite policy, and game-resource
placement. The library itself operates on memory buffers and logical resource
paths. Third-party source dependencies are vendored, and produced binaries have
no third-party runtime dependencies.

## License

This project is distributed under the GNU General Public License version 3 or
later; see [LICENSE](LICENSE). Arx Fatalis-derived portions, including
modifications inherited through Arx Libertatis, retain their upstream notices
and are also subject to [ADDITIONAL_TERMS](ADDITIONAL_TERMS).

This is not the original Arx Fatalis program. This is an independent,
derived work that parses Arx Fatalis file formats.

Arx Fatalis is a trademark of ZeniMax Media Inc. This project is not
affiliated with or endorsed by Arkane Studios or ZeniMax Media Inc.

## Current Surface

| Native data | Native binary | OBJ | GLB | Compatible JSON |
| --- | :---: | :---: | :---: | :---: |
| FTL model | read/write | bidirectional, static | bidirectional, legacy | bidirectional |
| TEA animation | read/write | - | bidirectional with FTL, legacy | bidirectional |
| FTS geometry and rooms | read/write | - | bidirectional through Level | bidirectional |
| DLF scene objects | read/write | - | Level companion | bidirectional |
| LLF lighting | read/write | - | Level companion | bidirectional |

FTS, DLF, and LLF combine into one Level. FTS is mandatory; LLF and DLF are
optional inputs when using the loose-file workflow. Game-layout DLF input
discovers its mandatory FTS and optional LLF through mounted resources.

FTL, FTS, DLF, and LLF readers accept raw and PKWARE DCL-compressed data.
Their writers use game-compatible compression by default and can emit raw data
when requested. TEA remains uncompressed.

The public interfaces are still pre-1.0. Source and ABI compatibility are not
promised between minor releases.

## Quick Start

Build the CLI and convert a loose native Level bundle:

```text
just configure dev
just build cli
build/bin/arx-pistor level.fts level.llf level.dlf level.glb
```

Or address a game-layout Level through mounted resource roots:

```text
arx-pistor --mount "<user Arx directory>" --mount "<unpacked directory>" level:1 level1.glb
```

See `arx-pistor --help` for the installed option set and defaults.

## Documentation

- **[CLI Guide](docs/CLI.md)** - conversion, mounts, selectors, and output
  layouts.
- **[Authoring Guide](docs/AUTHORING_GUIDE.md)** - concrete OBJ and GLB
  authoring workflows.
- **[Authoring Reference](docs/AUTHORING_REFERENCE.md)** - exact semantic
  names, structures, flags, and defaults.
- **[API Guide](docs/API.md)** - C++ and C integration.
- **[Fidelity and Limitations](docs/LIMITATIONS.md)** - deliberate losses and
  format constraints.
- **[Building](docs/BUILD.md)** - prerequisites, targets, and installation.
- **[Testing and Fuzzing](https://github.com/Merxtef/arx-pistoris/blob/main/docs/TESTING.md)** -
  local quality workflows and optional game corpora.

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

Test models used in this project are credited in
[Attribution](https://github.com/Merxtef/arx-pistoris/blob/main/data/Attribution.md).
