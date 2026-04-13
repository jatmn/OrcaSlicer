# Cheetah G-Code Implementation - Executive Summary

## Project Context

### What is Cheetah?
Cheetah is a **custom motion planner firmware** developed for Ultimaker S6/S8 printers. It uses **non-standard G-codes** that differ significantly from Marlin.

### Why This Matters
- Ultimaker S6/S8 printers use Cheetah firmware
- OrcaSlicer needs to generate compatible G-code for these printers
- Current Marlin2 output is **incompatible** with Cheetah

---

## Key Findings from Analysis

### 1. Jerk Control - MAJOR DIFFERENCE

**Cura (Cheetah flavor) generates:**
```gcode
M215 X10000000 Y10000000
```

**OrcaSlicer (Marlin2) currently generates:**
```gcode
M205 X20 Y20 Z0.4 E5
```

**Differences:**
| Aspect | Cheetah | Marlin |
|--------|---------|--------|
| M-Code | `M215` | `M205` |
| Unit | m/s³ (meters/sec³) | mm/s |
| Values | 10,000,000 | 20 |
| Type | Integer | Float |
| Z/E | Not included | Included |

**Conversion:** `Cheetah = Marlin × 500,000`

---

### 2. Linear Advance - DIFFERENT M-CODE

**Cura (Cheetah flavor) generates:**
```gcode
M214 K0.75 R0.04        ; S8 format
M214 D0 K0.75 R0.04     ; S6 format (with D parameter)
```

**OrcaSlicer (Marlin2) currently generates:**
```gcode
M900 K0.02              ; Marlin
SET_PRESSURE_ADVANCE ADVANCE=0.02  ; Klipper
```

**Differences:**
| Firmware | Command | Parameters |
|----------|---------|------------|
| Marlin | `M900 K<val>` | K = linear advance |
| Klipper | `SET_PRESSURE_ADVANCE ADVANCE=<val>` | ADVANCE = linear advance |
| Cheetah | `M214 K<val> R0.04` | K = linear advance, R = constant |

**Note:** R parameter is always 0.04 in Cura configs. D parameter is optional (extruder index).

---

### 3. Acceleration Control - MINOR DIFFERENCE

**Cura (Cheetah flavor) generates:**
```gcode
M204 S2000
```

**OrcaSlicer (Marlin2) currently generates:**
```gcode
M204 P2000 R1000 T2000
```

**Differences:**
- Cheetah uses simple `S` parameter (like Marlin Legacy)
- Marlin2 uses `P` (print), `R` (retract), `T` (travel)
- **Good news**: OrcaSlicer already supports `M204 S` for legacy Marlin

---

## Implementation Checklist

### Phase 1: Add Cheetah Flavor (Required)
- [ ] **PrintConfig.hpp**: Add `gcfCheetah` to `GCodeFlavor` enum
- [ ] **PrintConfig.cpp**: Register `"cheetah"` string mapping
- [ ] **GCodeWriter.cpp**: Add Cheetah case to `set_jerk_xy()` for M215
- [ ] **GCodeWriter.cpp**: Add Cheetah case to `set_pressure_advance()` for M214

### Phase 2: Machine Limits (Required)
- [ ] **GCode.cpp**: Add Cheetah handling in `export_machine_config()`
- [ ] **GCodeWriter.cpp**: Add `gcfCheetah` to `use_mach_limits` check

### Phase 3: UI Updates (Required)
- [ ] **Tab.cpp**: Dynamic jerk unit labels (mm/s ↔ m/s³)
- [ ] **PrintConfig.cpp**: Update tooltips for pressure_advance
- [ ] **PrintConfig.cpp**: Update tooltips for machine_max_jerk

### Phase 4: Testing (Required)
- [ ] Build and verify compilation
- [ ] Slice test model with Cheetah flavor
- [ ] Verify M215 output format (jerk)
- [ ] Verify M214 output format (linear advance)
- [ ] Verify M204 S output format (acceleration)
- [ ] Compare with Cura-generated reference

---

## Code Locations

| File | Function | Line (approx) | Change |
|------|----------|---------------|--------|
| `PrintConfig.hpp` | `GCodeFlavor` enum | ~33 | Add `gcfCheetah` |
| `PrintConfig.cpp` | `s_keys_map_GCodeFlavor` | ~143 | Add `"cheetah"` mapping |
| `PrintConfig.cpp` | GUI enum values | ~3604 | Add `"cheetah"` to list |
| `GCodeWriter.cpp` | `set_jerk_xy()` | ~254 | Add M215 for Cheetah |
| `GCodeWriter.cpp` | `set_pressure_advance()` | ~354 | Add M214 for Cheetah |
| `GCodeWriter.cpp` | `apply_print_config()` | ~27 | Add to `use_mach_limits` |
| `GCode.cpp` | `export_machine_config()` | ~3829 | Add Cheetah handling |
| `Tab.cpp` | `toggle_options()` | ~5340 | Dynamic unit labels |

---

## Sample Output Comparison

### Cura Cheetah Output
```gcode
;LAYER:2
M204 S2500                    ; Acceleration
M215 X2000000 Y2000000        ; Jerk: 2,000,000 m/s³
;TYPE:WALL-OUTER
G1 F1800 E42.51276
G1 F3600 X174.794 Y110.202 E42.68782
M204 S10000                   ; High acceleration
M215 X10000000 Y10000000      ; High jerk for travel
G0 F30000 X174.394 Y129.394
```

### Expected OrcaSlicer Cheetah Output
```gcode
;LAYER:2
M204 S2500                    ; Correct: S parameter
M215 X2000000 Y2000000        ; Correct: M215 with m/s³
;TYPE:WALL-OUTER
G1 F1800 E42.51276
M214 K0.02 R0.04              ; Linear advance (M214)
G1 F3600 X174.794 Y110.202 E42.68782
M204 S10000                   ; Correct: S parameter
M215 X10000000 Y10000000      ; Correct: M215
G0 F30000 X174.394 Y129.394
```

---

## Unit Conversion Reference

### Jerk Conversion Table
| Application | Marlin (mm/s) | Cheetah (m/s³) |
|-------------|---------------|----------------|
| Outer Wall | 4 | 2,000,000 |
| Inner Wall | 8 | 4,000,000 |
| Infill | 15 | 7,500,000 |
| Travel | 20 | 10,000,000 |

**Formula:** `Cheetah = Marlin × 500,000`

### Linear Advance
- Same K value range (0.02-2.0 typical)
- Cheetah format: `M214 K<value> R0.04`
- R parameter is constant 0.04

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Wrong conversion factor | Motion issues | Verify with firmware docs |
| Missing R in M214 | Linear advance fails | Always include R0.04 |
| Backward compatibility | Breaking existing users | Only apply when `gcfCheetah` selected |

---

## Cheetah-Specific M-Codes

| M-Code | Description | Format | Example |
|--------|-------------|--------|---------|
| M204 | Acceleration | `M204 S<val>` | `M204 S2000` |
| M214 | Linear Advance | `M214 K<val> R0.04` | `M214 K0.02 R0.04` |
| M215 | Jerk | `M215 X<int> Y<int>` | `M215 X10000000` |
| M213 | Undercut | `M213 U<mm>` | `M213 U0.1` |

---

## Related Documents

- `cheetah_gcode_implementation_plan.md` - Full technical details with code
- `cheetah_quick_reference.md` - Quick reference for developers
- `../info/Cheetah migration integration assesment.pdf` - Firmware documentation
- `../cura files/` - Cura-generated reference G-code

---

*Version: 1.1*
*Date: 2026-04-12*
*Status: Ready for Implementation*
