# AGENTS.md - Code Mode

This file provides guidance to agents when writing or modifying code in this repository.

## Non-Obvious Coding Patterns

### Thread Safety & GUI Updates
- **wxWidgets threading model**: GUI updates must happen on main thread; background processing uses `wxQueueEvent` or `CallAfter()` for cross-thread communication
- **OpenGL context management**: 3D rendering requires valid GL context; `GLVolume` system manages context lifecycle
- **Parallel slicing architecture**: `PrintObject::slice()` uses parallel execution; algorithms must be thread-safe or use thread-local storage

### Memory & Performance
- **Configuration copy-on-write**: `DynamicPrintConfig` uses copy-on-write; modifications affect all references unless cloned
- **Model ownership hierarchy**: `Model` owns `ModelObject` instances; `ModelObject` owns `ModelVolume` instances. Use factory methods (`Model::add_object()`) not direct construction
- **Geometry custom containers**: `Slic3r::Points`, `Slic3r::Polygons`, `Slic3r::ExPolygons` provide type safety and performance optimizations over STL containers
- **G-code generation pipeline**: `GCode::process_layer()` is performance-critical; avoid allocations, prefer pre-sized containers

### Geometry & Algorithms
- **ClipperLib integration**: Geometry algorithms use `ClipperUtils` wrapper functions; direct Clipper calls may break coordinate scaling
- **Polygon operations**: `union_()`, `diff()`, `intersection()` with `ClipperUtils` are expensive; cache results when possible
- **AABB tree construction**: `AABBTreeIndirect` used for spatial queries; rebuild only when geometry changes

### String & File Operations
- **Boost dependency**: String formatting uses `boost::format`; filesystem operations use `boost::filesystem`

## Code Style (Non-Obvious Only)

### Formatting
- `.clang-format` enforces 4-space indents, 140-column limit, aligned initializers
- Run `clang-format -i <file>` before committing; CMake `clang-format` target available when LLVM tools on PATH
- Keep headers self-contained and align include order with IWYU pragmas

### Naming (from existing codebase patterns)
- `CamelCase` for classes
- `snake_case` for functions and locals  
- `SCREAMING_CASE` for constants

## Testing

- Unit tests use Catch2 (`tests/catch2/`)
- **Catch2 custom rules**: No thread-safe assertions, unique SECTION names in loops
- Name specs after component under test: `tests/libslic3r/TestPlanarHole.cpp`
- Tag long-running cases so `ctest -L fast` remains useful
- Add fixtures to `tests/data/`, reference with relative paths

## Extension Points

- **Print host integration**: Implement `PrintHost` subclass in `src/slic3r/Utils/`; register in `src/slic3r/GUI/PrintHostDialogs.cpp`
- **Custom G-code scripts**: Hook into `GCode::process_layer()` via `CustomGCode` system
- **Test fixtures**: Add to `tests/data/`, reference with relative paths
