# Process Preset Recreation Plan - VERIFIED AND CORRECTED

## Critical Findings from Codebase Verification

### 1. Machine Profiles ALREADY Have `printer_extruder_variant`
**Current State (Verified in `UltiMaker S6 0.4 nozzle.json`):**
```json
"printer_extruder_variant": [
    "AA+ 0.4",
    "AA+ 0.4"
]
```
**Action Required:** UPDATE existing values to match new naming (e.g., "AA 0.4" for AA core presets), not ADD the field.

### 2. Existing Process Presets Have Mixed `compatible_printers`
**Current State (Verified in `0.20mm Standard @UltiMaker S6-S8 0.4 nozzle.json`):**
```json
"compatible_printers": [
    "UltiMaker S6 AA 0.4",
    "UltiMaker S6 BB 0.4",
    "UltiMaker S6 CC 0.4",
    "UltiMaker S6 AA+ 0.4",
    "UltiMaker S6 CC+ 0.4",
    "UltiMaker S6 0.4 nozzle",  // <-- GENERIC ENTRY
    "UltiMaker S8 AA 0.4",
    "UltiMaker S8 BB 0.4",
    "UltiMaker S8 CC 0.4",
    "UltiMaker S8 AA+ 0.4",
    "UltiMaker S8 CC+ 0.4",
    "UltiMaker S8 0.4 nozzle"   // <-- GENERIC ENTRY
]
```

### 3. Layer Height Decision Required
**Current State:** Existing files use `0.15mm` for 0.25 nozzle (e.g., `0.15mm Standard @UltiMaker S6-S8 0.25 nozzle.json`)
**Plan Says:** Use `0.10mm` for 0.25 nozzle
**Decision:** Keep `0.15mm` to match existing convention OR change to `0.10mm` for finer detail

---

## Problem Statement

Current process presets have `compatible_printers` lists that are TOO GENERIC. They mix different core types that should have separate process presets:

**Current (WRONG):**
```json
"compatible_printers": [
    "UltiMaker S6 AA 0.4",
    "UltiMaker S6 BB 0.4",
    "UltiMaker S6 CC 0.4",
    "UltiMaker S6 AA+ 0.4",
    "UltiMaker S6 CC+ 0.4",
    "UltiMaker S6 0.4 nozzle",  // Generic entry mixes everything
    ...
]
```

**Required (CORRECT):**
Each core type gets its OWN process preset with a NARROW `compatible_printers` list containing ONLY that core type.

---

## Layer Height Configuration (DECISION PENDING)

| Nozzle Size | Layer Height | Notes |
|-------------|--------------|-------|
| 0.25mm | **0.15mm** | **RECOMMENDED: Keep existing convention** |
| 0.4mm | 0.2mm | Standard quality |
| 0.6mm | 0.2mm | Standard quality |
| 0.8mm | 0.2mm | Standard quality |

**⚠️ WARNING:** If you choose 0.10mm for 0.25 nozzle, you must rename files from `0.15mm` to `0.10mm` prefix.

---

## Core-Specific Process Preset Structure

### For Each Printer Family:
- S3/S5/S7 (grouped)
- S6/S8 (grouped)
- Factor 4 (separate)

### For Each Nozzle Size:
- 0.25mm
- 0.4mm
- 0.6mm
- 0.8mm

### For Each Core Type:
- AA (standard)
- BB (support)
- CC (abrasive)
- AA+ (hardened standard) - where applicable
- CC+ (hardened abrasive) - where applicable
- **CC RED** (S3/S5/S7 0.6mm only)
- **HT** (S6/S8 and Factor 4 0.4mm only)

---

## Required Process Presets (CORRECTED COUNT: 38 files)

### S3/S5/S7 Family (13 files)

**0.25mm Nozzle (0.15mm layer height):**
- 0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25
- 0.15mm Standard @UltiMaker S3-S5-S7 BB 0.25

**0.4mm Nozzle (0.2mm layer height):**
- 0.20mm Standard @UltiMaker S3-S5-S7 AA 0.4
- 0.20mm Standard @UltiMaker S3-S5-S7 BB 0.4
- 0.20mm Standard @UltiMaker S3-S5-S7 CC 0.4
- 0.20mm Standard @UltiMaker S3-S5-S7 AA+ 0.4
- 0.20mm Standard @UltiMaker S3-S5-S7 CC+ 0.4

**0.6mm Nozzle (0.2mm layer height):**
- 0.20mm Standard @UltiMaker S3-S5-S7 AA 0.6
- 0.20mm Standard @UltiMaker S3-S5-S7 CC 0.6
- 0.20mm Standard @UltiMaker S3-S5-S7 CC+ 0.6
- **0.20mm Standard @UltiMaker S3-S5-S7 CC RED 0.6**

**0.8mm Nozzle (0.2mm layer height):**
- 0.20mm Standard @UltiMaker S3-S5-S7 AA 0.8
- 0.20mm Standard @UltiMaker S3-S5-S7 BB 0.8

### S6/S8 Family (13 files)

