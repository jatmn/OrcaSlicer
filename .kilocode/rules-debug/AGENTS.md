# AGENTS.md - Debug Mode

This file provides guidance to agents when debugging code in this repository.

## Non-Obvious Debugging Discoveries

- **Catch2 test assertions are not thread-safe**: Never call `REQUIRE()` or `CHECK()` from worker threads. Use atomic counters to collect results and assert on the main thread.
- **SECTION names in loops must be unique**: Using identical `SECTION("Same name")` inside a loop causes undefined behavior. Use `DYNAMIC_SECTION` or incorporate loop counter into name.
- **wxWidgets GUI debugging**: GUI updates must happen on main thread; use `wxQueueEvent` or `CallAfter()` for cross-thread updates.
- **OpenGL context validation**: GL operations fail silently without valid context; check `GLVolume::is_initialized()` before drawing calls.
- **Memory debugging**: Use custom allocators in `deps/`; memory corruption in geometry algorithms often manifests as ClipperLib assertion failures.
- **Test data location**: Test fixtures are in `tests/data/`; reference them with relative paths from test executable location.
- **CMake build types**: Debug builds enable extensive validation; Release builds disable many checks for performance.
- **Parallel slicing debugging**: `PrintObject::slice()` uses parallel execution; race conditions may cause non-deterministic failures.

## Hidden Failure Modes

- **Configuration copy-on-write**: `DynamicPrintConfig` uses copy-on-write; modifying a shared config may not update all references.
- **Model object lifecycle**: `ModelObject` instances are owned by `Model`; dangling pointers occur if objects are deleted incorrectly.
- **Geometry algorithm precision**: Clipper operations use integer coordinates; floating-point inputs must be scaled appropriately.
- **File path encoding**: Windows paths with Unicode characters require UTF-8 conversion; use `boost::filesystem::path` for cross-platform handling.

## Debugging Tools

- **Catch2 tags**: Use `[!shouldfail]`, `[!nonportable]`, `[!mayfail]` tags for expected failures; `ctest -L fast` runs only fast tests.
- **Geometry visualization**: Debug output can be written as SVG via `debug::export_to_svg()` in `src/libslic3r/`.
- **Performance profiling**: Hot paths are in `GCode::process_layer()` and `PrintObject::slice()`; use instrumentation builds.
- **Memory profiling**: Custom allocators in geometry types track allocations; enable with `-DSLIC3R_DEBUG=ON`.
