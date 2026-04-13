# Cheetah G-Code Flavor - Remaining Implementation Tasks

## Overview

The Cheetah G-code flavor implementation is partially complete. Backend G-code generation is done, but UI updates and machine limits export are missing.

## Completed Work

### Backend (GCodeWriter.cpp)
- [x] `gcfCheetah` added to GCodeFlavor enum
- [x] `"cheetah"` flavor string mapping registered
- [x] `"Cheetah (UltiMaker)"` label added to GUI
- [x] M215 jerk handling implemented (×500,000 conversion to m/s³)
- [x] M214 K pressure advance implemented (with R0.04 constant)
- [x] `gcfCheetah` added to `use_mach_limits` check

## Remaining Tasks

### 1. Machine Limits Export in GCode.cpp

**File:** [`src/libslic3r/GCode.cpp:3815`](src/libslic3r/GCode.cpp:3815)

**Current Behavior:**
The `print_machine_envelope()` function only handles `gcfMarlinLegacy`, `gcfMarlinFirmware`, and `gcfRepRapFirmware`. It writes:
- M201 for max acceleration
- M203 for max speed
- M204 for acceleration settings
- M205/M566 for jerk limits

**Required Change:**
Add Cheetah handling to export machine limits with correct G-codes:
- M204 S for acceleration (same as Marlin Legacy)
- M215 X Y for jerk (with m/s³ conversion: ×500,000)

**Implementation Location:**
Insert after line 3815 where the flavor check begins. Add a new conditional block for `gcfCheetah`:

```cpp
// Add to the condition at line 3815
if ((flavor == gcfMarlinLegacy || flavor == gcfMarlinFirmware || 
     flavor == gcfRepRapFirmware || flavor == gcfCheetah) &&
    print.config().emit_machine_limits_to_gcode.value == true) {
```

Then add Cheetah-specific handling before the jerk output (around line 3851):

```cpp
// Cheetah jerk output (M215 with m/s³ conversion)
if (flavor == gcfCheetah) {
    // Cheetah uses M215 with integer values in m/s³
    // Conversion: mm/s × 500,000 = m/s³
    long jerk_x = std::lrint(print.config().machine_max_jerk_x.values.front() * 500000.0);
    long jerk_y = std::lrint(print.config().machine_max_jerk_y.values.front() * 500000.0);
    file.write_format("M215 X%ld Y%ld ; sets the jerk limits, m/s^3\n", jerk_x, jerk_y);
    // Skip Z/E jerk for Cheetah (not supported)
} else {
    // Existing M205/M566 code
    file.write_format(flavor == gcfRepRapFirmware ... );
}
```

**Note:** The acceleration code (M204) already works correctly for Cheetah because it falls through to the simple `M204 P R T` format. However, Cheetah should use `M204 S` (like Marlin Legacy). Verify the fallthrough behavior.

---

### 2. Dynamic Unit Labels in Tab.cpp

**File:** [`src/slic3r/GUI/Tab.cpp:5340`](src/slic3r/GUI/Tab.cpp:5340)

**Current Behavior:**
The jerk unit labels are static "mm/s" regardless of flavor.

**Required Change:**
Add a new method `update_jerk_unit_labels()` that:
1. Finds the "Jerk limitation" optgroup in the "Motion ability" page
2. Updates the sidetext for jerk fields based on flavor:
   - `gcfCheetah`: "m/s³"
   - Other flavors: "mm/s"

**Implementation Steps:**

1. **Add method declaration to Tab.hpp** (in TabPrinter class):
```cpp
void update_jerk_unit_labels(GCodeFlavor flavor);
```

2. **Add method implementation to Tab.cpp**:
```cpp
void TabPrinter::update_jerk_unit_labels(GCodeFlavor flavor)
{
    auto page = get_page(L("Motion ability"));
    if (!page) return;
    
    for (auto& optgroup : page->m_optgroups) {
        if (optgroup->title == L("Jerk limitation")) {
            const char* unit = (flavor == gcfCheetah) ? "m/s³" : "mm/s";
            
            for (const std::string& axis : {"x", "y", "z", "e"}) {
                std::string opt_key = "machine_max_jerk_" + axis;
                auto* option = optgroup->get_option(opt_key);
                if (option) {
                    option->opt.sidetext = wxString(unit);
                }
            }
            optgroup->reload_settings();
            break;
        }
    }
}
```

