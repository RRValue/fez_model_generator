# FEZ Model Generator – Modernization Design

- **Date:** 2026-10-12
- **Branch:** `feature/fez_mode-777-modernize-and-refactor-current`
- **Status:** Approved design, pending implementation plan

## 1. What the program does today

The tool converts assets of the game **FEZ** (already unpacked to XML + PNG) into Wavefront OBJ/MTL files
with copied textures, for use in Blender.

Flow (`source/main.cpp`, `source/Application.cpp`):

1. A `QApplication` opens a folder dialog — the only UI.
2. Three phases run sequentially; inside each phase files are processed in parallel via `QtConcurrent::run`:
   - `art objects/*.xml` → one mesh each → `<dir>/ao_export/<name>.obj`
   - `trile sets/*.xml` → one OBJ per trile → `<dir>/ts_export/<set>/<key>_<name>.obj`
   - `levels/*.xml` → one OBJ per level → `<dir>/lv_export/<level>/<level>.obj`, containing
     - trile instances (4 orientations, rotation around Y),
     - art object instances (position − 0.5, quaternion, scale),
     - background planes (quad sized from texture at 16 px/unit, first frame for animated textures,
       pushed out by 0.05 along +Z, opacity texture if the PNG has translucent pixels),
     - NPCs (quad textured with the first frame of the first action's animation).
3. `all_texture_sample_closest.py` is a Blender helper that sets texture interpolation to *Closest*.

Geometry decoding details that must be preserved:

- `Normal` in `VertexPositionNormalTextureInstance` is a face index 0–5 mapped to −X, −Y, −Z, +X, +Y, +Z.
- Texture V is flipped (`1 - y`) and U is offset by the face index (`u + side`).
- Triangle winding is swapped on export (`i0, i2, i1`).

Layers today: `model/` (Eigen-based structs), `parser/` (`QDomDocument`), `writer/` (hand-built `aiScene`,
assimp OBJ exporter).

### Known problems

| Area | Problem |
|---|---|
| Build | `MESSAGE(FATAL, …)` does not abort; deps from `RVDep` env var with hard-coded `vc143` lib names; `FILE(GLOB_RECURSE)`; output into `bin/` in the source tree; `QtConcurrent` used but `Qt6::Concurrent` not linked; C++20. |
| Structure | Vector3/Quaternion XML reading copy-pasted ~8×; quad building duplicated (planes vs. NPCs); assimp node creation duplicated 4× in `LevelWriter`; `LevelParser.cpp` is 888 lines. |
| State | `LevelParser` uses static caches with global mutexes; the trile-set lock is held during a full parse; whole trile sets are copied per level. |
| Side effects | `TrileSetParser` creates `exported/` directories inside the input folder. |
| Includes | `Character.h`, `BackgroundPlane.h` use `Geometry` without including it; `Level.h` lacks `<map>`; `Texture.h` lacks `<tuple>`. |
| Correctness | Background plane size uses integer division (`m_Width / 16`), NPCs use float; `trileId`/`orientation` read via `toFloat`; `M_PI`; export errors ignored; an empty mesh is added for geometry without vertices; `pos_*`/`neg_*` names inverted in normal decoding. |
| Testability | Parsing, I/O, caching and threading are interleaved; GUI dialog is the only entry; no tests. |

## 2. Goals and non-goals

**Goals**

- Dependencies (assimp, Eigen, Catch2) from a user-provided **vcpkg** installation (classic mode, prebuilt).
- **C++26** (`CMAKE_CXX_STANDARD 26` → `/std:c++latest` on MSVC 18.x / Visual Studio 2026).
- Layered, testable code structure.
- **Catch2 v3** tests: unit, mini end-to-end, opt-in golden regression.
- Phase 1 refactor keeps generated output **byte-identical**; bug fixes follow as separate, tested steps.

**Non-goals**

GitHub Actions CI, other output formats (glTF), GUI, Linux/macOS builds, C++ modules, clang-tidy.

## 3. Build and dependencies

### Repository layout

```
CMakeLists.txt              # project, C++26, options, add_subdirectory(src, tests)
CMakePresets.json           # committed; toolchain from $env{VCPKG_ROOT}
CMakeUserPresets.json       # gitignored; local vcpkg root, Qt path, FEZ_DATA_DIR, FEZ_GOLDEN_DIR
cmake/CompilerWarnings.cmake
src/core/                   # fez_core static library (moved from source/ via git mv)
src/cli/                    # fez_model_generator executable
tests/unit/
tests/data/                 # self-authored fixtures, mini_export/, mini_expected/
tests/golden/
tools/blender/all_texture_sample_closest.py
```

### Dependencies

- vcpkg classic mode: `VCPKG_MANIFEST_MODE=OFF`, triplet `x64-windows`, toolchain
  `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`. No `vcpkg.json` in the repo.
- One-time setup by the developer: `vcpkg install assimp eigen3 catch2 --triplet x64-windows`.
- `find_package(assimp CONFIG REQUIRED)` → `assimp::assimp`;
  `find_package(Eigen3 CONFIG REQUIRED)` → `Eigen3::Eigen`;
  `find_package(Catch2 3 CONFIG REQUIRED)` → `Catch2::Catch2WithMain`.
- Qt 6 from the developer's own Qt install via `CMAKE_PREFIX_PATH` in `CMakeUserPresets.json`;
  components `Core Gui Xml`. `Widgets` is dropped. `Concurrent` is linked only while `QtConcurrent` is still in use
  (Phase 0 and Phase 1 steps 1–8) and removed in Phase 1 step 9.
- assimp DLLs are deployed next to the executable by vcpkg app-local deployment.
  Qt DLLs are deployed only on install via `qt_generate_deploy_app_script`.
- Tests get Qt and vcpkg `bin` directories on `PATH` via `catch_discover_tests(... DL_PATHS ...)`.

### Compiler and targets

- `CMAKE_CXX_STANDARD 26`, `CMAKE_CXX_STANDARD_REQUIRED ON`.
- MSVC flags: `/W4 /permissive- /utf-8 /Zc:__cplusplus`. `/WX` is enabled in Phase 2 step 6.
- Explicit source lists, no globbing.
- Binaries go to `build/<preset>/bin`; nothing is written into the source tree.

### Presets

- Generator `Visual Studio 18 2026`, architecture `x64`.
- Configure/build/test presets `windows-debug` and `windows-release`.
- `ctest --preset windows-debug` runs unit and mini end-to-end tests; golden tests run with label `golden`
  and only when `FEZ_DATA_DIR` and `FEZ_GOLDEN_DIR` are set.

## 4. Core library (`fez_core`, `namespace fez`)

Existing style is kept: `m_` member prefix, PascalCase types, existing `.clang-format`.

| Module | Responsibility |
|---|---|
| `model/` | Plain data: `Vertex`, `Geometry`, `Texture`, `AnimationFrame{duration, offset, size}`, `Level`, `ArtObject`, `TrileEmplacement` (`m_Orientation`), `BackgroundPlane`, `Character`. Every header includes what it uses. |
| `xml/` | `loadXml(path)`, `attribute<T>(elem, name)`, `readBool`, `readVector2`, `readVector3`, `readQuaternion`, `childElements(elem, name)` range. All fallible functions return `std::expected<T, Error>`. Only `xml/` and `parser/` include `QDom*`. |
| `parser/` | `parseGeometry` (`ShaderInstancedIndexedPrimitives`), `parseArtObject`, `parseTrileSet`, `parseAnimatedTexture`, `parseLevel`. `parseLevel` only reads XML into a `LevelDescription`; it loads no referenced files. Level reading is split into per-section files (triles, art objects, background planes, NPCs). |
| `repository/` | `AssetRepository(inputRoot, Loaders = defaults)`: `trileSet(name)`, `artObject(name)`, `texture(dir, name, animated)`. Thread-safe, loads each key once, returns `std::shared_ptr<const T>`. Loaders are injectable for tests. |
| `level/` | `resolveLevel(LevelDescription, AssetRepository&)` attaches geometries and textures and builds plane/NPC quads. |
| `geometry/` | Pure functions: `decodeFaceNormal(side)`, `faceUvOffset(side)`, `makeTexturedQuad(size, uvRect, depthOffset)`, `hasTranslucentPixels(const QImage&)`. |
| `export/` | `SceneBuilder` (meshes/materials owned in `std::vector<std::unique_ptr<…>>`, handed to `aiScene` once; `addMesh(Geometry)`, `addNode(name, Transform, meshId)`); transform helpers (trile orientation → quaternion, AO/plane −0.5 offset); `exportObj(scene, dir, name)` returning `std::expected`; `TextureCopier`. Only `export/` includes assimp headers. |
| `pipeline/` | `convertArtObjects`, `convertTrileSets`, `convertLevels(input, output, repo, options)` returning a `ConversionReport`. All three share one `AssetRepository`. |
| `log/` | Thin `std::print`-to-stderr facade with levels and a `std::formatter<QString>`; replaces `qDebug`. |

Dependency direction: `model` ← `xml` ← `parser` ← `repository`, `level` ← `export` ← `pipeline` ← `cli`.

C++ features used where MSVC ships them: `std::expected`, `std::print`/`std::format`, `std::numbers::pi_v<float>`,
ranges/views, `std::span`, designated initializers, `std::jthread`/`std::stop_token`.

## 5. CLI (`src/cli`)

```
fez_model_generator --input <export-dir> [--output <dir>] [--only ao,ts,lv] [--jobs N] [--verbose]
```

- `QCoreApplication` + `QCommandLineParser`. No dialog.
- `--output` defaults to `--input`, keeping today's layout:
  `<out>/ao_export/`, `<out>/ts_export/<set>/`, `<out>/lv_export/<level>/`.
- `--only` defaults to all three kinds; `--jobs` defaults to `std::thread::hardware_concurrency()`.
- Exit codes: `0` all assets converted, `1` at least one asset failed (summary printed), `2` usage error.
- `VS_DEBUGGER_COMMAND_ARGUMENTS` is set from cache variable `FEZ_DEBUG_ARGS` so F5 runs against local data.

## 6. Data flow and concurrency

```
cli → Options → pipeline
  convertArtObjects: "art objects/*.xml" → parallelFor: repo.artObject → SceneBuilder → exportObj → TextureCopier
  convertTrileSets:  "trile sets/*.xml"  → parallelFor: repo.trileSet  → per trile: SceneBuilder → exportObj → TextureCopier
  convertLevels:     "levels/*.xml"      → parallelFor: parseLevel → resolveLevel(repo) → SceneBuilder → exportObj → TextureCopier
```

- Phases run sequentially; files within a phase run in parallel via `parallelFor(items, jobs, fn)`: a pool of
  `--jobs` `std::jthread`s pulling indices from a `std::atomic<size_t>`. (`std::execution::par` was rejected because it
  cannot bound the thread count.)
- Each output file is written by exactly one task; mesh and node order follow XML order. Parallelism cannot
  change file contents.
- `AssetRepository` uses `std::mutex` + `std::map<Key, std::shared_future<Result>>`: the first requester loads
  outside the lock; later requesters wait on the future. Different keys never block each other.

## 7. Error handling

- `struct Error { ErrorKind kind; std::string message; std::string source; };`
  with `ErrorKind` ∈ {`FileNotFound`, `XmlSyntax`, `MissingElement`, `MissingAttribute`, `BadValue`, `Export`, `Io`}.
- `source` carries file, line/column for XML syntax errors, and an element path such as
  `Level/ArtObjects/Entry[3]/ArtObjectInstance@name` for content errors.
- Every parser, repository lookup and export returns `std::expected<T, Error>`. `noexcept` is removed from functions
  that allocate.
- Exceptions only for programming errors and out-of-memory; the pipeline catches `std::exception` per asset.
- Failure policy unchanged in Phase 1: a failed asset is skipped and the run continues; a level fails as a whole
  when any referenced trile set, art object or texture fails.

## 8. Testing

Single test executable `fez_tests` (links `fez_core`, `Catch2::Catch2WithMain`), registered with `catch_discover_tests`.

**Unit tests (always run)**

- `xml/`: valid, missing and malformed attributes; case-insensitive bools; error kind and element path.
- `geometry/`: all six face normals and an invalid index; UV face offset; V flip; quad vertices, indices, UVs;
  `hasTranslucentPixels`.
- `parser/`: one minimal fixture per format plus negative cases (index count not divisible by 3, missing
  `trileSetName`, invalid `animated` value, missing `Vector3` attributes).
- `repository/`: N threads request the same key → loader called exactly once; different keys load concurrently.
- `export/`: mesh, material and node counts; winding `(0, 2, 1)`; trile orientation quaternions; −0.5 offset;
  opacity texture only for planes with translucent pixels; `exportObj` writes `.obj`/`.mtl` to a temp dir and returns
  an `Export` error for an unwritable path.

**Mini end-to-end test (always runs)**

`tests/data/mini_export/` is a self-authored FEZ-style tree: one art object, one trile set with two triles, one level
with a static plane, an animated plane and one NPC, plus 2×2 PNGs. The full pipeline runs into a temp dir and the
result is compared byte-for-byte with committed `tests/data/mini_expected/`. No copyrighted game data is committed.

**Golden regression test (opt-in)**

- Catch2 tag `[.golden]`, CTest label `golden`; runs only if `FEZ_DATA_DIR` and `FEZ_GOLDEN_DIR` are set.
- Converts the full local export into a temp dir and compares file set and bytes against `FEZ_GOLDEN_DIR`;
  for `.obj`/`.mtl` the first differing line is reported.
- The reference must be produced by the **old code built against the vcpkg assimp** (assimp writes version-specific
  headers and number formatting), see Phase 0.

## 9. Phasing

### Phase 0 – Safety net (no behavior change)

1. Build the unchanged code with vcpkg, presets, the VS 18 generator and C++26. Only fixes required by
   `/std:c++latest` (missing includes, `M_PI`), plus linking `Qt6::Concurrent`.
2. Generate the reference: run the old program (dialog) on the local export, move the produced
   `ao_export`/`ts_export`/`lv_export` into `FEZ_GOLDEN_DIR` outside the repo.
3. Wire up Catch2 and the golden comparator; it passes against a fresh run of the old code.

### Phase 1 – Restructure (golden test green after every step)

1. `git mv` to `src/core`, `src/cli`, `tools/`; add the CLI; `Application` driven by arguments.
2. Model cleanup (includes, `AnimationFrame`, `m_Orientation`).
3. `xml/` helpers, `Error`, `std::expected`.
4. `geometry/` pure functions.
5. Port parsers one at a time: geometry → art object → trile set → texture → level read.
6. `AssetRepository` replaces static caches; `TrileSetParser` no longer creates `exported/` in the input folder.
7. `level/` resolve.
8. `SceneBuilder`, `exportObj`, `TextureCopier`; export errors reported.
9. `pipeline/` with `parallelFor` (`std::jthread` pool) and `ConversionReport`; remove `Application` and `QtConcurrent`.
10. `log/` replaces `qDebug`; README with build, vcpkg setup and usage.

Unit tests are written alongside each step; new code is test-driven. The mini end-to-end test is added once the CLI
exists (step 1) and its expected output is generated from the then-current code.

### Phase 2 – Behavior fixes (each: failing test → fix → deliberately regenerate references)

1. Background plane size: float division like NPCs.
2. No empty mesh/material for geometry with zero vertices or faces.
3. Read `trileId`/`orientation` as integers; investigate and document trile id `-1`.
4. Rename inverted `pos_*`/`neg_*` normal constants (naming only, values unchanged — no reference change).
5. Texture copy: report failures; define overwrite behavior for existing files.
6. Enable `/WX`.
