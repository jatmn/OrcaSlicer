# MakerBot Thumbnail Fix Plan

## Problem Summary

The current implementation has a **thumbnail mismatch**:

| Source | Thumbnail Count | Details |
|--------|-----------------|---------|
| **Format Config** (`sketch_small.json`) | **7 thumbnails** | 120x120, 320x320, 640x640, 90x90, 140x106, 212x300, 960x1460 |
| **Current Code** | **3 thumbnails** | 320x320, 160x160 (wrong!), 80x80 (wrong!) |
| **Good Example** | **6 thumbnails** | Missing 960x1460, has correct sizes |

## Root Cause

In `BackgroundSlicingProcess::build_ufp_container()` (lines 1186-1216):

1. **Wrong sizes**: Code generates 160x160 and 80x80 which aren't in the format config
2. **No actual resizing**: The comment "PNG compression will handle the size" is **incorrect** - `GCodeThumbnails::compress_thumbnail()` only compresses to PNG format, it doesn't resize the image data
3. **Missing thumbnails**: 4 thumbnails from the config are not generated

## Solution

### Option A: Generate All 7 Thumbnails with Proper Resizing (RECOMMENDED)

Update `BackgroundSlicingProcess::build_ufp_container()` to:
1. Define all 7 thumbnails matching the format config exactly
2. Use `render_thumbnails()` callback to generate properly sized thumbnails, OR
3. Resize the cached thumbnail data before compression using image processing

### Option B: Update Profiles to Match Reality

Update the machine profiles to only request the 3 thumbnails that are actually generated (though this may cause issues with printer compatibility).

## Implementation Steps

### Step 1: Update BackgroundSlicingProcess.cpp

Replace the current thumbnail generation (lines 1186-1216) with:

```cpp
// MakerBot format: Generate all 7 thumbnails as defined in format config
// These filenames are REQUIRED by MakerBot firmware and Digital Factory
const std::vector<std::pair<int, std::string>> makerbot_thumbnails = {
    {120, "isometric_thumbnail_120x120.png"},
    {320, "isometric_thumbnail_320x320.png"},
    {640, "isometric_thumbnail_640x640.png"},
    {90, "thumbnail_90x90.png"},
    {140, "thumbnail_140x106.png"},  // Note: height will be ~106 for aspect ratio
    {212, "thumbnail_212x300.png"},  // Note: height will be ~300 for aspect ratio
    {960, "thumbnail_960x1460.png"}    // Note: height will be ~1460 for aspect ratio
};

// Generate thumbnails using the render callback with proper sizes
std::vector<Vec2d> thumbnail_sizes;
for (const auto& [size, filename] : makerbot_thumbnails) {
    thumbnail_sizes.emplace_back(size, size);  // width = height for square thumbnails
}

ThumbnailsParams params{thumbnail_sizes, true, true, true, true, 0};
ThumbnailsList rendered_thumbs = this->render_thumbnails(params);

// Compress each rendered thumbnail to PNG
for (size_t i = 0; i < rendered_thumbs.size() && i < makerbot_thumbnails.size(); ++i) {
    if (rendered_thumbs[i]->is_valid()) {
        auto compressed = GCodeThumbnails::compress_thumbnail(*rendered_thumbs[i], GCodeThumbnailsFormat::PNG);
        if (compressed && compressed->data && compressed->size) {
            std::vector<uint8_t> png_data((uint8_t*)compressed->data,
                                          (uint8_t*)compressed->data + compressed->size);
            thumbnails.emplace_back(std::move(png_data), makerbot_thumbnails[i].second);
        }
    }
}
```

### Step 2: Verify MakerBotWriter Handles All Thumbnails

The `MakerBotWriter::write_container()` already handles multiple thumbnails correctly (lines 152-170). No changes needed there.

### Step 3: Test Against Reference

After the fix, the generated .makerbot file should contain:
- isometric_thumbnail_120x120.png
- isometric_thumbnail_320x320.png  
- isometric_thumbnail_640x640.png
- thumbnail_90x90.png
- thumbnail_140x106.png
- thumbnail_212x300.png
- thumbnail_960x1460.png

## Files to Modify

1. `src/slic3r/GUI/BackgroundSlicingProcess.cpp` - Lines 1186-1216

## Backward Compatibility

- UFP format continues to use single thumbnail (no changes)
- Existing MakerBot export will now include all required thumbnails
- No breaking changes to APIs
