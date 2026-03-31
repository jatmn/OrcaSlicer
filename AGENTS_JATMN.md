# OrcaSlicer Agent Guide (jatmn's Fork)

> **Last Updated:** 2026-03-29
>
> **Change Tracking:** This file tracks implementation status specific to jatmn's fork. See the [Change Log](#change-log) section at the end for revision history.

## Project Context

This is **jatmn's fork** of [OrcaSlicer](https://github.com/jatmn/OrcaSlicer), a 3D printer slicer based on BambuStudio.

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

- **No Linux tools available** — this project is developed on Windows 11. Common Linux commands like `grep`, `find`, `ls`, `cat`, and similar CLI tools are **NOT available** in the terminal.
- **Use agent tools instead** — use the provided `search_files`, `list_files`, `read_file`, and `list_code_definition_names` tools for all file operations. These work cross-platform and are the proper way to search/navigate the codebase.
- **Avoid execute_command for file searches** — do not try to run `grep`, `find`, or similar commands via `execute_command`. Use the dedicated search tools.

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
| Container Format Export (.ufp) | ✅ Completed | Works for all UltiMaker printers |
| Container Format Export (.makerbot) | ⚠️ Needs Work | Writer needs redo due to UFP writer changes |
| Digital Factory Upload | ✅ Completed | Two-step upload with container conversion |
| LAN Printing | ✅ Completed | Does not support UltiMaker 2+ Connect (firmware limitation) |
| Printer Profiles | ⚠️ In Progress | Created but have known issues |
| Process Profiles | ⚠️ In Progress | Basic templates exist, need validation |
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
| `ultimaker2_plus_connect` | UltiMaker | `.ufp` | ⚠️ MISMATCH: file is `ultimaker_2pc.json` |
| `sketch_small` | MakerBot | `.makerbot` | `resources/formats/makerbot/sketch_small.json` |
| `sketch_sprint` | MakerBot | `.makerbot` | `resources/formats/makerbot/sketch_sprint.json` |
| `sketch_large` | MakerBot | `.makerbot` | `resources/formats/makerbot/sketch_large.json` |
| `method_x` | MakerBot | `.makerbot` | `resources/formats/makerbot/method_x.json` |
| `method_xl` | MakerBot | `.makerbot` | `resources/formats/makerbot/method_xl.json` |

**Orphan Files** (exist but not used by any printer):
- `ultimaker_f4.json` — duplicate of `ultimaker_factor4.json`

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

### Known Issues

**FORMAT_CONFIG_ID Mismatch:**
- `ultimaker2_plus_connect` printer uses ID `ultimaker2_plus_connect` but config file is named `ultimaker_2pc.json`
- Fix options: rename printer ID to `ultimaker_2pc` OR rename config file to `ultimaker2_plus_connect.json`

**Orphan Config File:**
- `ultimaker_f4.json` exists but no printer uses it (duplicate of `ultimaker_factor4.json`)
- Recommendation: delete `ultimaker_f4.json` to avoid confusion

**MakerBot Format:**
- `.makerbot` writer needs to be redone due to considerable changes in the UFP writer implementation

---

## UltiMaker Digital Factory Integration

### Status: ✅ COMPLETED

Upload to UltiMaker Digital Factory with container conversion is fully implemented.

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

File upload to UltiMaker printers over LAN (no cloud, no mDNS) is working.

### Implementation Details

**PrintHost Type**: `htUltiMakerLAN` added to `PrintHostType` enum in `PrintConfig.cpp`

**Class**: `UltiMakerLANPrintHost` implemented in `src/slic3r/Utils/UltiMakerLAN.cpp`
- Extends `PrintHost` base class
- Stores: `host` (IP/hostname), `port` (default 80), `auth_id` (username), `auth_key` (password)
- **Auth flow**: HTTP Digest authentication with 3-step flow:
  1. `POST /api/v1/auth/request` with `application=OrcaSlicer` and `user=<username>`
  2. Poll `GET /api/v1/auth/check/{id}` until user approves on printer
  3. Cache `id`/`key` for digest auth on subsequent calls
- **Upload**: `POST /api/v1/print_job` with multipart file data
- **Print control**: `PUT /api/v1/print_job/state` for pause/resume/abort
- **Status monitoring**: `GET /api/v1/print_job` and `GET /api/v1/printer/status`

