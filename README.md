# arx-pistoris

A C++20 library for parsing and converting Arx Fatalis 3D file formats (FTL, TEA, FTS, DLF, LLF)
into modern formats (OBJ, GLTF, JSON). Exposes a stable C API wrapping a C++ implementation,
with no external dependencies.

## License

This project is licensed under the GNU General Public License v3.0 with
additional terms — see [LICENSE](LICENSE) and [ADDITIONAL_TERMS](ADDITIONAL_TERMS).

This is not the original Arx Fatalis program. This is an independent,
derived work that parses Arx Fatalis file formats.

## Status

| Source format | OBJ  | GLB(GLTF) | JSON |
|---------------|------|------|------|
| FTL           | done | done | done |
| TEA           | x    | done (with FTL) | done |
| FTS           | —    | —    | —    |
| DLF           | —    | —    | —    |
| LLF           | —    | —    | —    |

TEA & FTL are fully done

## Documentation

- **[Building from Source](docs/BUILD.md)** — Requirements and compilation steps.
- **[CLI Usage](docs/CLI.md)** — Comprehensive guide for the `pistoris` command-line tool.
- **[API Integration](docs/API.md)** — Guide for integrating the C or C++ library into your project.
- **[Authoring Guide](docs/AUTHORING_GUIDE.md)** — Naming conventions for 3D tools (Blender, Maya).
- **[Fidelity & Limitations](docs/LIMITATIONS.md)** — Technical details on conversion trade-offs.
- **[Testing & Fuzzing](docs/TESTING.md)** — Quality assurance and development workflows.

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

Test models used in this project are credited in [Attribution](data/Attribution.md).

Arx Fatalis is a trademark of ZeniMax Media Inc. This project is not
affiliated with or endorsed by Arkane Studios or ZeniMax Media Inc.
