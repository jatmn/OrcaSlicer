# Comprehensive Plan: Process Preset Selection + Profile Compatibility

## Overview

This plan covers TWO workstreams:
1. **Code Implementation:** Implement `update_process_presets()` to automatically select compatible process presets
2. **Profile Updates:** Update existing process presets to properly support all core types (AA, BB, CC, AA+, CC+)

## Workstream 1: Code Implementation

### Current State
The `update_process_presets()` function in `ExtruderVariantWidget.cpp` is a placeholder stub.

### Implementation

Replace the placeholder with this implementation that uses `compatible_printers`:

```cpp
// Update process presets based on selected core type and nozzle size
// Uses compatible_printers field in process presets to find matching profiles
void ExtruderVariantWidget::update_process_presets(const wxString& core_type)
{
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets ENTER - core_type=" << core_type;
    
    // Get the controlling core index
    int controlling_idx = get_controlling_core_index();
    
    // Validate controlling index
    if (controlling_idx < 0 || controlling_idx >= (int)m_extruder_variants.size()) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets EXIT - invalid controlling_idx";
        return;
    }
    
    wxChoice* combo = m_extruder_variants[controlling_idx];
    if (!combo) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets EXIT - null combo";
        return;
    }
    
    int sel = combo->GetSelection();
    if (sel == wxNOT_FOUND) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets EXIT - no selection";
        return;
    }
    
    wxString selected_core = combo->GetString(sel);
    wxString core_type_str = extract_type_from_core(selected_core);
    wxString nozzle_size = extract_size_from_core(selected_core);
    
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - selected_core=" << selected_core 
                               << " core_type=" << core_type_str << " nozzle_size=" << nozzle_size;
    
    if (nozzle_size.IsEmpty()) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets EXIT - empty nozzle size";
        return;
    }
    
    // Get preset bundle
    PresetBundle* preset_bundle = wxGetApp().preset_bundle;
    if (!preset_bundle) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets EXIT - no preset bundle";
        return;
    }
    
    // Get current printer preset name (e.g., "UltiMaker S6 AA 0.4")
    const std::string& printer_preset_name = preset_bundle->printers.get_edited_preset().name;
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - printer_preset=" << printer_preset_name;
    
    // Get all print presets
    const PresetCollection& prints = preset_bundle->prints;
    std::vector<const Preset*> compatible_presets;
    
    for (const Preset& preset : prints.get_presets()) {
        if (!preset.is_compatible || preset.is_default || !preset.is_visible) {
            continue;
        }
        
        // Check if current printer preset is in compatible_printers list
        const ConfigOptionStrings* compatible_printers = preset.config.option<ConfigOptionStrings>("compatible_printers");
        if (!compatible_printers || compatible_printers->values.empty()) {
            continue;
        }
        
        bool is_compatible = false;
        for (const std::string& compatible : compatible_printers->values) {
            if (compatible == printer_preset_name) {
                is_compatible = true;
                break;
            }
        }
        
        if (is_compatible) {
            compatible_presets.push_back(&preset);
            BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - compatible preset: " << preset.name;
        }
    }
    
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - found " << compatible_presets.size() << " compatible presets";
    
    if (compatible_presets.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - no compatible presets found";
        return;
    }
    
    // Get current process preset name
    std::string current_process = prints.get_selected_preset_name();
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - current_process=" << current_process;
    
    // Check if current preset is already compatible
    bool current_is_compatible = false;
    for (const Preset* preset : compatible_presets) {
        if (preset->name == current_process) {
            current_is_compatible = true;
            break;
        }
    }
    
    // If current preset is already compatible, don't change it
    if (current_is_compatible) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - current preset is compatible, no change needed";
        return;
    }
    
    // Select the best compatible preset
    // Priority: try to find a "Standard" preset first
    const Preset* best_preset = nullptr;
    
    for (const Preset* preset : compatible_presets) {
        if (preset->name.find("Standard") != std::string::npos) {
            best_preset = preset;
            break;
        }
    }
    
    // If no Standard preset found, use the first compatible one
    if (!best_preset && !compatible_presets.empty()) {
        best_preset = compatible_presets[0];
    }
    
    if (best_preset) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - selecting preset: " << best_preset->name;
        
        // Select the preset via Tab interface
        Tab* print_tab = wxGetApp().get_tab(Preset::TYPE_PRINT);
        if (print_tab) {
            print_tab->select_preset(best_preset->name);
            BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - preset selected successfully";
        } else {
            // Fallback: use preset bundle directly
            preset_bundle->prints.select_preset_by_name(best_preset->name, true);
            BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - preset selected via bundle";
        }
        
        // Update the sidebar to reflect the change
        wxGetApp().plater()->sidebar().update_presets(Preset::TYPE_PRINT);
    }
    
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets EXIT";
}
```

## Workstream 2: Process Preset Compatibility Updates

### Current Process Presets

