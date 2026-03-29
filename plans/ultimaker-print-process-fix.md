# UltiMaker Print Process Compatibility Fix Plan

## Problem Analysis

### Current State
1. **Process Profiles** (in `resources/profiles/UltiMaker/process/`):
   - Have `compatible_printers` lists with names like "UltiMaker S6 AA+ 0.4"
   - Follow pattern: `UltiMaker <model> <extruder_variant> <nozzle_size>`
   - Include multiple extruder variants (AA, BB, CC, AA+, CC+, HT, etc.)

2. **Printer Profiles** (in `resources/profiles/UltiMaker/machine/`):
   - Named like "UltiMaker S6 0.4 nozzle"
   - Follow pattern: `UltiMaker <model> <nozzle_size> nozzle`
   - Missing extruder variant in name

3. **Compatibility Check** (in `src/libslic3r/Preset.cpp`):
   - Uses exact string matching between printer profile name and process profile's `compatible_printers` list
   - No match = process profile not shown (falls back to "Default Setting")

### Printer Families and Their Process Profiles

1. **S3/S5/S7 Family** (share same process profile structure):
   - `0.20mm Standard @UltiMaker S3.json`
   - `0.20mm Standard @UltiMaker S5.json`  
   - `0.20mm Standard @UltiMaker S7.json`
   - Compatible printers include: AA 0.4, AA 0.25, AA 0.8, BB 0.4, BB 0.8, CC 0.4, CC 0.6, CC RED 0.6, AA+ 0.4, CC+ 0.4, CC+ 0.6, HT 0.6

2. **S6/S8 Family** (share same process profile structure):
   - `0.20mm Standard @UltiMaker S6.json`
   - `0.20mm Standard @UltiMaker S8.json`
   - Compatible printers include: AA+ 0.4, AA+ 0.25, AA+ 0.6, AA+ 0.8

3. **Factor 4 Family**:
   - `0.20mm Standard @UltiMaker Factor 4.json`
   - Compatible printers include: AA 0.4, AA 0.25, AA 0.8, BB 0.4, BB 0.8, CC 0.4, CC 0.6, CC RED 0.6, AA+ 0.4, CC+ 0.4, CC+ 0.6, HT 0.6

4. **UltiMaker 2 Family**:
   - `0.12mm Fine @UltiMaker 2.json`
   - `0.18mm Standard @UltiMaker 2.json`
   - `0.25mm Darft @UltiMaker 2.json`
   - Need to check compatibility

5. **Other Models**:
   - `0.20mm Standard @UltiMaker 2+ Connect.json`
   - Need to check compatibility

## Solution: Update Process Profiles (Option 1)

Add printer profile names to each process profile's `compatible_printers` list.

### Actual Printer Extruder Variant Support
Based on examining printer profile files:

1. **S3/S5/S7 Family**:
   - Printer profiles have `printer_extruder_variant: ["AA 0.4", "AA 0.4"]` (AA, not AA+)
   - Do NOT support "+" extruders (AA+, CC+) or HT extruders
   - Support AA, BB, CC variants only

2. **S6/S8 Family**:
   - Printer profiles have `printer_extruder_variant: ["AA+ 0.4", "AA+ 0.4"]` (AA+)
   - Support AA+ variant only

3. **Factor 4 Family**:
   - Need to check actual printer profile, but likely similar to S3/S5/S7

### Mapping Rules
For each printer profile name pattern "UltiMaker <model> <nozzle_size> nozzle", add to process profiles:
- For S3/S5/S7: Add AA variant with matching nozzle size (since printers only support AA)
- For S6/S8: Add AA+ variant with matching nozzle size  
- For Factor 4: Add AA variant with matching nozzle size (assuming similar to S3/S5/S7)

**CRITICAL**: The process profiles currently have INCORRECT `compatible_printers` lists that include extruder variants the printers don't support (AA+, CC+, HT). We need to:
1. Fix the process profiles to only include supported extruder variants
2. Add printer profile names to the compatible_printers lists

### Specific Changes Needed

#### 1. S6/S8 Family Process Profiles
**Current issue**: S6 process profile lists "UltiMaker S6 AA+ 0.4" which matches the printer's extruder variant, but printer profile name "UltiMaker S6 0.4 nozzle" doesn't match.

**Fix**: Add printer profile names to `compatible_printers` arrays:

**For S6 process profile:**
- Add "UltiMaker S6 0.4 nozzle" (maps to "UltiMaker S6 AA+ 0.4")
- Add "UltiMaker S6 0.25 nozzle" (maps to "UltiMaker S6 AA+ 0.25")
- Add "UltiMaker S6 0.6 nozzle" (maps to "UltiMaker S6 AA+ 0.6")
- Add "UltiMaker S6 0.8 nozzle" (maps to "UltiMaker S6 AA+ 0.8")

**For S8 process profile:**
- Add "UltiMaker S8 0.4 nozzle" (maps to "UltiMaker S8 AA+ 0.4")
- Add "UltiMaker S8 0.25 nozzle" (maps to "UltiMaker S8 AA+ 0.25")
- Add "UltiMaker S8 0.6 nozzle" (maps to "UltiMaker S8 AA+ 0.6")
- Add "UltiMaker S8 0.8 nozzle" (maps to "UltiMaker S8 AA+ 0.8")

