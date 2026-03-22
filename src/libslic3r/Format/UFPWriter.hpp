#ifndef slic3r_UFPWriter_hpp_
#define slic3r_UFPWriter_hpp_

#include "GCodeContainerWriter.hpp"

namespace Slic3r {

class UFPWriter : public GCodeContainerWriter {
public:
    UFPWriter(const PrinterFormatConfig& config) : GCodeContainerWriter(config) {}
    
    // Set print statistics directly (bypass G-code parsing)
    void set_print_stats(int duration_s, double filament_mm, double filament_g) {
        m_duration_s = duration_s;
        m_filament_mm = filament_mm;
        m_filament_g = filament_g;
        m_has_stats = true;
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
    
private:
    std::string generate_slicemetadata_json(const GCodeMetadata& meta);
    std::string generate_ufp_global_json(const GCodeMetadata& meta);
    std::string generate_material_xml(const GCodeMetadata& meta);
    std::string generate_content_types_xml();
    std::string generate_rels_xml();
    std::string generate_gcode_rels_xml(bool has_thumbnail = false);
    std::string generate_build_date();
};

} // namespace Slic3r

#endif
