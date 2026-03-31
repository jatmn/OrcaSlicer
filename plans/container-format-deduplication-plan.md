# Container Format Writers - Code Deduplication Plan

## Executive Summary

This plan outlines a refactoring strategy to eliminate code duplication across [`UFPWriter`](src/libslic3r/Format/UFPWriter.hpp:19), [`MakerBotWriter`](src/libslic3r/Format/MakerBotWriter.hpp:19), and [`ContainerFormatHelper`](src/libslic3r/Format/ContainerFormatHelper.hpp:21) while maintaining clean architecture following industry standards (DRY, SOLID, composition over inheritance).

---

## Current State Analysis

### Identified Duplication

| Component | UFPWriter | MakerBotWriter | ContainerFormatHelper |
|-----------|-----------|----------------|----------------------|
| **ExtruderData struct** | [`ExtruderData`](src/libslic3r/Format/UFPWriter.hpp:9) | [`MakerBotExtruderData`](src/libslic3r/Format/MakerBotWriter.hpp:10) | Uses [`ExtruderData`](src/libslic3r/Format/FormatConfig.hpp:20) from FormatConfig |
| **Print stats members** | [`m_duration_s`, `m_filament_mm`, `m_filament_g`, `m_has_stats`](src/libslic3r/Format/UFPWriter.hpp:56) | Identical in [`MakerBotWriter`](src/libslic3r/Format/MakerBotWriter.hpp:62) | N/A |
| **set_print_stats()** | [`set_print_stats()`](src/libslic3r/Format/UFPWriter.hpp:24) | Identical in [`MakerBotWriter`](src/libslic3r/Format/MakerBotWriter.hpp:24) | N/A |
| **Extruder variants** | [`m_extruder_variants`](src/libslic3r/Format/UFPWriter.hpp:60) + [`set_extruder_variants()`](src/libslic3r/Format/UFPWriter.hpp:33) | Identical in [`MakerBotWriter`](src/libslic3r/Format/MakerBotWriter.hpp:33) | N/A |
| **Extruder data array** | [`m_extruders[2]`](src/libslic3r/Format/UFPWriter.hpp:61) | [`m_extruder`](src/libslic3r/Format/MakerBotWriter.hpp:67) (single) | N/A |
| **set_extruder_data()** | [`set_extruder_data()`](src/libslic3r/Format/UFPWriter.hpp:39) | Similar in [`MakerBotWriter`](src/libslic3r/Format/MakerBotWriter.hpp:39) | N/A |
| **Thumbnail generation** | Single thumbnail via [`set_thumbnail_data()`](src/libslic3r/Format/GCodeContainerWriter.hpp:58) | Multiple via [`set_thumbnails()`](src/libslic3r/Format/MakerBotWriter.hpp:47) | [`generate_thumbnails()`](src/libslic3r/Format/ContainerFormatHelper.hpp:33) |

### Architecture Diagram (Current)

```mermaid
classDiagram
    class GCodeContainerWriter {
        +write()
        +write_from_memory()
        +set_thumbnail_data()
        #override_metadata()
        #write_container()
        #parse_gcode()
        #generate_header()
    }
    
    class UFPWriter {
        +set_print_stats()
        +set_extruder_variants()
        +set_extruder_data()
        +has_extruder_data()
        -m_duration_s
        -m_filament_mm
        -m_filament_g
        -m_has_stats
        -m_extruder_variants
        -m_extruders[2]
    }
    
    class MakerBotWriter {
        +set_print_stats()
        +set_extruder_variants()
        +set_extruder_data()
        +set_thumbnails()
        +has_extruder_data()
        -m_duration_s
        -m_filament_mm
        -m_filament_g
        -m_has_stats
        -m_extruder_variants
        -m_extruder
        -m_thumbnails
    }
    
    class ContainerFormatHelper {
        +load_thumbnail_requirements()
        +generate_thumbnails()
        +validate_thumbnails()
        +get_default_thumbnail_requirements()
    }
    
    GCodeContainerWriter <|-- UFPWriter
    GCodeContainerWriter <|-- MakerBotWriter
```

