# MakerBotWriter Update Plan - Detailed Implementation Guide

## Overview

This document provides a comprehensive plan for updating the MakerBotWriter to match the improvements made to the UFPWriter. The goal is to make MakerBotWriter dynamic and configurable, removing hardcoded values and enabling proper material GUID matching, print statistics injection, and format-specific metadata generation.

## Current Status Analysis

### MakerBot Printer Profiles

**Legacy Files (to be deleted):**
- `resources/profiles/UltiMaker/machine/MakerBot Sketch.json` (type: "machine_model", no build volume)
- `resources/profiles/UltiMaker/machine/MakerBot Sketch Large.json` (type: "machine_model", no build volume)
- `resources/profiles/UltiMaker/machine/MakerBot Sketch Sprint.json` (type: "machine_model", no build volume)

**Current Nozzle-Specific Profiles (to be updated):**
- `MakerBot Sketch 0.40.json` - machine_area: 150x150x150
- `MakerBot Sketch Large 0.40.json` - machine_area: 220x200x250  
- `MakerBot Sketch Sprint 0.40.json` - machine_area: 220x220x220

**Missing:** FORMAT_CONFIG_ID in printer_notes field

### MakerBot Materials

**Filament Diameter Status:**
- ✅ All MakerBot materials are correctly set to 1.75mm filament diameter
- ❌ No MakerBot materials are incorrectly set to 2.85mm

**MATERIAL_GUID Status (Updated with correct values):**
- ✅ MakerBot Sketch Tough PLA: `de031137-a8ca-4a72-bd1b-17bb964033ad`
- ✅ MakerBot Sketch PLA: `abb9c58e-1f56-48d1-bd8f-055fde3a5b56`
- ✅ MakerBot Sketch Metallic PLA: `3fac1543-dd0c-462d-9cbc-d94137d43999`
- ✅ MakerBot Method Tough PLA: `de031137-a8ca-4a72-bd1b-17bb964033ad`

**MakerBot Material Codes (for JSON "material" field):**
- MakerBot Sketch Tough PLA: `im-pla`
- MakerBot Sketch PLA: `pla`
- MakerBot Sketch Metallic PLA: `metallic-pla`
- MakerBot Method Tough PLA: `im-pla`

### Code Analysis

**UFPWriter vs MakerBotWriter Differences:**
1. **UFPWriter** has:
   - `ExtruderData` struct for per-extruder metadata
   - `set_print_stats()`, `set_extruder_variants()`, `set_extruder_data()` methods
   - `override_metadata()` method for GUID prioritization
   - Dynamic data injection from `FormatConfig::export_to_container()`

2. **MakerBotWriter** lacks:
   - All the above methods and data structures
   - Uses hardcoded config values for "bot_type", "tool_type", "tool_types"
   - Static metadata generation without user selections

**FormatConfig.cpp Analysis:**
- Lines 340-380: UFPWriter receives extruder variants/data but MakerBotWriter does not
- MakerBotWriter instantiated without data injection

## Implementation Plan

### Phase 1: Profile Cleanup & Configuration

1. **Delete legacy MakerBot profile files**
   - Remove the three legacy machine_model files:
     - `resources/profiles/UltiMaker/machine/MakerBot Sketch.json`
     - `resources/profiles/UltiMaker/machine/MakerBot Sketch Large.json`
     - `resources/profiles/UltiMaker/machine/MakerBot Sketch Sprint.json`
   - These cause UI issues with incorrect/no build volumes

2. **Remove legacy entries from UltiMaker.json**
   - Delete lines 40-51 referencing the legacy machine_model files
   - Keep only the nozzle-specific profiles

3. **Add FORMAT_CONFIG_ID to nozzle-specific profiles**
   - Add `"printer_notes": "FORMAT_CONFIG_ID:sketch_small"` to MakerBot Sketch 0.40.json
   - Add `"printer_notes": "FORMAT_CONFIG_ID:sketch_large"` to MakerBot Sketch Large 0.40.json
   - Add `"printer_notes": "FORMAT_CONFIG_ID:sketch_sprint"` to MakerBot Sketch Sprint 0.40.json

