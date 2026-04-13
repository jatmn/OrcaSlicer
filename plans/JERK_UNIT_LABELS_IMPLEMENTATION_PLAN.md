# Implementation Plan: Jerk Unit Labels Update for Cheetah G-code Flavor

## Overview

When the G-code flavor is set to "Cheetah (UltiMaker)", the jerk settings require different unit labels. Standard jerk units are "mm/s" but Cheetah uses a proprietary jerk format with a 500,000x conversion factor, requiring the label "mm/s² × 500,000".

## Problem Analysis

### Original Broken Implementation
The original `update_jerk_unit_labels()` function had two fundamental flaws:

1. **Called non-existent method**: `get_page()` doesn't exist on Tab class
2. **Modified a copy**: `get_option()` returns a copy of ConfigOption, not a reference

### Correct Approach
Update field labels by:
1. Getting the Field pointer via `Tab::get_field(opt_key, &page, opt_index)`
2. Accessing the underlying widget via `Field::getWindow()`
3. Casting to the appropriate widget type (TextInput* or SpinInput*)
4. Calling `SetLabel()` on the widget

## Architecture Reference

### Widget Hierarchy
```
Tab
├── m_pages: std::vector<std::unique_ptr<Page>>
│   └── Page
│       ├── m_optgroups: std::vector<ConfigOptionsGroup*>
│       └── ConfigOptionsGroup
│           └── m_fields: std::map<std::string, t_field>
│               └── Field (unique_ptr)
│                   └── window member (TextInput* or SpinInput*)
```

### Field Types for Jerk Options
- `machine_max_jerk_x/y/z/e` are `coFloats` type
- These use `TextCtrl` field class (defined in Field.hpp line 305)
- `TextCtrl::BUILD()` creates `TextInput` widget (Field.cpp line 828)
- `TextInput::SetLabel()` updates the side text label

### Key Methods

| Method | Location | Purpose |
|--------|----------|---------|
| `Tab::get_field(opt_key, Page**, opt_index)` | Tab.cpp:1419 | Get Field* and Page* for option |
| `Field::getWindow()` | Field.hpp:252 | Get underlying widget |
| `TextInput::SetLabel(wxString)` | TextInput.hpp:46 | Update side label text |
| `SpinInput::SetLabel(wxString)` | SpinInput.hpp:55 | Update side label text |

## Implementation Steps

### Step 1: Locate Tab.cpp Function Implementation Area
Find where `update_jerk_unit_labels()` should be implemented in Tab.cpp, likely near other G-code flavor related functions.

### Step 2: Implement the Function

```cpp
void Tab::update_jerk_unit_labels(GCodeFlavor flavor)
{
    // Determine the appropriate label based on G-code flavor
    wxString jerk_label = (flavor == gcfCheetah) 
        ? wxString::FromUTF8("mm/s² × 500,000")
        : wxString::FromUTF8("mm/s");
    
    // Jerk field names to update
    const std::vector<std::string> jerk_fields = {
        "machine_max_jerk_x",
        "machine_max_jerk_y", 
        "machine_max_jerk_z",
        "machine_max_jerk_e"
    };
    
    // Update each jerk field's label
    for (const auto& field_name : jerk_fields) {
        Page* page = nullptr;
        Field* field = get_field(field_name, &page);
        
        if (field && page) {
            wxWindow* window = field->getWindow();
            if (window) {
                // TextCtrl fields use TextInput widget
                TextInput* text_input = dynamic_cast<TextInput*>(window);
                if (text_input) {
                    text_input->SetLabel(jerk_label);
                }
            }
        }
    }
}
```

### Step 3: Add Function Declaration in Tab.hpp
Add declaration in the appropriate section:

```cpp
void update_jerk_unit_labels(GCodeFlavor flavor);
```

### Step 4: Call Site Integration
Call this function when G-code flavor changes. This should be in the `on_value_change` handler or similar callback for the `gcode_flavor` option.

### Step 5: Include Required Headers
Ensure Tab.cpp includes the necessary widget headers:
- `#include "Widgets/TextInput.hpp"` (likely already included)

## Testing Considerations

1. **Manual Testing:**
   - Open OrcaSlicer
   - Change G-code flavor to "Cheetah (UltiMaker)"
   - Verify jerk field labels change to "mm/s² × 500,000"
   - Change to different G-code flavor
   - Verify jerk field labels revert to "mm/s"

2. **Edge Cases:**
   - Field doesn't exist (returns nullptr)
   - Page hasn't been created yet
   - Widget type is unexpected

## Files to Modify

| File | Changes |
|------|---------|
| `src/slic3r/GUI/Tab.cpp` | Implement `update_jerk_unit_labels()` |
| `src/slic3r/GUI/Tab.hpp` | Add function declaration |

## Notes

- The function should be called when `gcode_flavor` option changes value
- Must be called after pages/fields are built (not during initialization)
- The label uses UTF-8 encoding for the ² superscript and × multiplication sign