**0.25mm Nozzle (0.15mm layer height):**
- 0.15mm Standard @UltiMaker S6-S8 AA 0.25
- 0.15mm Standard @UltiMaker S6-S8 BB 0.25

**0.4mm Nozzle (0.2mm layer height):**
- 0.20mm Standard @UltiMaker S6-S8 AA 0.4
- 0.20mm Standard @UltiMaker S6-S8 BB 0.4
- 0.20mm Standard @UltiMaker S6-S8 CC 0.4
- 0.20mm Standard @UltiMaker S6-S8 AA+ 0.4
- 0.20mm Standard @UltiMaker S6-S8 CC+ 0.4
- **0.20mm Standard @UltiMaker S6-S8 HT 0.4**

**0.6mm Nozzle (0.2mm layer height):**
- 0.20mm Standard @UltiMaker S6-S8 AA 0.6
- 0.20mm Standard @UltiMaker S6-S8 CC 0.6
- 0.20mm Standard @UltiMaker S6-S8 CC+ 0.6

**0.8mm Nozzle (0.2mm layer height):**
- 0.20mm Standard @UltiMaker S6-S8 AA 0.8
- 0.20mm Standard @UltiMaker S6-S8 BB 0.8

### Factor 4 Family (12 files)

**0.25mm Nozzle (0.15mm layer height):**
- 0.15mm Standard @UltiMaker Factor 4 AA 0.25
- 0.15mm Standard @UltiMaker Factor 4 BB 0.25

**0.4mm Nozzle (0.2mm layer height):**
- 0.20mm Standard @UltiMaker Factor 4 AA 0.4
- 0.20mm Standard @UltiMaker Factor 4 BB 0.4
- 0.20mm Standard @UltiMaker Factor 4 CC 0.4
- 0.20mm Standard @UltiMaker Factor 4 AA+ 0.4
- 0.20mm Standard @UltiMaker Factor 4 CC+ 0.4
- **0.20mm Standard @UltiMaker Factor 4 HT 0.4**

**0.6mm Nozzle (0.2mm layer height):**
- 0.20mm Standard @UltiMaker Factor 4 AA 0.6
- 0.20mm Standard @UltiMaker Factor 4 CC 0.6
- 0.20mm Standard @UltiMaker Factor 4 CC+ 0.6

**0.8mm Nozzle (0.2mm layer height):**
- 0.20mm Standard @UltiMaker Factor 4 AA 0.8
- 0.20mm Standard @UltiMaker Factor 4 BB 0.8

---

## Inheritance Strategy

Create UltiMaker-specific common presets for better organization:

```
fdm_process_common (system-wide base)
    └── fdm_process_ultimaker_common (UltiMaker-specific base)
            ├── fdm_process_ultimaker_s3s5s7_common
            ├── fdm_process_ultimaker_s6s8_common
            └── fdm_process_ultimaker_factor4_common
```

**New Common Presets to Create (4 files):**
1. `fdm_process_ultimaker_common.json` - Base for all UltiMaker process presets
2. `fdm_process_ultimaker_s3s5s7_common.json` - S3/S5/S7 family specific
3. `fdm_process_ultimaker_s6s8_common.json` - S6/S8 family specific
4. `fdm_process_ultimaker_factor4_common.json` - Factor 4 specific

---

## Complete File Inventory

### New Common Preset Files (4 files)

**Location:** `resources/profiles/UltiMaker/process/`

1. `fdm_process_ultimaker_common.json`
2. `fdm_process_ultimaker_s3s5s7_common.json`
3. `fdm_process_ultimaker_s6s8_common.json`
4. `fdm_process_ultimaker_factor4_common.json`

### New Process Preset Files (38 files)

**S3/S5/S7 Family (13 files):**

