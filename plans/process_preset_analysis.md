# Process Preset Analysis and Recreation Plan

## Current Issues Identified

### 1. Incompatible Printer Coverage Problems

**0.4mm Nozzle - S3/S5/S7:**
- Missing AA+ and CC+ hardened core variants
- Only has AA, BB, CC (incomplete)

**0.25mm Nozzle - S3/S5/S7:**
- Missing BB cores entirely
- Only has AA and CC

**0.6mm Nozzle - S3/S5/S7:**
- Missing AA cores
- Has invalid "CC RED" core names (not real UltiMaker cores)

**Factor 4:**
- Has "HT" core which needs verification
- Missing AA+ and CC+ variants

### 2. Invalid Core Names Found
- "CC RED 0.6" - NOT a valid UltiMaker core
- "HT" for Factor 4 - needs verification

### 3. Missing Process Variants
Only "Standard" profiles exist. Missing:
- Draft (fast printing)
- Fine (high quality)
- Quality (balanced)
- Speed (fast with acceptable quality)

### 4. Settings Not Optimized
Current settings appear to be generic defaults, not UltiMaker-specific.

## Correct Core Types by Nozzle Size

Based on UltiMaker documentation:

**0.25mm Nozzle:**
- AA 0.25 (standard)
- BB 0.25 (support)

**0.4mm Nozzle:**
- AA 0.4 (standard)
- BB 0.4 (support)
- CC 0.4 (abrasive)
- AA+ 0.4 (hardened standard)
- CC+ 0.4 (hardened abrasive)

**0.6mm Nozzle:**
- AA 0.6 (standard)
- CC 0.6 (abrasive)
- CC+ 0.6 (hardened abrasive)

**0.8mm Nozzle:**
- AA 0.8 (standard)
- BB 0.8 (support)

## Required Process Presets

### S3/S5/S7 Family (grouped together)

**0.25mm Nozzle:**
- 0.15mm Standard @UltiMaker S3-S5-S7 0.25 nozzle
- 0.15mm Fine @UltiMaker S3-S5-S7 0.25 nozzle
- 0.15mm Draft @UltiMaker S3-S5-S7 0.25 nozzle

**0.4mm Nozzle:**
- 0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle
- 0.20mm Quality @UltiMaker S3-S5-S7 0.4 nozzle
- 0.20mm Draft @UltiMaker S3-S5-S7 0.4 nozzle
- 0.20mm Speed @UltiMaker S3-S5-S7 0.4 nozzle

**0.6mm Nozzle:**
- 0.30mm Standard @UltiMaker S3-S5-S7 0.6 nozzle
- 0.30mm Draft @UltiMaker S3-S5-S7 0.6 nozzle
- 0.30mm Speed @UltiMaker S3-S5-S7 0.6 nozzle

**0.8mm Nozzle:**
- 0.40mm Standard @UltiMaker S3-S5-S7 0.8 nozzle
- 0.40mm Draft @UltiMaker S3-S5-S7 0.8 nozzle

### S6/S8 Family (grouped together)

**0.25mm Nozzle:**
- 0.15mm Standard @UltiMaker S6-S8 0.25 nozzle
- 0.15mm Fine @UltiMaker S6-S8 0.25 nozzle
- 0.15mm Draft @UltiMaker S6-S8 0.25 nozzle

**0.4mm Nozzle:**
- 0.20mm Standard @UltiMaker S6-S8 0.4 nozzle
- 0.20mm Quality @UltiMaker S6-S8 0.4 nozzle
- 0.20mm Draft @UltiMaker S6-S8 0.4 nozzle
- 0.20mm Speed @UltiMaker S6-S8 0.4 nozzle

**0.6mm Nozzle:**
- 0.30mm Standard @UltiMaker S6-S8 0.6 nozzle
- 0.30mm Draft @UltiMaker S6-S8 0.6 nozzle
- 0.30mm Speed @UltiMaker S6-S8 0.6 nozzle

**0.8mm Nozzle:**
- 0.40mm Standard @UltiMaker S6-S8 0.8 nozzle
- 0.40mm Draft @UltiMaker S6-S8 0.8 nozzle

### Factor 4 Family

**0.25mm Nozzle:**
- 0.15mm Standard @UltiMaker Factor 4 0.25 nozzle
- 0.15mm Fine @UltiMaker Factor 4 0.25 nozzle

**0.4mm Nozzle:**
- 0.20mm Standard @UltiMaker Factor 4 0.4 nozzle
- 0.20mm Quality @UltiMaker Factor 4 0.4 nozzle
- 0.20mm Draft @UltiMaker Factor 4 0.4 nozzle

**0.6mm Nozzle:**
- 0.30mm Standard @UltiMaker Factor 4 0.6 nozzle
- 0.30mm Draft @UltiMaker Factor 4 0.6 nozzle

**0.8mm Nozzle:**
- 0.40mm Standard @UltiMaker Factor 4 0.8 nozzle
- 0.40mm Draft @UltiMaker Factor 4 0.8 nozzle

## Implementation Plan

### Phase 1: Fix Existing Presets (Quick Fix)
Update `compatible_printers` in existing files to include all valid core types.

### Phase 2: Create New Process Presets
Create complete set of process presets with proper settings.

### Phase 3: Validate and Test
Test each preset with actual printer configurations.

## Settings Reference

Key settings to configure for UltiMaker printers:
- Layer heights appropriate for nozzle size
- Line widths (typically 100-120% of nozzle diameter)
- Speeds (UltiMaker S series can handle higher speeds)
- Accelerations (S6/S8 have higher limits than S3/S5/S7)
- Jerk settings
- Temperatures (based on material)
- Retraction settings (UltiMaker uses direct drive)
- Support settings (different for BB cores)

## Files to Create/Update

### Existing Files to Fix:
1. 0.15mm Standard @UltiMaker S3-S5-S7 0.25 nozzle.json
2. 0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle.json
3. 0.20mm Standard @UltiMaker S3-S5-S7 0.6 nozzle.json
4. 0.20mm Standard @UltiMaker S3-S5-S7 0.8 nozzle.json
5. 0.15mm Standard @UltiMaker S6-S8 0.25 nozzle.json
6. 0.20mm Standard @UltiMaker S6-S8 0.4 nozzle.json
7. 0.20mm Standard @UltiMaker S6-S8 0.6 nozzle.json
8. 0.20mm Standard @UltiMaker S6-S8 0.8 nozzle.json
9. 0.15mm Standard @UltiMaker Factor 4 0.25 nozzle.json
10. 0.20mm Standard @UltiMaker Factor 4 0.4 nozzle.json
11. 0.20mm Standard @UltiMaker Factor 4 0.6 nozzle.json
12. 0.20mm Standard @UltiMaker Factor 4 0.8 nozzle.json

### New Files to Create:
Approximately 20-30 new process preset files for different quality levels.

## Next Steps

1. Confirm Factor 4 core types (AA, BB, CC, AA+, CC+, HT?)
2. Get reference settings from Cura profiles or UltiMaker specs
3. Decide: Fix existing or delete and recreate?
4. Create process preset template
5. Generate all required presets
6. Test with actual printer configurations