4. **Update MakerBot material GUIDs and add MATERIAL_CODE**
   - Add correct GUIDs to all MakerBot materials:
     - MakerBot Sketch Tough PLA: `de031137-a8ca-4a72-bd1b-17bb964033ad`
     - MakerBot Sketch PLA: `abb9c58e-1f56-48d1-bd8f-055fde3a5b56`
     - MakerBot Sketch Metallic PLA: `3fac1543-dd0c-462d-9cbc-d94137d43999`
     - MakerBot Method Tough PLA: `de031137-a8ca-4a72-bd1b-17bb964033ad`
   - Add MATERIAL_CODE to filament_notes for JSON "material" field:
     - MakerBot Sketch Tough PLA: `im-pla`
     - MakerBot Sketch PLA: `pla`
     - MakerBot Sketch Metallic PLA: `metallic-pla`
     - MakerBot Method Tough PLA: `im-pla`
   - Format: `"filament_notes": "MATERIAL_GUID:abb9c58e-1f56-48d1-bd8f-055fde3a5b56\nMATERIAL_CODE:pla"`

### Phase 2: MakerBotWriter Header Updates

5. **Update MakerBotWriter.hpp**
   - Add `ExtruderData` struct (copy from UFPWriter or create shared)
   - Add methods:
     ```cpp
     void set_print_stats(const PrintStatistics& stats);
     void set_extruder_variants(const std::vector<std::string>& variants);
     void set_extruder_data(const std::vector<ExtruderData>& data);
     ```
   - Add `override_metadata()` method for GUID prioritization
   - Add private member variables to store injected data

6. **Implement ExtruderData struct**
   - Same structure as UFPWriter:
     ```cpp
     struct ExtruderData {
         std::string guid;
         std::string material;
         std::string color;
         float temperature;
         float bed_temperature;
         float filament_used;
     };
     ```

### Phase 3: MakerBotWriter Implementation Updates

7. **Update generate_header() method**
   - Use injected print statistics (print time, filament used, etc.)
   - Use nozzle variants for header comments
   - Remove hardcoded values

8. **Update generate_meta_json() method**
   - **Critical Change**: Infer "bot_type", "tool_type", "tool_types" from FORMAT_CONFIG_ID
   - Mapping logic (Sketch printers are single extruder only):
     - `sketch_small` → bot_type: "sketch", tool_type: "single", tool_types: ["single"]
     - `sketch_large` → bot_type: "sketch_large", tool_type: "single", tool_types: ["single"]
     - `sketch_sprint` → bot_type: "sketch_sprint", tool_type: "single", tool_types: ["single"]
   - Use dynamic extruder data for material information
   - **Single extruder only**: MakerBot Sketch printers do not support multi-material
   - Read MATERIAL_CODE from filament_notes for JSON "material" field

9. **Update generate_slicemetadata_json() method**
   - Use dynamic extruder data
   - Include GUIDs when available
   - Read MATERIAL_CODE from filament_notes
   - Fallback to material names when GUIDs missing

10. **Implement override_metadata() method**
    - Prioritize GUID-matched materials (same as UFPWriter)
    - Fallback to filament_type matching
    - Ensure MakerBot materials are correctly identified
    - Handle MATERIAL_CODE extraction from filament_notes

### Phase 4: FormatConfig Integration

11. **Update FormatConfig.cpp**
    - Lines 340-380: Pass data to MakerBotWriter similar to UFPWriter
    - Call `set_print_stats()`, `set_extruder_variants()`, `set_extruder_data()`
    - Ensure MakerBotWriter receives the same data injection

### Phase 5: UltiMaker Digital Factory Upload Review

12. **Review UltiMaker Digital Factory host connection method**
    - Check `src/slic3r/Utils/UltiMaker.cpp` `get_mime_type_for_upload()` function
    - Verify .makerbot files are uploaded with correct MIME type
    - Ensure FORMAT_CONFIG_ID mapping to MIME types works for MakerBot formats
    - Confirm upload flow handles .makerbot container format correctly

### Phase 6: Testing & Validation

13. **Test with updated MakerBot printer profiles**
    - Verify FORMAT_CONFIG_ID detection works
    - Test export to .makerbot format
    - Check generated file structure
    - Verify single extruder only configuration

14. **Verify generated .makerbot files against reference files**
    - Compare with reference files in parent `/Cura files/` directory
    - Validate JSON metadata structure
    - Check G-code headers/footers
    - Verify MATERIAL_CODE appears correctly in JSON

## Technical Details

### FORMAT_CONFIG_ID to bot_type/tool_type Mapping

| FORMAT_CONFIG_ID | bot_type | tool_type | tool_types | Notes |
|------------------|----------|-----------|------------|-------|
| sketch_small | "sketch" | "single" | ["single"] | Confirmed from reference files |
| sketch_large | "sketch_large" | "single" | ["single"] | **Placeholder** - needs verification |
| sketch_sprint | "sketch_sprint" | "single" | ["single"] | Confirmed from reference files |
| method_x | "method" | "dual" | ["left", "right"] | For future implementation |
| method_xl | "method_xl" | "dual" | ["left", "right"] | For future implementation |