| File Name | Layer Height | Compatible Printers |
|-----------|--------------|---------------------|
| `0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25.json` | 0.15 | UltiMaker S3 AA 0.25, UltiMaker S5 AA 0.25, UltiMaker S7 AA 0.25 |
| `0.15mm Standard @UltiMaker S3-S5-S7 BB 0.25.json` | 0.15 | UltiMaker S3 BB 0.25, UltiMaker S5 BB 0.25, UltiMaker S7 BB 0.25 |
| `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.4.json` | 0.2 | UltiMaker S3 AA 0.4, UltiMaker S5 AA 0.4, UltiMaker S7 AA 0.4 |
| `0.20mm Standard @UltiMaker S3-S5-S7 BB 0.4.json` | 0.2 | UltiMaker S3 BB 0.4, UltiMaker S5 BB 0.4, UltiMaker S7 BB 0.4 |
| `0.20mm Standard @UltiMaker S3-S5-S7 CC 0.4.json` | 0.2 | UltiMaker S3 CC 0.4, UltiMaker S5 CC 0.4, UltiMaker S7 CC 0.4 |
| `0.20mm Standard @UltiMaker S3-S5-S7 AA+ 0.4.json` | 0.2 | UltiMaker S3 AA+ 0.4, UltiMaker S5 AA+ 0.4, UltiMaker S7 AA+ 0.4 |
| `0.20mm Standard @UltiMaker S3-S5-S7 CC+ 0.4.json` | 0.2 | UltiMaker S3 CC+ 0.4, UltiMaker S5 CC+ 0.4, UltiMaker S7 CC+ 0.4 |
| `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.6.json` | 0.2 | UltiMaker S3 AA 0.6, UltiMaker S5 AA 0.6, UltiMaker S7 AA 0.6 |
| `0.20mm Standard @UltiMaker S3-S5-S7 CC 0.6.json` | 0.2 | UltiMaker S3 CC 0.6, UltiMaker S5 CC 0.6, UltiMaker S7 CC 0.6 |
| `0.20mm Standard @UltiMaker S3-S5-S7 CC+ 0.6.json` | 0.2 | UltiMaker S3 CC+ 0.6, UltiMaker S5 CC+ 0.6, UltiMaker S7 CC+ 0.6 |
| `0.20mm Standard @UltiMaker S3-S5-S7 CC RED 0.6.json` | 0.2 | UltiMaker S3 CC RED 0.6, UltiMaker S5 CC RED 0.6, UltiMaker S7 CC RED 0.6 |
| `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.8.json` | 0.2 | UltiMaker S3 AA 0.8, UltiMaker S5 AA 0.8, UltiMaker S7 AA 0.8 |
| `0.20mm Standard @UltiMaker S3-S5-S7 BB 0.8.json` | 0.2 | UltiMaker S3 BB 0.8, UltiMaker S5 BB 0.8, UltiMaker S7 BB 0.8 |

**S6/S8 Family (13 files):**

| File Name | Layer Height | Compatible Printers |
|-----------|--------------|---------------------|
| `0.15mm Standard @UltiMaker S6-S8 AA 0.25.json` | 0.15 | UltiMaker S6 AA 0.25, UltiMaker S8 AA 0.25 |
| `0.15mm Standard @UltiMaker S6-S8 BB 0.25.json` | 0.15 | UltiMaker S6 BB 0.25, UltiMaker S8 BB 0.25 |
| `0.20mm Standard @UltiMaker S6-S8 AA 0.4.json` | 0.2 | UltiMaker S6 AA 0.4, UltiMaker S8 AA 0.4 |
| `0.20mm Standard @UltiMaker S6-S8 BB 0.4.json` | 0.2 | UltiMaker S6 BB 0.4, UltiMaker S8 BB 0.4 |
| `0.20mm Standard @UltiMaker S6-S8 CC 0.4.json` | 0.2 | UltiMaker S6 CC 0.4, UltiMaker S8 CC 0.4 |
| `0.20mm Standard @UltiMaker S6-S8 AA+ 0.4.json` | 0.2 | UltiMaker S6 AA+ 0.4, UltiMaker S8 AA+ 0.4 |
| `0.20mm Standard @UltiMaker S6-S8 CC+ 0.4.json` | 0.2 | UltiMaker S6 CC+ 0.4, UltiMaker S8 CC+ 0.4 |
| `0.20mm Standard @UltiMaker S6-S8 HT 0.4.json` | 0.2 | UltiMaker S6 HT 0.4, UltiMaker S8 HT 0.4 |
| `0.20mm Standard @UltiMaker S6-S8 AA 0.6.json` | 0.2 | UltiMaker S6 AA 0.6, UltiMaker S8 AA 0.6 |
| `0.20mm Standard @UltiMaker S6-S8 CC 0.6.json` | 0.2 | UltiMaker S6 CC 0.6, UltiMaker S8 CC 0.6 |
| `0.20mm Standard @UltiMaker S6-S8 CC+ 0.6.json` | 0.2 | UltiMaker S6 CC+ 0.6, UltiMaker S8 CC+ 0.6 |
| `0.20mm Standard @UltiMaker S6-S8 AA 0.8.json` | 0.2 | UltiMaker S6 AA 0.8, UltiMaker S8 AA 0.8 |
| `0.20mm Standard @UltiMaker S6-S8 BB 0.8.json` | 0.2 | UltiMaker S6 BB 0.8, UltiMaker S8 BB 0.8 |

**Factor 4 Family (12 files):**

