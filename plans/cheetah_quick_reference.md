# Cheetah G-Code Flavor - Quick Reference

## TL;DR - What You Need to Know

### The Problem
OrcaSlicer currently outputs:
- `M205 X20 Y20` for jerk (should be `M215 X10000000 Y10000000`)
- `M900 K0.02` for linear advance (should be `M214 K0.02 R0.04`)

### The Fix (4 Files)
1. Add `gcfCheetah` to enum in `PrintConfig.hpp`
2. Register `"cheetah"` string in `PrintConfig.cpp`
3. Add Cheetah case to `set_jerk_xy()` in `GCodeWriter.cpp` (M215)
4. Add Cheetah case to `set_pressure_advance()` in `GCodeWriter.cpp` (M214)

---

## Command Reference

### Acceleration
| Firmware | Command | Example |
|----------|---------|---------|
| Marlin2 | `M204 P3000 R1000 T5000` | P=print, R=retract, T=travel |
| Cheetah | `M204 S2000` | S=acceleration (all moves) |

### Jerk
| Firmware | Command | Example | Unit |
|----------|---------|---------|------|
| Marlin2 | `M205 X20 Y20` | `M205 X20 Y20 Z0.4 E5` | mm/s |
| Cheetah | `M215 X10000000 Y10000000` | `M215 X5000000 Y5000000` | m/s³ |

### Linear Advance
| Firmware | Command | Example |
|----------|---------|---------|
| Marlin2 | `M900 K0.02` | Standard Marlin |
| Klipper | `SET_PRESSURE_ADVANCE ADVANCE=0.02` | Klipper format |
| Cheetah | `M214 K0.02 R0.04` | D parameter optional |

---

## Unit Conversion (CRITICAL)

### Jerk Conversion
```
Cheetah Value = Marlin Value × 500,000
```

| mm/s (Marlin) | m/s³ (Cheetah) |
|---------------|----------------|
| 10 | 5,000,000 |
| 20 | 10,000,000 |
| 30 | 15,000,000 |

**Code:**
```cpp
long cheetah_jerk = std::lrint(marlin_jerk * 500000.0);
```

### Linear Advance
- Same K value (0.02-2.0 typical)
- Cheetah adds R0.04 constant
- Optional D parameter for extruder index

---

## Code Changes

### 1. PrintConfig.hpp (Line ~33)
```cpp
enum GCodeFlavor : unsigned char {
    // ... existing flavors ...
    gcfCheetah,  // <-- ADD
    gcfNoExtrusion
};
```

### 2. PrintConfig.cpp (Line ~143)
```cpp
{ "cheetah", gcfCheetah },  // <-- ADD
```

### 3. GCodeWriter.cpp - set_jerk_xy() (Line ~254)
Add Cheetah case for M215 with unit conversion.

### 4. GCodeWriter.cpp - set_pressure_advance() (Line ~354)
Add Cheetah case for M214:
```cpp
if (FLAVOR_IS(gcfCheetah)) {
    gcode << "M214 K" << std::setprecision(4) << pa << " R0.04";
}
```

---

## Testing

### Verify M215 (Jerk)
```bash
grep "M215" output.gcode  # Should show large integers
grep "M205" output.gcode  # Should NOT appear for Cheetah
```

### Verify M214 (Linear Advance)
```bash
grep "M214" output.gcode  # Should show K value with R0.04
grep "M900" output.gcode  # Should NOT appear for Cheetah
```

### Expected Output
```gcode
; Good - Cheetah format:
M215 X10000000 Y10000000
M214 K0.02 R0.04
M204 S2000

; Bad - Marlin format (wrong for Cheetah):
M205 X20 Y20
M900 K0.02
M204 P2000 T2000
```

---

## Common Pitfalls

1. **Wrong Jerk Command**: Using `M205` instead of `M215`
2. **Wrong Jerk Units**: Outputting mm/s instead of m/s³
3. **Decimals in Jerk**: Cheetah expects integers, not floats
4. **Missing R Parameter**: M214 requires R0.04 constant
5. **Wrong Linear Advance**: Using `M900` instead of `M214`

---

## Resources

- Full Plan: `plans/cheetah_gcode_implementation_plan.md`
- Summary: `plans/IMPLEMENTATION_SUMMARY.md`
- Cura G-code: `../cura files/UltiMaker - S6 - PLA (AnyColor)/3D/model.gcode`
- Firmware Docs: `../info/Cheetah migration integration assesment.pdf`
