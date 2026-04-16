# OrcaSlicer Agent Guide (jatmn's Fork)

> **Last Updated:** 2026-04-15
>
> **Change Tracking:** This file tracks implementation status specific to jatmn's fork. See the [Change Log](#change-log) section at the end for revision history.
> When older sections conflict with the current-status audit below, the current-status audit wins.

## Project Context

This is **jatmn's fork** of [OrcaSlicer](https://github.com/jatmn/OrcaSlicer), a 3D printer slicer based on BambuStudio.

## Current Status Audit (2026-04-15)

This section is the current high-level truth for the fork and should be used as the starting point when preparing PRs or resuming work.

### Public Fork Status (`origin/main`)

- Public fork head currently includes work through commit `59e2bbfe71`.
- UltiMaker Digital Factory upload, UltiMaker LAN printing, `.ufp` export, `.makerbot` export, MakerBot Sketch profiles, UltiMaker S/F printer families, core-specific process matrix groundwork, and baseline Cheetah flavor support are already on the public fork.
- Public fork also includes recent Cheetah fixes for gcode flavor persistence, duplicate extruder tabs, Filament/Process tab consistency, UFP material GUID/name fixes, UFP path / multi-material export fixes, the temporary UltiMaker root-manifest pruning for incomplete models, MakerBot Sketch default-profile backports, optional Linux secure storage fallback, and the latest UltiMaker auth / LAN discovery refresh.

### Local-Only Status (`HEAD` not yet on `origin/main`)

- `HEAD` is currently aligned with `origin/main`.
- There are no fork-specific local-only commits to track at the moment.
- Treat the current AGENTS file as the authoritative status record for what has already landed publicly in jatmn's fork.

### Active Workstreams

- Cheetah support is now beyond proof-of-concept and into tuning / validation.
- UltiMaker default machine, process, and material values are still being actively tuned.
- Roaming-profile `- Copy` presets are currently the working area for tuned defaults before backporting them into source-controlled profiles.
- Dual-extrusion validation on UltiMaker S/F series is still incomplete even though the profile and container groundwork exists.
- UltiMaker setup-wizard / vendor-manifest behavior has a newly documented dependency rule: incomplete model removal must be done consistently across all root lists in the vendor manifest or the entire vendor bundle may stop loading correctly.
- UltiMaker Digital Factory auth now more closely mirrors Cura's behavior, but it still needs live user validation after the token-storage and refresh-path cleanup.
- UltiMaker LAN browse now behaves more like Cura with persistent discovery while the picker is open, but broader long-session validation is still needed.

### Current PR Guidance

- Treat the fork as two layers: public fork history in `origin/main`, plus local-only commits at `HEAD`.
- Do not log upstream rebases or imported upstream SoftFever commits in this guide; only track work authored in jatmn's fork.
- Before opening PRs, reconcile any active roaming `- Copy` presets back into source profiles intentionally rather than assuming the current user profile state is reflected in the repo.

## Reference Codebases

When working on Cura-related features (especially UltiMaker Digital Factory integration):