Based on the files in `resources/profiles/UltiMaker/process/`:

**For S3/S5/S7:**
- `0.15mm Standard @UltiMaker S3-S5-S7 0.25 nozzle.json`
- `0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle.json`
- `0.20mm Standard @UltiMaker S3-S5-S7 0.6 nozzle.json`
- `0.20mm Standard @UltiMaker S3-S5-S7 0.8 nozzle.json`

**For S6/S8:**
- `0.15mm Standard @UltiMaker S6-S8 0.25 nozzle.json`
- `0.20mm Standard @UltiMaker S6-S8 0.4 nozzle.json`
- `0.20mm Standard @UltiMaker S6-S8 0.6 nozzle.json`
- `0.20mm Standard @UltiMaker S6-S8 0.8 nozzle.json`

**For Factor 4:**
- `0.15mm Standard @UltiMaker Factor 4 0.25 nozzle.json`
- `0.20mm Standard @UltiMaker Factor 4 0.4 nozzle.json`
- `0.20mm Standard @UltiMaker Factor 4 0.6 nozzle.json`
- `0.20mm Standard @UltiMaker Factor 4 0.8 nozzle.json`

### Required compatible_printers Updates

Each process preset needs its `compatible_printers` list expanded to include ALL core types for that nozzle size.

**Example: 0.20mm Standard @UltiMaker S6-S8 0.4 nozzle**

Current compatible_printers (example):
```json
"compatible_printers": [
    "UltiMaker S6 AA 0.4",
    "UltiMaker S6 BB 0.4",
    "UltiMaker S6 CC 0.4",
    "UltiMaker S6 AA+ 0.4",
    "UltiMaker S6 CC+ 0.4",
    "UltiMaker S6 0.4 nozzle",
    "UltiMaker S8 AA 0.4",
    "UltiMaker S8 BB 0.4",
    "UltiMaker S8 CC 0.4",
    "UltiMaker S8 AA+ 0.4",
    "UltiMaker S8 CC+ 0.4",
    "UltiMaker S8 0.4 nozzle"
]
```

This already looks complete for S6/S8 0.4mm! Let me check what other nozzle sizes need.

### Full Compatibility Matrix

**S3/S5/S7 0.25mm nozzle:**
- UltiMaker S3 AA 0.25
- UltiMaker S3 BB 0.25
- UltiMaker S5 AA 0.25
- UltiMaker S5 BB 0.25
- UltiMaker S7 AA 0.25
- UltiMaker S7 BB 0.25

**S3/S5/S7 0.4mm nozzle:**
- UltiMaker S3 AA 0.4
- UltiMaker S3 BB 0.4
- UltiMaker S3 CC 0.4
- UltiMaker S3 AA+ 0.4
- UltiMaker S3 CC+ 0.4
- UltiMaker S5 AA 0.4
- UltiMaker S5 BB 0.4
- UltiMaker S5 CC 0.4
- UltiMaker S5 AA+ 0.4
- UltiMaker S5 CC+ 0.4
- UltiMaker S7 AA 0.4
- UltiMaker S7 BB 0.4
- UltiMaker S7 CC 0.4
- UltiMaker S7 AA+ 0.4
- UltiMaker S7 CC+ 0.4

**S3/S5/S7 0.6mm nozzle:**
- UltiMaker S3 AA 0.6
- UltiMaker S3 CC 0.6
- UltiMaker S3 CC+ 0.6
- UltiMaker S5 AA 0.6
- UltiMaker S5 CC 0.6
- UltiMaker S5 CC+ 0.6
- UltiMaker S7 AA 0.6
- UltiMaker S7 CC 0.6
- UltiMaker S7 CC+ 0.6

**S3/S5/S7 0.8mm nozzle:**
- UltiMaker S3 AA 0.8
- UltiMaker S3 BB 0.8
- UltiMaker S5 AA 0.8
- UltiMaker S5 BB 0.8
- UltiMaker S7 AA 0.8
- UltiMaker S7 BB 0.8

**S6/S8 0.25mm nozzle:**
- UltiMaker S6 AA 0.25
- UltiMaker S6 BB 0.25
- UltiMaker S8 AA 0.25
- UltiMaker S8 BB 0.25

**S6/S8 0.4mm nozzle:**
- UltiMaker S6 AA 0.4
- UltiMaker S6 BB 0.4
- UltiMaker S6 CC 0.4
- UltiMaker S6 AA+ 0.4
- UltiMaker S6 CC+ 0.4
- UltiMaker S8 AA 0.4
- UltiMaker S8 BB 0.4
- UltiMaker S8 CC 0.4
- UltiMaker S8 AA+ 0.4
- UltiMaker S8 CC+ 0.4

**S6/S8 0.6mm nozzle:**
- UltiMaker S6 AA 0.6
- UltiMaker S6 CC 0.6
- UltiMaker S6 CC+ 0.6
- UltiMaker S8 AA 0.6
- UltiMaker S8 CC 0.6
- UltiMaker S8 CC+ 0.6

