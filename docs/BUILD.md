# Building

The full Pistoris project, including the CLI, builds on Windows and Linux. The
core library and Python package also build on macOS 14 or newer on ARM64.

## Requirements

- CMake 3.26 or newer
- Clang with C++20 support
- Ninja
- [just](https://github.com/casey/just) for the documented shortcuts
- Python 3.12 or newer, with matching CPython development headers, when
  building the Python package

The same presets are used on Windows and Linux.

Python source builds select Clang automatically on Windows and Linux. For a
direct Linux CMake build outside the presets, select it before configuration.
Disable the CLI for a Python-only build; this is also required on macOS:

```text
CC=clang CXX=clang++ cmake -S . -B build-python -G Ninja \
  -DARX_BUILD_CLI=OFF \
  -DARX_BUILD_PYTHON_BINDINGS=ON \
  -DBUILD_TESTING=OFF
```

A virtual environment supplies Python packages, not the CPython headers and
link metadata needed to compile an extension. Install the development package
matching the interpreter selected for the build (for example, the appropriate
`python3-devel` or versioned equivalent on Linux).

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
just python        # rebuild the Python bindings and open a development shell
just python-check  # build, install, and test the Python wheel in isolation
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

## Building the Python Package

Build the current bindings and open an isolated interpreter:

```text
just python
```

The first run creates `.venv/`. Every run incrementally rebuilds the bindings
below `build-python/` before starting that environment's interpreter. This is
the development path; `just python-check` verifies the installed wheel in a
fresh environment.

Build a wheel from the repository root:

```text
python -m pip wheel .
```

The wheel uses CPython's 3.12 stable ABI. Release wheels are built separately
for Windows x64, Windows ARM64, macOS ARM64, and manylinux x64, then tested
with CPython 3.12, 3.13, and 3.14 on the same platform.

For a non-interactive in-tree development build:

```text
cmake --preset python -DPython_EXECUTABLE=<python>
cmake --build --preset python --target pistoris_python_package
```

Set `PYTHONPATH` to `build-python/python` when importing that development
build. The wheel contains the private extension, generated type stubs, and the
public `pistoris` package.

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
