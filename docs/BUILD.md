# Building

This guide is for contributors and source users building Pistoris on Windows
or Linux.

## Requirements

- CMake 3.25 or newer
- Clang with C++20 support
- Ninja
- [just](https://github.com/casey/just) for the documented shortcuts

The same presets are used on Windows and Linux.

## Development Build

Configure, build, and run the development test suite:

```text
just dev
```

For separate steps:

```text
just configure dev
just build
just test
```

Build only one logical target:

```text
just build lib
just build cli
just build unit
just build api
just build corpus
just build tests
```

The `build` recipe takes the logical target first and the preset second. For
example, `just build cli release` builds only the CLI using the release preset.

Equivalent direct CMake commands are:

```text
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Development output is written below `build/`:

| Artifact | Windows | Linux |
| --- | --- | --- |
| CLI | `build/bin/arx-pistor.exe` | `build/bin/arx-pistor` |
| C ABI shared library | `build/bin/arx_pistoris.dll` | `build/bin/libarx_pistoris.so` |
| C++ static library | `build/lib/arx_pistoris_cpp.lib` | `build/lib/libarx_pistoris_cpp.a` |

`arx_pistoris` is the shared C ABI library. `arx_pistoris_cpp` is the static
C++ library.

## Other Presets

```text
just release     # configure and build build-release/
just sanitize    # configure, build, and test build-sanitize/
just tidy        # apply available clang-tidy fixes through build-tidy/
just tidy-check  # enforced clang-tidy build in build-tidy-check/
just fuzz-build  # libFuzzer build in build-fuzz/
just package-smoke  # install and verify the release CLI package
just pre-release    # complete local release gate
```

Formatting and coverage are documented in
[Testing and Fuzzing](TESTING.md).

## Linking the Library

For C++20 code:

```cmake
target_link_libraries(my_target PRIVATE arx_pistoris_cpp)
```

Include `arx_pistoris/pistoris.hpp` or a focused `.hpp` header.

For C or another language using the C ABI:

```cmake
target_link_libraries(my_target PRIVATE arx_pistoris_c)
```

Include `arx_pistoris/arx_pistoris.h` or focused `.h` headers. The produced
shared library is named `arx_pistoris`.

The current install target is CLI-oriented and does not install development
headers or CMake library targets. Library consumers should use the source tree
or add it as a CMake subdirectory for this pre-1.0 release.

## Installing the CLI

Build and install from source:

```text
cmake --preset release
cmake --build --preset release
cmake --install build-release --prefix <install-prefix>
```

The executable is installed below `<install-prefix>/bin`. Put that directory
on `PATH`.

Release installers:

```powershell
irm https://raw.githubusercontent.com/Merxtef/arx-pistoris/main/scripts/install.ps1 | iex
```

```bash
curl -fsSL https://raw.githubusercontent.com/Merxtef/arx-pistoris/main/scripts/install.sh | sh
```

Default locations:

- Windows: `%LOCALAPPDATA%\arx-pistoris\bin`
- Linux: `$HOME/.local/bin`

Open a new terminal if the installer updated `PATH`.
