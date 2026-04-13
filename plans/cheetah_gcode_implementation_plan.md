# Cheetah G-Code Flavor Implementation Plan

## Executive Summary

This document provides a detailed analysis of the differences between **Cura's Cheetah G-code flavor** (used for Ultimaker S6/S8 with Cheetah firmware) and **OrcaSlicer's Marlin2 implementation**. The goal is to implement Cheetah flavor support in OrcaSlicer's UFPWriter to ensure compatibility with Ultimaker S6 printers running Cheetah firmware.

**⚠️ CRITICAL WARNING**: Cheetah is custom firmware with NON-STANDARD G-code. Do not assume standard Marlin behavior applies.

---

## Part 1: Analysis of Differences

### 1.1 Acceleration Commands

#### Cheetah (Cura Output)
```gcode
M204 S2000      ; Set acceleration to 2000 mm/s^2
M204 S2500      ; Layer change - set to 2500 mm/s^2  
M204 S10000     ; High speed moves
M204 S20000     ; Print moves
```

**Key Characteristics:**
- **Command**: `M204 S<value>`
- **Unit**: mm/s² (same as Marlin)
- **Format**: Simple S parameter only
- **No separate travel/print acceleration** via P/T/R parameters

#### Marlin2 (OrcaSlicer Current)
```gcode
; For gcfMarlinFirmware:
M204 P3000 R1000 T5000   ; P=print, R=retract, T=travel

; For gcfMarlinLegacy:
M204 S3000                 ; Simple S parameter
```

**Key Differences:**
| Aspect | Cheetah | Marlin2 (OrcaSlicer) |
|--------|---------|---------------------|
| Command | `M204 S<val>` | `M204 P<val> R<val> T<val>` or `M204 S<val>` |
| Parameters | S only | P, R, T (firmware) / S (legacy) |
| Separate travel accel | No (S covers all) | Yes (T parameter) |

---

### 1.2 Jerk Commands - MAJOR DIFFERENCE

#### Cheetah (Cura Output)
```gcode
M215 X10000000 Y10000000    ; Very high jerk for travel (10,000,000)
M215 X5000000 Y5000000      ; Medium jerk for printing (5,000,000)
M215 X2000000 Y2000000      ; Lower jerk for outer walls (2,000,000)
```

**Key Characteristics:**
- **Command**: `M215 X<value> Y<value>` (NOT M205!)
- **Unit**: m/s³ (meters per second cubed, NOT mm/s)
- **Format**: Integer values only, no decimals
- **Large values**: Values in millions (e.g., 10,000,000)

#### Marlin2 (OrcaSlicer Current)
```gcode
M205 X20 Y20 Z0.4 E5.0      ; Standard Marlin jerk (mm/s)
```

**Key Differences:**
| Aspect | Cheetah | Marlin2 (OrcaSlicer) |
|--------|---------|---------------------|
| Command | `M215` | `M205` |
| Unit | m/s³ | mm/s |
| Format | `M215 X<int> Y<int>` | `M205 X<float> Y<float> Z<float> E<float>` |
| Example Values | 10,000,000 | 20 |
| Z/E jerk | Not set via M215 | Set via M205 Z/E |

**⚠️ CRITICAL UNIT CONVERSION:**

Based on observed Cura output and Ultimaker S8 config:
- Cura outputs: `10000000` for jerk
- Config defines: `1000000` as default XY jerk with unit `m/s³`
- Marlin equivalent: ~20 mm/s

**Conversion Factor:**
```
Cheetah Value = Marlin Value (mm/s) × 500,000
```

Example conversions:
| Marlin (mm/s) | Cheetah (m/s³) |
|---------------|----------------|
| 10 | 5,000,000 |
| 20 | 10,000,000 |
| 30 | 15,000,000 |

---

### 1.3 Other Cheetah-Specific Codes

From Cura-generated gcode and documentation:

```gcode
; Header initialization
M213 U0.1              ; Undercut setting (Cheetah-specific)
M214 D0 K0.75 R0.04    ; Linear advance (different syntax from M900)
```

