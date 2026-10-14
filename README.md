# fez_model_generator

Converts an XML/PNG export of the FEZ game assets into Wavefront OBJ models (`.obj` + `.mtl` + textures):
art objects, trile sets and complete levels (triles, art objects, background planes and NPCs).

The project is a C++26 command-line program for Windows, built with CMake, Visual Studio 2026, Qt 6 and
dependencies from vcpkg. It consists of the static library `fez_core` (`src/core`), the executable
`fez_model_generator` (`src/cli`) and the Catch2 test program `fez_tests` (`tests`).

## Requirements

- Windows x64 with **Visual Studio 2026** (version 18, workload "Desktop development with C++").
- **CMake** with the `Visual Studio 18 2026` generator (at least 3.25 by `cmake_minimum_required`; tested with 4.4.2).
- **Qt 6** for MSVC x64 (tested with 6.11.2, `msvc2022_64`), components Core, Gui, Xml and Concurrent.
- **vcpkg** (classic mode) providing assimp, Eigen 3 and Catch2 3.

## One-time setup

### 1. vcpkg

Any vcpkg root works; it does not need to be on `PATH`, and no `VCPKG_ROOT` environment variable is needed.

```bat
git clone https://github.com/microsoft/vcpkg C:\path\to\vcpkg
C:\path\to\vcpkg\bootstrap-vcpkg.bat
C:\path\to\vcpkg\vcpkg install assimp eigen3 catch2 --triplet x64-windows
```

The project uses classic mode (`VCPKG_MANIFEST_MODE=OFF`, there is no `vcpkg.json`) and triplet `x64-windows`.
CMake finds vcpkg through the cache variable **`VCPKG_FEZ_MODEL_BASE`** (the vcpkg root): before `project()`,
`CMakeLists.txt` checks that `<VCPKG_FEZ_MODEL_BASE>/scripts/buildsystems/vcpkg.cmake` exists and uses it as the
toolchain file. Configuration stops with an error if the variable is unset or wrong.

### 2. CMakeUserPresets.json

`CMakePresets.json` (committed) only contains **hidden** base presets (`windows-vs-base`, `windows-debug-base`, ...).
The visible presets live in your own, gitignored `CMakeUserPresets.json`. Copy the **whole** template and edit the
paths:

```bat
copy CMakeUserPresets.example.json CMakeUserPresets.json
```

