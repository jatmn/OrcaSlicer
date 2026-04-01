# AGENTS.md - Debug Mode

This file provides guidance to agents when debugging issues in this repository.

## Non-Obvious Debugging Gotchas

### Logging
- **Info level logs do not appear** — only WARNING and ERROR level messages show up in the logs. Do not rely on info/log statements for debugging; use WARNING or ERROR to ensure messages are visible.

### Build & Test Debugging
- **CMake version constraint**: Minimum 3.13, maximum 3.31.x on Windows
- **Dependencies built separately**: First build in `deps/build/`, then link to main application
- **Platform-specific toolchains**: `cmake/modules/` for custom find modules, `cmake/msvc/` for Windows toolchain files

### Test Framework (Catch2)
- **No thread-safe assertions**: Catch2 tests cannot safely assert from multiple threads
- **Unique SECTION names required**: In loops, SECTION names must be unique to avoid test failures
- **Tag long-running tests**: Use tags so `ctest -L fast` can skip slow tests

### Windows Development
- **No Linux tools available**: This project is developed on Windows 11. Common Linux commands like `grep`, `find`, `ls`, `cat`, and similar CLI tools are NOT available in the terminal.
- **Use agent tools instead**: Use the provided `search_files`, `list_files`, `read_file`, and `list_code_definition_names` tools for all file operations.
- **Avoid execute_command for file searches**: Do not try to run `grep`, `find`, or similar commands via `execute_command`. Use the dedicated search tools.

### Common Debug Targets
- **Slicing pipeline**: `PrintObject::slice()` — entry point for parallel slicing
- **G-code generation**: `GCode::process_layer()` — performance-critical, dominates slicing time
- **Configuration issues**: `DynamicPrintConfig` — check copy-on-write behavior
- **Geometry problems**: `ClipperUtils` wrapper functions — direct Clipper calls may break coordinate scaling
- **File I/O**: `libslic3r/Format/` — 3MF, AMF, STL, OBJ, STEP format handlers

### Performance Debugging
- **Hot paths to profile**: 
  - `PrintObject::slice()`
  - `GCode::process_layer()`
  - `ClipperUtils` polygon operations
- **Memory allocation**: Avoid dynamic allocations in hot paths; prefer pre-sized containers
- **AABB tree**: `AABBTreeIndirect` — rebuild only when geometry changes

### Cross-Thread Issues
- **wxWidgets threading**: GUI updates must happen on main thread
- **Background processing**: Use `wxQueueEvent` or `CallAfter()` for cross-thread communication
- **OpenGL context**: 3D rendering requires valid GL context managed by `GLVolume` system

## Debug Workflow

1. **Identify the symptom** — slicing failure, crash, incorrect G-code, UI issue
2. **Locate the entry point** — `Print.cpp` for slicing, `GCode.cpp` for generation, `GUI/` for UI
3. **Check thread safety** — parallel code uses TBB; verify thread-local storage usage
4. **Validate geometry operations** — use `ClipperUtils` wrappers, not direct Clipper calls
5. **Test with fixtures** — add sample models/G-code to `tests/data/` for reproduction