From PDF documentation:
- `M204` - Set acceleration (standard S parameter)
- `M205` - In Cheetah: used for "minimum print/travel/segment times"
- `M215` - Set extrusion subsampling (in Cheetah's older implementation)

**⚠️ Note**: The PDF suggests `M215` was originally for "extrusion subsampling", but current Cura uses it for jerk. This confirms Cheetah is custom firmware with evolving G-code usage.

---

## Part 2: Current OrcaSlicer Implementation

### 2.1 GCodeWriter.cpp - set_jerk_xy()

**Current Code (lines 254-290):**
```cpp
std::string GCodeWriter::set_jerk_xy(double jerk)
{
    if (jerk < 0.01 || is_approx(jerk, m_last_jerk))
        return std::string();
    
    m_last_jerk = jerk;

    std::ostringstream gcode;
    if (FLAVOR_IS(gcfKlipper)) {
        gcode << "SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=" << jerk;
    } else {
        double jerk_x = jerk;
        double jerk_y = jerk;
        if (m_max_jerk_x > 0 && jerk_x > m_max_jerk_x)
            jerk_x = m_max_jerk_x;
        if (m_max_jerk_y > 0 && jerk_y > m_max_jerk_y)
            jerk_y = m_max_jerk_y;
        
        gcode << "M205 X" << jerk_x << " Y" << jerk_y;  // <-- Need M215 for Cheetah
    }
      
    if (m_is_bbl_printers)
        gcode << std::setprecision(2) << " Z" << m_max_jerk_z << " E" << m_max_jerk_e;

    if (GCodeWriter::full_gcode_comment) gcode << " ; adjust jerk";
    gcode << "\n";

    return gcode.str();
}
```

### 2.2 GCodeWriter.cpp - set_acceleration_internal()

**Current Code (lines 215-252):**
```cpp
std::string GCodeWriter::set_acceleration_internal(Acceleration type, unsigned int acceleration)
{
    // ... clamping logic ...

    std::ostringstream gcode;
    if (FLAVOR_IS(gcfRepetier))
        gcode << (separate_travel ? "M202 X" : "M201 X") << acceleration << " Y" << acceleration;
    else if (FLAVOR_IS(gcfRepRapFirmware) || FLAVOR_IS(gcfMarlinFirmware))
        gcode << (separate_travel ? "M204 T" : "M204 P") << acceleration;
    else if (FLAVOR_IS(gcfKlipper)) {
        gcode << "SET_VELOCITY_LIMIT ACCEL=" << acceleration;
        // ...
    }
    else
        gcode << "M204 S" << acceleration;  // <-- Cheetah uses this (good)

    // ...
}
```

**For Cheetah**: The acceleration code already works because Cheetah uses `M204 S` like Marlin Legacy.

---

## Part 3: Implementation Steps

### Step 1: Add Cheetah to GCodeFlavor Enum

**File: `src/libslic3r/PrintConfig.hpp`**

Locate the enum (around line 33):
```cpp
enum GCodeFlavor : unsigned char {
    gcfMarlinLegacy, gcfKlipper, gcfRepRapFirmware, gcfMarlinFirmware, 
    gcfRepRapSprinter, gcfRepetier, gcfTeacup, gcfMakerWare, 
    gcfSailfish, gcfMach3, gcfMachinekit, gcfSmoothie, gcfNoExtrusion
};
```

Add `gcfCheetah` before `gcfNoExtrusion`:
```cpp
enum GCodeFlavor : unsigned char {
    gcfMarlinLegacy, gcfKlipper, gcfRepRapFirmware, gcfMarlinFirmware, 
    gcfRepRapSprinter, gcfRepetier, gcfTeacup, gcfMakerWare, 
    gcfSailfish, gcfMach3, gcfMachinekit, gcfSmoothie, 
    gcfCheetah,  // <-- ADD THIS
    gcfNoExtrusion
};
```

### Step 2: Register in PrintConfig.cpp

**File: `src/libslic3r/PrintConfig.cpp`**

Find `s_keys_map_GCodeFlavor` (around line 143) and add:
```cpp
static t_config_enum_values s_keys_map_GCodeFlavor {
    { "marlin",         gcfMarlinLegacy },
    { "reprap",         gcfRepRapSprinter },
    { "reprapfirmware", gcfRepRapFirmware },
    { "repetier",       gcfRepetier },
    { "teacup",         gcfTeacup },
    { "makerware",      gcfMakerWare },
    { "marlin2",        gcfMarlinFirmware },
    { "sailfish",       gcfSailfish },
    { "klipper",        gcfKlipper },
    { "smoothie",       gcfSmoothie },
    { "cheetah",        gcfCheetah },  // <-- ADD THIS
    { "no-extrusion",   gcfNoExtrusion }
};
```

Find the GUI enum values (around line 3604) and add:
```cpp
def->enum_values.push_back("cheetah");  // <-- ADD THIS
```

### Step 3: Update set_jerk_xy() for Cheetah

**File: `src/libslic3r/GCodeWriter.cpp`**

Modify the `set_jerk_xy()` function to handle Cheetah:

```cpp
std::string GCodeWriter::set_jerk_xy(double jerk)
{
    if (jerk < 0.01 || is_approx(jerk, m_last_jerk))
        return std::string();
    
    m_last_jerk = jerk;

    std::ostringstream gcode;
    if (FLAVOR_IS(gcfKlipper)) {
        // ... existing Klipper code ...
    } 
    else if (FLAVOR_IS(gcfCheetah)) {
        // Cheetah uses M215 with integer values in m/s³
        // Convert from mm/s to Cheetah units (multiply by 500,000)
        double jerk_x = jerk;
        double jerk_y = jerk;
        
        // Clamp to machine limits if set
        if (m_max_jerk_x > 0 && jerk_x > m_max_jerk_x)
            jerk_x = m_max_jerk_x;
        if (m_max_jerk_y > 0 && jerk_y > m_max_jerk_y)
            jerk_y = m_max_jerk_y;
        
        // Convert to Cheetah units and output as integers
        long cheetah_jerk_x = std::lrint(jerk_x * 500000.0);
        long cheetah_jerk_y = std::lrint(jerk_y * 500000.0);
        
        gcode << "M215 X" << cheetah_jerk_x << " Y" << cheetah_jerk_y;
    }
    else {
        // ... existing Marlin code (M205) ...
    }
      
    // ... rest of function ...
}
```

### Step 4: Update Machine Limits Export

**File: `src/libslic3r/GCode.cpp`**

Find the machine limits export code (around line 3829) and add Cheetah handling:

```cpp
// Around line 3829 in GCode::export_machine_config()

if (flavor == gcfCheetah) {
    // Cheetah uses M204 S format for acceleration
    int accel = int(print.config().machine_max_acceleration_extruding.values.front() + 0.5);
    file.write_format("M204 S%d ; sets acceleration, mm/sec^2\n", accel);
    
    // Cheetah uses M215 for jerk (in m/s³)
    long cheetah_jerk_x = std::lrint(print.config().machine_max_jerk_x.values.front() * 500000.0);
    long cheetah_jerk_y = std::lrint(print.config().machine_max_jerk_y.values.front() * 500000.0);
    file.write_format("M215 X%ld Y%ld ; sets the jerk limits, m/s^3\n",
        cheetah_jerk_x, cheetah_jerk_y);
}
else if (flavor == gcfRepRapFirmware) {
    // ... existing code ...
}
// ... rest of existing code ...
```

### Step 5: Add Cheetah Support in apply_print_config()

**File: `src/libslic3r/GCodeWriter.cpp`**

Update the `apply_print_config()` to include Cheetah in machine limits handling:

```cpp
void GCodeWriter::apply_print_config(const PrintConfig &print_config)
{
    // ... existing code ...
    
    // Add gcfCheetah to the use_mach_limits check
    bool use_mach_limits = print_config.gcode_flavor.value == gcfMarlinLegacy || 
                           print_config.gcode_flavor.value == gcfMarlinFirmware ||
                           print_config.gcode_flavor.value == gcfKlipper || 
                           print_config.gcode_flavor.value == gcfRepRapFirmware ||
                           print_config.gcode_flavor.value == gcfCheetah;  // <-- ADD THIS
    
    // ... rest of existing code ...
}
```

---

## Part 4: Reference Tables

### 4.1 Cheetah G-Code Summary

| M-Code | Description | Format | Example | Notes |
|--------|-------------|--------|---------|-------|
| M204 | Set acceleration | `M204 S<mm/s²>` | `M204 S2000` | Same as Marlin Legacy |
| M215 | Set jerk | `M215 X<int> Y<int>` | `M215 X10000000 Y10000000` | Unit: m/s³ |
| M213 | Undercut | `M213 U<mm>` | `M213 U0.1` | Cheetah-specific |
| M214 | Linear advance | `M214 D<val> K<val> R<val>` | `M214 D0 K0.75 R0.04` | Different from M900 |

### 4.2 Unit Conversion Reference

| Jerk (mm/s) | Cheetah (m/s³) | Use Case |
|-------------|----------------|----------|
| 10 | 5,000,000 | Conservative |
| 20 | 10,000,000 | Default/Travel |
| 30 | 15,000,000 | Aggressive |
| 40 | 20,000,000 | High speed |

**Formula:** `Cheetah = mm/s × 500,000`

---

## Part 5: Testing Checklist

### Unit Tests
- [ ] `gcfCheetah` enum value exists
- [ ] `"cheetah"` string maps to `gcfCheetah`
- [ ] `set_jerk_xy()` outputs `M215` for Cheetah
- [ ] `set_jerk_xy()` converts units correctly (20 → 10,000,000)
- [ ] `set_acceleration_internal()` outputs `M204 S` for Cheetah
- [ ] Machine limits export uses correct format

### Integration Tests
- [ ] Slice model with Cheetah flavor selected
- [ ] Verify output contains `M215 X<int> Y<int>` (not M205)
- [ ] Verify output contains `M204 S<val>` (not M204 P/T/R)
- [ ] Compare output values with Cura-generated gcode

### Hardware Validation
- [ ] Load gcode on Ultimaker S6 with Cheetah firmware
- [ ] Verify no G-code errors
- [ ] Verify smooth motion (no stuttering)
- [ ] Verify jerk limits are respected

---

## Appendix: Sample G-code Comparison

### Cura Cheetah Output
```gcode
;LAYER:0
M106 S255
M204 S2000
M215 X10000000 Y10000000
G1 F600 Z0.2
...
M215 X5000000 Y5000000
;TYPE:WALL-OUTER
G1 F3000 X... E...
```

### Expected OrcaSlicer Cheetah Output
```gcode
; Should match Cura format:
M204 S2000              ; Acceleration
M215 X10000000 Y10000000 ; Jerk in m/s³
```

### Current OrcaSlicer Marlin2 Output (WRONG for Cheetah)
```gcode
M204 P2000 T2000        ; Wrong: uses P/T parameters
M205 X20 Y20            ; Wrong: uses M205, wrong unit
```

---

## Summary of Key Differences

| Feature | Marlin2 (Current) | Cheetah (Required) |
|---------|-------------------|-------------------|
| Jerk Command | `M205` | `M215` |
| Jerk Unit | mm/s | m/s³ |
| Jerk Format | Float | Integer |
| Jerk Example | `M205 X20` | `M215 X10000000` |
| Accel Command | `M204 P/T/R` | `M204 S` |
| Accel Unit | mm/s² | mm/s² |

---

*Document Version: 1.0*
*Created: 2026-04-12*


---

## Part 6: UI Updates (Critical for User Experience)

Since Cheetah uses different units (m/s³ instead of mm/s) and a different M-code (M215 instead of M205), the UI must be updated to reflect these changes when Cheetah flavor is selected. Otherwise, users will see "mm/s" in the UI but the output will be in m/s³, causing confusion.

### 6.1 Motion Ability Page - Dynamic Unit Labels

**File:** `src/slic3r/GUI/Tab.cpp`

When Cheetah flavor is selected, the "Motion ability" page must:
1. Show "m/s³" instead of "mm/s" for jerk fields
2. Disable junction deviation (Cheetah uses Classic Jerk only)
3. Enable jerk fields for Cheetah (always enabled, unlike Marlin Firmware)

**Current Code Location:** (line ~5340 in `toggle_options()`)

Add to the Motion ability page logic:

```cpp
if (m_active_page->title() == L("Motion ability")) {
    auto gcf = m_config->option<ConfigOptionEnum<GCodeFlavor>>("gcode_flavor")->value;
    bool silent_mode = m_config->opt_bool("silent_mode");
    int  max_field   = silent_mode ? 2 : 1;
    
    // Existing travel acceleration toggle
    for (int i = 0; i < max_field; ++i)
        toggle_option("machine_max_acceleration_travel", gcf != gcfMarlinLegacy && gcf != gcfKlipper, i);
    toggle_line("machine_max_acceleration_travel", gcf != gcfMarlinLegacy && gcf != gcfKlipper);
    
    // UPDATED: Junction deviation only for Marlin Firmware (NOT Cheetah)
    for (int i = 0; i < max_field; ++i)
        toggle_option("machine_max_junction_deviation", gcf == gcfMarlinFirmware, i);
    toggle_line("machine_max_junction_deviation", gcf == gcfMarlinFirmware);

    // UPDATED: Enable jerk for Cheetah AND non-MarlinFirmware flavors
    // Cheetah ALWAYS uses Classic Jerk (M215), never junction deviation
    bool enable_jerk = (gcf != gcfMarlinFirmware) || (gcf == gcfCheetah);
    
    if (gcf == gcfMarlinFirmware) {
        // Existing logic for Marlin Firmware only
        const auto *junction_deviation = m_config->option<ConfigOptionFloats>("machine_max_junction_deviation");
        if (junction_deviation != nullptr) {
            const auto &values = junction_deviation->values;
            enable_jerk = std::all_of(values.begin(), values.end(), [](double val) { return val == 0.0; });
        } else {
            enable_jerk = true;
        }
    }
    
    for (int i = 0; i < max_field; ++i) {
        toggle_option("machine_max_jerk_x", enable_jerk, i);
        toggle_option("machine_max_jerk_y", enable_jerk, i);
        toggle_option("machine_max_jerk_z", enable_jerk, i);
        toggle_option("machine_max_jerk_e", enable_jerk, i);
    }
    
    // ADD: Update unit labels dynamically based on flavor
    update_jerk_unit_labels(gcf);
}
```

### 6.2 New Function: update_jerk_unit_labels()

**File:** `src/slic3r/GUI/Tab.cpp` (add as new method to TabPrinter class)

```cpp
void TabPrinter::update_jerk_unit_labels(GCodeFlavor flavor)
{
    // Get the optgroup for "Jerk limitation"
    auto page = get_page(L("Motion ability"));
    if (!page) return;
    
    for (auto& optgroup : page->m_optgroups) {
        if (optgroup->title == L("Jerk limitation")) {
            // Determine unit based on flavor
            const char* unit = (flavor == gcfCheetah) ? "m/s³" : "mm/s";
            
            // Update sidetext for all jerk options
            for (const std::string& axis : {"x", "y", "z", "e"}) {
                std::string opt_key = "machine_max_jerk_" + axis;
                auto* option = optgroup->get_option(opt_key);
                if (option) {
                    option->opt.sidetext = wxString(unit);
                }
            }
            
            // Force UI refresh
            optgroup->reload_settings();
            break;
        }
    }
}
```

Also add declaration to `Tab.hpp`:
```cpp
void update_jerk_unit_labels(GCodeFlavor flavor);
```

### 6.3 Update PrintConfig Tooltips

**File:** `src/libslic3r/PrintConfig.cpp` (line ~4197)

Update the jerk configuration tooltip to explain the flavor differences:

```cpp
// Add the machine jerk limits for XYZE axes
def = this->add("machine_max_jerk_" + axis.name, coFloats);
def->full_label = (boost::format("Maximum jerk %1%") % axis_upper).str();
// ... existing label setup ...

// UPDATED: Comprehensive tooltip explaining flavor differences
def->tooltip = L("Maximum jerk of the axis.\n\n"
                 "Input values are always in mm/s.\n\n"
                 "Output depends on G-code flavor:\n"
                 "• Marlin/Repetier/Klipper: M205 (mm/s)\n"
                 "• Cheetah: M215 (m/s³, auto-converted from mm/s)\n\n"
                 "Conversion for Cheetah: Value × 500,000 = m/s³");

// Default unit shown in UI (will be dynamically updated)
def->sidetext = L("mm/s");
def->min = 0;
def->mode = comSimple;
def->set_default_value(new ConfigOptionFloats(axis.max_jerk));
```

### 6.4 Update G-Code Flavor Tooltip

**File:** `src/libslic3r/PrintConfig.cpp` (line ~3600)

Update the gcode_flavor option description to include Cheetah:

```cpp
def = this->add("gcode_flavor", coEnum);
def->label = L("G-code flavor");
def->tooltip = L("What kind of G-code the printer is compatible with.\n\n"
                 "• Marlin: Standard Marlin firmware (M205 for jerk in mm/s)\n"
                 "• Klipper: Klipper firmware with extended commands\n"
                 "• RepRapFirmware: Duet/RepRapFirmware\n"
                 "• Marlin 2: Newer Marlin with separate travel accel (P/T/R)\n"
                 "• Cheetah: Ultimaker S6/S8 firmware (M215 for jerk in m/s³)\n\n"
                 "Note: Cheetah requires different jerk units (m/s³ vs mm/s). "
                 "Values are automatically converted.");
```

### 6.5 Files to Modify - UI Summary

| File | Change | Purpose |
|------|--------|---------|
| `src/slic3r/GUI/Tab.cpp` | Update `toggle_options()` | Enable jerk for Cheetah, call unit update |
| `src/slic3r/GUI/Tab.cpp` | Add `update_jerk_unit_labels()` | Dynamic unit label switching |
| `src/slic3r/GUI/Tab.hpp` | Add method declaration | Header for new function |
| `src/libslic3r/PrintConfig.cpp` | Update tooltips | Explain unit conversion to users |

---

## Part 7: Updated Testing Checklist

### 7.1 Backend Tests
- [ ] `gcfCheetah` enum value exists and compiles
- [ ] `"cheetah"` string maps correctly to `gcfCheetah`
- [ ] `set_jerk_xy()` outputs `M215 X<int> Y<int>` for Cheetah
- [ ] Unit conversion correct: 20 mm/s → 10,000,000 m/s³
- [ ] `set_acceleration_internal()` outputs `M204 S` for Cheetah
- [ ] Machine limits export uses Cheetah format

### 7.2 UI Tests (NEW)
- [ ] Cheetah appears in G-code flavor dropdown
- [ ] Selecting Cheetah updates jerk unit labels to "m/s³"
- [ ] Selecting Cheetah enables jerk fields (not junction deviation)
- [ ] Switching flavors updates units dynamically (mm/s ↔ m/s³)
- [ ] Tooltips explain unit conversion behavior
- [ ] Motion ability page shows correct units for selected flavor

### 7.3 Integration Tests
- [ ] Slice model with Cheetah flavor selected
- [ ] Verify output contains `M215 X<int> Y<int>` (not M205)
- [ ] Verify output contains `M204 S<val>` (not M204 P/T/R)
- [ ] Verify no `M205` in output for Cheetah
- [ ] Compare values with Cura-generated reference gcode

### 7.4 Hardware Validation
- [ ] Load gcode on Ultimaker S6 with Cheetah firmware
- [ ] Verify no G-code errors
- [ ] Verify smooth motion (no stuttering)
- [ ] Verify jerk limits are respected

---

## Part 8: Complete File Summary

### Backend Files (Phase 1)
| File | Changes |
|------|---------|
| `src/libslic3r/PrintConfig.hpp` | Add `gcfCheetah` to enum |
| `src/libslic3r/PrintConfig.cpp` | Register flavor, update tooltips |
| `src/libslic3r/GCodeWriter.cpp` | Update `set_jerk_xy()`, `apply_print_config()` |
| `src/libslic3r/GCode.cpp` | Update `export_machine_config()` |

### UI Files (Phase 2 - CRITICAL)
| File | Changes |
|------|---------|
| `src/slic3r/GUI/Tab.cpp` | Update `toggle_options()`, add `update_jerk_unit_labels()` |
| `src/slic3r/GUI/Tab.hpp` | Add `update_jerk_unit_labels()` declaration |

---

## Part 9: Implementation Priority

1. **P0 (Critical)**: Backend changes (Phase 1) - Required for correct G-code generation
2. **P1 (High)**: UI unit labels (Phase 2.1-2.2) - Critical for user understanding
3. **P2 (Medium)**: Tooltips and documentation (Phase 2.3-2.4)
4. **P3 (Low)**: Advanced features (M214 Linear Advance, etc.)

**⚠️ IMPORTANT**: Do not deploy without UI updates (Phase 2). Users will be confused if the UI shows "mm/s" but the output is in different units.

---

*Document Version: 1.1*
*Updated: 2026-04-12*
*Status: Ready for Implementation*


---

## Part 10: Linear Advance (M214) - VERIFIED INFORMATION

### 10.1 M214 Format (From Documentation)

**From Cheetah PDF (line 700):**
```
214: self.__handleSetLinearAdvance, # M214 K<linear advance gain>
```

**From Cura Ultimaker S6/S8 Extruder Configs:**
```json
// Ultimaker S6 - both extruders
"machine_extruder_start_code": { "value": "\"M214 D0 K{material_pressure_advance_factor} R0.04\"" }

// Ultimaker S8 - both extruders  
"machine_extruder_start_code": { "value": "\"M214 K{material_pressure_advance_factor} R0.04\"" }
```

**Observed Format:**
```gcode
M214 D0 K0.75 R0.04    ; S6 - with D parameter
M214 K0.75 R0.04       ; S8 - without D parameter
```

### 10.2 Parameter Analysis

| Parameter | Presence | Purpose |
|-----------|----------|---------|
| D | Optional | Extruder index (D0 = extruder 0) |
| K | Required | Linear advance factor (0.02-2.0 typical) |
| R | Required | Unknown constant (always 0.04 in configs) |

**Note:** S6 includes D0, S8 omits it. When D is omitted, applies to current/all extruders.

### 10.3 Implementation for set_pressure_advance()

**File:** `src/libslic3r/GCodeWriter.cpp` (line ~354)

Current function handles M900 (Marlin), SET_PRESSURE_ADVANCE (Klipper), M572 (RepRap). Add M214 for Cheetah:

```cpp
std::string GCodeWriter::set_pressure_advance(double pa) const
{
    std::ostringstream gcode;
    if (pa < 0)
        return gcode.str();
        
    if (FLAVOR_IS(gcfCheetah)) {
        // Cheetah uses M214 K<value> R0.04
        // D parameter omitted (applies to current extruder)
        gcode << "M214 K" << std::setprecision(4) << pa << " R0.04";
    }
    else if(m_is_bbl_printers) {
        gcode << "M900 K" << std::setprecision(4) << pa << " L1000 M10";
    }
    else {
        if (FLAVOR_IS(gcfKlipper))
            gcode << "SET_PRESSURE_ADVANCE ADVANCE=" << std::setprecision(4) << pa;
        else if(FLAVOR_IS(gcfRepRapFirmware))
            gcode << "M572 D0 S" << std::setprecision(4) << pa;
        else
            gcode << "M900 K" << std::setprecision(4) << pa;
    }
    
    if (GCodeWriter::full_gcode_comment) 
        gcode << " ; Override pressure advance value";
    gcode << "\n";
    
    return gcode.str();
}
```

### 10.4 UI Updates for Linear Advance

**File:** `src/libslic3r/PrintConfig.cpp` (line ~2183)

Update pressure_advance tooltip:

```cpp
def = this->add("pressure_advance", coFloats);
def->label = L("Pressure advance");
// UPDATED: Include Cheetah M214

def->tooltip = L("Pressure advance (Klipper) AKA Linear advance factor (Marlin/Cheetah).\n\n"
                 "Different firmwares use different G-codes:\n"
                 "• Marlin: M900 K<value>\n"
                 "• Klipper: SET_PRESSURE_ADVANCE ADVANCE=<value>\n"
                 "• Cheetah: M214 K<value> R0.04\n"
                 "• RepRap: M572 D0 S<value>");

def->max = 2;
def->mode = comAdvanced;
def->set_default_value(new ConfigOptionFloats { 0.02 });
```

### 10.5 Testing M214 Output

- [ ] Verify M214 K format in output gcode
- [ ] Verify R0.04 constant is included
- [ ] Verify no D parameter (single extruder)
- [ ] Compare with Cura-generated reference

---

*End of Linear Advance Section*
*Updated: 2026-04-12*
