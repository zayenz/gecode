# CMake Build and Package Guide

This document describes the CMake build, install, and package-consumption flow
for Gecode.

## Requirements

- CMake 3.21 or newer
- A C++17-capable compiler
- `uv` on `PATH` (always required for build scripts; required for
  `GECODE_REGENERATE_VARIMP=ON`)
- Optional dependencies based on enabled modules:
  - MPFR for float support with MPFR
  - Qt5/Qt6 for Gist

## Quick Start

Single-config generators (Unix Makefiles / Ninja):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --build build --target check
cmake --install build --prefix /path/to/install
```

Multi-config generators (Visual Studio / Xcode):

```bash
cmake -S . -B build
cmake --build build --config Release
cmake --build build --config Release --target check
cmake --install build --config Release --prefix /path/to/install
```

## Visual Studio + vcpkg (MPFR)

This repository includes a vcpkg manifest (`vcpkg.json`) and CMake presets
for a ready-to-run Visual Studio path with MPFR enabled.

Prerequisites:

- Visual Studio 2022 with C++ toolchain
- CMake 3.21 or newer
- `VCPKG_ROOT` set to your vcpkg checkout

Preset flow:

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset vs2022-vcpkg-release
cmake --build --preset vs2022-vcpkg-check
```

Equivalent command-line flow (without presets):

```powershell
cmake -S . -B build/vs2022-vcpkg -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DGECODE_ENABLE_MPFR=ON -DGECODE_ENABLE_QT=OFF -DGECODE_ENABLE_GIST=OFF
cmake --build build/vs2022-vcpkg --config Release
cmake --build build/vs2022-vcpkg --config Release --target check
cmake --install build/vs2022-vcpkg --config Release --prefix C:/path/to/install
```

Visual Studio is a multi-config generator, so use `--config Release` (or
`Debug`) for build/install/check commands rather than `CMAKE_BUILD_TYPE`.

## Build Conventions and Key Options

### Common CMake options

- `BUILD_SHARED_LIBS`: conventional default selector
  - `ON` default behavior: `GECODE_BUILD_SHARED=ON`, `GECODE_BUILD_STATIC=OFF`
  - `OFF` default behavior: `GECODE_BUILD_SHARED=OFF`, `GECODE_BUILD_STATIC=ON`
- `BUILD_TESTING`: controls whether `gecode-test` and `check` are built
- `CMAKE_BUILD_TYPE` / multi-config `--config`: standard build mode control

### Gecode-specific options

- `GECODE_BUILD_SHARED`, `GECODE_BUILD_STATIC`: explicit library variants
  - If set directly, these override `BUILD_SHARED_LIBS`-based defaults.
- `GECODE_INSTALL`: enables install/export/package rules
  - Defaults to `ON` when Gecode is top-level; defaults to `OFF` as subproject.
- `GECODE_ENABLE_*`: module toggles (search/int/set/float/minimodel/driver/flatzinc/gist, etc.)
- `GECODE_REGENERATE_VARIMP`: regenerate checked-in var-imp headers during build

Top-level defaults:

- `GECODE_ENABLE_EXAMPLES=ON`
- `BUILD_TESTING=ON`
- `GECODE_INSTALL=ON`

Subproject defaults (`add_subdirectory`):

- `GECODE_ENABLE_EXAMPLES=OFF`
- `BUILD_TESTING=OFF` (unless enabled by parent project)
- `GECODE_INSTALL=OFF`

## Using Gecode From Other CMake Projects

### Recommended (compatibility aggregate target)

```cmake
find_package(Gecode CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE Gecode::gecode)
```

### Component targets

Canonical component names:

- `support`, `kernel`, `search`, `int`, `set`, `float`, `minimodel`, `driver`,
  `flatzinc`, `gist`

Example:

```cmake
find_package(Gecode CONFIG REQUIRED COMPONENTS driver)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE Gecode::gecodedriver)
```

Legacy component spellings are also accepted in `COMPONENTS`:

- `gecodesupport`, `gecodekernel`, `gecodesearch`, `gecodeint`, `gecodeset`,
  `gecodefloat`, `gecodeminimodel`, `gecodedriver`, `gecodeflatzinc`,
  `gecodegist`

### Package location hints

Use one of:

- `-DCMAKE_PREFIX_PATH=/path/to/install`
- `-DGecode_ROOT=/path/to/install`

### Version checks

Use `Gecode_VERSION` from package config:

```cmake
find_package(Gecode CONFIG REQUIRED)
message(STATUS "Gecode version: ${Gecode_VERSION}")
```

Do not rely on parsing installed headers such as `config.hpp` for version checks.

## Migration Notes

### Target migration

| Old usage | Preferred modern usage |
|---|---|
| `Gecode::gecode` | `Gecode::gecode` (compatibility target is exported) |
| Implicit monolithic linkage assumptions | Explicit component targets when needed (`Gecode::gecodedriver`, etc.) |

### Deprecated cache variable aliases

| Deprecated | Replacement |
|---|---|
| `ENABLE_THREADS` | `GECODE_ENABLE_THREAD` |
| `ENABLE_GIST` | `GECODE_ENABLE_GIST` |
| `BUILD_EXAMPLES` | `GECODE_ENABLE_EXAMPLES` |
| `ENABLE_CPPROFILER` | `GECODE_ENABLE_CPPROFILER` |

Deprecation horizon:

- These aliases are accepted for the 6.x line to preserve migration compatibility.
- New configurations should use the `GECODE_*` names.

## Troubleshooting

- `Target "..." links to Gecode::gecode but the target was not found`:
  - Ensure you are using an installation built with this CMake export layout.
  - Check `CMAKE_PREFIX_PATH` / `Gecode_ROOT` points to the intended install.
- `find_package(Gecode COMPONENTS ...)` fails:
  - Verify requested component is enabled in the installed build.
  - For optional modules (`flatzinc`, `gist`, `float` with MPFR), ensure dependencies were available.
- Qt/Gist issues:
  - `GECODE_ENABLE_GIST=ON` requires Qt discovery; if Qt is absent, Gist is disabled.
- MPFR issues:
  - Use `CMAKE_PREFIX_PATH`, `MPFR_ROOT`, or toolchain include/link paths so `find_package(MPFR)` succeeds.
