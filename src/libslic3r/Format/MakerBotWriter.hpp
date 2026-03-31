#ifndef slic3r_MakerBotWriter_hpp_
#define slic3r_MakerBotWriter_hpp_

#include "GCodeContainerWriter.hpp"
#include "miniz.h"

namespace Slic3r {

// Per-extruder metadata for MakerBot export
struct MakerBotExtruderData {
    std::string material_guid;
    std::string material_name;
    int extruder_temp = 0;
    double filament_mm = 0.0;
    double filament_g = 0.0;
    bool empty() const { return material_guid.empty() && extruder_temp == 0 && filament_mm == 0.0; }
};

class MakerBotWriter : public GCodeContainerWriter {
public:
    MakerBotWriter(const PrinterFormatConfig& config) : GCodeContainerWriter(config) {}
    
    // Set print statistics directly (bypass G-code parsing)
    void set_print_stats(int duration_s, double filament_mm, double filament_g) {
        m_duration_s = duration_s;
        m_filament_mm = filament_mm;
        m_filament_g = filament_g;
        m_has_stats = true;
    }
    
    // Set extruder variants (nozzle info)
    // variants: List of extruder variant names (e.g., ["AA 0.4"])
    void set_extruder_variants(const std::vector<std::string>& variants) {
        m_extruder_variants = variants;
    }
    
    // Set per-extruder data for metadata
    // idx: 0 for first extruder (MakerBot Sketch only has 1 extruder)
    void set_extruder_data(int idx, const MakerBotExtruderData& data) {
        if (idx >= 0 && idx < 1) {  // MakerBot only supports 1 extruder
            m_extruder = data;
        }
    }
    
    // Check if extruder data has been set
    bool has_extruder_data() const {
        return !m_extruder.empty();
    }
    
protected:
    void override_metadata(GCodeMetadata& meta) override;
    std::string generate_header(const GCodeMetadata& meta) override;
    bool write_container(const GCodeMetadata& meta, const std::string& gcode_content, const std::string& output_path) override;
    
private:
    int m_duration_s = 0;
    double m_filament_mm = 0.0;
    double m_filament_g = 0.0;
    bool m_has_stats = false;
    std::vector<std::string> m_extruder_variants;
    MakerBotExtruderData m_extruder;  // Single extruder for MakerBot Sketch
    
private:
    std::string generate_meta_json(const GCodeMetadata& meta);
    std::string generate_slicemetadata_json(const GCodeMetadata& meta);
    std::string generate_slicemetadata_json_minimal(const GCodeMetadata& meta);
    
    // Helper to infer bot_type and tool_type from FORMAT_CONFIG_ID
    std::pair<std::string, std::string> get_bot_and_tool_type() const;
};

} // namespace Slic3r

#endif