3. **Call the method from toggle_options()** (around line 5367):
```cpp
// After the jerk toggle loop
update_jerk_unit_labels(gcf);
```

---

### 3. Update Tooltips in PrintConfig.cpp

#### 3.1 Jerk Tooltips

**File:** [`src/libslic3r/PrintConfig.cpp`](src/libslic3r/PrintConfig.cpp) (search for `machine_max_jerk`)

**Required Change:**
Update the tooltip for jerk configuration to explain Cheetah unit conversion:

```cpp
def->tooltip = L("Maximum jerk of the axis.\n\n"
                 "Input values are always in mm/s.\n\n"
                 "Output depends on G-code flavor:\n"
                 "• Marlin/Repetier/Klipper: M205 (mm/s)\n"
                 "• Cheetah: M215 (m/s³, auto-converted from mm/s)\n\n"
                 "Conversion for Cheetah: Value × 500,000 = m/s³");
```

#### 3.2 G-code Flavor Tooltip

**File:** [`src/libslic3r/PrintConfig.cpp`](src/libslic3r/PrintConfig.cpp) (search for `gcode_flavor`)

**Required Change:**
Add Cheetah to the flavor tooltip:

```cpp
def->tooltip = L("What kind of G-code the printer is compatible with.\n\n"
                 "• Marlin: Standard Marlin firmware (M205 for jerk in mm/s)\n"
                 "• Klipper: Klipper firmware with extended commands\n"
                 "• RepRapFirmware: Duet/RepRapFirmware\n"
                 "• Marlin 2: Newer Marlin with separate travel accel (P/T/R)\n"
                 "• Cheetah: Ultimaker S6/S8 firmware (M215 for jerk in m/s³)\n\n"
                 "Note: Cheetah requires different jerk units (m/s³ vs mm/s). "
                 "Values are automatically converted.");
```

#### 3.3 Pressure Advance Tooltip

**File:** [`src/libslic3r/PrintConfig.cpp`](src/libslic3r/PrintConfig.cpp) (search for `pressure_advance`)

**Required Change:**
Add Cheetah M214 to the pressure advance tooltip:

```cpp
def->tooltip = L("Pressure advance (Klipper) AKA Linear advance factor (Marlin/Cheetah).\n\n"
                 "Different firmwares use different G-codes:\n"
                 "• Marlin: M900 K<value>\n"
                 "• Klipper: SET_PRESSURE_ADVANCE ADVANCE=<value>\n"
                 "• Cheetah: M214 K<value> R0.04\n"
                 "• RepRap: M572 D0 S<value>");
```

---

## Testing Checklist

After implementing remaining tasks:

### Backend Tests
- [ ] Machine limits export includes M215 for Cheetah
- [ ] M215 values are correctly converted (mm/s × 500,000)
- [ ] Acceleration export uses M204 S format for Cheetah

### UI Tests
- [ ] Cheetah appears in G-code flavor dropdown
- [ ] Selecting Cheetah updates jerk unit labels to "m/s³"
- [ ] Switching away from Cheetah updates jerk unit labels back to "mm/s"
- [ ] Tooltips explain unit conversion behavior

### Integration Tests
- [ ] Slice model with Cheetah flavor
- [ ] Verify output contains `M215 X<int> Y<int>` (not M205)
- [ ] Verify output contains `M204 S<val>` (not M204 P/T/R)
- [ ] Compare values with Cura-generated reference gcode

---

## Priority

1. **P0 (Critical)**: Machine limits export (GCode.cpp) - Required for correct G-code output
2. **P1 (High)**: UI unit labels (Tab.cpp) - Critical for user understanding
3. **P2 (Medium)**: Tooltips (PrintConfig.cpp) - Documentation improvement

---

*Document Version: 1.0*
*Created: 2026-04-12*