**S6/S8 0.8mm nozzle:**
- UltiMaker S6 AA 0.8
- UltiMaker S6 BB 0.8
- UltiMaker S8 AA 0.8
- UltiMaker S8 BB 0.8

**Factor 4 0.25mm nozzle:**
- UltiMaker Factor 4 AA 0.25
- UltiMaker Factor 4 BB 0.25

**Factor 4 0.4mm nozzle:**
- UltiMaker Factor 4 AA 0.4
- UltiMaker Factor 4 BB 0.4
- UltiMaker Factor 4 CC 0.4
- UltiMaker Factor 4 AA+ 0.4
- UltiMaker Factor 4 CC+ 0.4

**Factor 4 0.6mm nozzle:**
- UltiMaker Factor 4 AA 0.6
- UltiMaker Factor 4 CC 0.6
- UltiMaker Factor 4 CC+ 0.6

**Factor 4 0.8mm nozzle:**
- UltiMaker Factor 4 AA 0.8
- UltiMaker Factor 4 BB 0.8

### Files to Update

All files in `resources/profiles/UltiMaker/process/` need their `compatible_printers` lists updated:

1. `0.15mm Standard @UltiMaker Factor 4 0.25 nozzle.json`
2. `0.15mm Standard @UltiMaker S3-S5-S7 0.25 nozzle.json`
3. `0.15mm Standard @UltiMaker S6-S8 0.25 nozzle.json`
4. `0.20mm Standard @UltiMaker Factor 4 0.4 nozzle.json`
5. `0.20mm Standard @UltiMaker Factor 4 0.6 nozzle.json`
6. `0.20mm Standard @UltiMaker Factor 4 0.8 nozzle.json`
7. `0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle.json`
8. `0.20mm Standard @UltiMaker S3-S5-S7 0.6 nozzle.json`
9. `0.20mm Standard @UltiMaker S3-S5-S7 0.8 nozzle.json`
10. `0.20mm Standard @UltiMaker S6-S8 0.4 nozzle.json`
11. `0.20mm Standard @UltiMaker S6-S8 0.6 nozzle.json`
12. `0.20mm Standard @UltiMaker S6-S8 0.8 nozzle.json`

## Implementation Order

### Phase 1: Code Implementation
1. Update `update_process_presets()` in `ExtruderVariantWidget.cpp`
2. Build and test

### Phase 2: Profile Updates
1. Read each process preset file
2. Update `compatible_printers` list with all applicable core types
3. Save updated files
4. Rebuild to test

### Phase 3: Testing
1. Test each printer model (S3, S5, S6, S7, S8, Factor 4)
2. Test each nozzle size (0.25, 0.4, 0.6, 0.8)
3. Test each core type (AA, BB, CC, AA+, CC+)
4. Verify process preset auto-selection works correctly

## Example Updated Process Preset

**File:** `0.20mm Standard @UltiMaker S6-S8 0.4 nozzle.json`

```json
{
    "type": "process",
    "name": "0.20mm Standard @UltiMaker S6-S8 0.4 nozzle",
    "inherits": "fdm_process_common",
    "from": "system",
    "instantiation": "true",
    "print_settings_id": "0.20mm Standard @UltiMaker S6-S8 0.4 nozzle",
    "compatible_printers": [
        "UltiMaker S6 AA 0.4",
        "UltiMaker S6 BB 0.4",
        "UltiMaker S6 CC 0.4",
        "UltiMaker S6 AA+ 0.4",
        "UltiMaker S6 CC+ 0.4",
        "UltiMaker S6 0.4 nozzle",
        "UltiMaker S8 AA 0.4",
        "UltiMaker S8 BB 0.4",
        "UltiMaker S8 CC 0.4",
        "UltiMaker S8 AA+ 0.4",
        "UltiMaker S8 CC+ 0.4",
        "UltiMaker S8 0.4 nozzle"
    ],
    "compatible_printers_condition": "",
    "inherits": "fdm_process_common",
    ... rest of settings ...
}
```

## Build Instructions

After making changes:

```bash
# Build the application
cmake --build build --target OrcaSlicer --config Release

# Copy updated profiles to system directory for testing
$source = "resources/profiles/UltiMaker/process/"
$dest = "C:\Users\$env:USERNAME\AppData\Roaming\OrcaSlicer\system\UltiMaker\process\"
Copy-Item -Path "$source\*" -Destination $dest -Recurse -Force
```

## Rollback Plan

1. **Code Rollback:** Revert `ExtruderVariantWidget.cpp` to placeholder implementation
2. **Profile Rollback:** Restore original process preset files from git

## Future Enhancements

1. **Core-Specific Profiles:** Create specialized profiles for BB (support) and CC (abrasive) cores with different settings
2. **Material-Specific Profiles:** Link process presets to specific material types
3. **User Custom Profiles:** Allow users to create custom process presets for specific core combinations
