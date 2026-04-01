# UltiMaker Print Core Selection UI Fix - Detailed Implementation Plan

## Table of Contents
1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Architecture Understanding](#architecture-understanding)
4. [Phase 1: Font Styling Fixes](#phase-1-font-styling-fixes)
5. [Phase 2: Core Size Mismatch Validation](#phase-2-core-size-mismatch-validation)
6. [Phase 3: Print Core -> Process Selection Linkage](#phase-3-print-core---process-selection-linkage)
7. [Phase 4: Printer-Specific Control Logic](#phase-4-printer-specific-control-logic)
8. [Testing Guide](#testing-guide)
9. [Common Pitfalls](#common-pitfalls)
10. [Reference Code](#reference-code)

---

## Overview

### Problem Statement
The UltiMaker print core selection UI has several incomplete features that need to be fixed:

1. **Font Styling Issues**: The "Print Core Configuration" title and "Print Core 1/2" labels don't match the visual style of other sidebar elements
2. **Core Size Mismatch**: Users can select incompatible core sizes (e.g., AA 0.4 and AA 0.8) without any warning
3. **No Process Selection Linkage**: Changing print core type doesn't filter available print process presets
4. **Missing Printer-Specific Logic**: S Series and Factor 4 printers should use different print cores for process selection

### Files You'll Modify
- `src/slic3r/GUI/ExtruderVariantWidget.cpp` - Main widget implementation
- `src/slic3r/GUI/ExtruderVariantWidget.hpp` - Widget header
- `src/slic3r/GUI/Plater.cpp` - Sidebar integration (reference only for dialog pattern)

### Key Concepts
- **Print Core Format**: UltiMaker uses combined format like "AA 0.4" (type + size)
- **printer_extruder_variant**: Config option storing core selections as strings
- **compatible_printers**: Process preset filtering mechanism
- **OptionsGroup**: Sidebar UI component system

---

## Prerequisites

### Required Knowledge
- Basic C++ and wxWidgets
- Understanding of OrcaSlicer's preset system
- Familiarity with config options and DynamicPrintConfig

### Tools Needed
- Visual Studio or VSCode with C++ extension
- OrcaSlicer build environment set up
- Git for version control

### Before You Start
1. Build OrcaSlicer in Debug mode
2. Verify you can run the application
3. Have an UltiMaker S6 or Factor 4 printer preset configured

---

## Architecture Understanding

### Current Widget Structure

```
ExtruderVariantWidget (wxPanel)
├── Title: "Print Core Configuration" (Label with Body_10 font)
├── Row for each extruder:
│   ├── Label: "Print Core N" (Label with Body_10 font)
│   └── ComboBox: ["AA 0.4", "AA 0.6", "BB 0.4", ...]
└── Updates config: printer_extruder_variant
```

### How Print Cores Work

1. **Storage**: Print cores are stored in `printer_extruder_variant` config option as strings like "AA 0.4"
2. **Format**: The combined format is "TYPE SIZE" (e.g., "AA 0.4", "BB 0.8")
3. **Extraction**: Type = first part (AA, BB, CC), Size = second part (0.4, 0.6, 0.8)
4. **Process Filtering**: Process presets have `compatible_printers` that must match the selected core

### Reference: Bambu H2D Dialog Pattern

Location: `src/slic3r/GUI/Plater.cpp` around line 1221

```cpp
// This is the pattern to copy for our mismatch dialog
MessageDialog dlg(this->plater,
                  _L("The software does not support using different diameter of nozzles for one print. "
                     "If the left and right nozzles are inconsistent, we can only proceed with single-head printing. "
                     "Please confirm which nozzle you would like to use for this project."),
                  _L("Switch diameter"), wxYES_NO | wxNO_DEFAULT);
dlg.SetButtonLabel(wxID_YES, wxString::Format(_L("Left nozzle: %smm"), diameter_left));
dlg.SetButtonLabel(wxID_NO, wxString::Format(_L("Right nozzle: %smm"), diameter_right));

if (dlg.ShowModal() == wxID_YES) {
    // Use left nozzle size
} else {
    // Use right nozzle size
}
```

---

## Phase 1: Font Styling Fixes

### Goal
Match the font styling of the Line width section in Process > Quality settings.

### Understanding the Target Style

In `src/slic3r/GUI/Tab.cpp` line 2289:
```cpp
optgroup = page->new_optgroup(L("Line width"), L"param_line_width");
```

The optgroup title uses the default OptionsGroup title font. Sub-items like "First layer" use the label font.

### Step 1.1: Fix "Print Core Configuration" Title Font

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: Lines 38-41

**Current Code**:
```cpp
// Title - use Label class with Body_10 font to match sidebar style
auto* title = new Label(this, Label::Body_10, _L("Print Core Configuration"));
sizer->Add(title, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
```

**Problem**: The title should use the same font as OptionsGroup titles (like "Line width"), not Body_10.

**Solution**: 
The OptionsGroup titles use `Label::Head_14` (bold, larger). Let's verify this by checking how optgroup titles are rendered.

Looking at `src/slic3r/GUI/OG_CustomCtrl.cpp` line 54:
```cpp
m_font = Label::Body_14;  // This is the font used in custom controls
```

And `src/slic3r/GUI/OptionsGroup.hpp` line 127-128:
```cpp
wxFont          sidetext_font {wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT) };
wxFont          label_font {wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT) };
```

The title font for optgroups is actually set in the static box. Looking at `src/slic3r/GUI/Widgets/LabeledStaticBox.cpp` line 12:
```cpp
m_font = Label::Head_14;
```

**Implementation**:
```cpp
// BEFORE (line 40):
auto* title = new Label(this, Label::Body_10, _L("Print Core Configuration"));

// AFTER:
auto* title = new Label(this, Label::Head_14, _L("Print Core Configuration"));
title->SetForegroundColour(StateColor::darkModeColorFor(wxColour(38, 46, 48)));
```

**Why**: 
- `Head_14` is the bold, 14pt font used for section titles
- The foreground color matches the dark mode compatible color used elsewhere

### Step 1.2: Fix "Print Core 1/2" Label Font

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: Lines 117-121

**Current Code**:
```cpp
// Print Core label - use Label class with Body_10 to match sidebar style
wxString label_text = wxString::Format(_L("Print Core %d"), (int)(i + 1));
auto* label = new Label(this, Label::Body_10, label_text);
row_sizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
```

**Problem**: These labels should match the style of sub-item labels like "First layer" in the Line width section.

**Solution**:
Looking at how option labels are rendered in `src/slic3r/GUI/OptionsGroup.cpp`, they use `label_font` which defaults to system font but is often set to `Body_14`.

```cpp
// BEFORE (line 120):
auto* label = new Label(this, Label::Body_10, label_text);

// AFTER:
auto* label = new Label(this, Label::Body_13, label_text);
label->SetForegroundColour(StateColor::darkModeColorFor(wxColour(50, 58, 61)));
```

**Why**:
- `Body_13` matches the size of option labels in the sidebar
- The color #323A3D (50, 58, 61) is the standard text color used for labels

### Step 1.3: Add Spacing for Visual Hierarchy

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: After line 41

**Add**:
```cpp
// Add separator line to match optgroup visual style
auto* line = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(200), 1));
line->SetForegroundColour(wxColour(220, 220, 220));
sizer->Add(line, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(5));
```

**Why**: The Line width section has a visual separator. Adding this makes the print core section look consistent.

---

## Phase 2: Core Size Mismatch Validation

### Goal
Prevent users from selecting incompatible core sizes (e.g., AA 0.4 and AA 0.8) by showing a dialog similar to Bambu H2D.

### Understanding the Problem

When a user selects:
- Print Core 1: AA 0.4
- Print Core 2: AA 0.8

This is invalid because UltiMaker printers require both cores to have the same nozzle size for dual extrusion.

### Step 2.1: Add Helper Method to Extract Size from Core String

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: After line 260 (before the closing brace of the class)

**Add this private method**:

```cpp
// Extract nozzle size from core string (e.g., "AA 0.4" -> "0.4")
wxString ExtruderVariantWidget::extract_size_from_core(const wxString& core)
{
    // Split the core string by space
    wxArrayString parts = wxSplit(core, ' ');
    if (parts.size() >= 2) {
        return parts[parts.size() - 1];  // Last part is the size
    }
    return wxString();  // Return empty if format is invalid
}
```

**Why**: We need to compare just the size portion (0.4, 0.6, etc.) regardless of the core type (AA, BB, CC).

### Step 2.2: Add Method to Check for Size Mismatch

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: After extract_size_from_core method

**Add**:

```cpp
// Check if selected cores have different nozzle sizes
bool ExtruderVariantWidget::has_size_mismatch(wxString& size1, wxString& size2)
{
    if (m_extruder_variants.size() < 2) {
        return false;  // Single extruder, no mismatch possible
    }
    
    // Get current selections from combo boxes
    wxString core1 = m_extruder_variants[0]->GetValue();
    wxString core2 = m_extruder_variants[1]->GetValue();
    
    // Extract sizes
    size1 = extract_size_from_core(core1);
    size2 = extract_size_from_core(core2);
    
    // Compare sizes
    return !size1.IsEmpty() && !size2.IsEmpty() && size1 != size2;
}
```

**Why**: This method checks if the two selected cores have different sizes and returns the sizes for the dialog.

### Step 2.3: Add Method to Show Mismatch Dialog

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: After has_size_mismatch method

**Add**:

```cpp
// Show dialog asking user to select which core size to use
// Returns true if user made a selection, false if cancelled
bool ExtruderVariantWidget::show_mismatch_dialog(const wxString& size1, const wxString& size2)
{
    // Get full core names for the dialog
    wxString core1 = m_extruder_variants[0]->GetValue();
    wxString core2 = m_extruder_variants[1]->GetValue();
    
    // Create dialog similar to Bambu H2D pattern
    MessageDialog dlg(this,
                      _L("The selected print cores have different nozzle sizes. "
                         "UltiMaker printers require both print cores to have the same nozzle size "
                         "for dual extrusion printing.\n\n"
                         "Please select which nozzle size you would like to use for both cores."),
                      _L("Print Core Size Mismatch"),
                      wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    
    // Set custom button labels showing core options
    dlg.SetButtonLabel(wxID_YES, wxString::Format(_L("Use %s (%smm)"), core1, size1));
    dlg.SetButtonLabel(wxID_NO, wxString::Format(_L("Use %s (%smm)"), core2, size2));
    
    // Show dialog and handle result
    int result = dlg.ShowModal();
    
    if (result == wxID_YES) {
        // User chose to use size from core 1
        update_core_size(1, size1);  // Update core 2 to match size1
        return true;
    } else if (result == wxID_NO) {
        // User chose to use size from core 2
        update_core_size(0, size2);  // Update core 1 to match size2
        return true;
    }
    
    // User cancelled (shouldn't happen with YES_NO dialog, but handle anyway)
    return false;
}
```

**Why**: This follows the exact pattern from Bambu H2D but adapted for print cores instead of nozzles.

### Step 2.4: Add Method to Update Core Size

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: After show_mismatch_dialog method

**Add**:

```cpp
// Update a specific core to a new size while preserving its type
void ExtruderVariantWidget::update_core_size(int extruder_idx, const wxString& new_size)
{
    if (extruder_idx < 0 || extruder_idx >= m_extruder_variants.size()) {
        return;
    }
    
    // Get current core value
    wxString current_core = m_extruder_variants[extruder_idx]->GetValue();
    
    // Extract type (first part before space)
    wxArrayString parts = wxSplit(current_core, ' ');
    if (parts.size() < 2) {
        return;  // Invalid format
    }
    
    wxString core_type = parts[0];  // e.g., "AA", "BB", "CC"
    
    // Build new core string with same type but new size
    wxString new_core = wxString::Format("%s %s", core_type, new_size);
    
    // Update the combo box
    m_extruder_variants[extruder_idx]->SetValue(new_core);
    
    // Trigger the change event to update config
    wxCommandEvent evt(wxEVT_COMBOBOX);
    evt.SetEventObject(m_extruder_variants[extruder_idx]);
    on_variant_changed(evt);
}
```

**Why**: When the user selects a size, we need to update the other core to match while keeping the same core type (AA, BB, etc.).

### Step 2.5: Add Header Declarations

**File**: `src/slic3r/GUI/ExtruderVariantWidget.hpp`
**Location**: In the private section, after line 45

**Add**:

```cpp
    // Helper methods for size mismatch detection
    wxString extract_size_from_core(const wxString& core);
    bool has_size_mismatch(wxString& size1, wxString& size2);
    bool show_mismatch_dialog(const wxString& size1, const wxString& size2);
    void update_core_size(int extruder_idx, const wxString& new_size);
```

**Why**: These methods need to be declared in the header so they can be called from the implementation.

### Step 2.6: Call Validation in on_variant_changed

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: In on_variant_changed method, after line 145

**Current code at line 145**:
```cpp
// Update the config
wxGetApp().preset_bundle->printers.get_edited_preset().set_dirty();
```

**After this line, add**:

```cpp
    // Check for size mismatch after updating config
    wxString size1, size2;
    if (has_size_mismatch(size1, size2)) {
        // Temporarily disconnect the event handler to prevent recursion
        for (auto* combo : m_extruder_variants) {
            combo->Unbind(wxEVT_COMBOBOX, &ExtruderVariantWidget::on_variant_changed, this);
        }
        
        // Show dialog and update if user made a selection
        show_mismatch_dialog(size1, size2);
        
        // Reconnect event handlers
        for (auto* combo : m_extruder_variants) {
            combo->Bind(wxEVT_COMBOBOX, &ExtruderVariantWidget::on_variant_changed, this);
        }
    }
```

**Why**: 
- We check for mismatch after the config is updated
- We temporarily unbind events to prevent infinite recursion (updating a combo triggers on_variant_changed)
- After showing the dialog, we rebind the events

---

## Phase 3: Print Core -> Process Selection Linkage

### Goal
When the user changes print core type (AA vs BB vs CC), filter the available print process presets based on `compatible_printers`.

### Understanding the Problem

Process presets have a `compatible_printers` option that lists which printer configurations they work with. For UltiMaker:
- AA cores are for standard printing
- BB cores are for support material
- CC cores are for abrasive materials

When a user selects a BB core, we should only show process presets compatible with BB cores.

### Step 3.1: Add Method to Get Controlling Core Index

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: After update_core_size method

**Add**:

```cpp
// Determine which print core controls process selection based on printer type
// Returns: 0 for Print Core 1, 1 for Print Core 2
int ExtruderVariantWidget::get_controlling_core_index()
{
    // Get current printer preset
    const Preset& printer_preset = wxGetApp().preset_bundle->printers.get_edited_preset();
    const std::string& printer_name = printer_preset.name;
    
    // Factor 4 uses Print Core 2 for process selection
    if (printer_name.find("Factor 4") != std::string::npos) {
        return 1;  // Print Core 2
    }
    
    // S Series (S3, S5, S6, S7, S8) use Print Core 1 for process selection
    if (printer_name.find("S3") != std::string::npos ||
        printer_name.find("S5") != std::string::npos ||
        printer_name.find("S6") != std::string::npos ||
        printer_name.find("S7") != std::string::npos ||
        printer_name.find("S8") != std::string::npos) {
        return 0;  // Print Core 1
    }
    
    // Default to Print Core 1 for unknown UltiMaker printers
    return 0;
}
```

**Why**: Different printer families use different logic for which core controls the process selection.

### Step 3.2: Add Method to Extract Core Type

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: After get_controlling_core_index method

**Add**:

```cpp
// Extract core type from core string (e.g., "AA 0.4" -> "AA")
wxString ExtruderVariantWidget::extract_type_from_core(const wxString& core)
{
    wxArrayString parts = wxSplit(core, ' ');
    if (parts.size() >= 1) {
        return parts[0];  // First part is the type
    }
    return wxString();
}
```

**Why**: We need to know if the core is AA, BB, or CC to filter processes.

### Step 3.3: Add Method to Update Process Selection

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: After extract_type_from_core method

**Add**:

```cpp
// Update available process presets based on selected print core
void ExtruderVariantWidget::update_process_presets()
{
    // Get the controlling core
    int controlling_idx = get_controlling_core_index();
    if (controlling_idx < 0 || controlling_idx >= m_extruder_variants.size()) {
        return;
    }
    
    // Get the selected core type
    wxString selected_core = m_extruder_variants[controlling_idx]->GetValue();
    wxString core_type = extract_type_from_core(selected_core);
    
    if (core_type.IsEmpty()) {
        return;
    }
    
    // Get the preset bundle
    PresetBundle* preset_bundle = wxGetApp().preset_bundle;
    if (!preset_bundle) {
        return;
    }
    
    // Get current printer name
    const std::string& printer_name = preset_bundle->printers.get_edited_preset().name;
    
    // Filter process presets based on core type
    // This is done by updating the compatible_printers filter
    PresetCollection& prints = preset_bundle->prints;
    
    // Iterate through process presets and update compatibility
    for (size_t i = 0; i < prints.size(); ++i) {
        Preset& preset = prints.get_preset(i);
        if (preset.is_system || preset.is_user) {
            // Check if this preset is compatible with the selected core type
            // by examining its compatible_printers option
            const ConfigOptionStrings* compatible = preset.config.option<ConfigOptionStrings>("compatible_printers");
            if (compatible) {
                // Update preset visibility based on core compatibility
                // This is a simplified version - actual implementation may need
                // to interact with the preset combo box directly
            }
        }
    }
    
    // Notify the UI to refresh preset lists
    // This will be implemented based on how OrcaSlicer handles preset updates
    wxGetApp().plater()->sidebar().update_presets();
}
```

**Note**: This is a placeholder implementation. The actual implementation needs to interact with the preset system more deeply. See the "Common Pitfalls" section for more details.

### Step 3.4: Add Header Declarations

**File**: `src/slic3r/GUI/ExtruderVariantWidget.hpp`
**Location**: In the private section, after the previous declarations

**Add**:

```cpp
    // Helper methods for process selection linkage
    int get_controlling_core_index();
    wxString extract_type_from_core(const wxString& core);
    void update_process_presets();
```

### Step 3.5: Call Update in on_variant_changed

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: In on_variant_changed method, after the size mismatch check

**Add**:

```cpp
    // Update process presets based on selected core type
    update_process_presets();
```

---

## Phase 4: Printer-Specific Control Logic

### Goal
Implement the logic where:
- S Series (S3, S5, S6, S7, S8): Process selection controlled by Print Core 1
- Factor 4: Process selection controlled by Print Core 2

### Understanding

This is already partially implemented in Phase 3.1 with `get_controlling_core_index()`. However, we need to ensure the UI reflects this.

### Step 4.1: Add Visual Indicator for Controlling Core

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: In create_widgets method, around line 117

**Current code**:
```cpp
wxString label_text = wxString::Format(_L("Print Core %d"), (int)(i + 1));
```

**Replace with**:

```cpp
wxString label_text;
int controlling_idx = get_controlling_core_index();
if (i == controlling_idx) {
    // This is the controlling core - add indicator
    label_text = wxString::Format(_L("Print Core %d (controls process)"), (int)(i + 1));
} else {
    label_text = wxString::Format(_L("Print Core %d"), (int)(i + 1));
}
```

**Why**: This gives users visual feedback about which core is controlling their process selection.

### Step 4.2: Update Visual Indicator When Printer Changes

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: Add new method

**Add**:

```cpp
// Update labels when printer changes to show which core controls process
void ExtruderVariantWidget::update_controlling_core_indicator()
{
    int controlling_idx = get_controlling_core_index();
    
    for (size_t i = 0; i < m_extruder_variants.size(); ++i) {
        // Find the label for this row (it's the first child of the row sizer)
        wxSizer* row_sizer = m_extruder_variants[i]->GetContainingSizer();
        if (!row_sizer) continue;
        
        wxSizerItem* label_item = row_sizer->GetItem((size_t)0);
        if (!label_item) continue;
        
        Label* label = dynamic_cast<Label*>(label_item->GetWindow());
        if (!label) continue;
        
        // Update label text
        wxString label_text;
        if ((int)i == controlling_idx) {
            label_text = wxString::Format(_L("Print Core %d (controls process)"), (int)(i + 1));
            // Make controlling core label bold
            label->SetFont(Label::Body_13.Bold());
        } else {
            label_text = wxString::Format(_L("Print Core %d"), (int)(i + 1));
            label->SetFont(Label::Body_13);
        }
        label->SetLabel(label_text);
    }
    
    Layout();
    Refresh();
}
```

**Why**: When the user switches between S Series and Factor 4 printers, the UI needs to update to show which core is now controlling.

### Step 4.3: Add Header Declaration

**File**: `src/slic3r/GUI/ExtruderVariantWidget.hpp`
**Location**: In private section

**Add**:

```cpp
    void update_controlling_core_indicator();
```

### Step 4.4: Call Update When Printer Changes

This requires connecting to the printer preset change event. Add this in the constructor or initialization:

**File**: `src/slic3r/GUI/ExtruderVariantWidget.cpp`
**Location**: In constructor (around line 30)

**Add after creating widgets**:

```cpp
    // Listen for printer preset changes to update controlling core indicator
    wxGetApp().preset_bundle->printers.set_selection_changed_callback([this]() {
        wxGetApp().CallAfter([this]() {
            update_controlling_core_indicator();
        });
    });
```

**Why**: The `CallAfter` ensures the update happens on the main thread after the preset change is complete.

---

## Testing Guide

### Test Case 1: Font Styling

**Steps**:
1. Open OrcaSlicer
2. Select an UltiMaker S6 printer
3. Look at the Print Core Configuration section in the sidebar

**Expected**:
- "Print Core Configuration" title should match the style of "Line width" in Process > Quality
- "Print Core 1" and "Print Core 2" should match the style of "First layer" sub-item
- There should be a visual separator line under the title

### Test Case 2: Core Size Mismatch Detection

**Steps**:
1. Select UltiMaker S6 printer
2. Set Print Core 1 to "AA 0.4"
3. Set Print Core 2 to "AA 0.8"

**Expected**:
- A dialog appears with message about size mismatch
- Dialog shows two options: "Use AA 0.4 (0.4mm)" and "Use AA 0.8 (0.8mm)"
- Selecting an option updates both cores to the selected size
- Core types are preserved (AA stays AA, BB stays BB)

### Test Case 3: Same Size Cores

**Steps**:
1. Select UltiMaker S6 printer
2. Set Print Core 1 to "AA 0.4"
3. Set Print Core 2 to "BB 0.4"

**Expected**:
- No dialog appears (different types with same size is OK)
- Both cores retain their selections

### Test Case 4: S Series Process Control

**Steps**:
1. Select UltiMaker S6 printer
2. Observe Print Core labels

**Expected**:
- "Print Core 1" label should show "(controls process)" indicator
- "Print Core 1" label should be bold
- "Print Core 2" should be normal weight

### Test Case 5: Factor 4 Process Control

**Steps**:
1. Select UltiMaker Factor 4 printer
2. Observe Print Core labels

**Expected**:
- "Print Core 2" label should show "(controls process)" indicator
- "Print Core 2" label should be bold
- "Print Core 1" should be normal weight

### Test Case 6: Printer Switching

**Steps**:
1. Select UltiMaker S6 (note which core controls)
2. Switch to UltiMaker Factor 4

**Expected**:
- The controlling core indicator should switch from Print Core 1 to Print Core 2
- Labels should update immediately

---

## Common Pitfalls

### Pitfall 1: Event Recursion

**Problem**: When updating a combo box value programmatically, it triggers `on_variant_changed`, which can lead to infinite recursion.

**Solution**: Always unbind the event handler before programmatic changes:

```cpp
combo->Unbind(wxEVT_COMBOBOX, &ExtruderVariantWidget::on_variant_changed, this);
combo->SetValue(new_value);
combo->Bind(wxEVT_COMBOBOX, &ExtruderVariantWidget::on_variant_changed, this);
```

### Pitfall 2: Config Not Persisting

**Problem**: Changes to `printer_extruder_variant` don't seem to save.

**Solution**: Ensure you call:
```cpp
wxGetApp().preset_bundle->printers.get_edited_preset().set_dirty();
```

This marks the preset as modified so it gets saved.

### Pitfall 3: Font Not Applied

**Problem**: Font changes don't appear in the UI.

**Solution**: After setting a font, you may need to call:
```cpp
label->Refresh();
label->Update();
```

Or for the entire widget:
```cpp
Layout();
Refresh();
```

### Pitfall 4: Process Preset Filtering

**Problem**: The process preset filtering in Phase 3 is complex and may not work as expected.

**Reality**: The actual implementation of process preset filtering based on print cores requires deeper integration with the preset system. The provided code is a starting point but may need refinement based on how OrcaSlicer handles preset compatibility.

**Recommendation**: For Phase 3, focus on:
1. Getting the controlling core index correctly
2. Updating the UI indicator
3. The actual preset filtering may need to be handled differently, possibly through the existing `compatible_printers` mechanism

### Pitfall 5: Thread Safety

**Problem**: UI updates from callbacks cause crashes or assertion failures.

**Solution**: Always use `wxGetApp().CallAfter()` to schedule UI updates on the main thread:

```cpp
wxGetApp().CallAfter([this]() {
    // UI update code here
});
```

### Pitfall 6: Dark Mode Compatibility

**Problem**: Hardcoded colors look wrong in dark mode.

**Solution**: Use `StateColor::darkModeColorFor()`:

```cpp
label->SetForegroundColour(StateColor::darkModeColorFor(wxColour(50, 58, 61)));
```

---

## Reference Code

### Complete Header File (ExtruderVariantWidget.hpp)

```cpp
#ifndef EXTRUDER_VARIANT_WIDGET_HPP
#define EXTRUDER_VARIANT_WIDGET_HPP

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/splitter.h>

#include <vector>

class ExtruderVariantWidget : public wxPanel
{
public:
    ExtruderVariantWidget(wxWindow* parent);
    ~ExtruderVariantWidget();

    void create_widgets();
    void update_ui_from_config();

private:
    void on_variant_changed(wxCommandEvent& evt);
    
    // Helper methods for size mismatch detection
    wxString extract_size_from_core(const wxString& core);
    bool has_size_mismatch(wxString& size1, wxString& size2);
    bool show_mismatch_dialog(const wxString& size1, const wxString& size2);
    void update_core_size(int extruder_idx, const wxString& new_size);
    
    // Helper methods for process selection linkage
    int get_controlling_core_index();
    wxString extract_type_from_core(const wxString& core);
    void update_process_presets();
    void update_controlling_core_indicator();

    std::vector<wxComboBox*> m_extruder_variants;
};

#endif // EXTRUDER_VARIANT_WIDGET_HPP
```

### Key Code Patterns

**Parsing Core String**:
```cpp
wxArrayString parts = wxSplit(core, ' ');
wxString type = parts[0];   // "AA"
wxString size = parts[1];   // "0.4"
```

**Creating a Dialog**:
```cpp
MessageDialog dlg(this, message, title, wxYES_NO | wxICON_WARNING);
dlg.SetButtonLabel(wxID_YES, "Option 1");
dlg.SetButtonLabel(wxID_NO, "Option 2");
int result = dlg.ShowModal();
```

**Updating Config**:
```cpp
DynamicPrintConfig& config = wxGetApp().preset_bundle->printers.get_edited_preset().config;
ConfigOptionStrings* variants = config.option<ConfigOptionStrings>("printer_extruder_variant");
if (variants) {
    variants->values[extruder_idx] = new_value;
}
wxGetApp().preset_bundle->printers.get_edited_preset().set_dirty();
```

**Font Styling**:
```cpp
// Title style (bold, larger)
label->SetFont(Label::Head_14);

// Normal label style
label->SetFont(Label::Body_13);

// Bold label style
label->SetFont(Label::Body_13.Bold());

// Dark mode compatible color
label->SetForegroundColour(StateColor::darkModeColorFor(wxColour(50, 58, 61)));
```

---

## Summary

This implementation plan provides:

1. **Font fixes** to match the Line width section styling
2. **Size mismatch validation** with a user-friendly dialog
3. **Process selection linkage** framework (requires further refinement)
4. **Printer-specific control logic** with visual indicators

### Implementation Order

1. Start with Phase 1 (Font fixes) - easiest and most visible
2. Then Phase 2 (Size mismatch) - adds important validation
3. Then Phase 4 (Control logic) - adds visual indicators
4. Finally Phase 3 (Process linkage) - most complex, may need iteration

### Questions?

If you encounter issues:
1. Check the Common Pitfalls section
2. Verify your code against the Reference Code section
3. Test incrementally - don't implement everything at once
4. Use the Bambu H2D code in Plater.cpp as a working reference

Good luck!