- **Parent directory / Cura-main** — contains UltiMaker Cura source code. Use for reference when impersonating Cura's API behavior.
- **Parent directory / Cura files/** — Reference `.ufp` and `.makerbot` files produced by Cura. These are **CRITICAL** for understanding the required file structure, `.json` metadata, thumbnails, and `.gcode` headers/footers inside these container formats.
- **Parent directory / postprocessors/** — Python scripts that produce working `.ufp` and `.makerbot` files. These are **reference only** and should never be edited. They demonstrate the expected output but the implementation must be built into OrcaSlicer directly with **zero dependency on Python or these scripts**.
- **Parent directory / Info** — Documentation on firmware, `.ufp` and `.makerbot` requirements including:
  - UFP Functional Specification
  - MakerBot Functional Specification
  - Griffin Gcode file format (headers, toolpath instructions, metadata)
  - UMF (UltiMaker Manufacturing Format) specification
  - Cheetah migration/integration assessment
  - OrcaSlicer profile validator documentation
- **Parent directory / Valid postprocessed ufp makerbot files** — Valid `.ufp` and `.makerbot` files produced with Python postprocessors. These represent the **MINIMUM requirement for valid files**. Deviations from this file structure, `.json` contents, or `.gcode` header/footer regions are **not considered valid**. Includes:
  - `UltiMaker_S6_-_Cube.ufp`
  - `MakerBot_Sketch_Small_-_Cube.makerbot`
  - `MakerBot_Sketch_Sprint_-_Cube.makerbot`
- **Uranium** — https://github.com/Ultimaker/Uranium — Cura's underlying framework, useful for understanding plugin architecture and messaging.
- **UltiMaker Digital Factory API** — https://docs.api.ultimaker.com/index.html — Always reference this for API endpoints, authentication, and expected behavior.

### Important Constraints

- **ALL UltiMaker Digital Factory API calls must impersonate Cura entirely** — UltiMaker Digital Factory only accepts Cura. Every header, agent-type, meta, version number, software identification, and request format must exactly match what Cura sends. Nothing else will work.
- **Never edit Python postprocessor scripts** — they are reference only
- **Zero external dependencies** — postprocessing must work without Python or any external scripts
- **Do not modify Cura source code** — it is reference only
- **Use the API docs first** — before asking questions, check the UltiMaker API documentation
- **Local commits are fine** — see global rules for Git etiquette

### Context Management Rules

- **Compress frequently when searching files** — with a 20GB+ codebase and small context window, compress **every 1-2 tool calls** when doing file searches or reads. After searching 3-5 files, compress before continuing to the next batch.
- **Todo items are the primary work unit** — if a `todowrite` list exists, each item is an independent, compressible unit. When one item is complete and verified, compress it before moving to the next.
- **Compress closed ranges only** — only compress ranges that are fully resolved and no longer needed. Do not compress active work still in progress.
- **Include subagent results** — compress after a `Task` (explore/general) subagent completes its findings; the subagent already has fresh context, so the raw output in the main thread is noise.
- **Quality over brevity in summaries** — use `compress` with exhaustive summaries that preserve file paths, function names, decisions, and key findings. Raw context can be discarded once it is faithfully summarized.
- **Batch independent compressions** — if multiple independent ranges are stale at the same time, run multiple `compress` calls in parallel rather than one large compression.

### Windows 11 Development

- **Primary shell is PowerShell on Windows 11** — always assume Windows paths with spaces and quote them explicitly.
- **Native Windows tools are available** — PowerShell cmdlets, `git`, `cmake`, and fast text search tools such as `rg` are available and should be preferred when practical.
- **Do not assume Bash/Linux semantics** — avoid Bash-specific piping, quoting, or path assumptions when documenting or scripting local development steps.
- **Be careful with copied build resources** — some generated resource trees do not auto-refresh, so verify which directory is actually being used before assuming a rebuild picked up profile/config changes.

### Debugging & Logging

- **Info level logs do not appear** — only WARNING and ERROR level messages show up in the logs. Do not rely on info/log statements for debugging; use WARNING or ERROR to ensure messages are visible.

### Efficient Codebase Navigation

- **Narrow search scope** — limit searches to specific directories (e.g., `path: src/slic3r/GUI`) rather than broad recursive searches across the entire codebase.
- **Use list_files first** — explore directory structure with `list_files` before doing deep searches.
- **Batch operations efficiently** — plan searches to minimize the number of tool calls before each compression point.

---

## UltiMaker Integration Status Overview

| Feature | Status | Notes |
|---------|--------|-------|
| Container Format Export (.ufp) | ✅ Completed | Works for UltiMaker printers; recent fixes cover GUID/name handling, multi-material export, spaced paths, and wrong stop-marker retraction |
| Container Format Export (.makerbot) | ✅ Completed | Sketch series path works; Method configs remain reserved; single-thumbnail path and logging cleanup landed locally |
| Digital Factory Upload | ✅ Completed | Two-step upload with container conversion |
| LAN Printing | ✅ Completed | Supports UltiMaker S series and Factor series; browse/discovery now uses Cura-style persistent scanning while the picker is open |
| Cheetah G-code Flavor | ⚠️ Active Tuning | Baseline support is in; calibration, preview, UI, and motion handling are still being refined |
| Printer Profiles | ⚠️ Active Tuning | Core/profile matrix exists; defaults are still being tuned and validated |
| Process Profiles | ⚠️ Active Tuning | Dynamic/core-specific groundwork exists; tuned defaults still being validated and selectively backported |
| Material GUID Matching | ✅ Completed | Two-pass search prioritizes GUIDs |
| Material Association Matrix | ❌ Not Started | Need printer/core compatibility |

---

## Container Format Export (.ufp and .makerbot)

### Status: ✅ COMPLETED (with known issues)

Container export to `.ufp` and `.makerbot` formats is implemented and working.

### FORMAT_CONFIG_ID System

To enable container format export for a printer preset, add `FORMAT_CONFIG_ID:<id>` to the printer notes.

**Supported FORMAT_CONFIG_IDs:**

| FORMAT_CONFIG_ID | Format | Extension | Config File |
|-----------------|--------|-----------|-------------|
| `ultimaker_s3` | UltiMaker | `.ufp` | `resources/formats/ufp/ultimaker_s3.json` |
| `ultimaker_s5` | UltiMaker | `.ufp` | `resources/formats/ufp/ultimaker_s5.json` |
| `ultimaker_s6` | UltiMaker | `.ufp` | `resources/formats/ufp/ultimaker_s6.json` |
| `ultimaker_s7` | UltiMaker | `.ufp` | `resources/formats/ufp/ultimaker_s7.json` |
| `ultimaker_s8` | UltiMaker | `.ufp` | `resources/formats/ufp/ultimaker_s8.json` |
| `ultimaker_factor4` | UltiMaker | `.ufp` | `resources/formats/ufp/ultimaker_factor4.json` |
| `ultimaker2_plus_connect` | UltiMaker | `.ufp` | `resources/formats/ufp/ultimaker2_plus_connect.json` |
| `sketch_small` | MakerBot | `.makerbot` | `resources/formats/makerbot/sketch_small.json` |
| `sketch_sprint` | MakerBot | `.makerbot` | `resources/formats/makerbot/sketch_sprint.json` |
| `sketch_large` | MakerBot | `.makerbot` | `resources/formats/makerbot/sketch_large.json` |
| `method_x` | MakerBot | `.makerbot` | Reserved in code (config removed) |
| `method_xl` | MakerBot | `.makerbot` | Reserved in code (config removed) |

**Orphan Files:** None currently known.

### How It Works

1. During export, `FormatConfig::get_format_type_for_printer()` checks `printer_notes` for `FORMAT_CONFIG_ID:<id>`
2. If found, `FormatConfig::export_to_container()` creates the appropriate container format
3. Container files are created in the system temp directory and cleaned up after copy
4. **No fallback** — if the config file doesn't exist, export fails with a descriptive error

### Implementation Details

- **Build**: `src/libslic3r/CMakeLists.txt` — Format files are compiled into libslic3r
- **Detection**: `FormatConfig::get_format_type_for_printer()` — parses printer notes
- **Export**: `FormatConfig::export_to_container()` — creates container using UFPWriter/MakerBotWriter
- **Integration**: `BackgroundSlicingProcess::export_gcode()` — hooks container conversion into export flow
- **Dialog**: `Plater::export_gcode()` — updates file dialog title/extension for container formats
- **Helper**: `ContainerFormatHelper` — manages format-specific thumbnail requirements for both UFP and MakerBot formats

### Known Issues

**MakerBot Format:**
- `.makerbot` writer is implemented for Sketch series; full validation still pending
- `sketch_large` added to `makerbot/manifest.json`
- `method_x.json` and `method_xl.json` config files removed (reserved in `FormatConfig` code for future use)
- ContainerFormatHelper class manages thumbnail generation
- Thumbnail format in printer profiles must use `120x120/PNG` format (with PNG specifier)
- Oversized 960×1460 thumbnail removed from MakerBot configs and from `ContainerFormatHelper` default fallback

**Dual Extrusion on S/F Series (UltiMaker S3/S5/S6/S7/S8, Factor 4):**
- ⚠️ **Needs more work** - Dual extrusion functionality is not fully implemented
- Material usage is not properly reported in `.ufp` container files for dual extrusion prints
- **Zero validation prints** have been performed for dual extrusion - completely untested
- Single extrusion prints work correctly; dual extrusion requires additional development and testing

---

## UltiMaker Digital Factory Integration

### Status: ✅ COMPLETED

Upload to UltiMaker Digital Factory with container conversion is fully implemented.

### Authentication / Token Handling Notes

- OAuth refresh-token persistence now follows a safer Cura-like policy:
  - refresh tokens are stored in OS secure storage when available
  - refresh tokens are **not** persisted in plain JSON metadata anymore
  - legacy JSON refresh tokens are scrubbed after migration / load
- If secure storage is unavailable, the refresh token remains available only for the current session and re-authentication may be required after restart or token expiry.
- The synchronous refresh path was also reduced so UI-triggered Digital Factory actions no longer proactively refresh before every call and now attempt only a single on-demand refresh when the API reports an expired token.
- `OAuthJob` was hardened to tolerate token responses that omit `refresh_token`, preserving the previously stored refresh token when appropriate.

### Upload Integration

- **Two-step upload flow**: `UltiMaker::upload()` creates `.ufp` container before uploading to Digital Factory
- **Process**: 
  1. Export G-code to temp file
  2. Convert to `.ufp` container using `FormatConfig::export_to_container()`
  3. Upload container to Digital Factory via existing HTTP POST
  4. Cleanup temp files after upload
- **Key commit**: `b69768c0dc` — UltiMaker upload: Add two-step upload flow with container format support
- **Helper method**: `Http::set_put_body_raw()` — allows sending raw file data for container uploads

### Supported Features

- ✅ Project folder listing and selection
- ✅ New Project folder creation and selection
- ✅ Upload of `.ufp` files (container format)
- ✅ Authentication and API integration

### Limitations

- ❌ Sending files directly to printer from Digital Factory
- ❌ Monitoring printer status through Digital Factory
- ❌ Managing print files (deletion, organization)
- ❌ Print job control (pause/resume/cancel) via Digital Factory

### UI Integration

- **File type enums**: `FT_UFP` and `FT_MAKERBOT` added to `FileType` enum in `GUI_App.cpp`
- **Export dialog**: File dialog shows appropriate extensions for container formats
- **Integration**: `Plater::export_gcode()` handles container format detection and file naming

---

## UltiMaker LAN Printing

### Status: ✅ COMPLETED (with limitations)

File upload to UltiMaker printers over LAN is working. Cloud is not required; optional local printer discovery is available through the UltiMaker LAN browse dialog.

### Implementation Details

**PrintHost Type**: `htUltiMakerLAN` added to `PrintHostType` enum in `PrintConfig.cpp`

**Class**: `UltiMakerLAN` implemented in `src/slic3r/Utils/UltiMakerLAN.cpp`
- Extends `PrintHost` base class
- Stores: `host` (IP/hostname), `port` (default 80), `username` (`printhost_user`), `password` (`printhost_password`)
- **Auth flow**: HTTP Digest authentication via libcurl when credentials are provided. No manual auth request or printer-screen approval flow is implemented.
- **Upload**: `POST /cluster-api/v1/print_jobs/` with multipart form-data (`owner` + `file` fields)
- **Print control**: `PUT /api/v1/print_job/state` for pause/resume/abort
- **Status monitoring**: `GET /api/v1/print_job` and `GET /api/v1/printer/status`

**HTTP Digest Implementation**:
- Uses libcurl's native HTTP Digest support via `CURLOPT_USERNAME` and `CURLOPT_PASSWORD`
- No manual digest algorithm implementation needed

**Factory Integration**: Added case in `PrintHost::get_print_host()` for `htUltiMakerLAN`

**UI Integration**:
- "UltiMaker (LAN)" option in host printer type dropdown
- Fields: IP/hostname, username, port (default 80)
- Upload progress and print status display
- Browse dialog now returns/stores the discovered printer IP address instead of Bonjour `full_address`, matching Cura's address model more closely.
- Browse dialog discovery is now persistent while the dialog remains open by continuously running short discovery passes, rather than ending after a single one-shot burst.
- Legacy `print_host` values containing paths/query strings/trailing slashes are sanitized before LAN API and Device-tab web UI URLs are built.

### API Reference (implemented endpoints)

| Method | Endpoint | Auth | Purpose |
|---|---|---|---|
| `GET` | `/cluster-api/v1/system` | None / Digest | Test connection (with fallback to `/api/v1/system`) |
| `POST` | `/cluster-api/v1/print_jobs/` | Digest | Upload file + start print |
| `GET` | `/api/v1/print_job` | Digest | Current job status |
| `PUT` | `/api/v1/print_job/state` | Digest | Pause/resume/abort |
| `GET` | `/api/v1/printer/status` | Digest | Printer status string |

**Authentication:** HTTP Digest (RFC 2617) when credentials are configured. No fallback to basic. There is no auth-request/polling endpoint in the current implementation.

### Key Implementation Files

| Reference | Purpose |
|---|---|
| `src/slic3r/Utils/UltiMakerLAN.cpp` | **Primary implementation** — UltiMakerLANPrintHost class |
| `src/slic3r/Utils/UltiMakerLAN.hpp` | Header file |
| `src/slic3r/Utils/PrintHost.cpp` | Factory integration |
| `src/slic3r/GUI/PrintHostDialogs.cpp` | UI dialog for UltiMaker LAN |
| `src/slic3r/Utils/Http.hpp` | HTTP client with digest auth support |

### Testing

- Successfully tested with UltiMaker S6 at IP 10.10.10.246
- File upload works with `.ufp` container format
- Current implementation uses configured HTTP Digest credentials; no printer-screen approval flow is implemented in code
- Print job control (pause/resume/abort) implemented

### Supported Printers

- ✅ UltiMaker S Series (S3, S5, S6, S7, S8)
- ✅ UltiMaker Factor 4
- ❌ UltiMaker 2+ Connect — **Firmware limitation** (not a software issue)
- ❌ MakerBot Sketch/Method series (not supported)

### Limitations

- ✅ File upload to printer
- ✅ Basic print job control (pause/resume/abort)
- ⚠️ Browse for local printers feature is implemented and now persistent, but still needs broader validation on real networks
- ❌ Direct printer control beyond basic commands
- ❌ Print management (job queue, history)
- ❌ Monitoring printer status in real-time
- ❌ Live camera feed support

---

## UltiMaker & MakerBot Printer Profiles

### Status: ⚠️ IN PROGRESS

All UltiMaker and MakerBot printer profiles have been created but have known issues that need resolution.

### Current Bundle / Surface Status

- `resources/profiles/UltiMaker.json` is currently pruned so incomplete `UltiMaker S3`, `UltiMaker S5`, `UltiMaker S7`, and `UltiMaker 2+ Connect` models are removed from the active shipped/testing bundle.
- The backup snapshot for the pre-pruning manifest is committed as `resources/profiles/UltiMaker.json.setup_wizard_backup`.
- This pruning was done deliberately at the root-manifest level, not by deleting the underlying machine/process JSON files.
- MakerBot Sketch default machine/process tuning has now had its first selective backport into source-controlled defaults.

### Implementation Summary

**Machine Profiles Created:**
- **UltiMaker Factor 4**: 0.25, 0.4, 0.6, 0.8 nozzle variants
- **UltiMaker S3**: 0.25, 0.4, 0.6, 0.8 nozzle variants
- **UltiMaker S5**: 0.25, 0.4, 0.6, 0.8 nozzle variants
- **UltiMaker S6**: 0.25, 0.4, 0.6, 0.8 nozzle variants
- **UltiMaker S7**: 0.25, 0.4, 0.6, 0.8 nozzle variants
- **UltiMaker S8**: 0.25, 0.4, 0.6, 0.8 nozzle variants

**Process Profiles Created:**
- **0.15mm Standard** for 0.25mm nozzle variants
- **0.20mm Standard** for 0.4mm, 0.6mm, and 0.8mm nozzle variants
- Grouped by printer family: S3-S5-S7, S6-S8, Factor 4

**Key Features:**
- Each machine profile includes `FORMAT_CONFIG_ID` for container export
- Process profiles use `compatible_printers` filtering by nozzle size
- Filament presets include `MATERIAL_GUID` for UltiMaker material identification
- Two-pass filament search implemented to prioritize GUID-matched presets

### Filament Compatibility System

**MATERIAL_GUID Integration:**
- UltiMaker filament presets store GUID in `filament_notes` field
- `BackgroundSlicingProcess.cpp` implements two-pass search:
  1. First pass: Find presets with matching `MATERIAL_GUID`
  2. Second pass: Fallback to `filament_type` matching
- Eliminates "fallback to generic filament" warnings in logs
- Ensures UltiMaker materials are correctly identified by GUID first

### UltiMaker Material GUID Status

| Material Name | GUID Status | GUID Value | Notes |
|---------------|-------------|------------|-------|
| UltiMaker Tough PLA | ✅ Has GUID | `2c31d5ae-a75c-4be6-83bf-377341fc6d24` | Valid GUID from Cura AnyColor profiles |
| UltiMaker PLA | ✅ Has GUID | `5b890432-a9f1-45e4-aad7-a73995600276` | Valid GUID from Cura AnyColor profiles |
| UltiMaker PETG | ✅ Has GUID | `91bd2402-1766-4cb0-9b21-6435e5095395` | Valid GUID from Cura AnyColor profiles |
| UltiMaker ABS | ✅ Has GUID | `94209c78-8d4d-4866-8a60-5e1f7adb0c36` | Valid GUID from Cura AnyColor profiles |
| UltiMaker PPS-CF | ⚠️ Missing GUID | `unknown (not available in Cura AnyColor profiles)` | GUID not available in reference profiles |
| UltiMaker PET-CF | ⚠️ Missing GUID | `unknown (not available in Cura AnyColor profiles)` | GUID not available in reference profiles |
| MakerBot Sketch PLA | ✅ Has GUID | `abb9c58e-1f56-48d1-bd8f-055fde3a5b56` | Valid GUID with MATERIAL_CODE:pla |
| MakerBot Sketch Tough PLA | ✅ Has GUID | `de031137-a8ca-4a72-bd1b-17bb964033ad` | Valid GUID with MATERIAL_CODE:im-pla |
| MakerBot Sketch Metallic PLA | ✅ Has GUID | `3fac1543-dd0c-462d-9cbc-d94137d43999` | Valid GUID with MATERIAL_CODE:metallic-pla |

**Note:** The two-pass filament search in `BackgroundSlicingProcess.cpp` will prioritize presets with MATERIAL_GUID, falling back to `filament_type` matching for materials without GUIDs.

### Known Issues

**1. Printer Extruder Variant (deferred to future major project):**
- All nozzle-specific machine profiles currently have `printer_extruder_variant: ["AA+ 0.4", "AA+ 0.4"]` regardless of actual nozzle size
- This needs deeper analysis of UltiMaker core naming conventions and will be addressed in a dedicated profile cleanup project

**2. Print Process Profiles:
- Currently exist for 0.2mm Standard but are not yet validated or updated
- Exist only as basic templates that need selection and validation
- **Known bug**: Changing print core size does not change possible print process selections

**3. UltiMaker Brand Materials:**
- Still need proper association matrix against printer models and print core options
- Material compatibility needs to be validated for each printer model

### Implementation Files

**Machine Profiles:**
- `resources/profiles/UltiMaker/machine/UltiMaker Factor 4 0.25 nozzle.json`
- `resources/profiles/UltiMaker/machine/UltiMaker S3 0.4 nozzle.json`
- `resources/profiles/UltiMaker/machine/UltiMaker S5 0.6 nozzle.json`
- `resources/profiles/UltiMaker/machine/UltiMaker S6 0.8 nozzle.json`
- `resources/profiles/UltiMaker/machine/UltiMaker S7 0.25 nozzle.json`
- `resources/profiles/UltiMaker/machine/UltiMaker S8 0.4 nozzle.json`

**Process Profiles:**
- `resources/profiles/UltiMaker/process/0.15mm Standard @UltiMaker Factor 4 0.25 nozzle.json`
- `resources/profiles/UltiMaker/process/0.15mm Standard @UltiMaker S3-S5-S7 0.25 nozzle.json`
- `resources/profiles/UltiMaker/process/0.15mm Standard @UltiMaker S6-S8 0.25 nozzle.json`
- `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker Factor 4 0.4 nozzle.json`
- `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle.json`
- `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S6-S8 0.4 nozzle.json`
- `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker Factor 4 0.6 nozzle.json`
- `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker Factor 4 0.8 nozzle.json`
- `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S3-S5-S7 0.6 nozzle.json`
- `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S3-S5-S7 0.8 nozzle.json`
- `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S6-S8 0.6 nozzle.json`
- `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S6-S8 0.8 nozzle.json`

**Code Changes:**
- `src/slic3r/GUI/BackgroundSlicingProcess.cpp` - Two-pass filament search
- `src/slic3r/Utils/UltiMaker.cpp` - Upload integration with container format
- `src/slic3r/Utils/UltiMakerLAN.cpp` - LAN printing implementation
- `src/libslic3r/Format/FormatConfig.cpp` - FORMAT_CONFIG_ID parsing
- `src/slic3r/GUI/GUI_App.cpp` - FT_UFP/FT_MAKERBOT file type enums

### Testing Status

- ✅ **Setup Wizard**: Can add UltiMaker S6 printer successfully
- ✅ **Process Selection**: Nozzle-specific process profiles appear correctly
- ✅ **Filament Selection**: GUID-based filament matching works
- ✅ **Export**: .ufp container export works with FORMAT_CONFIG_ID
- ✅ **Upload**: Digital Factory upload with container conversion works
- ✅ **LAN Printing**: UltiMaker LAN upload and print works
- ⚠️ **Core Variant**: `printer_extruder_variant` values need review (deferred to future project)
- ⚠️ **UltiMaker Digital Factory Auth**: refreshed implementation is compiled and pushed, but still needs broader live validation after the storage/refresh cleanup
- ⚠️ **UltiMaker LAN Browse**: persistent picker behavior and IP-only selection are compiled and pushed, but still need broader validation on real LAN environments

### Reference Files

- `resources/profiles/Flashforge/machine/` - Nozzle variant pattern
- `resources/profiles/UltiMaker.json` - Existing UltiMaker structure

### Profile Storage Locations

**System Profiles** (for development/testing - installed OrcaSlicer):
- Path: `C:\Users\<username>\AppData\Roaming\OrcaSlicer\system\`
- Structure mirrors `resources/profiles/`:
  - Machine profiles: `system/UltiMaker/machine/`
  - Filament profiles: `system/OrcaFilamentLibrary/filament/MakerBot/`
  - Filament profiles: `system/OrcaFilamentLibrary/filament/UltiMaker/`

**User Profiles** (user-created presets via UI):
- Location: `C:\Users\<username>\AppData\Roaming\OrcaSlicer\user\<preset_folder>\`
- Subfolders: `machine/`, `filament/`, `process/`
- Note: These are for user-created presets, NOT for development profile testing

### Roaming `- Copy` Preset Workflow

When tuning "default" UltiMaker or MakerBot values through the OrcaSlicer UI, the active working files are usually user presets under:

- `C:\Users\<username>\AppData\Roaming\OrcaSlicer\user\default\machine\`
- `C:\Users\<username>\AppData\Roaming\OrcaSlicer\user\default\process\`
- `C:\Users\<username>\AppData\Roaming\OrcaSlicer\user\default\filament\`

Important rules for these files:

- `- Copy.json` presets are **delta presets**, not clean source presets.
- They are valid for tuning and comparison, but they must **not** be copied directly into `resources/profiles/`.
- Always compare a roaming `- Copy` preset against the correct source-controlled counterpart and backport only the meaningful tuning values.

### Backport Mapping Rules

- **Machine copy presets** should be compared against the specific nozzle preset first, then against the printer family common file if the change is family-wide.
  - Example: `UltiMaker S6 0.4 nozzle - Copy.json` maps first to `resources/profiles/UltiMaker/machine/UltiMaker S6 0.4 nozzle.json`
  - Family-wide machine defaults often belong in `resources/profiles/UltiMaker/machine/fdm_ultimaker_s68_common.json`
- **Process copy presets** often inherit from a lightweight instanced process file whose real defaults live in a common file.
  - Example: `0.20mm Standard @UltiMaker S6-S8 AA+ 0.4 - Copy.json` maps to:
    - `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S6-S8 AA+ 0.4.json`
    - `resources/profiles/UltiMaker/process/fdm_process_ultimaker_s68_aa+04_common.json`
- **Filament copy presets** should be compared against both `@System` and `@base`.
  - Broad material tuning usually belongs in `@base`
  - Printer compatibility or light system-level scoping usually belongs in `@System`

### Fields That Should Normally Stay Out of Source Profile Backports

- `name`
- `from`
- `inherits`
- `version`
- `*_settings_id`
- `host_type`
- `print_host`
- `print_host_webui`

### Profile Backport Pitfalls

- User presets may contain expanded arrays or UI-generated vector lengths that do not match the intended source profile structure.
- Some useful values in a `- Copy` preset may actually belong in a shared common/base file, not in the instanced profile file with the matching name.
- Connection-specific state from a tuned local preset should never be mistaken for repo default behavior.
- Treat roaming presets as a tuning reference, not as authoritative source files.

### Build Resource Sync Notes

- `build/src/Release/resources` is normally a link/junction to source and tends to stay current automatically.
- `build/OrcaSlicer/resources` can drift stale and may need manual copying when testing packaged-resource behavior.
- When configs are removed or renamed in source, remove stale build-copy configs too so packaged tests do not accidentally pick up obsolete files.

**Source Code Profiles** (for rebuilding app):
- Machine profiles: `resources/profiles/UltiMaker/machine/`
- Filament profiles: `resources/profiles/OrcaFilamentLibrary/filament/MakerBot/`
- Filament profiles: `resources/profiles/OrcaFilamentLibrary/filament/UltiMaker/`
- After copying new profiles here, rebuild the app to test code changes.
- For pure profile / manifest JSON testing, a rebuild is not always required; syncing the changed JSON into the packaged build copy and roaming `system` copy is usually enough, followed by a full OrcaSlicer restart.

### Critical Vendor Manifest Rule

When adding or removing printer models from a vendor root manifest such as `resources/profiles/UltiMaker.json`, treat the file as a **bundle definition**, not as a setup-wizard-only list.

Important dependency rules:

- `machine_model_list` defines the vendor's valid printer models / variants for preset loading.
- `machine_list` contains concrete machine presets that must reference only models still present in `machine_model_list`.
- `process_list` often contains concrete process presets tied to the removed models and should be cleaned up at the same time for consistency.
- Removing an entry from `machine_model_list` **without** also removing the dependent `machine_list` entries can invalidate the vendor bundle and make even still-listed printers disappear.

UltiMaker-specific lesson learned:

- Removing only `UltiMaker S3`, `UltiMaker S5`, `UltiMaker S7`, and `UltiMaker 2+ Connect` from `machine_model_list` successfully hid them from the setup wizard **but also broke all UltiMaker printer availability**, including still-listed models like `S6`.
- The fix was to remove those incomplete models comprehensively from:
  - `machine_model_list`
  - `machine_list`
  - `process_list`
- The underlying machine / process JSON files can remain on disk if the intent is only to disable them temporarily from the shipped vendor bundle.

Safe procedure for temporary model removal:

1. Back up the vendor root manifest first, for example `resources/profiles/UltiMaker.json.setup_wizard_backup`.
2. Remove the target model from `machine_model_list`.
3. Remove all concrete `machine_list` entries whose `printer_model` points at that removed model.
4. Remove the matching concrete `process_list` entries tied only to that removed model family.
5. Sync the updated root manifest to:
   - `build/OrcaSlicer/resources/profiles/<Vendor>.json`
   - `C:\Users\<username>\AppData\Roaming\OrcaSlicer\system\<Vendor>.json`
6. Fully restart OrcaSlicer before testing.

Do **not** assume that hiding a model from the root manifest affects only the setup wizard. It directly affects vendor preset loading.

### File Copying Guidelines for Testing

When copying UltiMaker/MakerBot profiles for testing, follow these rules:

**1. Banned Tools:**
- ❌ **xcopy** - Do not use `xcopy` for file copying operations
- ❌ **robocopy** - Avoid unless absolutely necessary with verification
- ✅ **PowerShell Copy-Item** - Preferred method with explicit verification
- ✅ **Manual verification** - Always verify files were copied successfully

**2. Correct Destination Locations:**
- ✅ **System directory**: `C:\Users\<username>\AppData\Roaming\OrcaSlicer\system\`
- ❌ **User directory**: `C:\Users\<username>\AppData\Roaming\OrcaSlicer\user\*` - This is auto-generated, placing files here is wasted effort

**3. Required Verification Steps:**
1. **Count verification**: Compare file counts between source and destination
2. **Size verification**: Check that file sizes match (not zero bytes)
3. **Content spot-check**: Open 2-3 random files to ensure they contain valid JSON
4. **Path verification**: Confirm files are in correct subdirectories (machine/, process/, etc.)

**4. Copying Procedure Example:**
```powershell
# Copy machine profiles with verification
$source = "resources/profiles/UltiMaker/machine/"
$dest = "C:\Users\$env:USERNAME\AppData\Roaming\OrcaSlicer\system\UltiMaker\machine\"

# Create destination directory if needed
if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest -Force }

# Copy files
Copy-Item -Path "$source\*" -Destination $dest -Recurse -Force

# Verification
$sourceCount = (Get-ChildItem $source -Recurse -File).Count
$destCount = (Get-ChildItem $dest -Recurse -File).Count
Write-Host "Source: $sourceCount files, Destination: $destCount files"
if ($sourceCount -ne $destCount) { throw "File count mismatch!" }
```

**5. Common Pitfalls to Avoid:**
- Copying to `user\*` directories (auto-generated, will be overwritten)
- Using `xcopy` without verification (silent failures)
- Not checking for zero-byte files (copy failures)
- Assuming copy succeeded without verification

**6. Testing After Copy:**
1. Delete the `user` directory: `C:\Users\<username>\AppData\Roaming\OrcaSlicer\user\` — this ensures cached presets are cleared
2. Rebuild the app if source profiles changed
3. Launch OrcaSlicer and verify printers appear in setup wizard
4. Test export functionality with new profiles
5. Check logs for any profile loading errors

---

## Change Log

### 2026-04-01
- **Format Config Cleanup**
  - Renamed `ultimaker_2pc.json` → `ultimaker2_plus_connect.json` and updated internal `printer_name`/`target_machine` to "Ultimaker 2+ Connect"
  - Deleted orphan `ultimaker_f4.json`
  - Added `sketch_large` to `makerbot/manifest.json`
  - Removed unused `method_x.json` and `method_xl.json` MakerBot configs (reserved in code for future use)
  - Removed 960×1460 thumbnail from `ContainerFormatHelper` default fallback
  - Corrected `AGENTS_JATMN.md` LAN auth description to match actual implementation

### 2026-04-14
- **Cheetah Calibration / Motion / Preview Refresh**
  - Centralized Cheetah `M400` + `M214 D0 K...` handling in the writer path
  - Added Cheetah preview parsing for `M214` and `M215`
  - Fixed Cheetah material PA disable-to-zero behavior
  - Improved Cheetah cornering test generation so tower variants use coarse visible `M215` bands instead of an almost invisible micro-gradient
  - Added Cheetah-aware calibration dialog guidance and safer test selection behavior
- **UltiMaker Host UI Refresh**
  - Fixed top-bar hit testing issue that affected menu access
  - Improved Physical Printer dialog behavior for UltiMaker host types
  - Device-tab web UI routing now points Digital Factory to `https://digitalfactory.ultimaker.com` and LAN printers to `http://<ip>/`
  - Added UltiMaker LAN support note clarifying supported printer families
- **Container Writer Cleanup**
  - Fixed UFP writer retraction injection at the wrong stop marker
  - Cleaned MakerBot/UFP writer logging and container helper plumbing
  - Fixed MakerBot single-thumbnail routing and made UFP machine bounds config-driven
  - Cleaned stale packaged resource configs after syncing copied format configs
- **Profile / UI Cleanup**
  - Print core labels in Prepare were restyled to match the rest of the UI
  - Extruder/process UI consistency fixes for Cheetah and print-core selection are now reflected in the current local state

### 2026-04-15
- **UltiMaker Root Manifest Dependency Rule**
  - Confirmed that `resources/profiles/UltiMaker.json` is not a setup-wizard-only list; it is part of the real vendor bundle definition.
  - Confirmed that removing entries only from `machine_model_list` can invalidate the whole UltiMaker vendor bundle because `machine_list` printer presets are validated against the remaining model list.
  - Documented the safe temporary-removal procedure: when disabling incomplete UltiMaker models, remove them consistently from `machine_model_list`, `machine_list`, and related `process_list` entries, then sync the manifest copies and restart OrcaSlicer.
  - The active/public fork state now removes incomplete `S3`, `S5`, `S7`, and `2+ Connect` models from the active UltiMaker bundle while leaving the underlying JSON files on disk.
- **MakerBot Sketch Default Profile Backport**
  - First selective default-profile backport landed for standard MakerBot Sketch.
  - `MakerBot Sketch 0.40.json` was kept single-extruder and had the incorrect copied `T1` heater start-gcode line removed.
  - `fdm_process_common_sketch.json` received the corresponding standard Sketch process tuning cleanup.
- **Cross-Platform / Secure Storage**
  - Linux secure storage support was made optional so the project should still configure/build without `libsecret`.
  - UltiMaker Digital Factory refresh tokens now follow a safer storage policy: secure storage when available, no plaintext JSON refresh-token persistence.
- **UltiMaker Host / Discovery Refresh**
  - UltiMaker Digital Factory refresh behavior was reduced to an on-demand single sync refresh attempt to avoid long UI freezes from repeated retries.
  - `OAuthJob` now tolerates refresh responses that omit `refresh_token`.
  - UltiMaker LAN browse now stores the discovered printer IP address instead of Bonjour `full_address`.
  - UltiMaker LAN browse now behaves persistently while the dialog is open by continuously re-running short discovery passes, closer to Cura's ongoing Zeroconf discovery behavior.
  - LAN host / Device-tab URL building now sanitizes stale legacy host values containing paths, query strings, or extra slashes.

### 2026-04-12 to 2026-04-13
- **Cheetah Flavor Baseline**
  - Added UltiMaker/Cheetah G-code flavor support
  - Fixed Cheetah flavor persistence so it no longer saves back as RepRap
  - Fixed duplicate extruder tabs and gcode-flavor dropdown behavior for Cheetah printers
  - Added and then trimmed temporary debugging around Motion Ability tab issues
  - Improved UI consistency when switching between Filament and Process tabs on Cheetah setups

### 2026-04-02 to 2026-04-09
- **UltiMaker Profile / UFP Improvements**
  - Added proof-of-concept dynamic process matching by print core
  - Generated a fuller core-specific process preset matrix
  - Refreshed `ExtruderVariantWidget` behavior on nozzle/core switching
  - Isolated process inheritance per core variant
  - Fixed UFPWriter material GUID/name issues and later fixed multi-material / paths-with-spaces UFP export issues

### 2026-03-31
- **MakerBot Thumbnail Generation Fix** (commit `762ff9d3ef`)
  - Added `ContainerFormatHelper` class to manage format-specific thumbnail requirements
  - Fixed thumbnail generation to use `render_thumbnails()` callback for proper sizing
  - Updated MakerBot format configs to remove oversized 960x1460 thumbnail (exceeded 1000px limit)
  - Fixed MakerBot printer profiles to use proper PNG format specifier (`120x120/PNG` format)
  - Renamed `build_ufp_container()` to `build_container_format()` for clarity
  - Support both UFP and MakerBot container formats with proper thumbnail handling
