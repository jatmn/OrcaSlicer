# AGENTS.md - Code Mode

This file provides guidance to agents when working with code in this repository.

## Non-Obvious Coding Rules

- **FORMAT_CONFIG_ID system**: To enable container format export (.ufp/.makerbot) for a printer preset, add `FORMAT_CONFIG_ID:<id>` to printer notes. Config files are in `resources/formats/ufp/` and `resources/formats/makerbot/`.
- **Vendored dependencies**: `deps/` and `deps_src/` are vendored snapshots—do not modify without mirroring upstream tags.
- **Header includes**: Keep headers self-contained and align include order with IWYU pragmas.
- **Enum class usage**: Strongly prefer `enum class` over plain `enum` for type safety.
- **C++17 features**: Use structured bindings, `std::optional`, `std::variant`, and `std::string_view` where appropriate.
- **Custom containers**: Use `Slic3r::Points`, `Slic3r::Polygons`, `Slic3r::ExPolygons` instead of raw STL containers for geometry.
- **Error handling**: Use `throw Slic3r::RuntimeError` or `Slic3r::CriticalError` for fatal errors, not `assert()` or `exit()`.
- **Memory management**: Prefer `std::unique_ptr` with custom deleters for C library resources (e.g., `FILE*`, `tess`).
- **Geometry algorithms**: Use `ClipperLib` utilities via `Slic3r::ClipperUtils` wrapper functions, not raw Clipper calls.
- **String formatting**: Use `boost::format` or `Slic3r::format()` instead of `std::stringstream` for complex formatting.

## Hidden Dependencies

- **wxWidgets threading**: GUI updates must happen on main thread; use `wxQueueEvent` or `CallAfter()`.
- **OpenGL context**: GL operations require valid GL context; check `GLVolume::is_initialized()` before drawing.
- **Configuration system**: `DynamicPrintConfig` uses copy-on-write; modify via `set()` methods, not direct member access.
- **ModelObject lifecycle**: `ModelObject` instances are owned by `Model`; use `Model::add_object()` instead of `new ModelObject()`.

## Performance Critical Paths

- **AABB trees**: Use `AABBTreeIndirect` for spatial queries on triangle meshes.
- **Polygon operations**: `union_()`, `diff()`, `intersection()` with `ClipperUtils` can be expensive; cache results.
- **G-code generation**: `GCode::process_layer()` is hot path; avoid allocations in loops.
- **Slicing**: `PrintObject::slice()` uses parallel execution; ensure thread safety.
