#ifndef slic3r_MakerBotWriter_hpp_
#define slic3r_MakerBotWriter_hpp_

#include "GCodeContainerWriter.hpp"
#include "ContainerWriterContext.hpp"
#include "FormatConfig.hpp"
#include "miniz.h"

namespace Slic3r {

class MakerBotWriter : public GCodeContainerWriter {
public:
    MakerBotWriter(const PrinterFormatConfig& config) : GCodeContainerWriter(config) {}

    // Set print statistics directly (bypass G-code parsing)
    void set_print_stats(int duration_s, double filament_mm, double filament_g) {
        m_context.set_print_stats(duration_s, filament_mm, filament_g);
    }

    // Set extruder variants (nozzle info)
    // variants: List of extruder variant names (e.g., ["AA 0.4"])
    void set_extruder_variants(const std::vector<std::string>& variants) {
        m_context.set_extruder_variants(variants);
    }

    // Set per-extruder data for metadata
    // idx: 0 for first extruder, 1 for second (dual-extruder MakerBot models like Method X/XL)
    void set_extruder_data(int idx, const ExtruderData& data) {
        m_context.set_extruder_data(idx, data);
    }

    // Set multiple thumbnails with their filenames (for MakerBot format)
    // thumbnails: Vector of pairs containing (PNG data, filename)
    void set_thumbnails(const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails) {
        m_context.set_thumbnails(thumbnails);
    }

    // Check if extruder data has been set
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
    std::string generate_meta_json(const GCodeMetadata& meta);
    std::string generate_slicemetadata_json(const GCodeMetadata& meta);
    std::string generate_slicemetadata_json_minimal(const GCodeMetadata& meta);

    // Helper to infer bot_type and tool_type from FORMAT_CONFIG_ID
    std::pair<std::string, std::string> get_bot_and_tool_type() const;
};

} // namespace Slic3r

#endif