| File Name | Layer Height | Compatible Printers |
|-----------|--------------|---------------------|
| `0.15mm Standard @UltiMaker Factor 4 AA 0.25.json` | 0.15 | UltiMaker Factor 4 AA 0.25 |
| `0.15mm Standard @UltiMaker Factor 4 BB 0.25.json` | 0.15 | UltiMaker Factor 4 BB 0.25 |
| `0.20mm Standard @UltiMaker Factor 4 AA 0.4.json` | 0.2 | UltiMaker Factor 4 AA 0.4 |
| `0.20mm Standard @UltiMaker Factor 4 BB 0.4.json` | 0.2 | UltiMaker Factor 4 BB 0.4 |
| `0.20mm Standard @UltiMaker Factor 4 CC 0.4.json` | 0.2 | UltiMaker Factor 4 CC 0.4 |
| `0.20mm Standard @UltiMaker Factor 4 AA+ 0.4.json` | 0.2 | UltiMaker Factor 4 AA+ 0.4 |
| `0.20mm Standard @UltiMaker Factor 4 CC+ 0.4.json` | 0.2 | UltiMaker Factor 4 CC+ 0.4 |
| `0.20mm Standard @UltiMaker Factor 4 HT 0.4.json` | 0.2 | UltiMaker Factor 4 HT 0.4 |
| `0.20mm Standard @UltiMaker Factor 4 AA 0.6.json` | 0.2 | UltiMaker Factor 4 AA 0.6 |
| `0.20mm Standard @UltiMaker Factor 4 CC 0.6.json` | 0.2 | UltiMaker Factor 4 CC 0.6 |
| `0.20mm Standard @UltiMaker Factor 4 CC+ 0.6.json` | 0.2 | UltiMaker Factor 4 CC+ 0.6 |
| `0.20mm Standard @UltiMaker Factor 4 AA 0.8.json` | 0.2 | UltiMaker Factor 4 AA 0.8 |
| `0.20mm Standard @UltiMaker Factor 4 BB 0.8.json` | 0.2 | UltiMaker Factor 4 BB 0.8 |

### Existing Files to Delete (9 files)

These generic presets will be replaced by core-specific ones:

1. `0.15mm Standard @UltiMaker S3-S5-S7 0.25 nozzle.json`
2. `0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle.json`
3. `0.20mm Standard @UltiMaker S3-S5-S7 0.6 nozzle.json`
4. `0.20mm Standard @UltiMaker S3-S5-S7 0.8 nozzle.json`
5. `0.15mm Standard @UltiMaker S6-S8 0.25 nozzle.json`
6. `0.20mm Standard @UltiMaker S6-S8 0.4 nozzle.json`
7. `0.20mm Standard @UltiMaker S6-S8 0.6 nozzle.json`
8. `0.20mm Standard @UltiMaker S6-S8 0.8 nozzle.json`
9. `0.15mm Standard @UltiMaker Factor 4 0.25 nozzle.json`
10. `0.20mm Standard @UltiMaker Factor 4 0.4 nozzle.json`
11. `0.20mm Standard @UltiMaker Factor 4 0.6 nozzle.json`
12. `0.20mm Standard @UltiMaker Factor 4 0.8 nozzle.json`

**⚠️ NOTE:** Files for UltiMaker 2, MakerBot Sketch, and UltiMaker 2+ Connect are NOT being deleted - they are separate printer families.

---

## Example Process Preset Structure

**File:** `0.20mm Standard @UltiMaker S6-S8 AA 0.4.json`

```json
{
    "type": "process",
    "name": "0.20mm Standard @UltiMaker S6-S8 AA 0.4",
    "inherits": "fdm_process_ultimaker_s6s8_common",
    "from": "system",
    "instantiation": "true",
    "print_settings_id": "0.20mm Standard @UltiMaker S6-S8 AA 0.4",
    "compatible_printers": [
        "UltiMaker S6 AA 0.4",
        "UltiMaker S8 AA 0.4"
    ],
    "layer_height": "0.2",
    "line_width": "0.4",
    "outer_wall_line_width": "0.4",
    "inner_wall_line_width": "0.45",
    "sparse_infill_line_width": "0.5",
    "internal_solid_infill_line_width": "0.45",
    "top_surface_line_width": "0.4",
    "initial_layer_line_width": "0.42",
    "wall_loops": "2",
    "sparse_infill_density": "15%",
    "sparse_infill_pattern": "alignedrectilinear",
    "default_acceleration": "5000",
    "inner_wall_acceleration": "5000",
    "outer_wall_acceleration": "3000",
    "top_surface_acceleration": "3000",
    "travel_acceleration": "15000",
    "bridge_acceleration": "75%",
    "initial_layer_acceleration": "1000",
    "default_jerk": "8",
    "inner_wall_jerk": "8",
    "outer_wall_jerk": "2",
    "top_surface_jerk": "4",
    "travel_jerk": "8",
    "initial_layer_jerk": "5",
    "inner_wall_speed": "100",
    "outer_wall_speed": "80",
    "sparse_infill_speed": "200",
    "top_surface_speed": "60",
    "initial_layer_speed": "55",
    "initial_layer_infill_speed": "65",
    "travel_speed": "300",
    "initial_layer_travel_speed": "150",
    "overhang_3_4_speed": "40",
    "overhang_4_4_speed": "35",
    "elefant_foot_compensation": "0.15"
}
```

**CRITICAL:** Notice `compatible_printers` contains ONLY the AA core variants - no BB, CC, AA+, CC+, or generic entries!

---

## Example Common Preset Files

### 1. fdm_process_ultimaker_common.json

