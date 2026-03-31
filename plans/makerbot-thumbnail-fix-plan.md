# MakerBot Multi-Thumbnail Fix Plan

## Problem Summary
The MakerBot format requires **7 thumbnails** (as defined in `sketch_small.json`), but the current implementation only passes **1 thumbnail** to the MakerBotWriter. This causes the .makerbot files to be missing thumbnails that the printer expects.

## Root Cause
- `BackgroundSlicingProcess::build_ufp_container` only generates and passes a single thumbnail
- `FormatConfig::export_to_container` only accepts a single thumbnail vector
- `MakerBotWriter` only writes a single thumbnail

## Solution Overview
Update the code to support multiple thumbnails for MakerBot format while keeping UFP format unchanged (it only needs 1 thumbnail).

## Implementation Steps

### Step 1: Update MakerBot Machine Profiles
**Files:**
- `resources/profiles/UltiMaker/machine/MakerBot Sketch 0.40.json`
- `resources/profiles/UltiMaker/machine/MakerBot Sketch Large 0.40.json`
- `resources/profiles/UltiMaker/machine/MakerBot Sketch Sprint 0.40.json`

**Change:**
Add the `thumbnails` array with all 7 sizes that match the format config:
```json
"thumbnails": [
    "120x120",
    "320x320", 
    "640x640",
    "90x90",
    "140x106",
    "212x300",
    "960x1460"
]
```

### Step 2: Update FormatConfig.hpp
**File:** `src/libslic3r/Format/FormatConfig.hpp`

**Change:**
Add a new `export_to_container` overload that accepts multiple thumbnails:
```cpp
// New overload for multiple thumbnails (used by MakerBot)
bool export_to_container(
    const std::string& format_type,
    const std::string& input_gcode_path,
    const std::string& output_path,
    const std::string& printer_notes,
    const std::vector<std::string>& extruder_variants,
    const std::vector<ExtruderData>& extruder_data,
    const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails,  // data + filename
    std::string& error_message
);
```

### Step 3: Update FormatConfig.cpp
**File:** `src/libslic3r/Format/FormatConfig.cpp`

**Changes:**
1. Implement the new `export_to_container` overload
2. For MakerBot format: pass all thumbnails to MakerBotWriter
3. For UFP format: keep existing behavior (single thumbnail)
4. Existing single-thumbnail overload can call the new overload with a single-item vector

### Step 4: Update MakerBotWriter
**Files:**
- `src/libslic3r/Format/MakerBotWriter.hpp`
- `src/libslic3r/Format/MakerBotWriter.cpp`

**Changes:**
1. Add a new method to accept multiple thumbnails with filenames:
```cpp
void set_thumbnails(const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails);
```
2. Update `write_container()` to iterate through all thumbnails and write each one with its filename
3. Keep existing `set_thumbnail_data()` for backward compatibility

### Step 5: Update BackgroundSlicingProcess
**File:** `src/slic3r/GUI/BackgroundSlicingProcess.cpp`

**Changes:**
1. In `build_ufp_container()`, check if format is "makerbot"
2. If MakerBot: generate all 7 thumbnails using the thumbnail callback
3. If UFP: keep existing single thumbnail behavior
4. Call the appropriate `export_to_container` overload

### Step 6: Test and Validate
1. Build and test with MakerBot Sketch profile
2. Export a .makerbot file and verify it contains all 7 thumbnails
3. Test UFP export to ensure it still works with single thumbnail
4. Test regular G-code export (no thumbnails)
5. Compare thumbnail names with reference Cura output

## Backward Compatibility
- UFP format continues to use single thumbnail (no changes needed)
- Existing single-thumbnail API remains functional
- Only MakerBot format gets the multi-thumbnail treatment

## Files Modified
1. `resources/profiles/UltiMaker/machine/MakerBot Sketch 0.40.json`
2. `resources/profiles/UltiMaker/machine/MakerBot Sketch Large 0.40.json`
3. `resources/profiles/UltiMaker/machine/MakerBot Sketch Sprint 0.40.json`
4. `src/libslic3r/Format/FormatConfig.hpp`
5. `src/libslic3r/Format/FormatConfig.cpp`
6. `src/libslic3r/Format/MakerBotWriter.hpp`
7. `src/libslic3r/Format/MakerBotWriter.cpp`
8. `src/slic3r/GUI/BackgroundSlicingProcess.cpp`