| Entry | Where | Meaning |
|---|---|---|
| `VCPKG_FEZ_MODEL_BASE` | `windows-vs` cache variables | vcpkg root from step 1 (required) |
| `CMAKE_PREFIX_PATH` | `windows-vs` cache variables | Qt install, e.g. `C:/Qt/6.11.2/msvc2022_64` (required) |
| `FEZ_DATA_DIR`, `FEZ_GOLDEN_DIR` | `windows-vs` cache variables and the `environment` of every test preset | golden test input and reference (optional, see [Golden test](#golden-test)) |
| `FEZ_DEBUG_ARGS` | `windows-vs` cache variables | command-line arguments for F5 in Visual Studio (optional) |

The template defines the configure preset `windows-vs`, the build presets `windows-debug` / `windows-release` and the
test presets `windows-debug`, `windows-release`, `windows-debug-golden` and `windows-release-golden`.

Without presets, pass the same values on the command line, e.g.
`cmake -S . -B build/windows-vs -G "Visual Studio 18 2026" -A x64 -DVCPKG_FEZ_MODEL_BASE=C:/path/to/vcpkg
-DVCPKG_MANIFEST_MODE=OFF -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64`.

## Build

```bat
cmake --preset windows-vs
cmake --build --preset windows-debug
cmake --build --preset windows-release
```

`windows-vs` is one multi-config Visual Studio build tree in `build/windows-vs` (solution
`build/windows-vs/FezModelGenerator.slnx`). Binaries land in `build/windows-vs/bin/<Config>/`. The build copies the
Qt, assimp and other vcpkg DLLs the converter needs next to it, so `bin/<Config>/fez_model_generator.exe` runs without
Qt or vcpkg on `PATH`. Nothing is written into the source tree.

## Tests

```bat
ctest --preset windows-debug
ctest --preset windows-release
```

runs all unit and end-to-end tests (Catch2, about 200 test cases, a few seconds). Tests find Qt and vcpkg DLLs on
their own; they do not need either on `PATH`. A subset by Catch2 tag:

```bat
build\windows-vs\bin\Debug\fez_tests.exe "[pipeline]"
```

The end-to-end tests convert the small hand-written fixture `tests/data/mini_export/` (always from a temporary copy)
and compare the result byte for byte with `tests/data/mini_expected/`.

### Golden test

The golden regression test converts a complete FEZ export and compares every output file byte for byte with a
reference tree. It is excluded from the default test presets and has its own:

```bat
ctest --preset windows-release-golden   & rem about 10 minutes
ctest --preset windows-debug-golden     & rem about 1 hour
```

Setup:

1. `FEZ_DATA_DIR`: a **copy** of the FEZ export (folders `art objects`, `background planes`, `character animations`,
   `levels`, `trile sets`). The test fails if the converter changes anything in this tree.
2. `FEZ_GOLDEN_DIR`: the reference output (`ao_export/`, `ts_export/`, `lv_export/`), produced by a trusted build with
   `fez_model_generator --input <FEZ_DATA_DIR> --output <fresh, empty dir>`. References are versioned outside the
   repository (e.g. `.../golden/v1`, `v2`, ...); every deliberate output change produces a new version and
   `FEZ_GOLDEN_DIR` points at the newest.
3. Put both paths into the `environment` of the test presets in `CMakeUserPresets.json` (and the `windows-vs` cache
   variables), then re-run `cmake --preset windows-vs`.

If either variable is unset, the golden test reports "Skipped".

## Usage

```bat
fez_model_generator --input <FEZ export> --output <output dir> [--only ao,ts,lv] [--jobs N] [--verbose]
```

| Option | Meaning |
|---|---|
| `--input <dir>` | FEZ export root (contains `art objects`, `trile sets`, `levels`, ...). Required; only read. |
| `--output <dir>` | Output root; receives `ao_export/`, `ts_export/` and `lv_export/`. Defaults to `--input`. |
| `--only <kinds>` | Comma-separated subset of `ao` (art objects), `ts` (trile sets), `lv` (levels). Default: all. |
| `--jobs <N>` | Number of parallel conversions (positive). Default: number of logical processors. |
| `--verbose` | Also print debug messages (`parsing: ...`, `Write: ...`). |
| `--help` | Print the help text. |

Exit codes: **0** all assets converted, **1** at least one asset failed, **2** usage error (missing or invalid
`--input`, invalid `--only` or `--jobs`, unknown options; the error and the help text go to stderr).

Without `--verbose` a run prints only a summary to stderr: one line per converted kind
(`Levels: 156 of 156 converted, 0 failed`) and, if anything failed, each failed file with its errors.

Existing output files (OBJ, MTL and textures) are replaced, so a run into an existing output directory gives the same
files as a run into a fresh one. Files that the new run does not write (for example of an asset removed from the
input) are not deleted. A texture that cannot be copied is reported as an `Io` error of its asset (exit code 1).

## Install

```bat
cmake --install build/windows-vs --config Release --prefix C:\path\to\fez_model_generator
```

produces a self-contained folder: `bin/` holds `fez_model_generator.exe`, the Qt runtime (`Qt6Core`, `Qt6Gui`,
`Qt6Xml`, `Qt6Concurrent` and `qt.conf`, deployed by Qt's `qt_generate_deploy_app_script`; no Qt plugins, since the
converter is a console application and reads only PNG, which is built into `Qt6Gui`), assimp and its vcpkg
dependencies, and the MSVC runtime. It runs without Qt, vcpkg or Visual Studio on `PATH`.

## Blender helper

`tools/blender/all_texture_sample_closest.py` sets the interpolation of every material's image texture node to
`Closest`, so the low-resolution FEZ textures stay sharp instead of being blurred. Import an OBJ in Blender
(File > Import > Wavefront (.obj)), then open the file in the Scripting workspace and run it.

## Repository layout

```
CMakeLists.txt, CMakePresets.json, CMakeUserPresets.example.json
cmake/                 compiler warnings
src/core/              fez_core: parsers, model, level resolution, OBJ export, pipeline, logging
src/cli/               fez_model_generator: options, summary, main
tests/                 fez_tests: unit/, e2e/, golden/, support/, data/ (fixtures)
tools/blender/         Blender helper script
docs/                  design spec and implementation plan
```

## License

MIT, see [LICENSE](LICENSE).
