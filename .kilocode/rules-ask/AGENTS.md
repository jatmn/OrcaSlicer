# AGENTS.md - Ask Mode

This file provides guidance to agents when answering questions about this repository.

## Non-Obvious Documentation Context

### Project Structure (Counterintuitive Aspects)
- **20GB+ codebase**: Use narrow search scope; limit to specific directories (e.g., `path: src/slic3r/GUI`) rather than broad recursive searches
- **Vendored dependencies**: `deps/` and `deps_src/` are snapshots; modifications require mirroring upstream tags
- **Resource embedding**: Resource files embedded at build time with platform-specific handling
- **Two-part build**: Dependencies built separately in `deps/build/` before main application

### Key Documentation Locations
- **SoftFever_doc/**: Longer-form documentation specific to this fork
- **doc/**: General project documentation
- **resources/profiles/**: Printer and material profiles organized by manufacturer
- **localization/i18n/**: Source translation files (.pot, .po)
- **scripts/**: Python utilities for profile generation and validation

### Reference Codebases (External Context)
When answering questions about Cura-related features:
- **Parent directory / Cura-main**: UltiMaker Cura source for API behavior reference
- **Parent directory / Cura files/**: Reference `.ufp` and `.makerbot` files for file structure
- **Parent directory / postprocessors/**: Python scripts (reference only, never edit)
- **Parent directory / Info**: UFP/MakerBot specifications and documentation
- **Uranium**: https://github.com/Ultimaker/Uranium — Cura's underlying framework
- **UltiMaker Digital Factory API**: https://docs.api.ultimaker.com/index.html

### Container Format Export System
- Uses `FORMAT_CONFIG_ID:<id>` in printer notes to enable `.ufp`/`.makerbot` export
- Config files in `resources/formats/ufp/` and `resources/formats/makerbot/` define metadata, thumbnails, G-code headers/footers
- **No Python dependency**: Implementation must be built into OrcaSlicer directly

### Critical Constraints to Mention
- **UltiMaker Digital Factory API**: Must impersonate Cura entirely — headers, agent-type, version numbers must match exactly
- **wxWidgets threading**: GUI updates must happen on main thread
- **OpenGL context**: 3D rendering requires valid GL context managed by `GLVolume`
- **Copy-on-write configs**: `DynamicPrintConfig` modifications affect all references unless cloned

### Common Question Patterns

**Q: Where is the slicing logic?**
A: `src/libslic3r/Print.cpp` orchestrates the pipeline; `PrintObject::slice()` is the entry point for parallel execution.

**Q: Where are print settings defined?**
A: `src/libslic3r/PrintConfig.cpp` defines all print/printer/material settings with bounds and defaults.

**Q: How do I add a new printer?**
A: Create JSON profile in `resources/profiles/[manufacturer]/`, add `FORMAT_CONFIG_ID` for container export if needed.

**Q: Where is the GUI code?**
A: `src/slic3r/GUI/` — wxWidgets-based, uses `wxQueueEvent` or `CallAfter()` for cross-thread communication.

**Q: How do tests work?**
A: Catch2 framework in `tests/catch2/`; tests grouped by domain (`libslic3r/`, `fff_print/`, `sla_print/`); fixtures in `tests/data/`.

## Information Retrieval Strategy

1. **Use list_files first** — explore directory structure before deep searches
2. **Narrow search scope** — limit to specific directories (e.g., `src/slic3r/GUI`)
3. **Compress frequently** — with 20GB+ codebase, compress every 1-2 tool calls
4. **Batch operations** — plan searches to minimize tool calls before compression
5. **Check parent directory references** — for Cura/UltiMaker integration questions