```json
{
    "type": "process",
    "name": "fdm_process_ultimaker_common",
    "inherits": "fdm_process_common",
    "from": "system",
    "instantiation": "false",
    "print_settings_id": "fdm_process_ultimaker_common",
    "compatible_printers": [],
    "default_acceleration": "5000",
    "default_jerk": "8",
    "travel_acceleration": "15000",
    "travel_jerk": "8",
    "travel_speed": "300",
    "initial_layer_travel_speed": "150"
}
```

### 2. fdm_process_ultimaker_s3s5s7_common.json

```json
{
    "type": "process",
    "name": "fdm_process_ultimaker_s3s5s7_common",
    "inherits": "fdm_process_ultimaker_common",
    "from": "system",
    "instantiation": "false",
    "print_settings_id": "fdm_process_ultimaker_s3s5s7_common",
    "compatible_printers": []
}
```

### 3. fdm_process_ultimaker_s6s8_common.json

```json
{
    "type": "process",
    "name": "fdm_process_ultimaker_s6s8_common",
    "inherits": "fdm_process_ultimaker_common",
    "from": "system",
    "instantiation": "false",
    "print_settings_id": "fdm_process_ultimaker_s6s8_common",
    "compatible_printers": []
}
```

### 4. fdm_process_ultimaker_factor4_common.json

```json
{
    "type": "process",
    "name": "fdm_process_ultimaker_factor4_common",
    "inherits": "fdm_process_ultimaker_common",
    "from": "system",
    "instantiation": "false",
    "print_settings_id": "fdm_process_ultimaker_factor4_common",
    "compatible_printers": []
}
```

---

## Machine Profile Updates Required

### Current State (Verified)
Machine profiles ALREADY have `printer_extruder_variant` set. Example from `UltiMaker S6 0.4 nozzle.json`:

```json
"printer_extruder_variant": [
    "AA+ 0.4",
    "AA+ 0.4"
]
```

### Required Updates

**For each machine profile, UPDATE (not add) the `printer_extruder_variant` to match the DEFAULT core type:**

Since AA is the default/most common core, set all machine profiles to use AA variants:

**S3/S5/S7 Family (12 files):**
- `UltiMaker S3 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S3 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S3 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S3 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`
- `UltiMaker S5 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S5 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S5 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S5 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`
- `UltiMaker S7 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S7 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S7 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S7 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`

**S6/S8 Family (8 files):**
- `UltiMaker S6 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S6 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S6 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S6 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`
- `UltiMaker S8 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S8 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S8 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S8 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`

**Factor 4 Family (4 files):**
- `UltiMaker Factor 4 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker Factor 4 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker Factor 4 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker Factor 4 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`

### Default Print Profile Updates

**Update `default_print_profile` in each machine file to point to the AA core preset:**

| Machine File | Current `default_print_profile` | New `default_print_profile` |
|--------------|--------------------------------|------------------------------|
| `UltiMaker S3 0.25 nozzle.json` | `0.20mm Detail @UltiMaker S3-S5-S7 0.25 nozzle` | `0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25` |
| `UltiMaker S3 0.4 nozzle.json` | `0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle` | `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.4` |
| `UltiMaker S3 0.6 nozzle.json` | `0.20mm Draft @UltiMaker S3-S5-S7 0.6 nozzle` | `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.6` |
| `UltiMaker S3 0.8 nozzle.json` | `0.20mm Extra Draft @UltiMaker S3-S5-S7 0.8 nozzle` | `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.8` |
| `UltiMaker S5 0.25 nozzle.json` | `0.20mm Detail @UltiMaker S3-S5-S7 0.25 nozzle` | `0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25` |
| `UltiMaker S5 0.4 nozzle.json` | `0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle` | `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.4` |
| `UltiMaker S5 0.6 nozzle.json` | `0.20mm Draft @UltiMaker S3-S5-S7 0.6 nozzle` | `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.6` |
| `UltiMaker S5 0.8 nozzle.json` | `0.20mm Extra Draft @UltiMaker S3-S5-S7 0.8 nozzle` | `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.8` |
| `UltiMaker S6 0.25 nozzle.json` | `0.15mm Standard @UltiMaker S6-S8 0.25 nozzle` | `0.15mm Standard @UltiMaker S6-S8 AA 0.25` |
| `UltiMaker S6 0.4 nozzle.json` | `0.20mm Standard @UltiMaker S6-S8 0.4 nozzle` | `0.20mm Standard @UltiMaker S6-S8 AA 0.4` |
| `UltiMaker S6 0.6 nozzle.json` | `0.20mm Standard @UltiMaker S6-S8 0.6 nozzle` | `0.20mm Standard @UltiMaker S6-S8 AA 0.6` |
| `UltiMaker S6 0.8 nozzle.json` | `0.20mm Standard @UltiMaker S6-S8 0.8 nozzle` | `0.20mm Standard @UltiMaker S6-S8 AA 0.8` |
| `UltiMaker S7 0.25 nozzle.json` | `0.20mm Detail @UltiMaker S3-S5-S7 0.25 nozzle` | `0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25` |
| `UltiMaker S7 0.4 nozzle.json` | `0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle` | `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.4` |
| `UltiMaker S7 0.6 nozzle.json` | `0.20mm Draft @UltiMaker S3-S5-S7 0.6 nozzle` | `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.6` |
| `UltiMaker S7 0.8 nozzle.json` | `0.20mm Extra Draft @UltiMaker S3-S5-S7 0.8 nozzle` | `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.8` |
| `UltiMaker S8 0.25 nozzle.json` | `0.15mm Standard @UltiMaker S6-S8 0.25 nozzle` | `0.15mm Standard @UltiMaker S6-S8 AA 0.25` |
| `UltiMaker S8 0.4 nozzle.json` | `0.20mm Standard @UltiMaker S6-S8 0.4 nozzle` | `0.20mm Standard @UltiMaker S6-S8 AA 0.4` |
| `UltiMaker S8 0.6 nozzle.json` | `0.20mm Standard @UltiMaker S6-S8 0.6 nozzle` | `0.20mm Standard @UltiMaker S6-S8 AA 0.6` |
| `UltiMaker S8 0.8 nozzle.json` | `0.20mm Standard @UltiMaker S6-S8 0.8 nozzle` | `0.20mm Standard @UltiMaker S6-S8 AA 0.8` |
| `UltiMaker Factor 4 0.25 nozzle.json` | `0.15mm Standard @UltiMaker Factor 4 0.25 nozzle` | `0.15mm Standard @UltiMaker Factor 4 AA 0.25` |
| `UltiMaker Factor 4 0.4 nozzle.json` | `0.20mm Standard @UltiMaker Factor 4 0.4 nozzle` | `0.20mm Standard @UltiMaker Factor 4 AA 0.4` |
| `UltiMaker Factor 4 0.6 nozzle.json` | `0.20mm Standard @UltiMaker Factor 4 0.6 nozzle` | `0.20mm Standard @UltiMaker Factor 4 AA 0.6` |
| `UltiMaker Factor 4 0.8 nozzle.json` | `0.20mm Standard @UltiMaker Factor 4 0.8 nozzle` | `0.20mm Standard @UltiMaker Factor 4 AA 0.8` |