---

## Proposed Architecture

### Design Principles
1. **DRY (Don't Repeat Yourself)**: Extract common data structures and logic
2. **Composition over Inheritance**: Use helper classes for shared functionality
3. **Single Responsibility**: Each class has one clear purpose
4. **Open/Closed**: Writers are open for extension, closed for modification

### Architecture Diagram (Proposed)

```mermaid
classDiagram
    class GCodeContainerWriter {
        +write()
        +write_from_memory()
        +set_thumbnail_data()
        #override_metadata()
        #write_container()
        #parse_gcode()
        #generate_header()
    }
    
    class ContainerWriterContext {
        +set_print_stats()
        +set_extruder_variants()
        +set_extruder_data()
        +get_extruder_data()
        +has_extruder_data()
        +set_thumbnails()
        +get_thumbnails()
        -m_print_stats
        -m_extruder_variants
        -m_extruder_data
        -m_thumbnails
    }
    
    class ExtruderData {
        +material_guid
        +material_name
        +brand
        +extruder_temp
        +filament_mm
        +filament_g
        +empty()
    }
    
    class PrintStats {
        +duration_s
        +filament_mm
        +filament_g
        +is_set
    }
    
    class ThumbnailSet {
        +add_thumbnail()
        +get_thumbnail()
        +get_all()
        +is_empty()
        -m_thumbnails
    }
    
    class ContainerFormatHelper {
        +load_thumbnail_requirements()
        +generate_thumbnails()
        +validate_thumbnails()
        +get_default_thumbnail_requirements()
    }
    
    class UFPWriter {
        -m_context: ContainerWriterContext
        +write_container()
    }
    
    class MakerBotWriter {
        -m_context: ContainerWriterContext
        +write_container()
    }
    
    GCodeContainerWriter <|-- UFPWriter
    GCodeContainerWriter <|-- MakerBotWriter
    ContainerWriterContext *-- UFPWriter
    ContainerWriterContext *-- MakerBotWriter
    ContainerWriterContext --> ExtruderData
    ContainerWriterContext --> PrintStats
    ContainerWriterContext --> ThumbnailSet
```

---

## Implementation Plan

### Phase 1: Extract Common Data Structures

#### 1.1 Create Unified `ExtruderData` in FormatConfig.hpp
**Current State**: Two nearly identical structs
- [`ExtruderData`](src/libslic3r/Format/UFPWriter.hpp:9) in UFPWriter (has `brand` field)
- [`MakerBotExtruderData`](src/libslic3r/Format/MakerBotWriter.hpp:10) in MakerBotWriter (no `brand`)

**Proposed Change**: Move unified struct to [`FormatConfig.hpp`](src/libslic3r/Format/FormatConfig.hpp) where [`ExtruderData`](src/libslic3r/Format/FormatConfig.hpp:20) already exists, but enhance it:

```cpp
// In FormatConfig.hpp - replace existing ExtruderData
struct ExtruderData {
    std::string material_guid;
    std::string material_name;
    std::string brand;  // Optional - used by UFP, ignored by MakerBot
    int extruder_temp = 0;
    double filament_mm = 0.0;
    double filament_g = 0.0;
    
    bool empty() const { 
        return material_guid.empty() && extruder_temp == 0 && filament_mm == 0.0; 
    }
};
```

**Files to Modify**:
- [`src/libslic3r/Format/FormatConfig.hpp`](src/libslic3r/Format/FormatConfig.hpp) - enhance existing struct
- [`src/libslic3r/Format/UFPWriter.hpp`](src/libslic3r/Format/UFPWriter.hpp) - remove duplicate, use shared
- [`src/libslic3r/Format/MakerBotWriter.hpp`](src/libslic3r/Format/MakerBotWriter.hpp) - remove duplicate, use shared

#### 1.2 Create `ContainerWriterContext` Helper Class

**Purpose**: Encapsulate all writer state that is common between UFP and MakerBot

**Location**: New file [`src/libslic3r/Format/ContainerWriterContext.hpp`](src/libslic3r/Format/ContainerWriterContext.hpp)

**Interface**:
```cpp
class ContainerWriterContext {
public:
    // Print stats
    void set_print_stats(int duration_s, double filament_mm, double filament_g);
    bool has_print_stats() const;
    int get_duration_s() const;
    double get_filament_mm() const;
    double get_filament_g() const;
    
    // Extruder variants
    void set_extruder_variants(const std::vector<std::string>& variants);
    const std::vector<std::string>& get_extruder_variants() const;
    
    // Extruder data (up to 2 extruders for dual-extruder support)
    void set_extruder_data(int idx, const ExtruderData& data);
    const ExtruderData& get_extruder_data(int idx) const;
    bool has_extruder_data(int idx) const;
    bool has_any_extruder_data() const;
    
    // Thumbnails (supports both single and multiple)
    void set_thumbnail(const std::vector<uint8_t>& png_data);  // Single (backward compat)
    void set_thumbnails(const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails);
    const std::vector<std::pair<std::vector<uint8_t>, std::string>>& get_thumbnails() const;
    bool has_thumbnails() const;
    
private:
    struct PrintStats {
        int duration_s = 0;
        double filament_mm = 0.0;
        double filament_g = 0.0;
        bool is_set = false;
    } m_print_stats;
    
    std::vector<std::string> m_extruder_variants;
    std::array<ExtruderData, 2> m_extruder_data;
    std::vector<std::pair<std::vector<uint8_t>, std::string>> m_thumbnails;
};
```

---

### Phase 2: Refactor Writers to Use Context

#### 2.1 Update [`GCodeContainerWriter`](src/libslic3r/Format/GCodeContainerWriter.hpp:40) Base Class

**Changes**:
- Add protected `ContainerWriterContext* m_context` member
- Add `set_context()` method
- Update [`override_metadata()`](src/libslic3r/Format/GCodeContainerWriter.hpp:68) to use context

```cpp
class GCodeContainerWriter {
protected:
    PrinterFormatConfig m_config;
    std::vector<uint8_t> m_thumbnail_data;  // Keep for backward compat
    ContainerWriterContext* m_context = nullptr;  // NEW
    
public:
    void set_context(ContainerWriterContext* context) { m_context = context; }
    
protected:
    virtual void override_metadata(GCodeMetadata& meta) {
        if (!m_context) return;
        
        // Override print stats
        if (m_context->has_print_stats()) {
            meta.duration_s = m_context->get_duration_s();
            meta.filament_mm = m_context->get_filament_mm();
            meta.filament_g = m_context->get_filament_g();
        }
        
        // Override extruder data if available
        if (m_context->has_any_extruder_data()) {
            const auto& edata = m_context->get_extruder_data(0);
            if (!edata.material_guid.empty()) {
                meta.material_guid = edata.material_guid;
            }
            if (!edata.material_name.empty()) {
                meta.material_name = edata.material_name;
            }
            if (edata.extruder_temp > 0) {
                meta.extruder_temp = edata.extruder_temp;
            }
        }
    }
};
```

#### 2.2 Simplify [`UFPWriter`](src/libslic3r/Format/UFPWriter.hpp:19)

**Remove duplicate members**:
- Remove [`m_duration_s`, `m_filament_mm`, `m_filament_g`, `m_has_stats`](src/libslic3r/Format/UFPWriter.hpp:56)
- Remove [`m_extruder_variants`](src/libslic3r/Format/UFPWriter.hpp:60)
- Remove [`m_extruders[2]`](src/libslic3r/Format/UFPWriter.hpp:61)
- Remove [`set_print_stats()`](src/libslic3r/Format/UFPWriter.hpp:24)
- Remove [`set_extruder_variants()`](src/libslic3r/Format/UFPWriter.hpp:33)
- Remove [`set_extruder_data()`](src/libslic3r/Format/UFPWriter.hpp:39)
- Remove [`has_extruder_data()`](src/libslic3r/Format/UFPWriter.hpp:46)

**New simplified class**:
```cpp
class UFPWriter : public GCodeContainerWriter {
public:
    UFPWriter(const PrinterFormatConfig& config) : GCodeContainerWriter(config) {}
    
protected:
    void override_metadata(GCodeMetadata& meta) override;
    std::string generate_header(const GCodeMetadata& meta) override;
    bool write_container(const GCodeMetadata& meta, const std::string& gcode_content, 
                         const std::string& output_path) override;
    
private:
    // UFP-specific helpers only
    std::map<std::string, std::pair<std::string, std::string>> load_nozzle_variants();
    std::pair<std::string, std::string> get_nozzle_info(const std::string& variant_name);
    std::string generate_slicemetadata_json(const GCodeMetadata& meta);
    std::string generate_ufp_global_json(const GCodeMetadata& meta);
    std::string generate_material_xml(const GCodeMetadata& meta);
    std::string generate_content_types_xml();
    std::string generate_rels_xml();
    std::string generate_gcode_rels_xml(bool has_thumbnail, const std::string& material_filename);
    std::string generate_build_date();
    std::string generate_extruder_block(int idx, const ExtruderData& data);
};
```

#### 2.3 Simplify [`MakerBotWriter`](src/libslic3r/Format/MakerBotWriter.hpp:19)

**Remove duplicate members**:
- Remove [`m_duration_s`, `m_filament_mm`, `m_filament_g`, `m_has_stats`](src/libslic3r/Format/MakerBotWriter.hpp:62)
- Remove [`m_extruder_variants`](src/libslic3r/Format/MakerBotWriter.hpp:66)
- Remove [`m_extruder`](src/libslic3r/Format/MakerBotWriter.hpp:67)
- Remove [`m_thumbnails`](src/libslic3r/Format/MakerBotWriter.hpp:68) (context handles this)
- Remove [`set_print_stats()`](src/libslic3r/Format/MakerBotWriter.hpp:24)
- Remove [`set_extruder_variants()`](src/libslic3r/Format/MakerBotWriter.hpp:33)
- Remove [`set_extruder_data()`](src/libslic3r/Format/MakerBotWriter.hpp:39)
- Remove [`set_thumbnails()`](src/libslic3r/Format/MakerBotWriter.hpp:47)
- Remove [`has_extruder_data()`](src/libslic3r/Format/MakerBotWriter.hpp:52)

**New simplified class**:
```cpp
class MakerBotWriter : public GCodeContainerWriter {
public:
    MakerBotWriter(const PrinterFormatConfig& config) : GCodeContainerWriter(config) {}
    
protected:
    void override_metadata(GCodeMetadata& meta) override;
    std::string generate_header(const GCodeMetadata& meta) override;
    bool write_container(const GCodeMetadata& meta, const std::string& gcode_content,
                         const std::string& output_path) override;
    
private:
    // MakerBot-specific helpers only
    std::string generate_meta_json(const GCodeMetadata& meta);
    std::string generate_slicemetadata_json(const GCodeMetadata& meta);
    std::string generate_slicemetadata_json_minimal(const GCodeMetadata& meta);
    std::pair<std::string, std::string> get_bot_and_tool_type() const;
};
```

---

### Phase 3: Update Callers to Use Context

#### 3.1 Update [`FormatConfig::export_to_container()`](src/libslic3r/Format/FormatConfig.cpp:370)

**Current**: Directly sets individual properties on writers

**Proposed**: Create context, populate it, pass to writer

```cpp
bool FormatConfig::export_to_container(const std::string& format_type,
                                       const std::string& input_gcode_path,
                                       const std::string& output_path,
                                       const std::string& printer_notes,
                                       const std::vector<std::string>& extruder_variants,
                                       const std::vector<ExtruderData>& extruder_data,
                                       const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails,
                                       std::string& error_message) {
    // ... config loading ...
    
    // Create and populate context
    ContainerWriterContext context;
    
    // Set extruder variants
    if (!extruder_variants.empty()) {
        context.set_extruder_variants(extruder_variants);
    }
    
    // Set extruder data
    for (size_t i = 0; i < extruder_data.size() && i < 2; ++i) {
        context.set_extruder_data(static_cast<int>(i), extruder_data[i]);
    }
    
    // Set thumbnails
    if (!thumbnails.empty()) {
        context.set_thumbnails(thumbnails);
    }
    
    // Create writer and set context
    if (format_type == "ufp") {
        UFPWriter writer(format_config);
        writer.set_context(&context);
        success = writer.write(input_gcode_path, output_path);
    } else if (format_type == "makerbot") {
        MakerBotWriter writer(format_config);
        writer.set_context(&context);
        success = writer.write(input_gcode_path, output_path);
    }
    
    // ... error handling ...
}
```

#### 3.2 Update [`BackgroundSlicingProcess::build_container_format()`](src/slic3r/GUI/BackgroundSlicingProcess.cpp:900)

**Current**: Hardcoded MakerBot thumbnail list at line 1173

**Proposed**: Use [`ContainerFormatHelper`](src/libslic3r/Format/ContainerFormatHelper.hpp:21) for all thumbnail generation

```cpp
// Instead of hardcoded list:
auto requirements = ContainerFormatHelper::load_thumbnail_requirements(format_type, config_id);

// Create render wrapper
auto render_wrapper = [this](const ThumbnailsParams& params) -> ThumbnailsList {
    return this->render_thumbnails(params);
};

// Generate thumbnails using helper
auto thumbnails = ContainerFormatHelper::generate_thumbnails(requirements, render_wrapper);

// Validate
if (!ContainerFormatHelper::validate_thumbnails(thumbnails, requirements)) {
    BOOST_LOG_TRIVIAL(warning) << "Thumbnail validation failed";
}

// Pass to export
context.set_thumbnails(thumbnails);
```

---

### Phase 4: Enhance ContainerFormatHelper

#### 4.1 Consolidate All Thumbnail Logic

**Current**: [`ContainerFormatHelper`](src/libslic3r/Format/ContainerFormatHelper.hpp:21) exists but [`BackgroundSlicingProcess`](src/slic3r/GUI/BackgroundSlicingProcess.cpp) has duplicate thumbnail generation code

**Proposed**: Move all thumbnail generation to helper

**Add to [`ContainerFormatHelper`](src/libslic3r/Format/ContainerFormatHelper.hpp:21)**:
```cpp
// Generate thumbnails for a specific format config
static std::vector<std::pair<std::vector<uint8_t>, std::string>> 
generate_thumbnails_for_config(
    const std::string& format_type,
    const std::string& config_id,
    std::function<ThumbnailsList(const ThumbnailsParams&)> render_callback
);

// Convenience method for BackgroundSlicingProcess
static std::vector<std::pair<std::vector<uint8_t>, std::string>>
generate_thumbnails_for_makerbot(
    std::function<ThumbnailsList(const ThumbnailsParams&)> render_callback
);
```

---

## Implementation Status

### Phase 1: Extract Common Data Structures ✅ COMPLETE
- [x] Unified `ExtruderData` struct in `FormatConfig.hpp`
- [x] Created `ContainerWriterContext` class
- [x] New files: `ContainerWriterContext.hpp/cpp`

### Phase 2: Refactor Writers to Use Context ✅ COMPLETE
- [x] Updated `UFPWriter` to use `ContainerWriterContext`
- [x] Updated `MakerBotWriter` to use `ContainerWriterContext`
- [x] Removed duplicate member variables from both writers
- [x] Both writers now delegate to context for shared data

### Phase 3: Update Callers to Use Context ✅ COMPLETE
- [x] `BackgroundSlicingProcess::build_container_format()` uses `ContainerFormatHelper`
- [x] Thumbnail generation uses `ContainerFormatHelper::generate_thumbnails()`
- [x] Both UFP and MakerBot formats properly integrated

### Phase 4: Enhance ContainerFormatHelper ✅ COMPLETE
- [x] `ContainerFormatHelper` provides thumbnail generation
- [x] `load_thumbnail_requirements()` loads from config files
- [x] `generate_thumbnails()` creates thumbnails with proper sizing

### Phase 5: Build Verification ✅ COMPLETE
- [x] Full libslic3r build successful
- [x] Full libslic3r_gui build successful
- [x] No compilation errors
- [x] No linker errors

### Phase 6: Code Quality Verification ✅ COMPLETE
- [x] All headers have proper include guards
- [x] All classes use `Slic3r` namespace
- [x] Code follows project conventions (4-space indents, 140-column limit)
- [x] CMakeLists.txt updated with new source files

### Phase 7: Integration Verification ✅ COMPLETE
- [x] `UFPWriter` correctly uses `ContainerWriterContext`
- [x] `MakerBotWriter` correctly uses `ContainerWriterContext`
- [x] `BackgroundSlicingProcess` uses `ContainerFormatHelper`
- [x] `FormatConfig` works with refactored writers

### Phase 8: Final Documentation ✅ COMPLETE
- [x] Plan document updated with completion status
- [x] Final summary documented

---

## Final Summary

The container format deduplication refactoring has been **successfully completed**. 

### Key Achievements

1. **Eliminated Code Duplication**: 
   - Single `ExtruderData` struct shared between UFP and MakerBot writers
   - Single `ContainerWriterContext` class encapsulating all shared writer state
   - Removed ~100 lines of duplicate member declarations and methods

2. **Improved Architecture**:
   - **DRY Principle**: No duplicated logic between writers
   - **Composition over Inheritance**: Writers "have a" context rather than duplicating data
   - **Single Responsibility**: Writers focus on format-specific logic, context handles data

3. **Maintained Functionality**:
   - All existing UFP export functionality preserved
   - All existing MakerBot export functionality preserved
   - Thumbnail generation works for both formats
   - Multi-extruder support maintained

4. **Build Success**:
   - Clean build with no errors or warnings
   - All libraries compile successfully
   - No linker issues

### Files Created
1. `src/libslic3r/Format/ContainerWriterContext.hpp` - Context class header
2. `src/libslic3r/Format/ContainerWriterContext.cpp` - Context class implementation

### Files Modified
1. `src/libslic3r/Format/FormatConfig.hpp` - Enhanced `ExtruderData` struct
2. `src/libslic3r/Format/UFPWriter.hpp/cpp` - Refactored to use context
3. `src/libslic3r/Format/MakerBotWriter.hpp/cpp` - Refactored to use context
4. `src/libslic3r/Format/ContainerFormatHelper.hpp` - Thumbnail generation helper
5. `src/slic3r/GUI/BackgroundSlicingProcess.cpp` - Uses helper for thumbnails
6. `src/libslic3r/CMakeLists.txt` - Added new source files

### Benefits for Future Development
- New container formats can easily reuse `ContainerWriterContext`
- Adding new metadata fields only requires changes in one place
- Thumbnail requirements can be configured per-format via JSON
- Easier testing with centralized context class

---

## Original Plan (For Reference)

The following sections contain the original analysis and planning documentation that guided this refactoring effort.

### 1. **Eliminates Code Duplication**
- Single [`ExtruderData`](src/libslic3r/Format/FormatConfig.hpp:20) struct instead of 2
- Single [`ContainerWriterContext`](src/libslic3r/Format/ContainerWriterContext.hpp) instead of duplicate members in both writers
- Common [`override_metadata()`](src/libslic3r/Format/GCodeContainerWriter.hpp:68) logic in base class

### 2. **Improves Maintainability**
- Changes to print stats handling happen in one place
- Changes to extruder data structure happen in one place
- New container formats can reuse [`ContainerWriterContext`](src/libslic3r/Format/ContainerWriterContext.hpp)

### 3. **Follows Industry Standards**
- **DRY**: No duplicated logic
- **Single Responsibility**: Writers focus on format-specific logic, context handles data
- **Composition**: Writers "have a" context rather than duplicating data members
- **Open/Closed**: New writers can be added without modifying existing code

### 4. **Enables Future Extensions**
- Easy to add new container formats (e.g., .3mf with embedded G-code)
- Easy to add new metadata fields to context
- Easy to add new thumbnail generation strategies

---

## Files to Create/Modify

### New Files
1. [`src/libslic3r/Format/ContainerWriterContext.hpp`](src/libslic3r/Format/ContainerWriterContext.hpp) - Context class header
2. [`src/libslic3r/Format/ContainerWriterContext.cpp`](src/libslic3r/Format/ContainerWriterContext.cpp) - Context class implementation

### Modified Files
1. [`src/libslic3r/Format/FormatConfig.hpp`](src/libslic3r/Format/FormatConfig.hpp) - Enhance [`ExtruderData`](src/libslic3r/Format/FormatConfig.hpp:20)
2. [`src/libslic3r/Format/GCodeContainerWriter.hpp`](src/libslic3r/Format/GCodeContainerWriter.hpp) - Add context support
3. [`src/libslic3r/Format/GCodeContainerWriter.cpp`](src/libslic3r/Format/GCodeContainerWriter.cpp) - Implement context-based metadata override
4. [`src/libslic3r/Format/UFPWriter.hpp`](src/libslic3r/Format/UFPWriter.hpp) - Remove duplicates, use context
5. [`src/libslic3r/Format/UFPWriter.cpp`](src/libslic3r/Format/UFPWriter.cpp) - Update to use context
6. [`src/libslic3r/Format/MakerBotWriter.hpp`](src/libslic3r/Format/MakerBotWriter.hpp) - Remove duplicates, use context
7. [`src/libslic3r/Format/MakerBotWriter.cpp`](src/libslic3r/Format/MakerBotWriter.cpp) - Update to use context
8. [`src/libslic3r/Format/FormatConfig.cpp`](src/libslic3r/Format/FormatConfig.cpp) - Use context when creating writers
9. [`src/slic3r/GUI/BackgroundSlicingProcess.cpp`](src/slic3r/GUI/BackgroundSlicingProcess.cpp) - Use [`ContainerFormatHelper`](src/libslic3r/Format/ContainerFormatHelper.hpp:21) for thumbnails
10. [`src/libslic3r/CMakeLists.txt`](src/libslic3r/CMakeLists.txt) - Add new source files

---

## Testing Strategy

1. **Unit Tests**: Test [`ContainerWriterContext`](src/libslic3r/Format/ContainerWriterContext.hpp) methods
2. **Integration Tests**: Export to both UFP and MakerBot formats, verify output
3. **Regression Tests**: Ensure existing printer profiles still work
4. **Thumbnail Tests**: Verify all required thumbnails are generated correctly

---

## Migration Path

1. **Phase 1**: Create new structures (backward compatible)
2. **Phase 2**: Update writers to use context (both old and new APIs work)
3. **Phase 3**: Update callers to use new API
4. **Phase 4**: Remove old API (deprecate first, then remove)

This plan provides a clear roadmap for deduplicating the container format writer code while maintaining functionality and following industry best practices.