#### 2. S3/S5/S7 Family Process Profiles
**Current issue**: Process profiles list AA+, CC+, HT variants but printers only support AA, BB, CC variants.

**Fix**: 
1. **Remove unsupported variants** from `compatible_printers`:
   - Remove "UltiMaker S5 AA+ 0.4", "UltiMaker S5 CC+ 0.4", "UltiMaker S5 CC+ 0.6", "UltiMaker S5 HT 0.6"
   - Keep only AA, BB, CC variants

2. **Add printer profile names** to `compatible_printers`:

**For S5 0.4 nozzle profile:**
- Add "UltiMaker S5 0.4 nozzle" (maps to "UltiMaker S5 AA 0.4" since printer has AA 0.4)

**For S5 0.25 nozzle profile:**
- Add "UltiMaker S5 0.25 nozzle" (maps to "UltiMaker S5 AA 0.25")

**For S5 0.6 nozzle profile:**
- Add "UltiMaker S5 0.6 nozzle" (maps to "UltiMaker S5 CC 0.6")

**For S5 0.8 nozzle profile:**
- Add "UltiMaker S5 0.8 nozzle" (maps to "UltiMaker S5 AA 0.8")

**Apply same changes to S3 and S7 process profiles.**

#### 3. Factor 4 Family Process Profile
**Current issue**: Factor 4 process profile lists AA+, CC+, HT variants (lines 17-21). According to your knowledge:
- Factor 4 **does NOT support "+" extruders** (AA+, CC+)
- Factor 4 **DOES support HT extruder**
- Printer profiles show `printer_extruder_variant: ["AA 0.4", "AA 0.4"]` for all nozzle sizes, but there may be other configurations

**Fix**:
1. **Remove unsupported "+" variants** from `compatible_printers`:
   - Remove "UltiMaker Factor 4 AA+ 0.4", "UltiMaker Factor 4 CC+ 0.4", "UltiMaker Factor 4 CC+ 0.6"
   - **KEEP "UltiMaker Factor 4 HT 0.6"** (supported)
   - Keep AA 0.4, AA 0.25, AA 0.8, BB 0.4, BB 0.8, CC 0.4, CC 0.6, CC RED 0.6

2. **Add printer profile names** to `compatible_printers`:
   - Add "UltiMaker Factor 4 0.4 nozzle" (maps to "UltiMaker Factor 4 AA 0.4")
   - Add "UltiMaker Factor 4 0.25 nozzle" (maps to "UltiMaker Factor 4 AA 0.4")
   - Add "UltiMaker Factor 4 0.6 nozzle" (maps to "UltiMaker Factor 4 AA 0.4")
   - Add "UltiMaker Factor 4 0.8 nozzle" (maps to "UltiMaker Factor 4 AA 0.4")

**Note**: All Factor 4 printer profiles use AA 0.4 extruder variant regardless of nozzle size, so we need to map all printer profile names to "UltiMaker Factor 4 AA 0.4" in the process profile.

## Implementation Steps

1. **Update S6 process profile** (`0.20mm Standard @UltiMaker S6.json`)
2. **Update S8 process profile** (`0.20mm Standard @UltiMaker S8.json`)
3. **Update S3 process profile** (`0.20mm Standard @UltiMaker S3.json`)
4. **Update S5 process profile** (`0.20mm Standard @UltiMaker S5.json`)
5. **Update S7 process profile** (`0.20mm Standard @UltiMaker S7.json`)
6. **Update Factor 4 process profile** (`0.20mm Standard @UltiMaker Factor 4.json`)
7. **Check and update UltiMaker 2 process profiles** if needed
8. **Check and update UltiMaker 2+ Connect process profile** if needed
9. **Build and test** to verify process profiles appear in selection list

## Alternative Solution (Option 2)

Rename printer profiles to match process profile expectations:
- Change "UltiMaker S6 0.4 nozzle" to "UltiMaker S6 AA+ 0.4"
- Change "UltiMaker S5 0.4 nozzle" to multiple profiles for each extruder variant

**Pros of Option 1 (updating process profiles):**
- Simpler, fewer changes
- Maintains backward compatibility
- Doesn't break existing user configurations

**Cons of Option 1:**
- Process profiles become less specific (multiple extruder variants map to same printer)

## Testing Plan

1. Build OrcaSlicer after changes
2. Launch application
3. Select UltiMaker S6 printer with AA+ print core
4. Verify "0.20mm Standard @UltiMaker S6" appears in print process dropdown
5. Repeat for other printer models and nozzle sizes
6. Verify "Default Setting" is no longer the only option

## Files to Modify

1. `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S6.json`
2. `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S8.json`
3. `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S3.json`
4. `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S5.json`
5. `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker S7.json`
6. `resources/profiles/UltiMaker/process/0.20mm Standard @UltiMaker Factor 4.json`

## Code Reference

The compatibility check is in `src/libslic3r/Preset.cpp`, function `is_compatible_with_printer()` (lines 700-731). It performs exact string matching between printer profile name and entries in the process profile's `compatible_printers` list.
