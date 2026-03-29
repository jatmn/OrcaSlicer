# AGENTS.md - Ask Mode

This file provides guidance to agents when answering questions about this repository.

## Non-Obvious Documentation Context

- **This is jatmn's fork of OrcaSlicer**: Based on BambuStudio, with active development for UltiMaker Digital Factory integration and container format export (.ufp/.makerbot).
- **Reference codebases in parent directory**: When working on Cura-related features, reference `/Cura-main`, `/Cura files/`, `/postprocessors/`, and `/Info` directories in parent folder.
- **Container format export is implemented**: Use `FORMAT_CONFIG_ID:<id>` in printer notes to enable .ufp/.makerbot export. Config files in `resources/formats/ufp/` and `resources/formats/makerbot/`.
- **Vendored dependencies are snapshots**: `deps/` and `deps_src/` contain vendored third-party libraries; do not modify without mirroring upstream tags.
- **Testing uses Catch2 with strict rules**: See `tests/CLAUDE.md` for critical testing rules (thread safety, SECTION naming in loops).
- **Build system is CMake with platform scripts**: Use `build_linux.sh`, `build_release_macos.sh`, `build_release_vs2022.bat` for platform-specific builds.
- **Code style enforced by .clang-format**: 4-space indents, 140-column limit, aligned initializers, brace wrapping for classes/functions.
- **Naming conventions**: `CamelCase` for classes, `snake_case` for functions/locals, `SCREAMING_CASE` for constants.
- **Project structure**: C++17 sources in `src/`, assets in `resources/`, translations in `localization/`, tests in `tests/` grouped by domain.
- **Performance critical paths**: `GCode::process_layer()` (G-code generation), `PrintObject::slice()` (parallel slicing), polygon operations with ClipperUtils.

## Hidden Architectural Details

- **wxWidgets GUI framework**: Cross-platform GUI with threading restrictions (main thread for updates).
- **OpenGL for 3D rendering**: GLVolume system with context management.
- **Geometry uses custom containers**: `Slic3r::Points`, `Slic3r::Polygons`, `Slic3r::ExPolygons` instead of STL containers.
- **Configuration system**: `DynamicPrintConfig` with copy-on-write semantics.
- **Model ownership**: `Model` owns `ModelObject` instances; use `Model::add_object()` not `new ModelObject()`.
- **Error handling**: `Slic3r::RuntimeError` and `Slic3r::CriticalError` for fatal errors.

## Common Questions Answered

- **How to add new printer format?**: Add `FORMAT_CONFIG_ID:<id>` to printer notes and create config in `resources/formats/`.
- **How to run tests?**: `cmake --build build --target tests` then `ctest --test-dir build --output-on-failure`.
- **How to build on Windows?**: Use `build_release_vs2022.bat` or `cmake --build . --config %build_type% --target ALL_BUILD -- -m`.
- **How to format code?**: `clang-format -i <file>` or use CMake `clang-format` target.
- **Where are test fixtures?**: `tests/data/` contains sample models, G-code, and validation data.
