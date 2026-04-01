# UltiMaker Print Core Selection UI Fix - Implementation Plan

## Problem Summary

The UltiMaker print core selection UI has several incomplete features:

1. **Print core selection doesn't affect available print processes** - Changing print core type (AA vs BB vs AA+) should filter available print process presets, but currently doesn't

2. **Printer-specific process selection control**:
   - S Series (S3, S5, S6, S7, S8): Process selection should be controlled by Print Core 1
   - Factor 4: Process selection should be controlled by Print Core 2

3. **Core size mismatch validation missing** - Users can currently select incompatible core sizes (e.g., AA 0.4 and AA 0.8) which should not be allowed. Need a dialog similar to Bambu H2D's mismatch handling.

## Reference Implementation

### Bambu H2D Mismatch Dialog (from Plater.cpp:1221)
```cpp
MessageDialog dlg(this->plater,
                  _L("The software does not support using different diameter of nozzles for one print. "
                     "If the left and right nozzles are inconsistent, we can only proceed with single-head printing. "
                     "Please confirm which nozzle you would like to use for this project."),
                  _L("Switch diameter"), wxYES_NO | wxNO_DEFAULT);
dlg.SetButtonLabel(wxID_YES, wxString::Format(_L("Left nozzle: %smm"), diameter_left));
dlg.SetButtonLabel(wxID_NO, wxString::Format(_L("Right nozzle: %smm"), diameter_right));
```

## Implementation Plan

### Phase 1: Core Size Mismatch Validation

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`

1. Add method `check_core_size_mismatch()`:
   - Parse nozzle sizes from both print core selections
   - Compare sizes (e.g., "0.4" vs "0.8")
   - Return mismatch status and sizes

2. Add method `show_mismatch_dialog(const wxString& size1, const wxString& size2)`:
   - Create MessageDialog similar to H2D pattern
   - Ask user to select which core size to use
   - Update both cores to selected size (keeping core types)

3. Call validation in `on_variant_changed()`:
   - After updating config, check for mismatch
   - If mismatch detected, show dialog
   - Update selections based on user choice

### Phase 2: Print Core -> Process Selection Linkage

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`

1. Add method `get_process_controlling_core_idx()`:
   - Detect printer type (S series vs Factor 4)
   - Return 0 for S series (core 1 controls)
   - Return 1 for Factor 4 (core 2 controls)

2. Add method `update_compatible_processes()`:
   - Get the controlling core's variant
   - Parse core type (AA, BB, CC, etc.)
   - Filter print process presets by `compatible_printers` 
   - Trigger process preset list refresh

3. Connect to process selection:
   - Call `update_compatible_processes()` in `on_variant_changed()`
   - Update sidebar process preset list

### Phase 3: Process Preset Filtering

**Files**: 
- `src/slic3r/GUI/ExtruderVariantWidget.cpp`
- `src/slic3r/GUI/Plater.cpp` (sidebar integration)

1. Add method `filter_processes_by_core_type(const std::string& core_type)`:
   - Access preset bundle's print presets
   - Check `compatible_printers` for each process
   - Filter based on core type compatibility

2. Update sidebar:
   - Add `update_process_presets()` method to Sidebar
   - Call from ExtruderVariantWidget when core changes
   - Refresh process preset dropdown

### Phase 4: Testing

1. Test S Series (S3, S5, S6, S7, S8):
   - Verify core 1 controls process selection
   - Test core type filtering (AA vs BB vs CC)
   - Test mismatch dialog with different sizes

2. Test Factor 4:
   - Verify core 2 controls process selection
   - Test core type filtering
   - Test mismatch dialog

3. Test edge cases:
   - Single extruder mode
   - Both cores same type/size
   - Switching between printer types

## Key Code Locations

### ExtruderVariantWidget.cpp
- Lines 195-233: `on_variant_changed()` - main handler
- Lines 47-193: `update_from_config()` - UI creation
- Lines 235-260: `printer_has_variants()` - printer detection

### Plater.cpp
- Lines 1212-1240: `switch_diameter()` - reference dialog pattern
- Lines 3967-3983: `update_extruder_variant_widget()` - sidebar integration

### Tab.cpp
- Lines 5770-5972: Process preset selection logic
- Lines 6413-6416: `compatible_printers` handling

## Implementation Notes

1. **Core Type Parsing**: Variants stored as "AA 0.4", "BB 0.8", etc. Use `parse_variant()` helper.

2. **Process Compatibility**: Process presets have `compatible_printers` option that lists compatible printer/core combinations.

3. **UI Updates**: Use `wxTheApp->CallAfter()` for UI updates to avoid threading issues.

4. **Preset Bundle**: Access via `wxGetApp().preset_bundle` for all preset operations.

5. **Nozzle Diameter**: Stored in `nozzle_diameter` config option as float values.

## Success Criteria

- [ ] Core size mismatch dialog appears when selecting different sizes
- [ ] Dialog allows user to choose which size to use
- [ ] Both cores update to selected size (types preserved)
- [ ] Process list filters based on controlling core type
- [ ] S series uses core 1 for process control
- [ ] Factor 4 uses core 2 for process control
- [ ] No crashes or UI glitches when switching printers