**HTTP Digest Implementation**:
- Uses libcurl's native HTTP Digest support via `CURLOPT_USERNAME` and `CURLOPT_PASSWORD`
- No manual digest algorithm implementation needed

**Factory Integration**: Added case in `PrintHost::get_print_host()` for `htUltiMakerLAN`

**UI Integration**:
- "UltiMaker (LAN)" option in host printer type dropdown
- Fields: IP/hostname, username (for printer approval), port (default 80)
- Auth approval flow shows "Check printer screen to approve access"
- Upload progress and print status display

### API Reference (confirmed from `/docs/api/` on S6 at 10.10.10.246)

| Method | Endpoint | Auth | Purpose |
|---|---|---|---|
| `POST` | `/api/v1/auth/request` | None | Request auth credentials |
| `GET` | `/api/v1/auth/check/{id}` | None | Poll for user approval |
| `GET` | `/api/v1/auth/verify` | Digest | Test credentials |
| `GET` | `/api/v1/printer` | None | Full printer state |
| `GET` | `/api/v1/printer/status` | None | Printer status string |
| `GET` | `/api/v1/system/name` | None | Printer display name |
| `POST` | `/api/v1/print_job` | Digest | Upload file + start print |
| `GET` | `/api/v1/print_job` | Digest | Current job status |
| `GET` | `/api/v1/print_job/progress` | Digest | Print progress |
| `PUT` | `/api/v1/print_job/state` | Digest | Pause/resume/abort |
| `GET` | `/api/v1/history/print_jobs` | Digest | Print history |

**Authentication:** HTTP Digest (RFC 2617) — no fallback to basic. Auth flow: `POST /auth/request` → poll `GET /auth/check/{id}` → user approves on printer → use returned `id`/`key` as digest username/password for all subsequent calls.

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
- Authentication flow requires user approval on printer screen
- Print job control (pause/resume/abort) implemented

### Supported Printers

- ✅ UltiMaker S Series (S3, S5, S6, S7, S8)
- ✅ UltiMaker Factor 4
- ❌ UltiMaker 2+ Connect — **Firmware limitation** (not a software issue)
- ❌ MakerBot Sketch/Method series (not supported)

### Limitations

- ✅ File upload to printer
- ✅ Basic print job control (pause/resume/abort)
- ⚠️ Browse for local printers feature (exists but not validated)
- ❌ Direct printer control beyond basic commands
- ❌ Print management (job queue, history)
- ❌ Monitoring printer status in real-time
- ❌ Live camera feed support

---

## UltiMaker & MakerBot Printer Profiles

### Status: ⚠️ IN PROGRESS

All UltiMaker and MakerBot printer profiles have been created but have known issues that need resolution.

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

**Note:** The two-pass filament search in `BackgroundSlicingProcess.cpp` will prioritize presets with MATERIAL_GUID, falling back to `filament_type` matching for materials without GUIDs.

### Known Issues

**1. Printer Extruder Variant (paused pending user decision):**
- All nozzle-specific machine profiles currently have `printer_extruder_variant: ["AA+ 0.4", "AA+ 0.4"]` regardless of actual nozzle size
- This needs deeper analysis of UltiMaker core naming conventions:
  - Should 0.25mm nozzle be "AA 0.25" or "AA+ 0.25"?
  - Should 0.6mm nozzle be "CC 0.6" or "CC+ 0.6"?
  - Should 0.8mm nozzle be "BB 0.8"?

**2. Print Process Profiles:**
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
- ⚠️ **Core Variant**: `printer_extruder_variant` values need review

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
- Subfolders: `machine/`, `filament/`, `print/`
- Note: These are for user-created presets, NOT for development profile testing

**Source Code Profiles** (for rebuilding app):
- Machine profiles: `resources/profiles/UltiMaker/machine/`
- Filament profiles: `resources/profiles/OrcaFilamentLibrary/filament/MakerBot/`
- Filament profiles: `resources/profiles/OrcaFilamentLibrary/filament/UltiMaker/`
- After copying new profiles here, rebuild the app to test changes

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
1. Rebuild the app if source profiles changed
2. Launch OrcaSlicer and verify printers appear in setup wizard
3. Test export functionality with new profiles
4. Check logs for any profile loading errors