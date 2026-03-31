# MakerBotWriter Update Plan

## Overview

This document outlines the plan to update the MakerBotWriter to match the improvements made to the UFPWriter. Based on analysis of the current codebase, the MakerBotWriter already has most of the same structure and methods as UFPWriter, but needs several key updates to ensure parity and proper dynamic data handling.

## Current State Analysis

### UFPWriter vs MakerBotWriter Comparison

| Feature | UFPWriter | MakerBotWriter | Status |
|---------|-----------|----------------|--------|
| ExtruderData struct | `ExtruderData` with `material_guid`, `material_name`, `brand` | `MakerBotExtruderData` with `material_guid`, `material_name`, `material_code` | ✅ Similar, with MakerBot-specific `material_code` |
| Extruder support | 2 extruders (`m_extruders[2]`) | 1 extruder (`m_extruder`) | ✅ By design (MakerBot Sketch has 1 extruder) |
| `set_print_stats()` | ✅ Implemented | ✅ Implemented | ✅ Complete |
| `set_extruder_variants()` | ✅ Implemented | ✅ Implemented | ✅ Complete |
| `set_extruder_data()` | ✅ Implemented | ✅ Implemented | ✅ Complete |
| `has_extruder_data()` | ✅ Implemented | ✅ Implemented | ✅ Complete |
| `override_metadata()` | ✅ Sophisticated GUID propagation | ✅ Implemented with material GUID handling | ✅ Complete |
| `set_thumbnail_data()` | ✅ Inherited from GCodeContainerWriter | ✅ Inherited from GCodeContainerWriter | ✅ Complete |
| Data flow from FormatConfig | ✅ Extruder variants + data passed | ❌ Missing extruder variants | ⚠️ Needs update |
| Material code mapping | N/A (uses `brand` field) | ✅ `material_name_to_code()` helper | ✅ Complete |

### Key Issues Identified

1. **Missing extruder_variants in FormatConfig.cpp**: The UFPWriter section calls `writer.set_extruder_variants(extruder_variants)` but the MakerBotWriter section does not.
2. **Material code population**: Need to ensure `material_code` field is properly populated from material name.
3. **Nozzle variant handling**: MakerBotWriter needs to use nozzle variant data for metadata generation.
4. **Setup wizard completeness**: Missing MakerBot process profiles, bed model/texture files, and preview images.

## Implementation Plan

### Phase 1: Core MakerBotWriter Updates

1. **Update FormatConfig.cpp to pass extruder variants to MakerBotWriter**
   - Add `writer.set_extruder_variants(extruder_variants)` call in the MakerBot section
   - Ensure proper logging for debugging

2. **Verify material_code population**
   - Check that `material_name_to_code()` function correctly maps material names to MakerBot material codes
   - Ensure `material_code` is populated in `MakerBotExtruderData` when converting from `ExtruderData`

3. **Update MakerBotWriter to use nozzle variant data**
   - Review `generate_meta_json()` and `generate_slicemetadata_json()` methods to ensure they use nozzle variant information
   - Verify that nozzle diameter is correctly extracted from variant names

### Phase 2: Setup Wizard Completeness

1. **Create MakerBot process profiles**
   - Create print process profiles for MakerBot Sketch printers (0.15mm, 0.20mm Standard)
   - Ensure proper `compatible_printers` filtering by nozzle size

2. **Create missing MakerBot bed model/texture files**
   - Create or reference existing bed model files (`makerbot_build_plate_model.stl`)
   - Create or reference bed texture files (`makerbot_build_plate_texture.png`)

3. **Create MakerBot printer preview images**
   - Generate or obtain preview images for MakerBot Sketch printers
   - Place in appropriate resources directory

### Phase 3: Testing and Validation

1. **Test MakerBot export functionality**
   - Export a simple cube to `.makerbot` format
   - Verify container creation succeeds

2. **Test MakerBot export with dynamic data**
   - Test with material GUIDs, nozzle variants, and print statistics
   - Verify metadata in generated container matches expected values

3. **Validate MakerBot container format**
   - Compare generated `.makerbot` files with reference files from parent directory
   - Verify JSON structure, G-code headers, and file organization

4. **Test setup wizard for MakerBot printers**
   - Verify all MakerBot printers appear in setup wizard
   - Test printer addition, filament selection, and process profile selection

## Technical Details

### FormatConfig.cpp Update

Current MakerBot section (lines 371-389):
```cpp
} else if (format_type == "makerbot") {
    MakerBotWriter writer(format_config);
    // Pass thumbnail data directly (NOT extracted from gcode)
    if (!thumbnail_data.empty()) {
        writer.set_thumbnail_data(thumbnail_data);
        BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting thumbnail data for MakerBot export, size=" << thumbnail_data.size();
    }
    // Pass extruder data (GUIDs, temps, volumes)
    for (size_t i = 0; i < extruder_data.size() && i < 1; ++i) {
        MakerBotExtruderData mb_data;
        mb_data.material_guid = extruder_data[i].material_guid;
        mb_data.material_name = extruder_data[i].material_name;
        mb_data.extruder_temp = extruder_data[i].extruder_temp;
        mb_data.filament_mm = extruder_data[i].filament_mm;
        mb_data.filament_g = extruder_data[i].filament_g;
        writer.set_extruder_data(static_cast<int>(i), mb_data);
        BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting extruder data for MakerBot export - GUID: " << mb_data.material_guid;
    }
    success = writer.write(input_gcode_path, output_path);
```

**Required update**: Add extruder variants support:
```cpp
    // Pass extruder variants for nozzle diameter/name
    if (!extruder_variants.empty()) {
        writer.set_extruder_variants(extruder_variants);
        BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting " << extruder_variants.size() << " extruder variants for MakerBot export";
    }
```

### Material Code Population

The `material_name_to_code()` function in `MakerBotWriter.cpp` already handles material name to code conversion. Need to ensure it's called when populating `MakerBotExtruderData`:

```cpp
mb_data.material_code = material_name_to_code(extruder_data[i].material_name);
```

### MakerBotWriter Nozzle Variant Usage

Check `get_bot_and_tool_type()` method to ensure it correctly extracts nozzle information from variant names for metadata generation.

## Dependencies and Constraints

- **Zero external dependencies**: All implementation must be built into OrcaSlicer
- **Reference files only**: Use parent directory `/Cura files/` and `/postprocessors/` as reference only
- **Windows 11 development**: No Linux tools available, use agent tools for file operations
- **FORMAT_CONFIG_ID system**: All MakerBot printers must have correct FORMAT_CONFIG_ID in printer notes

## Success Criteria

1. ✅ MakerBot export works with dynamic material GUIDs and nozzle variants
2. ✅ Generated `.makerbot` files match reference file structure
3. ✅ All MakerBot printers appear in setup wizard
4. ✅ Process profiles available for all nozzle sizes
5. ✅ Bed model and texture files present
6. ✅ Print statistics correctly injected into metadata

## Risk Assessment

- **Low risk**: Most infrastructure already exists, updates are incremental
- **Medium risk**: MakerBot format validation requires comparison with reference files
- **Low risk**: Setup wizard issues already partially diagnosed and addressed

## Next Steps

1. Review this plan with stakeholders
2. Begin implementation with Phase 1 updates
3. Test incrementally after each change
4. Update AGENTS_JATMN.md with progress and findings

---

*Last Updated: 2026-03-31*
*Author: Kilo Code (Architect Mode)*