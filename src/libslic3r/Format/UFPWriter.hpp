#ifndef slic3r_UFPWriter_hpp_
#define slic3r_UFPWriter_hpp_

#include "ContainerWriterContext.hpp"
#include "FormatConfig.hpp"
#include "GCodeContainerWriter.hpp"

namespace Slic3r {

class UFPWriter : public GCodeContainerWriter {
public:
    UFPWriter(const PrinterFormatConfig& config) : GCodeContainerWriter(config) {}
    
    // Set print statistics directly (bypass G-code parsing)
    void set_print_stats(int duration_s, double filament_mm, double filament_g) {
        m_context.set_print_stats(duration_s, filament_mm, filament_g);
    }
    
    // Add a thumbnail to the container
    void add_thumbnail(const std::vector<uint8_t>& data, const std::string& filename) {
        m_context.add_thumbnail(data, filename);
    }
    
    // Override set_thumbnail_data to route to m_context (fixes thumbnail bug)
    // The base class method stores in m_thumbnail_data which UFPWriter never reads
    void set_thumbnail_data(const std::vector<uint8_t>& png_data) override {
        if (!png_data.empty()) {
            m_context.add_thumbnail(png_data, "thumbnail.png");
        }
    }
    
    // Set extruder variants for multi-extruder support
    // variants: List of extruder variant names (e.g., ["AA 0.4", "BB 0.4"])
    void set_extruder_variants(const std::vector<std::string>& variants) {
        m_context.set_extruder_variants(variants);
    }
    
    // Set per-extruder data for multi-extruder UFP export
    // idx: 0 for first extruder, 1 for second extruder
    void set_extruder_data(int idx, const ExtruderData& data) {
        m_context.set_extruder_data(idx, data);
    }
    
    // Check if any extruder data has been set
    bool has_extruder_data() const {
        return m_context.has_any_extruder_data();
    }
    
protected:
    void override_metadata(GCodeMetadata& meta) override;
    std::string generate_header(const GCodeMetadata& meta) override;
    bool write_container(const GCodeMetadata& meta, const std::string& gcode_content, const std::string& output_path) override;
    
private:
    ContainerWriterContext m_context;
    
private:
    // Helper to load nozzle variants from JSON file
    std::map<std::string, std::pair<std::string, std::string>> load_nozzle_variants();
    
    // Helper to get diameter and display name from variant name
    std::pair<std::string, std::string> get_nozzle_info(const std::string& variant_name);
    std::string generate_slicemetadata_json(const GCodeMetadata& meta);
    std::string generate_ufp_global_json(const GCodeMetadata& meta);
    std::string generate_material_xml(const GCodeMetadata& meta);
    std::string generate_content_types_xml();
    std::string generate_rels_xml();
    std::string generate_gcode_rels_xml(bool has_thumbnail = false, const std::string& material_filename = "");
    std::string generate_build_date();
    
    // Helper to generate extruder metadata block
    std::string generate_extruder_block(int idx, const ExtruderData& data);
};

} // namespace Slic3r

#endif
