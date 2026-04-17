# OrcaSlicer Agent Guide (jatmn's Fork)

> **Last Updated:** 2026-04-17
>
> **Change Tracking:** This file tracks implementation status specific to jatmn's fork. See the [Change Log](#change-log) section at the end for revision history.
> When older sections conflict with the current-status audit below, the current-status audit wins.

## Table of Contents

- [Project Context](#project-context)
- [Current Status Audit (2026-04-17)](#current-status-audit-2026-04-17)
  - [Public Fork Status (`origin/main`)](#public-fork-status-originmain)
- [Local-Only Status (`HEAD` now has local follow-up commits)](#local-only-status-head-now-has-local-follow-up-commits)
  - [Working Tree Follow-Ups (`git status` only)](#working-tree-follow-ups-git-status-only)
  - [Active Workstreams](#active-workstreams)
  - [Current PR Guidance](#current-pr-guidance)
- [UltiMaker Integration Status Overview](#ultimaker-integration-status-overview)
- [Container Format Export (.ufp and .makerbot)](#container-format-export-ufp-and-makerbot)
  - [Status: ✅ COMPLETED (with known issues)](#status--completed-with-known-issues)
  - [FORMAT_CONFIG_ID System](#format_config_id-system)
  - [How It Works](#how-it-works)
  - [Implementation Details](#implementation-details)
  - [Known Issues](#known-issues)
- [UltiMaker Digital Factory Integration](#ultimaker-digital-factory-integration)
  - [Status: ✅ COMPLETED](#status--completed)
  - [Authentication / Token Handling Notes](#authentication--token-handling-notes)
  - [Upload Integration](#upload-integration)
  - [Supported Features](#supported-features)
  - [Limitations](#limitations)
  - [UI Integration](#ui-integration)
- [UltiMaker LAN Printing](#ultimaker-lan-printing)
  - [Status: ✅ COMPLETED (with limitations)](#status--completed-with-limitations)
  - [Implementation Details](#implementation-details-1)
  - [API Reference (implemented endpoints)](#api-reference-implemented-endpoints)
  - [Key Implementation Files](#key-implementation-files)
  - [Testing](#testing)
  - [Supported Printers](#supported-printers)
  - [Limitations](#limitations-1)
- [UltiMaker & MakerBot Printer Profiles](#ultimaker--makerbot-printer-profiles)
  - [Status: ⚠️ IN PROGRESS](#status--in-progress)
  - [Current Bundle / Surface Status](#current-bundle--surface-status)
  - [Implementation Summary](#implementation-summary)
  - [Filament Compatibility System](#filament-compatibility-system)
  - [UltiMaker Material GUID Status](#ultimaker-material-guid-status)
  - [Known Issues](#known-issues-1)
  - [Implementation Files](#implementation-files)
  - [Testing Status](#testing-status)
  - [Reference Files](#reference-files)
- [Development Workflow & References](#development-workflow--references)
  - [Reference Codebases](#reference-codebases)
  - [Important Constraints](#important-constraints)
  - [Windows 11 Development](#windows-11-development)
  - [Debugging & Logging](#debugging--logging)
  - [Efficient Codebase Navigation](#efficient-codebase-navigation)
  - [Context Management Rules](#context-management-rules)
  - [Profile Storage Locations](#profile-storage-locations)
  - [Roaming `- Copy` Preset Workflow](#roaming---copy-preset-workflow)
  - [Backport Mapping Rules](#backport-mapping-rules)
  - [Fields That Should Normally Stay Out of Source Profile Backports](#fields-that-should-normally-stay-out-of-source-profile-backports)
  - [Profile Backport Pitfalls](#profile-backport-pitfalls)
  - [Build Resource Sync Notes](#build-resource-sync-notes)
  - [Critical Vendor Manifest Rule](#critical-vendor-manifest-rule)
  - [File Copying Guidelines for Testing](#file-copying-guidelines-for-testing)
- [Change Log](#change-log)

## Project Context

This is **jatmn's fork** of [OrcaSlicer](https://github.com/jatmn/OrcaSlicer), a 3D printer slicer based on BambuStudio.

### Executive Summary

- The fork's primary focus is adding and hardening **UltiMaker**, **MakerBot**, and **Cheetah firmware** support on top of upstream OrcaSlicer.
- The largest completed areas are:
  - container-format export for `.ufp` and `.makerbot`
  - UltiMaker Digital Factory upload
  - UltiMaker LAN printing
  - baseline UltiMaker / MakerBot profile families
  - baseline Cheetah gcode-flavor support
  - a currently working macOS / Apple Silicon (`arm64`) build path on a local M1 after recent compatibility fixes
- The largest active work areas are:
  - tuning default UltiMaker and MakerBot machine / process / material presets
  - validating Cheetah motion and calibration behavior on real hardware
  - validating dual-extrusion behavior on UltiMaker S/F series
  - tightening profile compatibility and material/core association rules
- This guide is intended to serve two purposes:
  - a current-status record of what jatmn's fork already does
  - an execution guide for resuming work, preparing PRs, and avoiding repeated mistakes in profiles, manifests, auth, and container-format handling
- A significant portion of the fork's implementation and investigation work has been performed with the help of AI coding assistants / agent-style tools, including:
  - OpenCode
  - Cline
  - Klio Code
  - Kimi Code
  - Codex
- AI models used during implementation, debugging, review, and iteration have included, in no specific order:
  - DeepSeek v3.2
  - GLM-5
  - MiniMax 2.5
  - MiniMax 2.7
  - Kimi K2.5
  - Kimi K2.5-Turbo
  - Kimi K2.6
  - Claude Sonnet
  - Claude Haiku
  - GPT-5.4
- Earlier proof-of-concept work, especially around Python postprocessors, also involved:
  - Gemini 2.5
  - Gemini 2.5 Flash
  - Gemini 3.0

## Current Status Audit (2026-04-17)

This section is the current high-level truth for the fork and should be used as the starting point when preparing PRs or resuming work.

### Public Fork Status (`origin/main`)

- Public fork head currently includes work through commit `10e6fe2dc7`.
- UltiMaker Digital Factory upload, UltiMaker LAN printing, `.ufp` export, `.makerbot` export, MakerBot Sketch profiles, UltiMaker S/F printer families, core-specific process matrix groundwork, and baseline Cheetah flavor support are already on the public fork.
- Public fork also includes recent Cheetah fixes for gcode flavor persistence, duplicate extruder tabs, Filament/Process tab consistency, UFP material GUID/name fixes, UFP path / multi-material export fixes, the temporary UltiMaker root-manifest pruning for incomplete models, MakerBot Sketch default-profile backports, optional Linux secure storage fallback, and the latest UltiMaker auth / LAN discovery refresh.
- Public fork now also includes the `95bc9c4257` macOS / `arm64` compatibility pass: the bundled `miniz` snapshot was reconciled with its header signature, `Http::allow_tls_flexible()` was made linkable cross-platform, `UltiMakerLAN` now guards Windows-only SSL revoke behavior correctly, and the `ExtruderVariantWidget` / `PrintHostDialogs` compile blockers were cleaned up for current Apple toolchains.
- The follow-up `cd0a2d7428` cleanup commit removed the temporary macOS build-plan scratch file after the build fixes landed.

### Local-Only Status (`HEAD` now has local follow-up commits)

- `HEAD` now carries three local-only follow-up commits that are not yet on `origin/main`.
- The public fork already contains the main Method-family export groundwork, vendor-bundle profile slice, Method-family compatibility flow, and first Prepare-side Method variant-selection path.
- The local-only follow-up stack currently includes:
  - adds explicit two-entry `nozzle_diameter` arrays across the shipped Method machine presets so the dual-extruder instances advertise both `0.4` nozzles consistently
  - keeps the real Method `thumbnail_960x1460.png` requirement in `resources/formats/makerbot/method*.json` instead of the shared printer preset `thumbnails` field, avoiding Orca's `>= 1000` preset-thumbnail validation failure
  - adds missing setup-wizard cover art for Method / Method X / Method XL under `resources/profiles/UltiMaker/`
  - refreshes the status/plan documents so they reflect the current public-vs-local split
  - refines shared UltiMaker / Method process-matrix handling in `ExtruderVariantWidget` and active machine presets
  - backports tuned UltiMaker S6 defaults from roaming `- Copy` presets into source-controlled machine/process profiles
- The committed matrix follow-up includes:
  - `ExtruderVariantWidget` now distinguishes only `UltiMaker` and `Method` matrix modes, preferring explicit `PROCESS_MATRIX_TYPE:*` tags in `printer_notes`
  - active UltiMaker S3 / S5 / S6 / S7 / S8 and Factor 4 nozzle presets now carry `PROCESS_MATRIX_TYPE:ultimaker` plus `PROCESS_MATRIX_CONTROL_SLOT:*`
  - active Method / Method X / Method XL machine-instance presets now carry `PROCESS_MATRIX_TYPE:method` plus `PROCESS_MATRIX_CONTROL_SLOT:1`
  - the widget now derives available core / extruder choices from visible process preset `compatible_printers_condition` rules instead of hard-coded printer-model lists
  - process visibility/remapping is now constrained by the active printer + matrix + selected variant combination for these tagged UltiMaker / Method presets
  - non-UltiMaker / non-MakerBot printers stay on the existing non-matrix path and are not intended to be affected by this refactor
- The committed S6-default backport currently covers:
  - `resources/profiles/UltiMaker/machine/UltiMaker S6 0.4 nozzle.json`
  - `resources/profiles/UltiMaker/process/fdm_process_ultimaker_s68_aa+04_common.json`
  - tuned start/end G-code, motion/retraction defaults, and the shared S6/S8 `AA+ 0.4` process baseline sourced from the roaming S6 tuning pass
- Local compile validation has been rerun after the matrix/tag refactor, S6 default backport, and latest UI leak fix:
  - `cmake --build build --target OrcaSlicer --config Release --parallel 4` succeeded in this workspace
- The current shipped Method slice remains intentionally conservative:
  - Method ships `1A`, `1C`, and `LABS`
  - Method X / XL ship `1XA`, `1C`, and `LABS`
  - Method X / XL also ship baked mixed `1XA+2XA`, `1C+2XA`, and `LABS+2XA` leaves with slot 0 as the build tool and slot 1 as the `2XA` support tool
  - plain Method `2A` plus any broader support-tool exposure are still deferred because the shipped preset/UI workflow is still incomplete
- Full app/runtime validation and real printer acceptance testing for the Method-family workflow remain pending.
- Treat the current AGENTS file as the authoritative status record for the public fork state plus any explicitly documented local follow-ups.

### Working Tree Follow-Ups (`git status` only)

- The workspace currently has one uncommitted UI leak-fix follow-up in:
  - `src/slic3r/GUI/Plater.cpp`
- That working-tree change converts `Sidebar::priv::timer_sync_printer` from an unmanaged heap allocation to `std::unique_ptr<wxTimer>` after a review of fork-specific UI ownership paths found that this timer was never freed.
- If local-only tool files remain outside this commit, treat them as non-repo workspace state unless they are explicitly needed for Method-family work.

### Active Workstreams

- Cheetah support is now beyond proof-of-concept and into tuning / validation.
- UltiMaker default machine, process, and material values are still being actively tuned.
- Roaming-profile `- Copy` presets are currently the working area for tuned defaults before backporting them into source-controlled profiles.
- A first tuned S-series backport has now landed locally for `UltiMaker S6 0.4 nozzle`; broader S6 nozzle/process coverage plus S8 follow-on tuning are still pending.
- Dual-extrusion validation on UltiMaker S/F series is still incomplete even though the profile and container groundwork exists.
- UltiMaker setup-wizard / vendor-manifest behavior has a newly documented dependency rule: incomplete model removal must be done consistently across all root lists in the vendor manifest or the entire vendor bundle may stop loading correctly.
- UltiMaker Digital Factory auth now more closely mirrors Cura's behavior, but it still needs live user validation after the token-storage and refresh-path cleanup.
- UltiMaker LAN browse now behaves more like Cura with persistent discovery while the picker is open, but broader long-session validation is still needed.
- MakerBot Method-family export now has a first native C++ groundwork pass locally plus an expanding vendor-bundle machine/process wiring pass and a new data-driven Prepare-side matrix path; tuned defaults and printer validation still need follow-through.
- A fork-specific UI ownership review has now been run against the recent UltiMaker / Method / host-dialog work; one real timer leak was found in `Plater.cpp` and is now fixed locally, while no other obvious leak regressions were identified in the reviewed fork-only UI paths.
- Printer-profile completeness is intentionally uneven right now:
  - some models are real tuned/tunable work in progress
  - some models are only placeholder templates kept in the tree for future work
- some MakerBot Method family entries now have export groundwork and minimal bundle wiring locally, but still do not have finished shipping-quality preset/UI support

### Current PR Guidance

- Treat the fork conceptually as two layers: public fork history in `origin/main`, plus any future local-only commits at `HEAD` when they exist.
- Do not log upstream rebases or imported upstream SoftFever commits in this guide; only track work authored in jatmn's fork.
- Before opening PRs, reconcile any active roaming `- Copy` presets back into source profiles intentionally rather than assuming the current user profile state is reflected in the repo.

## UltiMaker Integration Status Overview

| Feature | Status | Notes |
|---------|--------|-------|
| Container Format Export (.ufp) | ✅ Completed | Works for UltiMaker printers; recent fixes cover GUID/name handling, multi-material export, spaced paths, and wrong stop-marker retraction |
| Container Format Export (.makerbot) | ✅ Completed | Sketch series path is public; local `HEAD` now also contains an initial Method / Method X / Method XL export path, but Method validation is still incomplete |
| Digital Factory Upload | ✅ Completed | Two-step upload with container conversion for both `.ufp` and `.makerbot` formats |
| LAN Printing | ✅ Completed | Supports UltiMaker S series and Factor series; browse/discovery now uses Cura-style persistent scanning while the picker is open |
| Cheetah G-code Flavor | ⚠️ Active Tuning | Baseline support is in; calibration, preview, UI, and motion handling are still being refined |
| Printer Profiles | ⚠️ Active Tuning | Core/profile matrix exists, but several printers are still only placeholders or partially tuned |
| Process Profiles | ⚠️ Active Tuning | Dynamic/core-specific groundwork exists; tuned defaults are still being validated, selectively backported, or still missing for placeholder printers |
| Material GUID Matching | ✅ Completed | Two-pass search prioritizes GUIDs |
| Material Association Matrix | ⚠️ In Progress | Local `HEAD` now has a first-pass Method-family material/variant matrix for shipped Method system filaments; broader UltiMaker and missing Method materials remain pending |

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
| `method` | MakerBot | `.makerbot` | `resources/formats/makerbot/method.json` |
| `method_x` | MakerBot | `.makerbot` | `resources/formats/makerbot/method_x.json` |
| `method_xl` | MakerBot | `.makerbot` | `resources/formats/makerbot/method_xl.json` |

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
- `.makerbot` writer is implemented publicly for Sketch series; local `HEAD` also adds an initial Method-family branch
- `sketch_large` added to `makerbot/manifest.json`
- local Method-family support now restores `method.json`, `method_x.json`, and `method_xl.json`
- local Method-family support writes `print.jsontoolpath` instead of `print.gcode` and converts Orca-emitted G-code into Method-style move / tool-change / temperature / fan / layer-comment commands inside `MakerBotWriter`
- local Method-family support now accepts plain `method` in `FORMAT_CONFIG_ID` routing and `application/x-makerbot` MIME selection
- local Method-family support also passes up to two extruders of injected export metadata through the MakerBot export path and patches Method filament presets with native material GUID / material-code notes
- ContainerFormatHelper class manages thumbnail generation
- Thumbnail format in printer profiles must use `120x120/PNG` format (with PNG specifier)
- the generic MakerBot fallback still avoids oversized thumbnails, but local Method configs explicitly request the real Method `thumbnail_960x1460.png` member
- a first local Method-family vendor-bundle slice now exists with concrete `MakerBot Method 0.40`, `MakerBot Method X 0.40`, and `MakerBot Method XL 0.40` presets plus minimal `0.20mm Standard` process presets
- local `HEAD` now also ships thin `1C` / `LABS` Method-family machine/process leaves plus baked mixed Method X / XL `1XA+2XA`, `1C+2XA`, and `LABS+2XA` support leaves
- Method-family variant selection is no longer completely hidden: local `HEAD` now has a first Method-family `ExtruderVariantWidget` path in Prepare, and the current working tree broadens that into a data-driven matrix path keyed by preset tags and process conditions; runtime/workflow validation is still pending before it should be treated as finished shipping UI
  - first-pass Method-family filament compatibility enforcement now exists locally for the shipped `MakerBot Method Tough PLA`, `ABS-R`, `ABS-CF`, `ASA`, `Nylon CF`, `Nylon 12 CF`, `RapidRinse`, and `SR-30` system presets; broader Method material coverage plus finished UI/process/tuning validation are still pending
- full Method-family printer acceptance testing has not been completed yet, and finished UI/matrix/preset tuning work is still pending

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
- Recent follow-up work also added verbose OAuth callback/token debugging and ensured `Http::allow_tls_flexible()` links on non-Windows platforms so the auth path builds on macOS / `arm64`; broader live-token validation is still needed.

### Upload Integration

- **Two-step upload flow**: `UltiMaker::upload()` creates the appropriate container format before uploading to Digital Factory
- **Process**: 
  1. Export G-code to temp file
  2. Convert to the printer's required container format using `FormatConfig::export_to_container()`
  3. Upload container to Digital Factory via existing HTTP POST
  4. Cleanup temp files after upload
- **Key commit**: `b69768c0dc` — UltiMaker upload: Add two-step upload flow with container format support
- **Helper method**: `Http::set_put_body_raw()` — allows sending raw file data for container uploads

### Supported Features

- ✅ Project folder listing and selection
- ✅ New Project folder creation and selection
- ✅ Upload of `.ufp` and `.makerbot` files through container-format conversion
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

The active UltiMaker and MakerBot profile surface is partially wired and still has known issues, validation gaps, and incomplete families.

### Current Bundle / Surface Status

- `resources/profiles/UltiMaker.json` is currently pruned so incomplete `UltiMaker S3`, `UltiMaker S5`, `UltiMaker S7`, and `UltiMaker 2+ Connect` models are removed from the active shipped/testing bundle.
- The backup snapshot for the pre-pruning manifest is committed as `resources/profiles/UltiMaker.json.setup_wizard_backup`.
- This pruning was done deliberately at the root-manifest level, not by deleting the underlying machine/process JSON files.
- MakerBot Sketch default machine/process tuning has now had its first selective backport into source-controlled defaults.
- Local `HEAD` now also adds a first MakerBot Method-family bundle slice under `resources/profiles/UltiMaker/`:
  - `MakerBot Method`
  - `MakerBot Method X`
  - `MakerBot Method XL`
- The current Method-family bundle slice is intentionally minimal:
  - one concrete `0.40` baseline machine preset per printer family
  - one minimal `0.20mm Standard` baseline process preset per printer family
  - thin `1C` / `LABS` leaves across Method / Method X / Method XL
  - baked mixed `2XA` support leaves across Method X / XL
  - a first Method-family Prepare-side UI path plus a current working-tree matrix refactor, but not yet a fully validated shipping workflow
- The current working tree now also rolls explicit process-matrix tags onto the active presets:
  - active UltiMaker S3 / S5 / S6 / S7 / S8 nozzle presets use `PROCESS_MATRIX_TYPE:ultimaker`
  - active UltiMaker Factor 4 nozzle presets use `PROCESS_MATRIX_TYPE:ultimaker`
  - active Method / Method X / Method XL machine-instance presets use `PROCESS_MATRIX_TYPE:method`
  - S-series and Method-family presets advertise `PROCESS_MATRIX_CONTROL_SLOT:1`, while Factor 4 advertises `PROCESS_MATRIX_CONTROL_SLOT:2`
- Those tags are intended to let future UltiMaker / Method models join the same matrix behavior through preset data instead of widget-side printer-name lists.
- Several printer families in-tree should still be understood as placeholders or partial implementations, not finished shipping-quality defaults.

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
- Active UltiMaker / Method matrix-tagged machine presets now also carry `PROCESS_MATRIX_TYPE` and `PROCESS_MATRIX_CONTROL_SLOT` notes
- Process profiles use `compatible_printers` / `compatible_printers_condition` filtering by nozzle size and active variant selection
- `ExtruderVariantWidget` now derives available variant choices from visible process compatibility conditions for the tagged UltiMaker / Method families instead of maintaining per-model widget lists
- Filament presets include `MATERIAL_GUID` for UltiMaker material identification
- Two-pass filament search implemented to prioritize GUID-matched presets
- Local Method-family machine presets now also add `METHOD_PRINTER_FAMILY:<id>` notes so the initial Method process presets can match by family while Method UI support is still incomplete

### Filament Compatibility System

**MATERIAL_GUID Integration:**
- UltiMaker filament presets store GUID in `filament_notes` field
- `BackgroundSlicingProcess.cpp` implements two-pass search:
  1. First pass: Find presets with matching `MATERIAL_GUID`
  2. Second pass: Fallback to `filament_type` matching
- Eliminates "fallback to generic filament" warnings in logs
- Ensures UltiMaker materials are correctly identified by GUID first

**MakerBot Method Family Compatibility (local-only):**
- Local `HEAD` now adds first-pass `compatible_printers_condition` gating to the shipped Method-family `@System` filament presets using two existing Orca hooks:
  - `METHOD_PRINTER_FAMILY:<id>` markers in `printer_notes`
  - `printer_extruder_variant_0`, which Orca now remaps to the active slot during Method-family slot-aware compatibility evaluation in `Preset.cpp`
- The current shipped Method-family matrix encoded in Orca is:
  - `MakerBot Method Tough PLA @System`: plain Method, Method X, and Method XL on `1A`, `1C`, or `LABS`
  - `MakerBot Method ABS-R @System`: Method X and Method XL on `1XA`, `1C`, or `LABS`
  - `MakerBot Method ABS-CF @System`: Method X and Method XL on `1C` or `LABS`
  - `MakerBot Method ASA @System`: Method X and Method XL on `1XA`, `1C`, or `LABS`
- `MakerBot Method Nylon CF @System`: plain Method, Method X, and Method XL on `1C` or `LABS`
- `MakerBot Method Nylon 12 CF @System`: plain Method, Method X, and Method XL on `1C` or `LABS`
- `MakerBot Method RapidRinse @System`: Method X and Method XL on `2XA`
- `MakerBot Method SR-30 @System`: Method X and Method XL on `2XA`
- This matrix is still narrower than Cura's full Method-family support, but it now covers the currently shipped Method presets in Orca.
- The current local limitation is no longer missing `1C` / `LABS` reachability; local `HEAD` now ships concrete `1C` and `LABS` machine/process leaves plus the shared filament-manifest registrations needed for those materials to load.
- Local `HEAD` now also includes a broader baked mixed support-tool slice through Method X / XL `1XA+2XA`, `1C+2XA`, and `LABS+2XA`, with slot 0 left on the build tool so the current process path remains keyed to the print extruder while slot 1 carries `2XA` support materials.
- Those mixed `+2XA` process presets now key compatibility on both `printer_extruder_variant_0` and `printer_extruder_variant_1`, so they stop matching cleanly when slot 1 is switched away from `2XA`.
- The remaining reachability gap is now primarily plain-Method and validation oriented: `2A` exposure still needs real PVA wiring, the new baked `2XA` workflow still needs broader validation, and the new Method-family Prepare-side UI still needs runtime confirmation before it should be treated as finished shipping behavior.
- `Preset.cpp` is no longer the same blocker described in earlier notes: local `HEAD` now evaluates Method-family filament compatibility per slot and remaps `printer_extruder_variant_0` to the active slot during those checks.
- Cura evidence still shows broader Method-family support outside Orca's current shipped inventory, including `PLA`, `PETG`, `Nylon`, `PVA`, `ABS`, `PC-ABS`, `PC-ABS FR`, and selected LABS-only materials.
- Because the new Method-family `ExtruderVariantWidget` path is still only a first local implementation, this compatibility work remains important both as a guardrail for defaults/manual preset edits and as groundwork for a finished end-user variant workflow.

### UltiMaker Material GUID Status

| Material Name | GUID Status | GUID Value | Notes |
|---------------|-------------|------------|-------|
| UltiMaker Tough PLA | ✅ Has GUID | `2c31d5ae-a75c-4be6-83bf-377341fc6d24` | Present in `UltiMaker Tough PLA @base.json`; valid GUID from Cura AnyColor profiles |
| UltiMaker PLA | ✅ Has GUID | `5b890432-a9f1-45e4-aad7-a73995600276` | Present in `UltiMaker PLA @base.json`; valid GUID from Cura AnyColor profiles |
| UltiMaker PETG | ✅ Has GUID | `91bd2402-1766-4cb0-9b21-6435e5095395` | Present in `UltiMaker PETG @base.json`; valid GUID from Cura AnyColor profiles |
| UltiMaker ABS | ✅ Has GUID | `94209c78-8d4d-4866-8a60-5e1f7adb0c36` | Present in `UltiMaker ABS @base.json`; valid GUID from Cura AnyColor profiles |
| UltiMaker PPS-CF | ✅ Has GUID | `d86bf59a-9d10-4a25-99b6-2844e0bc1bfb` | Present in `UltiMaker PPS-CF @base.json` |
| UltiMaker PET-CF | ✅ Has GUID | `f0245d40-3657-4615-b9ab-19fc043944ca` | Present in `UltiMaker PET-CF @base.json` |
| UltiMaker Method Tough PLA | ✅ Has GUID | `de031137-a8ca-4a72-bd1b-17bb964033ad` | Present in `MakerBot Method Tough PLA @base.json`; valid Method-family GUID with `MATERIAL_CODE:im-pla` |
| UltiMaker Method ABS-R | ✅ Has GUID | `88c8919c-6a09-471a-b7b6-e801263d862d` | Present in `MakerBot Method ABS-R @base.json`; valid Method-family GUID with `MATERIAL_CODE:abs-wss1` |
| UltiMaker Method ABS-CF | ✅ Has GUID | `495a0ce5-9daf-4a16-b7b2-06856d82394d` | Present in `MakerBot Method ABS-CF @base.json`; valid Method-family GUID with `MATERIAL_CODE:abs-cf10` |
| UltiMaker Method ASA | ✅ Has GUID | `f79bc612-21eb-482e-ad6c-87d75bdde066` | Present in `MakerBot Method ASA @base.json`; valid Method-family GUID with `MATERIAL_CODE:asa` |
| UltiMaker Method Nylon CF | ✅ Has GUID | `17abb865-ca73-4ccd-aeda-38e294c9c60b` | Present in `MakerBot Method Nylon CF @base.json`; valid Method-family GUID with `MATERIAL_CODE:nylon-cf` |
| UltiMaker Method Nylon 12 CF | ✅ Has GUID | `3c6f2877-71cc-4760-84e6-4b89ab243e3b` | Present in `MakerBot Method Nylon 12 CF @base.json`; valid Method-family GUID with `MATERIAL_CODE:nylon12-cf` |
| UltiMaker Method RapidRinse | ✅ Has GUID | `a140ef8f-4f26-4e73-abe0-cfc29d6d1024` | Present in `MakerBot Method RapidRinse @base.json`; valid Method-family GUID with `MATERIAL_CODE:wss1` |
| UltiMaker Method SR-30 | ✅ Has GUID | `77873465-83a9-4283-bc44-4e542b8eb3eb` | Present in `MakerBot Method SR-30 @base.json`; valid Method-family GUID with `MATERIAL_CODE:sr30` |
| MakerBot Sketch PLA | ✅ Has GUID | `abb9c58e-1f56-48d1-bd8f-055fde3a5b56` | Present in `MakerBot Sketch PLA @base.json`; valid GUID with `MATERIAL_CODE:pla` |
| MakerBot Sketch Tough PLA | ✅ Has GUID | `de031137-a8ca-4a72-bd1b-17bb964033ad` | Present in `MakerBot Sketch Tough PLA @base.json`; valid GUID with `MATERIAL_CODE:im-pla` |
| MakerBot Sketch Metallic PLA | ✅ Has GUID | `3fac1543-dd0c-462d-9cbc-d94137d43999` | Present in `MakerBot Sketch Metallic PLA @base.json`; valid GUID with `MATERIAL_CODE:metallic-pla` |

**Note:** The two-pass filament search in `BackgroundSlicingProcess.cpp` will prioritize presets with MATERIAL_GUID, falling back to `filament_type` matching for materials without GUIDs.

### Known Issues

**1. Printer Extruder Variant / Core Naming Cleanup:**
- The old nozzle-variant mismatch is fixed for the active UltiMaker S/F nozzle profiles; their `printer_extruder_variant` values now generally match the intended nozzle/core combinations.
- Remaining cleanup is narrower now:
  - legacy / inactive models such as `UltiMaker 2+ Connect 0.40.json` still use less specific variant naming
  - broader UltiMaker core naming conventions still deserve a dedicated cleanup pass for consistency and long-term maintainability

**2. Print Process Profiles:**
- The active UltiMaker / Method matrix-tagged presets now have a local data-driven process-filtering path that is intended to remove stale process options when the selected core / extruder changes.
- The remaining work is runtime validation and broader tuning coverage, not the earlier "core change leaves the old process list visible" bug on these tagged profiles.
- Template/process coverage is still incomplete outside the current active preset slice, especially for placeholder models and unfinished Method combinations.

**3. MakerBot Method Family Profiles (local-only):**
- Plain Method, Method X, and Method XL now have initial bundle entries locally, but they should still be treated as baseline reachability presets rather than final tuned defaults
- The current Method-family presets now ship thin `1A` / `1C` / `LABS` leaves for Method, `1XA` / `1C` / `LABS` leaves for Method X / XL, and baked mixed Method X / XL `1XA+2XA`, `1C+2XA`, and `LABS+2XA` support leaves, but these are still conservative bundle presets rather than finished tuned defaults.
- This is still temporary because the new Method-family Prepare-side UI / process-matrix path has not yet been runtime validated in this workspace even though it now compiles locally.
- Method-family material-to-extruder compatibility is now partially enforced for the shipped Tough PLA / ABS-R / ABS-CF / ASA / Nylon CF / Nylon 12 CF / RapidRinse / SR-30 system presets, but the full Cura matrix is still not represented and the shipped machine/process matrix still does not cover `2A` and only partially covers `2XA`
- The current support-tool blocker is no longer just code-side: local `HEAD` already has a Method-family slot-aware compatibility path in `Preset.cpp`, a broader baked `2XA` preset slice for Method X / XL, and a first Method-family Prepare-side UI/process-remap path, but plain Method `2A`, tuning, and broader workflow validation still need follow-through before it can be shipped confidently.
- Variant-driven process remapping and Method-family UI support now exist in a first local form, so the current defaults are no longer purely fixed-default placeholders, but they are still a conservative first slice rather than finished shipping behavior

**4. UltiMaker Brand Materials:**
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
- `src/slic3r/GUI/ExtruderVariantWidget.cpp` - UltiMaker / Method process-matrix discovery, tag detection, and dynamic variant/process filtering
- `src/slic3r/Utils/UltiMaker.cpp` - Upload integration with container format
- `src/slic3r/Utils/UltiMakerLAN.cpp` - LAN printing implementation
- `src/libslic3r/Format/FormatConfig.cpp` - FORMAT_CONFIG_ID parsing
- `src/slic3r/GUI/GUI_App.cpp` - FT_UFP/FT_MAKERBOT file type enums

### Testing Status

| Area | Status | Notes |
|------|--------|-------|
| Setup Wizard | ✅ Passed | Can add UltiMaker S6 printer successfully |
| Build / Compile | ✅ Passed | Full `cmake --build build --target OrcaSlicer --config Release --parallel 4` succeeded after the matrix/tag refactor, S6 default backport, and latest `Plater.cpp` timer-ownership fix |
| Process Selection | ⚠️ Needs Validation | The new preset-driven UltiMaker / Method matrix filtering compiles, but Prepare-side runtime validation is still pending |
| Filament Selection | ✅ Passed | GUID-based filament matching works |
| Export | ✅ Passed | `.ufp` container export works with `FORMAT_CONFIG_ID` |
| Upload | ✅ Passed | Digital Factory upload with container conversion works for both `.ufp` and `.makerbot` |
| LAN Printing | ✅ Passed | UltiMaker LAN upload and print works |
| Core / Extruder Matrix | ⚠️ Needs Validation | `PROCESS_MATRIX_TYPE` / `PROCESS_MATRIX_CONTROL_SLOT` rollout is now in local-only committed presets for the active UltiMaker / Method families; manual switching validation is still pending |
| UI Leak Review | ✅ Passed with One Fix | Reviewed fork-specific GUI ownership paths; fixed the leaked sidebar printer-sync timer in `src/slic3r/GUI/Plater.cpp`; no second obvious leak regression was identified in the reviewed fork-only UI changes |
| UltiMaker Digital Factory Auth | ⚠️ Needs Validation | Refreshed implementation is compiled and pushed, but still needs broader live validation after the storage/refresh cleanup |
| UltiMaker LAN Browse | ⚠️ Needs Validation | Persistent picker behavior and IP-only selection are compiled and pushed, but still need broader validation on real LAN environments |

### Profile Backlog / Incomplete Status

| Printer / Family | Current State | Remaining Work |
|------------------|---------------|----------------|
| MakerBot Sketch Small | Exists and is active | Profile tuning and validation still needed |
| MakerBot Sketch Large | Placeholder template only | Needs real profile creation, tuning, and validation |
| UltiMaker S3 | Placeholder template only | Needs real profile creation, tuning, and validation; also uses Griffin/UFP path that still needs writer support updates |
| UltiMaker S5 | Placeholder template only | Needs real profile creation, tuning, and validation; also uses Griffin/UFP path that still needs writer support updates |
| UltiMaker S6 | First tuned `0.4` baseline backported locally | Broader nozzle/process tuning and validation still needed |
| UltiMaker S7 | Placeholder template only | Needs real profile creation, tuning, and validation; also uses Griffin/UFP path that still needs writer support updates |
| UltiMaker S8 | Dependency follow-on to S6 | Will be done after S6 due to the current dependency chain |
| UltiMaker 2+ Connect | Placeholder template only and currently not in active bundle | Needs real profile creation, tuning, and validation; likely uses Griffin/UFP path that still needs writer support updates; also does not use print cores and needs proper non-core configuration handling |
| MakerBot Method | Minimal local bundle entry now exists | Native Method export path is wired through a concrete `0.40` machine preset and minimal `0.20mm Standard` process, but variant UI, compatibility enforcement, tuning, and real printer validation are still pending |
| MakerBot Method X | Minimal local bundle entry now exists | Same as Method, but currently fixed to default `1XA`-family presets and still missing finished variant/material guardrails |
| MakerBot Method XL | Minimal local bundle entry now exists | Shares the new local Method export path and now has an XL-specific machine/process preset, but XL tuning, variant/material guardrails, and real printer validation are still pending |

### Reference Files

- `resources/profiles/Flashforge/machine/` - Nozzle variant pattern
- `resources/profiles/UltiMaker.json` - Existing UltiMaker structure

## Development Workflow & References

### Reference Codebases

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

### Context Management Rules

- **Compress frequently when searching files** — with a 20GB+ codebase and small context window, compress **every 1-2 tool calls** when doing file searches or reads. After searching 3-5 files, compress before continuing to the next batch.
- **Todo items are the primary work unit** — if a `todowrite` list exists, each item is an independent, compressible unit. When one item is complete and verified, compress it before moving to the next.
- **Compress closed ranges only** — only compress ranges that are fully resolved and no longer needed. Do not compress active work still in progress.
- **Include subagent results** — compress after a `Task` (explore/general) subagent completes its findings; the subagent already has fresh context, so the raw output in the main thread is noise.
- **Quality over brevity in summaries** — use `compress` with exhaustive summaries that preserve file paths, function names, decisions, and key findings. Raw context can be discarded once it is faithfully summarized.
- **Batch independent compressions** — if multiple independent ranges are stale at the same time, run multiple `compress` calls in parallel rather than one large compression.

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

### 2026-04-17
- **Method Wizard / Vendor-Bundle Follow-Up (local-only)**
  - Added explicit dual-`0.4` `nozzle_diameter` arrays across the shipped Method machine presets so the dual-extruder instances expose both nozzle entries consistently.
  - Fixed the UltiMaker vendor-bundle regression by removing the oversized `960x1460` entry from the shared Method printer preset `thumbnails` field; the real Method `thumbnail_960x1460.png` requirement remains only in the Method MakerBot format configs.
  - Added missing setup-wizard cover art for `MakerBot Method`, `MakerBot Method X`, and `MakerBot Method XL` under `resources/profiles/UltiMaker/`, sourced from the local Cura reference assets.
  - Refreshed `AGENTS_JATMN.md` and `plans/methodx-makerbot-export-plan.md` so the documented current state matches the local follow-up commit instead of the earlier pre-commit workspace snapshot.
- **Method Family Review Follow-Up (working tree)**
  - Preserved `PresetBundle::calibrate_filaments` as the calibration allowlist source of truth in `PresetComboBoxes` before applying slot-aware Method filtering, so calibration visibility stays aligned with bundle-owned compatibility decisions.
  - Adjusted the Windows wxWidgets module-mode fallback in `src/CMakeLists.txt` so multi-config generators can keep release+debug discovery and single-config Debug uses `mswud` instead of forcing release-only `mswu`.
  - That earlier same-day follow-up still needs a fresh local configure/build retry for validation where relevant, but it is not the current repo working-tree state described above.

### 2026-04-16
- **MakerBot Method Family Compatibility Edge Cases (local-only)**
  - Threaded Method-family slot-aware compatibility beyond `Preset.cpp` into `PresetBundle`, filament combo boxes, calibration loading, and the WebGuide fallback filament-selection path.
  - Filament auto-replacement and combo-box visibility now evaluate Method-family compatibility per slot instead of only reusing printer-wide compatibility decisions.
  - This tightens per-slot behavior for mixed build/support presets, but runtime/printer acceptance validation is still pending.
- **MakerBot Method Family Mixed Support Slice + Prepare UI Start (local-only)**
  - Expanded the baked mixed `2XA` support-tool preset slice beyond the first Method X pilot by adding:
    - `MakerBot Method XL 1XA+2XA 0.40`
    - `MakerBot Method X / XL 1C+2XA 0.40`
    - `MakerBot Method X / XL LABS+2XA 0.40`
  - Added matching `0.20mm Standard` process leaves and registered all new mixed presets in `resources/profiles/UltiMaker.json`.
  - Extended `ExtruderVariantWidget` with a first Method-family Prepare-side path that:
    - is scoped only to UltiMaker / MakerBot printers
    - presents Method-family rows as `Extruder`
    - supports the current baked build/support combinations
    - remaps process selection using combined Method keys such as `1XA+2XA`
  - Synced Method-family UI changes with `extruder_variant_list` updates so mixed-extruder capability state stays aligned with `printer_extruder_variant`.
  - Deferred full compile/runtime verification because this workspace currently cannot regenerate the build due to a missing local `wxWidgets` package during `ZERO_CHECK`.
- **MakerBot Method Family Variant Reachability Expansion (local-only)**
  - Added thin concrete `1C` and `LABS` machine leaves for Method / Method X / Method XL under `resources/profiles/UltiMaker/machine/`.
  - Added matching thin `0.20mm Standard` `1C` and `LABS` process leaves under `resources/profiles/UltiMaker/process/`.
  - Extended `resources/profiles/UltiMaker.json` so these new Method-family variant leaves are now part of the shipped local vendor bundle.
  - Kept this slice intentionally limited to build-tool variants at this stage; later same-day local work added slot-aware Method-family compatibility evaluation in `Preset.cpp`, but full shipped `2A` / `2XA` support-tool exposure still remains pending.
- **MakerBot Method Family Filament Library Expansion (local-only)**
  - Added local Method-family filament preset pairs for `Nylon CF`, `Nylon 12 CF`, `RapidRinse`, and `SR-30` under `resources/profiles/OrcaFilamentLibrary/filament/MakerBot/`.
  - Registered those new Method filament pairs in `resources/profiles/OrcaFilamentLibrary.json` so they are reachable through the shared filament library manifest instead of existing only as loose JSON files.
  - Extended the Method / Method X / Method XL machine-model `default_materials` lists so these new presets are part of the staged Method-family material inventory locally.
  - Kept the compatibility rules aligned with the Cura matrix:
    - `Nylon CF` / `Nylon 12 CF` on `1C` and `LABS`
    - `RapidRinse` / `SR-30` on `2XA`
  - Confirmed that these presets are now only partly future-facing because local `HEAD` now ships concrete `1C` and `LABS` machine/process coverage, while `2A` / `2XA` support-tool exposure still remains pending.
- **MakerBot Method Family Filament Compatibility (local-only)**
  - Added first-pass `compatible_printers_condition` rules to the shipped `MakerBot Method Tough PLA`, `ABS-R`, `ABS-CF`, and `ASA` `@System` presets.
  - The new conditions match Method-family printers by `METHOD_PRINTER_FAMILY:<id>` notes plus `printer_extruder_variant_0`, mirroring how the new Method process presets already gate by family/variant.
  - Encoded the currently shippable subset of the Cura matrix:
    - Tough PLA on Method / Method X / Method XL for `1A`, `1C`, and `LABS`
    - ABS-R on Method X / XL for `1XA`, `1C`, and `LABS`
    - ABS-CF on Method X / XL for `1C` and `LABS`
    - ASA on Method X / XL for `1XA`, `1C`, and `LABS`
  - Documented the remaining shipped-material gaps (`RapidRinse`, `SR-30`, `Nylon CF`, `Nylon 12 CF`) and the fact that Method variant UI/process remapping are still unfinished.
- **MakerBot Method Family Vendor-Bundle Wiring (local-only)**
  - Added local-only `MakerBot Method`, `MakerBot Method X`, and `MakerBot Method XL` machine-model entries plus concrete `0.40` machine presets under `resources/profiles/UltiMaker/machine/`.
  - Added a shared `fdm_makerbot_method_common.json` baseline and first minimal `0.20mm Standard` Method-family process presets under `resources/profiles/UltiMaker/process/`.
  - Extended `resources/profiles/UltiMaker.json` so the Method family is now reachable through the shipped UltiMaker/MakerBot vendor bundle locally instead of existing only as dormant export plumbing.
  - Deliberately started the local Method-family preset slice with mirrored default extruder variants (`1A` for Method, `1XA` for Method X/XL) before later expanding it with thin `1C` and `LABS` leaves; later same-day local work also added a first Method variant-selection UI path and slot-aware compatibility enforcement, but broader workflow validation is still pending.
- **MakerBot Method Family Export Groundwork (local-only)**
  - Added a first native C++ Method-family `.makerbot` export pass in `MakerBotWriter` that now branches between Sketch `print.gcode` archives and Method `print.jsontoolpath` archives.
  - Restored local Method format configs for plain Method, Method X, and Method XL under `resources/formats/makerbot/`.
  - Updated `FormatConfig` and the UltiMaker upload MIME routing so `FORMAT_CONFIG_ID:method`, `method_x`, and `method_xl` resolve through the MakerBot container path locally.
  - Extended the MakerBot export path to carry up to two injected extruders of material/temperature usage metadata and patched the shipped MakerBot Method filament presets with native Method GUID/material-code notes.
  - Verified successful local `Release` builds for `libslic3r` and `libslic3r_gui`; full app/runtime and printer acceptance validation remain pending.
- **AGENTS Status Refresh**
  - Updated the current-status audit date and public-fork head reference to match `origin/main` at `cd0a2d7428`.
  - Removed the stale AGENTS claim that `OAuthJob` currently preserves the previous refresh token when `refresh_token` is omitted from the token response.
  - Refreshed the summary/audit sections to note that the current public fork has a locally verified macOS / Apple Silicon build path again.
- **macOS / Apple Silicon Build Compatibility**
  - `95bc9c4257` fixed the current local/public macOS build blockers:
    - reconciled the bundled `miniz` implementation with its header signature
    - cleaned up `ExtruderVariantWidget` const handling for `full_config()` and preset option access
    - removed the stale `PrintHostType` forward declaration mismatch in `PrintHostDialogs.hpp`
    - made `Http::allow_tls_flexible()` linkable cross-platform instead of Windows-only in practice
    - gated `UltiMakerLAN`'s `ssl_revoke_best_effort()` calls behind `WIN32`
  - Verified a successful local `arm64` app build on an M1 Mac.
- **Cleanup**
  - `cd0a2d7428` removed the temporary `plans/build_orcaslicer_macos.md` scratch note after the build-fix commit landed.

### 2026-04-15
- **UltiMaker Root Manifest Dependency Rule**
  - Confirmed that `resources/profiles/UltiMaker.json` is not a setup-wizard-only list; it is part of the real vendor bundle definition.
  - Confirmed that removing entries only from `machine_model_list` can invalidate the whole UltiMaker vendor bundle because `machine_list` printer presets are validated against the remaining model list.
  - Documented the safe temporary-removal procedure: when disabling incomplete UltiMaker models, remove them consistently from `machine_model_list`, `machine_list`, and related `process_list` entries, then sync the manifest copies and restart OrcaSlicer.
  - The active/public fork state now removes incomplete `S3`, `S5`, `S7`, and `2+ Connect` models from the active UltiMaker bundle while leaving the underlying JSON files on disk.
- **Profile Backlog Documentation**
  - Added an explicit profile backlog / incomplete-status table for Sketch, S-series, 2+ Connect, and Method-family printers so the AGENTS guide makes clear which profiles are tuned work in progress versus placeholder templates.
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
  - Extruder/process UI consistency fixes for Cheetah and print-core selection are now part of the public fork state

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

### 2026-04-01
- **Format Config Cleanup**
  - Renamed `ultimaker_2pc.json` → `ultimaker2_plus_connect.json` and updated internal `printer_name`/`target_machine` to "Ultimaker 2+ Connect"
  - Deleted orphan `ultimaker_f4.json`
  - Added `sketch_large` to `makerbot/manifest.json`
  - Removed unused `method_x.json` and `method_xl.json` MakerBot configs (reserved in code for future use)
  - Removed 960×1460 thumbnail from `ContainerFormatHelper` default fallback
  - Corrected `AGENTS_JATMN.md` LAN auth description to match actual implementation

### 2026-03-31
- **MakerBot Thumbnail Generation Fix** (commit `762ff9d3ef`)
  - Added `ContainerFormatHelper` class to manage format-specific thumbnail requirements
  - Fixed thumbnail generation to use `render_thumbnails()` callback for proper sizing
  - Updated MakerBot format configs to remove oversized 960x1460 thumbnail (exceeded 1000px limit)
  - Fixed MakerBot printer profiles to use proper PNG format specifier (`120x120/PNG` format)
  - Renamed `build_ufp_container()` to `build_container_format()` for clarity
  - Support both UFP and MakerBot container formats with proper thumbnail handling