---

## UltiMaker.json Vendor Configuration Updates

**File:** `resources/profiles/UltiMaker.json`

### Step 1: Add 4 New Common Process Presets to `process_list`

Add these entries AFTER the `fdm_process_common` entry (around line 56):

```json
        {
            "name": "fdm_process_ultimaker_common",
            "sub_path": "process/fdm_process_ultimaker_common.json"
        },
        {
            "name": "fdm_process_ultimaker_s3s5s7_common",
            "sub_path": "process/fdm_process_ultimaker_s3s5s7_common.json"
        },
        {
            "name": "fdm_process_ultimaker_s6s8_common",
            "sub_path": "process/fdm_process_ultimaker_s6s8_common.json"
        },
        {
            "name": "fdm_process_ultimaker_factor4_common",
            "sub_path": "process/fdm_process_ultimaker_factor4_common.json"
        },
```

### Step 2: Add 38 New Process Presets to `process_list`

Add all 38 new process preset entries. Here are the first few as examples:

```json
        {
            "name": "0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25",
            "sub_path": "process/0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25.json"
        },
        {
            "name": "0.15mm Standard @UltiMaker S3-S5-S7 BB 0.25",
            "sub_path": "process/0.15mm Standard @UltiMaker S3-S5-S7 BB 0.25.json"
        },
        {
            "name": "0.20mm Standard @UltiMaker S3-S5-S7 AA 0.4",
            "sub_path": "process/0.20mm Standard @UltiMaker S3-S5-S7 AA 0.4.json"
        },
        ... (add all 38 entries)
```