### Material GUID Handling

The two-pass filament search in `BackgroundSlicingProcess.cpp` will:
1. First pass: Find presets with matching `MATERIAL_GUID`
2. Second pass: Fallback to `filament_type` matching

**MakerBot Material GUIDs (now corrected):**
- MakerBot Sketch Tough PLA: `de031137-a8ca-4a72-bd1b-17bb964033ad`
- MakerBot Sketch PLA: `abb9c58e-1f56-48d1-bd8f-055fde3a5b56`
- MakerBot Sketch Metallic PLA: `3fac1543-dd0c-462d-9cbc-d94137d43999`
- MakerBot Method Tough PLA: `de031137-a8ca-4a72-bd1b-17bb964033ad`

**MakerBot Material Codes (for JSON "material" field):**
- MakerBot Sketch Tough PLA: `im-pla`
- MakerBot Sketch PLA: `pla`
- MakerBot Sketch Metallic PLA: `metallic-pla`
- MakerBot Method Tough PLA: `im-pla`

MakerBotWriter must include both GUIDs and material codes in the generated JSON metadata.

### Multi-Extruder Support

MakerBot format JSON structure supports arrays for multi-extruder:
```json
"tool_types": ["left", "right"],
"extruder_temperature": [210, 210],
"material": ["MakerBot PLA", "MakerBot PLA"],
"material_guid": ["guid1", "guid2"]
```

## Files to Modify

### Profile Files
1. `resources/profiles/UltiMaker/machine/MakerBot Sketch 0.40.json`
2. `resources/profiles/UltiMaker/machine/MakerBot Sketch Large 0.40.json`
3. `resources/profiles/UltiMaker/machine/MakerBot Sketch Sprint 0.40.json`
4. `resources/profiles/UltiMaker.json` (remove legacy entries)

### Source Code Files
1. `src/libslic3r/Format/MakerBotWriter.hpp`
2. `src/libslic3r/Format/MakerBotWriter.cpp`
3. `src/libslic3r/Format/FormatConfig.cpp` (lines 340-380)
4. `src/libslic3r/Format/UFPWriter.hpp` (reference for ExtruderData)

### Material Files (Potential Updates)
1. `resources/profiles/OrcaFilamentLibrary/filament/MakerBot/MakerBot Sketch PLA @base.json` (add GUID)
2. Other MakerBot material files (add GUIDs if missing)

## Reference Files

### Valid Reference Files
- Parent directory: `/Cura files/MakerBot_Sketch_Small_-_Cube.makerbot`
- Parent directory: `/Cura files/MakerBot_Sketch_Sprint_-_Cube.makerbot`
- Parent directory: `/Valid postprocessed ufp makerbot files/`

### Configuration Files
- `resources/formats/makerbot/sketch_small.json`
- `resources/formats/makerbot/sketch_large.json`
- `resources/formats/makerbot/sketch_sprint.json`

## Success Criteria

1. ✅ MakerBot printer profiles work correctly in Setup Wizard
2. ✅ .makerbot export works with FORMAT_CONFIG_ID detection
3. ✅ Generated files match reference file structure
4. ✅ Material GUIDs are correctly used when available
5. ✅ Print statistics are included in generated files
6. ✅ "bot_type", "tool_type", "tool_types" are correctly inferred from FORMAT_CONFIG_ID
7. ✅ Multi-extruder support works (for Method printers)
8. ✅ No hardcoded values in MakerBotWriter

## Open Questions (Resolved)

1. **MakerBot Sketch Large bot_type**: ✅ Resolved - Use "sketch_large" as bot_type (consistent with FORMAT_CONFIG_ID naming)
2. **MakerBot material GUIDs**: ✅ Resolved - GUIDs provided by user and added to plan
3. **MakerBot material codes**: ✅ Resolved - MATERIAL_CODE values provided for JSON "material" field
4. **Single extruder only**: ✅ Resolved - MakerBot Sketch printers are single extruder only, no multi-material support
5. **Method printer support**: ⚠️ Not in scope - Focus is on Sketch printers only
6. **UltiMaker Digital Factory upload**: ✅ Resolved - Already handles .makerbot files correctly with MIME type mapping
7. **Filament diameter**: ✅ Resolved - All MakerBot materials are correctly 1.75mm (not 2.85mm)

## Next Steps

1. Get approval on this implementation plan
2. Begin with Phase 1 (profile cleanup)
3. Proceed through phases sequentially
4. Test after each major change
5. Final validation against reference files

---

*Last Updated: 2026-03-30*
*Author: Architect Mode Analysis*
*Project: OrcaSlicer MakerBotWriter Update*