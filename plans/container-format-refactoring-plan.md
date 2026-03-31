# Container Format Refactoring Plan

## Current Architecture Issues

### 1. Confusing Naming
- `build_ufp_container()` handles BOTH UFP and MakerBot formats
- Function name implies UFP-only but it's actually a generic container builder

### 2. Thumbnail Generation Mismatch
- Format config defines 7 thumbnails for MakerBot
- Code only generates 3 with wrong sizes
- No centralized thumbnail size management

### 3. Code Duplication
- Thumbnail generation logic scattered
- Similar container export patterns in multiple places
- Format-specific logic mixed with generic logic

## Proposed Refactoring

### Phase 1: Fix Immediate Issue (Thumbnail Generation)

**File**: `src/slic3r/GUI/BackgroundSlicingProcess.cpp`

**Changes**:
1. Update `build_ufp_container()` to generate all 7 MakerBot thumbnails
2. Use `render_thumbnails()` callback for proper sizing
3. Match thumbnail sizes to format config exactly

### Phase 2: Rename and Consolidate

**Step 1: Rename Function**
- `build_ufp_container()` → `build_container_format()`
- Update all call sites

**Step 2: Create ContainerFormatHelper Class**

```cpp
// New file: src/libslic3r/Format/ContainerFormatHelper.hpp

class ContainerFormatHelper {
public:
    // Load thumbnail requirements from format config
    static std::vector<ThumbnailRequirement> load_thumbnail_requirements(
        const std::string& format_type,
        const std::string& config_id
    );
    
    // Generate thumbnails for a format
    static std::vector<std::pair<std::vector<uint8_t>, std::string>> generate_thumbnails(
        const std::vector<ThumbnailRequirement>& requirements,
        ThumbnailRenderer& renderer,
        const ThumbnailData& cached_data
    );
    
    // Common container export logic
    static bool export_container(
        const std::string& format_type,
        const std::string& input_gcode_path,
        const std::string& output_path,
        const ContainerMetadata& metadata,
        const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails
    );
};
```

**Step 3: Move Common Logic**
- Extract thumbnail generation from `BackgroundSlicingProcess`
- Move to `ContainerFormatHelper` or `FormatConfig`
- Keep format-specific writers (UFPWriter, MakerBotWriter) separate

### Phase 3: Clean Up Writers

**Current Writers (keep separate - they handle different formats):**
- `UFPWriter` - UltiMaker format with OPC structure
- `MakerBotWriter` - MakerBot format with ZIP structure

**Potential Base Class Improvements:**
- `GCodeContainerWriter` already exists as base class
- Could add more shared utilities for:
  - ZIP file creation
  - JSON metadata generation
  - Thumbnail embedding

## Implementation Steps

### Step 1: Fix Thumbnail Generation (Immediate)

```cpp
// In BackgroundSlicingProcess::build_ufp_container()

// Replace current (wrong) thumbnail list:
const std::vector<std::pair<int, std::string>> makerbot_thumbnails = {
    {320, "isometric_thumbnail_320x320.png"},
    {160, "isometric_thumbnail_160x160.png"},  // WRONG
    {80, "isometric_thumbnail_80x80.png"}       // WRONG
};

// With correct list from format config:
const std::vector<std::pair<int, std::string>> makerbot_thumbnails = {
    {120, "isometric_thumbnail_120x120.png"},
    {320, "isometric_thumbnail_320x320.png"},
    {640, "isometric_thumbnail_640x640.png"},
    {90, "thumbnail_90x90.png"},
    {140, "thumbnail_140x106.png"},
    {212, "thumbnail_212x300.png"},
    {960, "thumbnail_960x1460.png"}
};

// Use render_thumbnails() for proper sizing
std::vector<Vec2d> thumbnail_sizes;
for (const auto& [size, filename] : makerbot_thumbnails) {
    thumbnail_sizes.emplace_back(size, size);
}
ThumbnailsParams params{thumbnail_sizes, true, true, true, true, 0};
ThumbnailsList rendered_thumbs = this->render_thumbnails(params);
```

### Step 2: Rename Function

```cpp
// Old:
bool build_ufp_container(const std::string& gcode_path, 
                         const std::string& output_path,
                         const std::string& format_type,  // "ufp" or "makerbot"
                         std::string& error_message);

// New:
bool build_container_format(const std::string& gcode_path,
                            const std::string& output_path,
                            const std::string& format_type,  // "ufp" or "makerbot"
                            std::string& error_message);
```

### Step 3: Create Helper Class

Create `src/libslic3r/Format/ContainerFormatHelper.hpp` and `.cpp`:

```cpp
#pragma once

#include <string>
#include <vector>
#include <utility>
#include "libslic3r/ThumbnailData.hpp"

namespace Slic3r {

struct ThumbnailRequirement {
    int width;
    int height;
    std::string filename;
};

class ContainerFormatHelper {
public:
    // Load thumbnail requirements from format config JSON
    static std::vector<ThumbnailRequirement> load_thumbnail_requirements(
        const std::string& format_type,
        const std::string& config_id
    );
    
    // Generate thumbnails using the render callback
    static std::vector<std::pair<std::vector<uint8_t>, std::string>> generate_thumbnails(
        const std::vector<ThumbnailRequirement>& requirements,
        std::function<ThumbnailsList(const ThumbnailsParams&)> render_callback
    );
    
    // Validate that generated thumbnails match requirements
    static bool validate_thumbnails(
        const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails,
        const std::vector<ThumbnailRequirement>& requirements
    );
};

} // namespace Slic3r
```

### Step 4: Update FormatConfig

Add method to load thumbnail requirements:

```cpp
// In FormatConfig.hpp
static std::vector<ThumbnailConfig> get_thumbnail_configs(
    const std::string& format_type,
    const std::string& config_id
);
```

## Files to Modify

1. **Immediate fix**:
   - `src/slic3r/GUI/BackgroundSlicingProcess.cpp` - Fix thumbnail generation

2. **Refactoring**:
   - `src/slic3r/GUI/BackgroundSlicingProcess.cpp` - Rename function
   - `src/slic3r/GUI/BackgroundSlicingProcess.hpp` - Update declaration
   - `src/libslic3r/Format/ContainerFormatHelper.hpp` - New file
   - `src/libslic3r/Format/ContainerFormatHelper.cpp` - New file
   - `src/libslic3r/Format/FormatConfig.hpp` - Add thumbnail config loading
   - `src/libslic3r/Format/FormatConfig.cpp` - Implement thumbnail config loading

3. **Call sites to update**:
   - Search for `build_ufp_container` calls and update to `build_container_format`

## Benefits

1. **Clearer naming** - Function name matches its actual purpose
2. **Centralized thumbnail management** - Single source of truth for thumbnail requirements
3. **Easier to add new formats** - Just add format config, no code changes needed
4. **Consistent behavior** - All container formats use same thumbnail generation logic
5. **Maintainability** - Less duplicated code, clearer separation of concerns

## Backward Compatibility

- Function rename requires updating call sites
- No changes to file format output (same .ufp and .makerbot files)
- No changes to external APIs
- Internal refactoring only