**Complete list of names to add:**
1. `0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25`
2. `0.15mm Standard @UltiMaker S3-S5-S7 BB 0.25`
3. `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.4`
4. `0.20mm Standard @UltiMaker S3-S5-S7 BB 0.4`
5. `0.20mm Standard @UltiMaker S3-S5-S7 CC 0.4`
6. `0.20mm Standard @UltiMaker S3-S5-S7 AA+ 0.4`
7. `0.20mm Standard @UltiMaker S3-S5-S7 CC+ 0.4`
8. `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.6`
9. `0.20mm Standard @UltiMaker S3-S5-S7 CC 0.6`
10. `0.20mm Standard @UltiMaker S3-S5-S7 CC+ 0.6`
11. `0.20mm Standard @UltiMaker S3-S5-S7 CC RED 0.6`
12. `0.20mm Standard @UltiMaker S3-S5-S7 AA 0.8`
13. `0.20mm Standard @UltiMaker S3-S5-S7 BB 0.8`
14. `0.15mm Standard @UltiMaker S6-S8 AA 0.25`
15. `0.15mm Standard @UltiMaker S6-S8 BB 0.25`
16. `0.20mm Standard @UltiMaker S6-S8 AA 0.4`
17. `0.20mm Standard @UltiMaker S6-S8 BB 0.4`
18. `0.20mm Standard @UltiMaker S6-S8 CC 0.4`
19. `0.20mm Standard @UltiMaker S6-S8 AA+ 0.4`
20. `0.20mm Standard @UltiMaker S6-S8 CC+ 0.4`
21. `0.20mm Standard @UltiMaker S6-S8 HT 0.4`
22. `0.20mm Standard @UltiMaker S6-S8 AA 0.6`
23. `0.20mm Standard @UltiMaker S6-S8 CC 0.6`
24. `0.20mm Standard @UltiMaker S6-S8 CC+ 0.6`
25. `0.20mm Standard @UltiMaker S6-S8 AA 0.8`
26. `0.20mm Standard @UltiMaker S6-S8 BB 0.8`
27. `0.15mm Standard @UltiMaker Factor 4 AA 0.25`
28. `0.15mm Standard @UltiMaker Factor 4 BB 0.25`
29. `0.20mm Standard @UltiMaker Factor 4 AA 0.4`
30. `0.20mm Standard @UltiMaker Factor 4 BB 0.4`
31. `0.20mm Standard @UltiMaker Factor 4 CC 0.4`
32. `0.20mm Standard @UltiMaker Factor 4 AA+ 0.4`
33. `0.20mm Standard @UltiMaker Factor 4 CC+ 0.4`
34. `0.20mm Standard @UltiMaker Factor 4 HT 0.4`
35. `0.20mm Standard @UltiMaker Factor 4 AA 0.6`
36. `0.20mm Standard @UltiMaker Factor 4 CC 0.6`
37. `0.20mm Standard @UltiMaker Factor 4 CC+ 0.6`
38. `0.20mm Standard @UltiMaker Factor 4 AA 0.8`
39. `0.20mm Standard @UltiMaker Factor 4 BB 0.8`

### Step 3: Remove 12 Old Generic Process Preset Entries from `process_list`

**DELETE these entries from `process_list`:**

1. `0.15mm Standard @UltiMaker S3-S5-S7 0.25 nozzle`
2. `0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle`
3. `0.20mm Standard @UltiMaker S3-S5-S7 0.6 nozzle`
4. `0.20mm Standard @UltiMaker S3-S5-S7 0.8 nozzle`
5. `0.15mm Standard @UltiMaker S6-S8 0.25 nozzle`
6. `0.20mm Standard @UltiMaker S6-S8 0.4 nozzle`
7. `0.20mm Standard @UltiMaker S6-S8 0.6 nozzle`
8. `0.20mm Standard @UltiMaker S6-S8 0.8 nozzle`
9. `0.15mm Standard @UltiMaker Factor 4 0.25 nozzle`
10. `0.20mm Standard @UltiMaker Factor 4 0.4 nozzle`
11. `0.20mm Standard @UltiMaker Factor 4 0.6 nozzle`
12. `0.20mm Standard @UltiMaker Factor 4 0.8 nozzle`

**KEEP these entries (other printer families):**
- `0.12mm Fine @UltiMaker 2`
- `0.18mm Standard @UltiMaker 2`
- `0.25mm Draft @UltiMaker 2`
- `0.20mm Standard @UltiMaker 2+ Connect`
- `0.20mm Standard @MakerBot Sketch 0.40`
- `0.20mm Standard @MakerBot Sketch Large 0.40`
- `0.20mm Standard @MakerBot Sketch Sprint 0.40`

---

## Implementation Steps (CORRECTED)

### Phase 1: Create Common Presets
1. Create `fdm_process_ultimaker_common.json` with UltiMaker-specific base settings
2. Create `fdm_process_ultimaker_s3s5s7_common.json` inheriting from ultimaker common
3. Create `fdm_process_ultimaker_s6s8_common.json` inheriting from ultimaker common
4. Create `fdm_process_ultimaker_factor4_common.json` inheriting from ultimaker common

**Verification:** Check that all 4 files exist in `resources/profiles/UltiMaker/process/`

### Phase 2: Create Core-Specific Process Presets
1. Generate all AA core presets first (most common) - 11 files
2. Generate BB core presets (support) - 8 files
3. Generate CC core presets (abrasive) - 7 files
4. Generate AA+/CC+ presets (hardened variants) - 10 files
5. Generate CC RED presets (S3/S5/S7 0.6mm only) - 1 file
6. Generate HT presets (S6/S8 and Factor 4 0.4mm) - 2 files

**Verification:** Check that all 38 files exist with correct naming

### Phase 3: Update Machine Profiles

**24 machine profiles need updates to `printer_extruder_variant`:**

**S3/S5/S7 Family (12 files):**
- `UltiMaker S3 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S3 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S3 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S3 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`
- `UltiMaker S5 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S5 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S5 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S5 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`
- `UltiMaker S7 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S7 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S7 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S7 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`

**S6/S8 Family (8 files):**
- `UltiMaker S6 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S6 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S6 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S6 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`
- `UltiMaker S8 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker S8 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker S8 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker S8 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`

