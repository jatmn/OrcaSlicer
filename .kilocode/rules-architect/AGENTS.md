# AGENTS.md - Architect Mode

This file provides guidance to agents when designing architecture in this repository.

## Non-Obvious Architectural Constraints

- **Container format export system**: Uses `FORMAT_CONFIG_ID:<id>` in printer notes to enable .ufp/.makerbot export. Config files define metadata, thumbnails, G-code headers/footers.
- **Reference implementations in parent directory**: Valid .ufp/.makerbot files and Python postprocessors in parent `/Cura files/` and `/postprocessors/` are reference only; implementation must be built into OrcaSlicer with zero dependency on Python.
- **Vendored dependencies as snapshots**: `deps/` and `deps_src/` are vendored; modifications require mirroring upstream tags and updating build scripts.
- **wxWidgets threading model**: GUI updates must happen on main thread; background processing uses `wxQueueEvent` or `CallAfter()` for cross-thread communication.
- **OpenGL context management**: 3D rendering requires valid GL context; `GLVolume` system manages context lifecycle.
- **Configuration copy-on-write**: `DynamicPrintConfig` uses copy-on-write; modifications affect all references unless cloned.
- **Model ownership hierarchy**: `Model` owns `ModelObject` instances; `ModelObject` owns `ModelVolume` instances. Use factory methods (`Model::add_object()`) not direct construction.
- **Geometry custom containers**: `Slic3r::Points`, `Slic3r::Polygons`, `Slic3r::ExPolygons` provide type safety and performance optimizations over STL containers.
- **Parallel slicing architecture**: `PrintObject::slice()` uses parallel execution; algorithms must be thread-safe or use thread-local storage.
- **G-code generation pipeline**: `GCode::process_layer()` is performance-critical; avoid allocations, prefer pre-sized containers.

## Hidden Coupling

- **ClipperLib integration**: Geometry algorithms use `ClipperUtils` wrapper functions; direct Clipper calls may break coordinate scaling.
- **Boost dependency**: String formatting uses `boost::format`; filesystem operations use `boost::filesystem`.
- **Catch2 test framework**: Tests rely on Catch2 with custom rules (no thread-safe assertions, unique SECTION names in loops).
- **CMake module system**: Platform-specific configuration in `cmake/modules/`; toolchain files in `cmake/msvc/`.

## Performance Bottlenecks

- **Polygon operations**: `union_()`, `diff()`, `intersection()` with `ClipperUtils` are expensive; cache results when possible.
- **AABB tree construction**: `AABBTreeIndirect` used for spatial queries; rebuild only when geometry changes.
- **G-code path generation**: Extrusion path planning in `GCode::process_layer()` dominates slicing time.
- **Memory allocation in hot paths**: Avoid dynamic allocations in `PrintObject::slice()` and `GCode::process_layer()`.

## Extension Points

- **Printer format export**: Add `FORMAT_CONFIG_ID:<id>` support by creating config in `resources/formats/ufp/` or `resources/formats/makerbot/`.
- **Print host integration**: Implement `PrintHost` subclass in `src/slic3r/Utils/`; register in `src/slic3r/GUI/PrintHostDialogs.cpp`.
- **Custom G-code scripts**: Hook into `GCode::process_layer()` via `CustomGCode` system.
- **Test fixture addition**: Add sample models/G-code to `tests/data/`; reference in test files with relative paths.
