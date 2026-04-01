#ifndef slic3r_GCodeContainerWriter_hpp_
#define slic3r_GCodeContainerWriter_hpp_

#include "FormatConfig.hpp"
#include <string>
#include <vector>
#include <map>
#include <boost/filesystem/path.hpp>

namespace Slic3r {

struct GCodeMetadata {
    // Print stats
    int duration_s;
    double filament_mm;
    double filament_g;
    double layer_height;
    int infill_percent;
    
    // Temperatures
    int extruder_temp;
    int bed_temp;
    
    // Material
    std::string material_name;
    std::string material_type;
    std::string material_guid;
    
    // Geometry
    double min_x, min_y, min_z;
    double max_x, max_y, max_z;
    
    // Slice UUID
    std::string slice_uuid;
    
    // Original G-code lines (from purge sequence onward)
    std::vector<std::string> gcode_body;
};

class GCodeContainerWriter {
protected:
    PrinterFormatConfig m_config;
    // Optional thumbnail PNG data (passed separately, NOT extracted from gcode)
    std::vector<uint8_t> m_thumbnail_data;
    
public:
    GCodeContainerWriter(const PrinterFormatConfig& config) : m_config(config) {}
    virtual ~GCodeContainerWriter() = default;
    
    // Main entry point: write from G-code file to output container
    bool write(const std::string& input_gcode_path, const std::string& output_path);
    
    // Write from G-code data in memory (for direct upload flow)
    bool write_from_memory(const std::string& gcode_data, const std::string& output_path);
    
    // Set thumbnail PNG data (pass directly, never extracted from gcode)
    // This is the CORRECT way - thumbnails should NEVER be embedded in gcode comments
    // Marked virtual so subclasses can override (e.g., UFPWriter routes to m_context)
    virtual void set_thumbnail_data(const std::vector<uint8_t>& png_data) { m_thumbnail_data = png_data; }
    
    // Check if thumbnail data is available
    bool has_thumbnail_data() const { return !m_thumbnail_data.empty(); }
    
protected:
    // Internal write method from lines
    bool write_from_lines(const std::vector<std::string>& lines, const std::string& output_path);
    
    // Override parsed metadata with injected values (default: no override, subclasses can override)
    virtual void override_metadata(GCodeMetadata& meta) {}
    
    // Write container with pre-built G-code content (implemented by derived classes)
    virtual bool write_container(const GCodeMetadata& meta, const std::string& gcode_content, const std::string& output_path) = 0;
    
    // Parse G-code metadata
    virtual GCodeMetadata parse_gcode(const std::vector<std::string>& lines);
    
    // Generate header
    virtual std::string generate_header(const GCodeMetadata& meta) = 0;
    
    // Build final G-code content (takes original lines for Python-script-style processing)
    virtual std::string build_gcode_content(const GCodeMetadata& meta, const std::vector<std::string>& original_lines);
    
    // Template substitution
    std::string substitute_template(const std::string& templ, const std::map<std::string, std::string>& values);
    
    // Generate UUID
    std::string generate_uuid();
    
    // Parse OrcaSlicer time format (e.g., "1h 5m 10s")
    int parse_time_string(const std::string& time_str);
};

} // namespace Slic3r

#endif