**Factor 4 Family (4 files):**
- `UltiMaker Factor 4 0.25 nozzle.json` → UPDATE to `["AA 0.25", "AA 0.25"]`
- `UltiMaker Factor 4 0.4 nozzle.json` → UPDATE to `["AA 0.4", "AA 0.4"]`
- `UltiMaker Factor 4 0.6 nozzle.json` → UPDATE to `["AA 0.6", "AA 0.6"]`
- `UltiMaker Factor 4 0.8 nozzle.json` → UPDATE to `["AA 0.8", "AA 0.8"]`

**Verification:** Check that all 24 machine profiles have correct `printer_extruder_variant` values

### Phase 4: Update Default Print Profile References

**24 machine profiles need `default_print_profile` updates:**

Update each machine file's `default_print_profile` to point to the corresponding AA core preset (see table in "Machine Profile Updates Required" section above).

**Verification:** Check that all 24 machine profiles have correct `default_print_profile` values

### Phase 5: Update UltiMaker.json Vendor Configuration

1. Add 4 new common process preset entries to `process_list`
2. Add 38 new process preset entries to `process_list`
3. Remove 12 old generic process preset entries from `process_list`

**Verification:** 
- Count entries in `process_list`: should have 4 (commons) + 38 (new) + 7 (kept old) = 49 total
- Verify no old generic entries remain
- Verify all new entries are present

### Phase 6: Delete Old Generic Presets
1. Delete all 12 existing generic UltiMaker process presets that mix core types

**Verification:** Check that only the 12 specified files are deleted, others remain

### Phase 7: Test and Validate
1. Test process preset selection with each core type
2. Verify `update_process_presets()` correctly selects matching preset
3. Validate settings are appropriate for each core type
4. Test with actual printer configurations

---

## Summary (CORRECTED)

**Total New Files:** 42 (4 common + 38 process presets)
**Total Files to Delete:** 12
**Total Machine Profiles to Update `printer_extruder_variant`:** 24
**Total Machine Profiles to Update `default_print_profile`:** 24
**Vendor Configuration Files to Update:** 1 (`UltiMaker.json`)

**Key Naming Convention:**
- Format: `{layer_height}mm Standard @UltiMaker {Family} {Core} {nozzle_size}.json`
- Examples:
  - `0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25.json`
  - `0.20mm Standard @UltiMaker S6-S8 CC+ 0.4.json`
  - `0.20mm Standard @UltiMaker Factor 4 HT 0.4.json`

---

## Troubleshooting Guide

### Common Mistake 1: Wrong Layer Height in Filename
**Problem:** Using `0.10mm` prefix for 0.25 nozzle files when layer height is 0.15mm
**Solution:** Filename prefix must match `layer_height` value in the file:
- If `layer_height: "0.15"` → filename starts with `0.15mm`
- If `layer_height: "0.2"` → filename starts with `0.20mm`

### Common Mistake 2: Including Wrong Printers in compatible_printers
**Problem:** Adding generic entries like `"UltiMaker S6 0.4 nozzle"` to AA preset
**Solution:** Each preset should ONLY contain printers matching its core type:
- AA preset → only `"UltiMaker S6 AA 0.4"`, `"UltiMaker S8 AA 0.4"`
- BB preset → only `"UltiMaker S6 BB 0.4"`, `"UltiMaker S8 BB 0.4"`

### Common Mistake 3: Forgetting to Update UltiMaker.json
**Problem:** Creating files but not registering them in vendor config
**Solution:** Always add entries to `process_list` in `UltiMaker.json` or presets won't appear in UI

### Common Mistake 4: Wrong inherits Value
**Problem:** Process preset inherits from `fdm_process_common` instead of family common
**Solution:** Use correct inheritance:
- S3/S5/S7 presets → inherit from `fdm_process_ultimaker_s3s5s7_common`
- S6/S8 presets → inherit from `fdm_process_ultimaker_s6s8_common`
- Factor 4 presets → inherit from `fdm_process_ultimaker_factor4_common`

### Common Mistake 5: Not Updating printer_extruder_variant
**Problem:** Leaving machine profiles with old values like `["AA+ 0.4", "AA+ 0.4"]`
**Solution:** Update ALL 24 machine profiles to use AA variants for default

### Verification Commands

**Count process files:**
```bash
ls resources/profiles/UltiMaker/process/*.json | wc -l
```

**Check for missing compatible_printers:**
```bash
grep -L "compatible_printers" resources/profiles/UltiMaker/process/*.json
```

**Verify UltiMaker.json syntax:**
```bash
python -m json.tool resources/profiles/UltiMaker.json > /dev/null && echo "Valid JSON"
```

---

## Next Steps

1. **DECISION REQUIRED:** Confirm layer height for 0.25 nozzle (0.15mm vs 0.10mm)
2. Create the 4 common preset files
3. Create the 38 process preset files following the inventory above
4. Update 24 machine profile files with correct `printer_extruder_variant`
5. Update 24 machine profile files with correct `default_print_profile`
6. Update `UltiMaker.json` vendor configuration file (add new presets to `process_list`, remove old ones)
7. Delete 12 old generic process preset files
8. Test and validate the complete setup
